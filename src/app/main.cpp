#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <signal.h>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QQuickImageProvider>
#include <QIcon>
#include <QTimer>
#include <QLockFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#include "AppController.h"
#include "models/EditorModel.h"
#include "logging/LogManager.h"
#include "logging/CrashHandler.h"
#include "dialogs/CrashReportDialog.h"
#include "TrayManager.h"

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
    app.setQuitOnLastWindowClosed(false);  // tray icon keeps app alive
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
        QStringLiteral("Start hidden to the system tray. Intended for autostart "
                       "launchers; ignored if no system tray is available."));
    parser.addOption(minimizedOption);
    parser.process(app);
    const bool simulateAll = parser.isSet(simulateAllOption);
    const bool editMode = parser.isSet(editOption);
    const bool startMinimized = parser.isSet(minimizedOption);

    // Single-instance guard — prevent two instances fighting over the device
    QLockFile lockFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/logitune.lock");
    if (!lockFile.tryLock(100)) {
        qCritical() << "Another instance of Logitune is already running";
        return 1;
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

    // AppController
    logitune::AppController controller;
    controller.init();

    // QML engine
    qCInfo(lcApp) << "Creating QML engine...";
    QQmlApplicationEngine engine;
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

    // Qt 6.5+: register into the module. Qt 6.4: module is protected by plugin,
    // use context properties instead (globally available in QML, same effect).
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",    controller.deviceModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",    controller.buttonModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",    controller.actionFilterModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel",   controller.profileModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "SettingsModel",  controller.settingsModel());
#else
    engine.rootContext()->setContextProperty("DeviceModel",    controller.deviceModel());
    engine.rootContext()->setContextProperty("ButtonModel",    controller.buttonModel());
    engine.rootContext()->setContextProperty("ActionModel",    controller.actionFilterModel());
    engine.rootContext()->setContextProperty("ProfileModel",   controller.profileModel());
    engine.rootContext()->setContextProperty("SettingsModel",  controller.settingsModel());

    // Qt 6.4: QML singletons from qmldir with "prefer :" don't resolve correctly.
    // Explicitly register Theme from its resource path.
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Logitune/qml/Theme.qml")),
                             "Logitune", 1, 0, "Theme");
#endif

    controller.startMonitoring(simulateAll, editMode);

    if (editMode && controller.editorModel()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        qmlRegisterSingletonInstance("Logitune", 1, 0, "EditorModel", controller.editorModel());
#else
        engine.rootContext()->setContextProperty("EditorModel", controller.editorModel());
#endif
    }

    qCInfo(lcApp) << "Loading QML...";
    engine.load(QUrl(QStringLiteral("qrc:/Logitune/qml/Main.qml")));
    qCInfo(lcApp) << "QML loaded, root objects:" << engine.rootObjects().size();

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcApp) << "QML failed to load — no root objects";
        return -1;
    }

    // Set theme via the root QML object which has access to the Theme singleton
    qCInfo(lcApp) << "Setting theme dark:" << isDark;
    if (!engine.rootObjects().isEmpty()) {
        QObject *root = engine.rootObjects().first();
        QQmlExpression expr(QQmlEngine::contextForObject(root), root,
            isDark ? QStringLiteral("Theme.dark = true")
                   : QStringLiteral("Theme.dark = false"));
        QVariant result = expr.evaluate();
        if (expr.hasError())
            qCWarning(lcApp) << "Theme expression error:" << expr.error().toString();
        else
            qCInfo(lcApp) << "Theme.dark applied:" << isDark;
    }

    // System tray
    logitune::TrayManager tray(controller.deviceModel());
    QObject::connect(&tray, &logitune::TrayManager::showWindowRequested, [&engine]() {
        for (auto *obj : engine.rootObjects()) {
            if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });
    QObject::connect(tray.quitAction(), &QAction::triggered, &app, &QApplication::quit);
    tray.show();

    const bool trayVisible = tray.trayIcon()->isVisible();
    if (!trayVisible) {
        // Without a tray icon (e.g. GNOME with no AppIndicator extension)
        // there is no way to re-open the window or quit from a tray menu, so
        // closing the window must terminate the process or the user gets
        // stranded with an invisible running app.
        app.setQuitOnLastWindowClosed(true);
        qCInfo(lcApp) << "Tray unavailable -- closing the last window will "
                         "quit the app";
    }
    if (startMinimized && trayVisible) {
        for (QObject *obj : engine.rootObjects()) {
            if (auto *window = qobject_cast<QQuickWindow*>(obj))
                window->hide();
        }
        qCInfo(lcApp) << "Startup: minimized to tray";
    } else if (startMinimized && !trayVisible) {
        qCInfo(lcApp) << "Startup: --minimized requested but tray is not "
                         "visible, showing window";
    } else {
        qCDebug(lcApp) << "Startup: showing window";
    }

    qCInfo(lcApp) << "Startup complete";

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
