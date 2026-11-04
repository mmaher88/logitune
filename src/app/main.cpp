#include <QApplication>
#include "models/InstalledAppsModel.h"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <signal.h>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickImageProvider>
#include <QIcon>
#include <QTimer>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#include "AppRoot.h"
#include "models/EditorModel.h"
#include "logging/LogManager.h"
#include "logging/CrashHandler.h"
#include "dialogs/CrashReportDialog.h"
#include "TrayManager.h"
#include "SingleInstance.h"
#include "MainWindowController.h"

int main(int argc, char *argv[])
{
    // Ignore SIGPIPE — hidraw writes to wrong interface cause EPIPE
    signal(SIGPIPE, SIG_IGN);

    // Handle SIGTERM/SIGINT gracefully so aboutToQuit fires and the lock
    // file is cleaned up (prevents false "crashed" dialog on next launch).
    auto termHandler = [](int) { QCoreApplication::quit(); };
    signal(SIGTERM, termHandler);
    signal(SIGINT, termHandler);

    // Prevent dconf hang on GNOME — Qt's platform theme triggers dconf
    // initialization which deadlocks on some D-Bus session configurations.
    // Memory backend is safe: we handle theming ourselves via Theme.qml.
    if (qEnvironmentVariableIsEmpty("GSETTINGS_BACKEND"))
        qputenv("GSETTINGS_BACKEND", "memory");

    // Prevent Kvantum/external Qt theme engines from overriding our Theme.qml.
    qunsetenv("QT_STYLE_OVERRIDE");

    // Qt 6.4 QML JIT hangs on some CPU/kernel combinations during compilation.
    // Force the interpreter — startup is ~100ms either way for our QML set.
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    if (qEnvironmentVariableIsEmpty("QV4_FORCE_INTERPRETER"))
        qputenv("QV4_FORCE_INTERPRETER", "1");
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION_FULL));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/Logitune/qml/assets/logitune-icon.svg"));

    // Command-line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Logitune — Logitech device configuration"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption simulateAllOption(
        QStringLiteral("simulate-all"),
        QStringLiteral(
            "Debug: populate the carousel with one fake card per descriptor "
            "in DeviceRegistry instead of scanning hardware. Used for "
            "visually inspecting every community descriptor without needing "
            "the physical mice."));
    parser.addOption(simulateAllOption);
    QCommandLineOption editOption(
        QStringLiteral("edit"),
        QStringLiteral("Enable in-app descriptor editor mode. Drag elements on "
                       "device pages to edit slot positions, hotspots, and images. "
                       "Save writes back to the source descriptor JSON."));
    parser.addOption(editOption);
    QCommandLineOption minimizedOption(
        QStringLiteral("minimized"),
        QStringLiteral("Start without a window. Launch again to show it."));
    parser.addOption(minimizedOption);
    QCommandLineOption quitOption(
        QStringLiteral("quit"),
        QStringLiteral("Quit the running instance without starting a new one."));
    parser.addOption(quitOption);
    QCommandLineOption ringOption(QStringLiteral("show-ring"), QStringLiteral("Toggle the Actions Ring"));
    ringOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(ringOption);
    parser.process(app);
    const bool simulateAll = parser.isSet(simulateAllOption);
    const bool editMode = parser.isSet(editOption);
    const bool startMinimized = parser.isSet(minimizedOption);

    const QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    logitune::SingleInstance singleInstance(runtimePath + "/logitune.lock",
                                           runtimePath + "/logitune");
    QObject::connect(&singleInstance, &logitune::SingleInstance::quitRequested,
                     &app, &QCoreApplication::quit);
    const auto command = parser.isSet(quitOption) ? logitune::SingleInstance::Command::Quit
        : parser.isSet(ringOption) ? logitune::SingleInstance::Command::Ring
                                  : logitune::SingleInstance::Command::Show;
    if (const auto exitCode = singleInstance.start(command)) {
        if (*exitCode != 0)
            qCritical() << "Could not start or contact the primary Logitune instance";
        return *exitCode;
    }

    // Initialize logging — enabled by default, user can disable in Settings
    auto &logMgr = logitune::LogManager::instance();
    logMgr.init(true);

    // Use INI format explicitly — avoids dconf/GSettings hangs on GNOME
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // Respect user's setting if they explicitly disabled logging
    QSettings settings;
    if (settings.contains("logging/enabled") && !settings.value("logging/enabled").toBool())
        logMgr.setLoggingEnabled(false);

    qCInfo(lcApp) << "Application started, PID" << QCoreApplication::applicationPid();

    // Install crash handler
    auto &crashHandler = logitune::CrashHandler::instance();
    crashHandler.install();

    // Set crash callback — shows dialog on caught crash
    crashHandler.setCrashCallback([](const logitune::CrashInfo &info) {
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
    });

    // Check for previous crash (lock file from unclean shutdown)
    if (crashHandler.previousSessionCrashed()) {
        qCInfo(lcApp) << "Previous session crashed — showing recovery dialog";
        auto info = crashHandler.previousSessionCrashInfo();
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::RecoveryReport, info);
        dlg.exec();
    }

    // Create lock file (removed on clean exit)
    crashHandler.createLockFile();

    // Clean shutdown: remove lock file, flush logs
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qCInfo(lcApp) << "Application shutting down cleanly";
        crashHandler.removeLockFile();
        logMgr.shutdown();
    });

    // Detect dark mode — user preference first, XDG portal second, palette fallback
    bool isDark = false;
    qCInfo(lcApp) << "settings file:" << settings.fileName()
                  << "contains theme/dark:" << settings.contains("theme/dark")
                  << "value:" << settings.value("theme/dark");
    if (settings.contains("theme/dark")) {
        isDark = settings.value("theme/dark").toBool();
    } else {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Settings",
            "Read");
        msg << QStringLiteral("org.freedesktop.appearance")
            << QStringLiteral("color-scheme");
        QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
            if (value.canConvert<QDBusVariant>())
                value = value.value<QDBusVariant>().variant();
            isDark = (value.toUInt() == 1);
        } else {
            QColor windowBg = app.palette().window().color();
            double lum = windowBg.redF() * 0.299 + windowBg.greenF() * 0.587
                         + windowBg.blueF() * 0.114;
            isDark = lum < 0.5;
        }
    }

    // AppRoot
    logitune::AppRoot controller;
    controller.init();

    // QML engine
    qCInfo(lcApp) << "Creating QML engine...";
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QCoreApplication::quit);
    qCInfo(lcApp) << "QML engine created";

    class IconProvider : public QQuickImageProvider {
    public:
        IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
        QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override {
            QIcon icon = QIcon::fromTheme(id);
            QSize s = requestedSize.isValid() ? requestedSize : QSize(22, 22);
            QPixmap pm = icon.pixmap(s);
            if (size) *size = pm.size();
            return pm;
        }
    };
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    controller.actionsRing()->setEngine(&engine);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     controller.actionsRing(), &logitune::ActionsRingService::shutdown);
    engine.rootContext()->setContextProperty("ActionsRing", controller.actionsRing());
    logitune::InstalledAppsModel installedApps;
    engine.rootContext()->setContextProperty("InstalledApps", &installedApps);
    QObject::connect(&singleInstance, &logitune::SingleInstance::ringRequested,
                     controller.actionsRing(), &logitune::ActionsRingService::toggle);

    // Qt 6.5+: register into the module. Qt 6.4: module is protected by plugin,
    // use context properties instead (globally available in QML, same effect).
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",    controller.deviceModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",    controller.buttonModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",    controller.actionFilterModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "GestureActionModel", controller.gestureActionFilterModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel",   controller.profileModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "SettingsModel",  controller.settingsModel());
#else
    engine.rootContext()->setContextProperty("DeviceModel",    controller.deviceModel());
    engine.rootContext()->setContextProperty("ButtonModel",    controller.buttonModel());
    engine.rootContext()->setContextProperty("ActionModel",    controller.actionFilterModel());
    engine.rootContext()->setContextProperty("GestureActionModel", controller.gestureActionFilterModel());
    engine.rootContext()->setContextProperty("ProfileModel",   controller.profileModel());
    engine.rootContext()->setContextProperty("SettingsModel",  controller.settingsModel());

    // Qt 6.4: QML singletons from qmldir with "prefer :" don't resolve correctly.
    // Explicitly register Theme from its resource path.
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Logitune/qml/Theme.qml")),
                             "Logitune", 1, 0, "Theme");
#endif

    const int themeId = qmlTypeId("Logitune", 1, 0, "Theme");
    if (auto *theme = engine.singletonInstance<QObject *>(themeId))
        theme->setProperty("dark", isDark);

    controller.startMonitoring(simulateAll, editMode);

    if (editMode && controller.editorModel()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        qmlRegisterSingletonInstance("Logitune", 1, 0, "EditorModel", controller.editorModel());
#else
        engine.rootContext()->setContextProperty("EditorModel", controller.editorModel());
#endif
    }

    logitune::MainWindowController windows(engine, isDark);
    auto showMainWindow = [&windows]() {
        if (!windows.showMainWindow())
            qCCritical(lcApp) << "QML failed to load — no main window";
    };
    QObject::connect(&singleInstance, &logitune::SingleInstance::showRequested,
                     &windows, showMainWindow);

    // System tray
    logitune::TrayManager tray(controller.deviceModel());
    QObject::connect(&tray, &logitune::TrayManager::showWindowRequested,
                     &windows, showMainWindow);
    QObject::connect(tray.quitAction(), &QAction::triggered, &app, &QApplication::quit);
    tray.show();

    if (!QSystemTrayIcon::isSystemTrayAvailable())
        qCInfo(lcApp) << "Tray unavailable -- launch again to show the window; use --quit to exit";

    if (startMinimized || parser.isSet(ringOption)) {
        qCInfo(lcApp) << "Startup: running without a window";
    } else {
        if (!windows.showMainWindow()) {
            qCCritical(lcApp) << "QML failed to load — no main window";
            return -1;
        }
    }

    qCInfo(lcApp) << "Startup complete";
    if (parser.isSet(ringOption))
        QTimer::singleShot(0, controller.actionsRing(), &logitune::ActionsRingService::toggle);

    // Run with exception safety net
    try {
        return app.exec();
    } catch (const std::exception &e) {
        qCCritical(lcApp) << "Unhandled exception in event loop:" << e.what();
        logitune::CrashInfo info;
        info.type = QString::fromUtf8(typeid(e).name()) + ": " + QString::fromUtf8(e.what());
        info.logTail = logMgr.tailLog(50);
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
        return -1;
    } catch (...) {
        qCCritical(lcApp) << "Unknown exception in event loop";
        logitune::CrashInfo info;
        info.type = QStringLiteral("Unknown exception");
        info.logTail = logMgr.tailLog(50);
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
        return -1;
    }
}
