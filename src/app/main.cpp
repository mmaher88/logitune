#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <signal.h>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QJSEngine>
#include <QJSValue>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDebug>
#include "DeviceManager.h"
#include "ProfileEngine.h"
#include "ActionExecutor.h"
#include "WindowTracker.h"
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
#include "models/ActionModel.h"
#include "models/ProfileModel.h"

int main(int argc, char *argv[])
{
    // Ignore SIGPIPE — hidraw writes to wrong interface cause EPIPE,
    // and without this the process terminates on the first failed write.
    signal(SIGPIPE, SIG_IGN);

    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        fprintf(stderr, "[Qt %d] %s:%d: %s\n", type, ctx.file ? ctx.file : "?", ctx.line, qPrintable(msg));
    });
    fprintf(stderr, "[logitune] starting...\n");
    QApplication app(argc, argv);
    fprintf(stderr, "[logitune] QApplication created\n");
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");

    // Detect dark mode from system palette
    QColor windowBg = app.palette().window().color();
    double lum = windowBg.redF() * 0.299 + windowBg.greenF() * 0.587 + windowBg.blueF() * 0.114;
    bool isDark = lum < 0.5;
    fprintf(stderr, "[logitune] palette bg: r=%.2f g=%.2f b=%.2f lum=%.2f dark=%d\n",
            windowBg.redF(), windowBg.greenF(), windowBg.blueF(), lum, isDark);

    fprintf(stderr, "[logitune] creating DeviceManager...\n");
    logitune::DeviceManager deviceManager;
    fprintf(stderr, "[logitune] creating DeviceModel...\n");
    logitune::DeviceModel deviceModel;
    deviceModel.setDeviceManager(&deviceManager);
    fprintf(stderr, "[logitune] DeviceModel ready\n");

    logitune::ButtonModel buttonModel;
    logitune::ActionModel actionModel;

    logitune::ProfileModel profileModel;
    logitune::KWinWindowTracker windowTracker;

    logitune::ProfileEngine profileEngine;

    // ActionExecutor — create uinput virtual keyboard device
    fprintf(stderr, "[logitune] creating ActionExecutor...\n");
    logitune::ActionExecutor actionExecutor;
    fprintf(stderr, "[logitune] init uinput...\n");
    if (!actionExecutor.initUinput()) {
        qWarning() << "[main] ActionExecutor: uinput init failed (no /dev/uinput access?). Keystrokes will not be injected.";
    }

    // ── Profile persistence helpers ──────────────────────────────────────────

    // Look up the ActionModel display name for a ButtonAction (reverse payload lookup)
    auto buttonActionToName = [&actionModel](const logitune::ButtonAction &ba) -> QString {
        if (ba.type == logitune::ButtonAction::Default)
            return QString();  // caller uses buttonName default
        if (ba.type == logitune::ButtonAction::GestureTrigger)
            return QStringLiteral("Gestures");
        if (ba.type == logitune::ButtonAction::Keystroke) {
            // Reverse-map payload → display name via ActionModel
            int count = actionModel.rowCount();
            for (int i = 0; i < count; ++i) {
                QModelIndex mi = actionModel.index(i);
                if (actionModel.data(mi, logitune::ActionModel::ActionTypeRole).toString() == QStringLiteral("keystroke") &&
                    actionModel.data(mi, logitune::ActionModel::PayloadRole).toString() == ba.payload) {
                    return actionModel.data(mi, logitune::ActionModel::NameRole).toString();
                }
            }
            return ba.payload; // fallback: use payload directly
        }
        return ba.payload;
    };

    // Build a ButtonAction from ButtonModel entry (for saving)
    auto buttonEntryToAction = [&actionModel](const QString &actionType, const QString &actionName)
            -> logitune::ButtonAction {
        if (actionType == QStringLiteral("default"))
            return {logitune::ButtonAction::Default, {}};
        if (actionType == QStringLiteral("gesture-trigger"))
            return {logitune::ButtonAction::GestureTrigger, {}};
        if (actionType == QStringLiteral("smartshift-toggle"))
            return {logitune::ButtonAction::Keystroke, QStringLiteral("smartshift-toggle")};
        if (actionType == QStringLiteral("keystroke")) {
            QString payload = actionModel.payloadForName(actionName);
            if (payload.isEmpty()) payload = actionName; // actionName might already be a keystroke
            return {logitune::ButtonAction::Keystroke, payload};
        }
        if (actionType == QStringLiteral("app-launch"))
            return {logitune::ButtonAction::AppLaunch, actionModel.payloadForName(actionName)};
        return {logitune::ButtonAction::Default, {}};
    };

    // Capture current device + button state into a Profile and save it via ProfileEngine
    static bool savingProfile = false;
    auto saveCurrentState = [&]() {
        if (savingProfile) return;  // prevent re-entrant save
        if (!deviceManager.deviceConnected()) return;
        if (profileEngine.activeProfileName().isEmpty()) return;
        savingProfile = true;
        logitune::Profile p = profileEngine.activeProfile();
        if (p.name.isEmpty()) p.name = QStringLiteral("Default");
        p.dpi                 = deviceManager.currentDPI();
        p.smartShiftEnabled   = deviceManager.smartShiftEnabled();
        p.smartShiftThreshold = deviceManager.smartShiftThreshold();
        p.hiResScroll         = deviceManager.scrollHiRes();
        p.scrollDirection     = deviceManager.scrollInvert() ? QStringLiteral("natural")
                                                              : QStringLiteral("standard");
        p.smoothScrolling     = !deviceManager.scrollRatchet();
        for (int i = 0; i < 7; ++i) {
            QString aType = buttonModel.actionTypeForButton(i);
            QString aName = buttonModel.actionNameForButton(i);
            p.buttons[static_cast<std::size_t>(i)] = buttonEntryToAction(aType, aName);
        }
        // Save gesture actions
        for (const auto &dir : {"up", "down", "left", "right", "click"}) {
            QString ks = deviceModel.gestureKeystroke(dir);
            if (!ks.isEmpty())
                p.gestures[dir] = {logitune::ButtonAction::Keystroke, ks};
        }
        profileEngine.updateActiveProfile(p);
        savingProfile = false;
    };

    // ── Gesture defaults (matches logid.cfg) ────────────────────────────────
    deviceModel.setGestureAction("up",    "",              "");
    deviceModel.setGestureAction("down",  "Show desktop",  "Super+D");
    deviceModel.setGestureAction("left",  "Switch desktop left",  "Ctrl+Super+Left");
    deviceModel.setGestureAction("right", "Switch desktop right", "Ctrl+Super+Right");
    deviceModel.setGestureAction("click", "Task switcher", "Super+W");

    // ── Signal wiring ────────────────────────────────────────────────────────

    // 0. When ButtonModel changes (user picks action in UI), divert/undivert the button
    //    CID lookup: buttonIndex → controlId
    static const uint16_t kButtonCids[] = {
        0x0050, 0x0051, 0x0052, 0x0053, 0x0056, 0x00C3, 0x00C4
    };
    QObject::connect(&buttonModel, &QAbstractListModel::dataChanged,
        [&buttonModel, &deviceManager](const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &) {
            int row = topLeft.row();
            if (row < 0 || row >= 7) return;
            QString actionType = buttonModel.actionTypeForButton(row);
            bool needsDivert = (actionType != "default");
            bool needsRawXY = (actionType == "gesture-trigger");
            uint16_t cid = kButtonCids[row];
            deviceManager.divertButton(cid, needsDivert, needsRawXY);
        });

    // 1. Window tracking → ProfileModel (per-app profiles in QML model)
    QObject::connect(&windowTracker, &logitune::WindowTracker::activeWindowChanged,
                     &profileModel,  &logitune::ProfileModel::setActiveByWmClass);

    // 2. Window tracking → ProfileEngine (per-app profile switching with debounce)
    QObject::connect(&windowTracker, &logitune::WindowTracker::activeWindowChanged,
        [&profileEngine](const QString &wmClass, const QString & /*title*/) {
            profileEngine.switchForApp(wmClass);
        });

    // 3. ProfileEngine → DeviceModel (keep UI in sync with active profile)
    QObject::connect(&profileEngine, &logitune::ProfileEngine::activeProfileChanged,
        [&deviceModel](const logitune::Profile &profile) {
            deviceModel.setCurrentDPI(profile.dpi);
            deviceModel.setSmartShiftState(profile.smartShiftEnabled, profile.smartShiftThreshold);
            deviceModel.setActiveProfileName(profile.name);
        });

    // 4. ProfileEngine delta → device push (HID++ calls via DeviceManager)
    QObject::connect(&profileEngine, &logitune::ProfileEngine::profileDelta,
        [&deviceManager](const logitune::ProfileDelta &delta, const logitune::Profile &profile) {
            if (delta.dpiChanged)
                deviceManager.setDPI(profile.dpi);
            if (delta.smartShiftChanged)
                deviceManager.setSmartShift(profile.smartShiftEnabled, profile.smartShiftThreshold);
            if (delta.scrollChanged)
                deviceManager.setScrollConfig(profile.hiResScroll,
                                              profile.scrollDirection == QStringLiteral("natural"));
        });

    // 4b. Device setup complete → configure per-device profile dir, load/create default profile
    QObject::connect(&deviceManager, &logitune::DeviceManager::deviceSetupComplete,
        [&]() {
            const QString configBase =
                QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            const QString profilesDir = configBase
                + QStringLiteral("/devices/")
                + QString::number(deviceManager.deviceVid(), 16)
                + QStringLiteral("-")
                + QString::number(deviceManager.devicePid(), 16)
                + QStringLiteral("-")
                + deviceManager.deviceSerial()
                + QStringLiteral("/profiles");

            QDir().mkpath(profilesDir);
            profileEngine.setDeviceConfigDir(profilesDir);
            qDebug() << "[main] profile dir:" << profilesDir;

            const QString defaultConf = profilesDir + QStringLiteral("/default.conf");
            if (!QFile::exists(defaultConf)) {
                // First connect — seed default profile from current device state
                logitune::Profile seed;
                seed.name                = QStringLiteral("Default");
                seed.dpi                 = deviceManager.currentDPI();
                seed.smartShiftEnabled   = deviceManager.smartShiftEnabled();
                seed.smartShiftThreshold = deviceManager.smartShiftThreshold();
                seed.hiResScroll         = deviceManager.scrollHiRes();
                seed.scrollDirection     = deviceManager.scrollInvert()
                    ? QStringLiteral("natural") : QStringLiteral("standard");
                seed.smoothScrolling     = !deviceManager.scrollRatchet();
                // buttons default-initialised (all Default)
                logitune::ProfileEngine::saveProfile(defaultConf, seed);
                qDebug() << "[main] created default profile at" << defaultConf;
            }

            // Load the default profile (emits activeProfileChanged + profileDelta)
            profileEngine.switchToProfile(QStringLiteral("default"));
            const logitune::Profile &p = profileEngine.activeProfile();

            // Apply to device (profileDelta handler above covers DPI/SmartShift/Scroll;
            // we also apply here directly to avoid the diff skipping identical-to-current values)
            deviceManager.setDPI(p.dpi);
            deviceManager.setSmartShift(p.smartShiftEnabled, p.smartShiftThreshold);
            deviceManager.setScrollConfig(p.hiResScroll,
                                          p.scrollDirection == QStringLiteral("natural"));

            // Restore button diversions from profile
            static const uint16_t kProfileButtonCids[] = { 0x0050, 0x0051, 0x0052, 0x0053, 0x0056, 0x00C3, 0x00C4 };
            static const QString kDefaultButtonNames[] = {
                QStringLiteral("Left click"), QStringLiteral("Right click"),
                QStringLiteral("Middle click"), QStringLiteral("Back"),
                QStringLiteral("Forward"), QStringLiteral("Gesture button"),
                QStringLiteral("Shift wheel mode")
            };
            for (int i = 0; i < 7; ++i) {
                const auto &ba = p.buttons[static_cast<std::size_t>(i)];
                bool needsDivert = (ba.type != logitune::ButtonAction::Default);
                QString aType;
                QString aName;
                switch (ba.type) {
                case logitune::ButtonAction::Default:
                    aType = QStringLiteral("default");
                    aName = kDefaultButtonNames[i];
                    break;
                case logitune::ButtonAction::GestureTrigger:
                    aType = QStringLiteral("gesture-trigger");
                    aName = QStringLiteral("Gestures");
                    break;
                case logitune::ButtonAction::Keystroke:
                    if (ba.payload == QStringLiteral("smartshift-toggle")) {
                        aType = QStringLiteral("smartshift-toggle");
                        aName = QStringLiteral("Shift wheel mode");
                    } else {
                        aType = QStringLiteral("keystroke");
                        aName = buttonActionToName(ba);
                    }
                    break;
                case logitune::ButtonAction::AppLaunch:
                    aType = QStringLiteral("app-launch");
                    aName = buttonActionToName(ba);
                    break;
                default:
                    aType = QStringLiteral("default");
                    aName = kDefaultButtonNames[i];
                    break;
                }
                buttonModel.setAction(i, aName, aType);
                if (needsDivert)
                    qDebug() << "[main] diverting button" << i << "CID" << Qt::hex << kProfileButtonCids[i] << "type:" << aType;
                bool needsRawXY = (aType == "gesture-trigger");
                deviceManager.divertButton(kProfileButtonCids[i], needsDivert, needsRawXY);
            }

            // Restore gesture actions from profile
            for (auto it = p.gestures.begin(); it != p.gestures.end(); ++it) {
                if (it->second.type == logitune::ButtonAction::Keystroke && !it->second.payload.isEmpty()) {
                    // Reverse lookup: find display name for this keystroke
                    QString name = it->second.payload;
                    int count = actionModel.rowCount();
                    for (int j = 0; j < count; ++j) {
                        QModelIndex mi = actionModel.index(j);
                        if (actionModel.data(mi, logitune::ActionModel::PayloadRole).toString() == it->second.payload) {
                            name = actionModel.data(mi, logitune::ActionModel::NameRole).toString();
                            break;
                        }
                    }
                    deviceModel.setGestureAction(it->first, name, it->second.payload);
                }
            }

            qDebug() << "[main] profile applied: DPI=" << p.dpi
                     << "SmartShift=" << p.smartShiftEnabled
                     << "scrollInvert=" << (p.scrollDirection == "natural");
        });

    // 4c. Save profile whenever device settings change
    QObject::connect(&deviceManager, &logitune::DeviceManager::currentDPIChanged,
                     [&saveCurrentState]() { saveCurrentState(); });
    QObject::connect(&deviceManager, &logitune::DeviceManager::smartShiftChanged,
                     [&saveCurrentState]() { saveCurrentState(); });
    QObject::connect(&deviceManager, &logitune::DeviceManager::scrollConfigChanged,
                     [&saveCurrentState]() { saveCurrentState(); });
    QObject::connect(&deviceManager, &logitune::DeviceManager::thumbWheelModeChanged,
                     [&saveCurrentState]() { saveCurrentState(); });
    QObject::connect(&buttonModel, &QAbstractListModel::dataChanged,
        [&saveCurrentState](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            saveCurrentState();
        });
    QObject::connect(&deviceModel, &logitune::DeviceModel::gestureChanged,
                     [&saveCurrentState]() { saveCurrentState(); });

    // 5. Diverted button press → profile action lookup → execute
    // Gesture state — accumulate raw XY deltas from HID++ while button held
    static int gestureTotalDx = 0;
    static int gestureTotalDy = 0;
    static bool gestureActive = false;
    static constexpr int kGestureThreshold = 50; // raw HID++ units

    // Accumulate raw mouse deltas from device (via ReprogControls RawXY diversion)
    QObject::connect(&deviceManager, &logitune::DeviceManager::gestureRawXY,
        [](int16_t dx, int16_t dy) {
            if (gestureActive) {
                gestureTotalDx += dx;
                gestureTotalDy += dy;
            }
        });

    QObject::connect(&deviceManager, &logitune::DeviceManager::divertedButtonPressed,
        [&buttonModel, &actionModel, &actionExecutor, &deviceManager, &deviceModel](uint16_t controlId, bool pressed) {

            // Map controlId to button index
            static const std::unordered_map<uint16_t, int> kControlMap = {
                {0x0050, 0}, {0x0051, 1}, {0x0052, 2},
                {0x0053, 3}, {0x0056, 4}, {0x00C3, 5}, {0x00C4, 6}
            };

            // Handle gesture release: CID=0 means all buttons released
            if (!pressed && gestureActive) {
                gestureActive = false;
                int dx = gestureTotalDx;
                int dy = gestureTotalDy;

                qDebug() << "[main] gesture delta: dx=" << dx << "dy=" << dy;
                QString dir;
                if (std::abs(dx) > kGestureThreshold || std::abs(dy) > kGestureThreshold) {
                    if (std::abs(dx) > std::abs(dy))
                        dir = dx > 0 ? "right" : "left";
                    else
                        dir = dy > 0 ? "down" : "up";
                } else {
                    dir = "click";
                }

                QString keystroke = deviceModel.gestureKeystroke(dir);
                if (!keystroke.isEmpty()) {
                    qDebug() << "[main] gesture" << dir << "→" << keystroke;
                    actionExecutor.injectKeystroke(keystroke);
                }
                return;
            }

            if (!pressed) return; // ignore other release events

            // Look up button
            auto it = kControlMap.find(controlId);
            if (it == kControlMap.end()) return;
            int idx = it->second;

            QString actionType = buttonModel.actionTypeForButton(idx);
            QString actionName = buttonModel.actionNameForButton(idx);

            if (actionType == "default") return;

            // Get the actual payload (keystroke combo) from ActionModel
            QString payload = actionModel.payloadForName(actionName);

            qDebug() << "[main] button" << idx << "→" << actionType << ":" << actionName << "payload:" << payload;

            if (actionType == "keystroke" && !payload.isEmpty()) {
                actionExecutor.injectKeystroke(payload);
            } else if (actionType == "smartshift-toggle") {
                // Toggle SmartShift ratchet/freespin
                bool current = deviceManager.smartShiftEnabled();
                deviceManager.setSmartShift(!current, deviceManager.smartShiftThreshold());
                qDebug() << "[main] SmartShift toggled to" << !current;
            } else if (actionType == "gesture-trigger") {
                // Record start position — direction resolved on release (above)
                gestureTotalDx = 0;
                gestureTotalDy = 0;
                gestureActive = true;
            } else if (actionType == "app-launch" && !payload.isEmpty()) {
                actionExecutor.launchApp(payload);
            }
        });

    // 6. Thumb wheel rotation → inject action based on mode
    static int thumbAccum = 0;
    static constexpr int kThumbThreshold = 15; // accumulate this many units before one action step
    QObject::connect(&deviceManager, &logitune::DeviceManager::thumbWheelRotation,
        [&deviceManager, &actionExecutor](int delta) {
            const QString &mode = deviceManager.thumbWheelMode();
            thumbAccum += delta;

            if (std::abs(thumbAccum) < kThumbThreshold)
                return; // not enough rotation yet

            int steps = thumbAccum / kThumbThreshold;
            thumbAccum %= kThumbThreshold;

            for (int i = 0; i < std::abs(steps); ++i) {
                if (mode == "volume") {
                    actionExecutor.injectKeystroke(steps < 0 ? "VolumeUp" : "VolumeDown");
                } else if (mode == "zoom") {
                    actionExecutor.injectCtrlScroll(steps < 0 ? 1 : -1);
                }
            }
        });

    // 7. Gesture event → GestureDetector → resolve direction → execute
    QObject::connect(&deviceManager, &logitune::DeviceManager::gestureEvent,
        [&actionExecutor, &deviceModel](int dx, int dy, bool released) {
            actionExecutor.gestureDetector().addDelta(dx, dy);
            if (released) {
                auto dir = actionExecutor.gestureDetector().resolve();
                actionExecutor.gestureDetector().reset();

                QString dirName;
                switch (dir) {
                    case logitune::GestureDirection::Up:    dirName = "up";    break;
                    case logitune::GestureDirection::Down:  dirName = "down";  break;
                    case logitune::GestureDirection::Left:  dirName = "left";  break;
                    case logitune::GestureDirection::Right: dirName = "right"; break;
                    case logitune::GestureDirection::Click: dirName = "click"; break;
                    default: return;
                }

                QString ks = deviceModel.gestureKeystroke(dirName);
                if (!ks.isEmpty()) {
                    qDebug() << "[main] gesture:" << dirName << "→" << ks;
                    actionExecutor.injectKeystroke(ks);
                }
            }
        });

    // ── QML engine ───────────────────────────────────────────────────────────

    fprintf(stderr, "[logitune] creating QML engine...\n");
    QQmlApplicationEngine engine;

    // Register singletons — provide existing instances
    // Expose isDark to QML — singletons can't read context properties,
    // so we register a singleton factory that returns a QJSValue
    qmlRegisterSingletonType("Logitune", 1, 0, "ThemeBridge",
        [isDark](QQmlEngine *, QJSEngine *jsEngine) -> QJSValue {
            QJSValue obj = jsEngine->newObject();
            obj.setProperty("isDark", isDark);
            return obj;
        });

    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",  &deviceModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",  &buttonModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",  &actionModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel", &profileModel);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        [](const QUrl &url) { fprintf(stderr, "QML CREATION FAILED: %s\n", qPrintable(url.toString())); });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        [](QObject *obj, const QUrl &url) { fprintf(stderr, "QML CREATED: %s obj=%p\n", qPrintable(url.toString()), obj); });

    // Debug: list QRC resources
    {
        QDirIterator it(":", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString p = it.next();
            if (p.contains("Logitune") || p.contains("Main") || p.contains("qml"))
                fprintf(stderr, "  QRC: %s\n", qPrintable(p));
        }
    }
    fprintf(stderr, "[logitune] loading QML...\n");
    engine.load(QUrl(QStringLiteral("qrc:/Logitune/qml/Main.qml")));
    fprintf(stderr, "[logitune] QML loaded, root objects: %d\n", (int)engine.rootObjects().size());

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No root objects loaded — QML module failed to initialize";
        return -1;
    }

    // Start device monitoring and window tracking after QML is loaded
    deviceManager.start();
    windowTracker.start();

    // ── System tray ──────────────────────────────────────────────────────────

    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(QIcon::fromTheme("input-mouse"));
    trayIcon.setToolTip("Logitune - MX Master 3S");

    QMenu trayMenu;
    QAction *showAction = trayMenu.addAction("Show Logitune");
    trayMenu.addSeparator();

    // Battery status (read-only text)
    QAction *batteryAction = trayMenu.addAction("Battery: ---%");
    batteryAction->setEnabled(false);

    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction("Quit");

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Connect tray actions
    QObject::connect(showAction, &QAction::triggered, [&engine]() {
        for (auto *obj : engine.rootObjects()) {
            if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });

    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

    // Left-click tray icon → show window
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated,
        [&engine](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                for (auto *obj : engine.rootObjects()) {
                    if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
                        window->show();
                        window->raise();
                        window->requestActivate();
                    }
                }
            }
        });

    // 7. Battery updates → tray text
    QObject::connect(&deviceManager, &logitune::DeviceManager::batteryLevelChanged,
        [batteryAction, &deviceManager]() {
            QString text = QString("Battery: %1%").arg(deviceManager.batteryLevel());
            if (deviceManager.batteryCharging()) text += " (charging)";
            batteryAction->setText(text);
        });

    return app.exec();
}
