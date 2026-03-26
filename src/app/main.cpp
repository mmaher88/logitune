#include <QApplication>
#include <QDirIterator>
#include <signal.h>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
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

    // ProfileEngine — config dir is per-device but we set a default here;
    // once the device serial is known it can be updated via setDeviceConfigDir.
    logitune::ProfileEngine profileEngine;
    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    profileEngine.setDeviceConfigDir(configBase + "/default");

    // ActionExecutor — create uinput virtual keyboard device
    fprintf(stderr, "[logitune] creating ActionExecutor...\n");
    logitune::ActionExecutor actionExecutor;
    fprintf(stderr, "[logitune] init uinput...\n");
    if (!actionExecutor.initUinput()) {
        qWarning() << "[main] ActionExecutor: uinput init failed (no /dev/uinput access?). Keystrokes will not be injected.";
    }

    // ── Signal wiring ────────────────────────────────────────────────────────

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
                qDebug() << "[main] profileDelta: would set DPI to" << profile.dpi;
            if (delta.smartShiftChanged)
                qDebug() << "[main] profileDelta: would set SmartShift"
                         << profile.smartShiftEnabled << profile.smartShiftThreshold;
            if (delta.scrollChanged)
                qDebug() << "[main] profileDelta: would set scroll mode"
                         << profile.scrollDirection << "hiRes=" << profile.hiResScroll;
            // Suppress unused-variable warning when qDebug is compiled out
            Q_UNUSED(deviceManager);
        });

    // 5. Diverted button press → profile action lookup → execute
    QObject::connect(&deviceManager, &logitune::DeviceManager::divertedButtonPressed,
        [&buttonModel, &actionModel, &actionExecutor](uint16_t controlId, bool pressed) {
            if (!pressed) return;

            // Map controlId to button index (from real device enumeration)
            static const std::unordered_map<uint16_t, int> kControlMap = {
                {0x0050, 0}, {0x0051, 1}, {0x0052, 2},
                {0x0053, 3}, {0x0056, 4}, {0x00C3, 5}, {0x00C4, 6}
            };

            auto it = kControlMap.find(controlId);
            if (it == kControlMap.end()) return;
            int idx = it->second;

            // Read the action from ButtonModel (updated by the UI)
            QString actionType = buttonModel.actionTypeForButton(idx);
            QString actionName = buttonModel.actionNameForButton(idx);

            if (actionType == "default") return;

            // Get the actual payload (keystroke combo) from ActionModel
            QString payload = actionModel.payloadForName(actionName);

            qDebug() << "[main] button" << idx << "→" << actionType << ":" << actionName << "payload:" << payload;

            if (actionType == "keystroke" && !payload.isEmpty()) {
                actionExecutor.injectKeystroke(payload);
            } else if (actionType == "gesture-trigger") {
                // gesture mode — handled separately
            } else if (actionType == "app-launch" && !payload.isEmpty()) {
                actionExecutor.launchApp(payload);
            }
        });

    // 6. Gesture event → GestureDetector → profile gesture action → execute
    QObject::connect(&deviceManager, &logitune::DeviceManager::gestureEvent,
        [&actionExecutor, &profileEngine](int dx, int dy, bool released) {
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

                const auto &profile = profileEngine.activeProfile();
                auto it = profile.gestures.find(dirName);
                if (it != profile.gestures.end())
                    actionExecutor.executeAction(it->second);
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
