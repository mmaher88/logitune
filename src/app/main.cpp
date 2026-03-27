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
    auto saveCurrentState = [&]() {
        if (!deviceManager.deviceConnected()) return;
        if (profileEngine.activeProfileName().isEmpty()) return;  // no profile loaded yet
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
        profileEngine.updateActiveProfile(p);
        qDebug() << "[main] profile saved:" << profileEngine.activeProfileName();
    };

    // ── Gesture keystrokes (matches logid.cfg) ────────────────────────────────
    static QMap<QString, QString> gestureKeystrokes = {
        {"down",  "Super+D"},              // Show Desktop
        {"left",  "Ctrl+Super+Left"},      // Switch desktop left
        {"right", "Ctrl+Super+Right"},     // Switch desktop right
        {"click", "Super+W"},              // Window Switcher / Overview
    };

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
            uint16_t cid = kButtonCids[row];
            deviceManager.divertButton(cid, needsDivert);
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
                    aType = QStringLiteral("keystroke");
                    aName = buttonActionToName(ba);
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
                deviceManager.divertButton(kProfileButtonCids[i], needsDivert);
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

    // 5. Diverted button press → profile action lookup → execute
    // Gesture state tracking
    static QPoint gestureStartPos;
    static bool gestureActive = false;
    static constexpr int kGestureThreshold = 50; // pixels

    QObject::connect(&deviceManager, &logitune::DeviceManager::divertedButtonPressed,
        [&buttonModel, &actionModel, &actionExecutor, &deviceManager](uint16_t controlId, bool pressed) {

            // Map controlId to button index
            static const std::unordered_map<uint16_t, int> kControlMap = {
                {0x0050, 0}, {0x0051, 1}, {0x0052, 2},
                {0x0053, 3}, {0x0056, 4}, {0x00C3, 5}, {0x00C4, 6}
            };

            auto it = kControlMap.find(controlId);
            if (it == kControlMap.end()) return;
            int idx = it->second;

            QString actionType = buttonModel.actionTypeForButton(idx);

            // Handle gesture button release — resolve swipe direction
            if (!pressed && actionType == "gesture-trigger" && gestureActive) {
                gestureActive = false;
                QPoint end = QCursor::pos();
                int dx = end.x() - gestureStartPos.x();
                int dy = end.y() - gestureStartPos.y();

                QString dir;
                if (std::abs(dx) > kGestureThreshold || std::abs(dy) > kGestureThreshold) {
                    if (std::abs(dx) > std::abs(dy))
                        dir = dx > 0 ? "right" : "left";
                    else
                        dir = dy > 0 ? "down" : "up";
                } else {
                    dir = "click";
                }

                auto git = gestureKeystrokes.find(dir);
                if (git != gestureKeystrokes.end() && !git.value().isEmpty()) {
                    qDebug() << "[main] gesture" << dir << "→" << git.value();
                    actionExecutor.injectKeystroke(git.value());
                }
                return;
            }

            if (!pressed) return; // ignore release for non-gesture buttons

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
                gestureStartPos = QCursor::pos();
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
        [&actionExecutor](int dx, int dy, bool released) {
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

                auto it = gestureKeystrokes.find(dirName);
                if (it != gestureKeystrokes.end() && !it.value().isEmpty()) {
                    qDebug() << "[main] gesture:" << dirName << "→" << it.value();
                    actionExecutor.injectKeystroke(it.value());
                }
            }
        });

    // ── QML engine ───────────────────────────────────────────────────────────

    fprintf(stderr, "[logitune] creating QML engine...\n");
    QQmlApplicationEngine engine;

    // Register singletons — provide existing instances
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
