#include <QApplication>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QQuickWindow>
#include "DeviceManager.h"
#include "WindowTracker.h"
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
#include "models/ActionModel.h"
#include "models/ProfileModel.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");

    // Create backend
    logitune::DeviceManager deviceManager;
    logitune::DeviceModel deviceModel;
    deviceModel.setDeviceManager(&deviceManager);

    logitune::ButtonModel buttonModel;
    logitune::ActionModel actionModel;

    logitune::ProfileModel profileModel;
    logitune::KWinWindowTracker windowTracker;

    QObject::connect(&windowTracker, &logitune::WindowTracker::activeWindowChanged,
                     &profileModel,  &logitune::ProfileModel::setActiveByWmClass);

    QQmlApplicationEngine engine;

    // Register singletons — provide existing instances
    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",  &deviceModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",  &buttonModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",  &actionModel);
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel", &profileModel);

    engine.loadFromModule("Logitune", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    // Start device monitoring after QML is loaded
    deviceManager.start();
    windowTracker.start();

    // System tray
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

    // Update battery text when it changes
    QObject::connect(&deviceManager, &logitune::DeviceManager::batteryLevelChanged,
        [batteryAction, &deviceManager]() {
            QString text = QString("Battery: %1%").arg(deviceManager.batteryLevel());
            if (deviceManager.batteryCharging()) text += " (charging)";
            batteryAction->setText(text);
        });

    return app.exec();
}
