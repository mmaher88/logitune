#include <QApplication>
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
#include <QLockFile>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#include "AppController.h"
#include "logging/LogManager.h"
#include "logging/CrashHandler.h"
#include "dialogs/CrashReportDialog.h"
#include "TrayManager.h"

int main(int argc, char *argv[])
{
    // Ignore SIGPIPE — hidraw writes to wrong interface cause EPIPE
    signal(SIGPIPE, SIG_IGN);

    // Prevent dconf hang on GNOME — Qt's platform theme triggers dconf
    // initialization which deadlocks on some D-Bus session configurations.
    // Memory backend is safe: we handle theming ourselves via Theme.qml.
    if (qEnvironmentVariableIsEmpty("GSETTINGS_BACKEND"))
        qputenv("GSETTINGS_BACKEND", "memory");

    // Qt 6.4 QML JIT hangs on some CPU/kernel combinations during compilation.
    // Force the interpreter — startup is ~100ms either way for our QML set.
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    if (qEnvironmentVariableIsEmpty("QV4_FORCE_INTERPRETER"))
        qputenv("QV4_FORCE_INTERPRETER", "1");
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");
    app.setApplicationVersion("0.1.0");
    app.setQuitOnLastWindowClosed(false);  // tray icon keeps app alive
    app.setWindowIcon(QIcon(":/Logitune/qml/assets/logitune-icon.svg"));

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

    // Detect dark mode — XDG portal first (works with GSETTINGS_BACKEND=memory),
    // palette luminance as fallback
    bool isDark = false;
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Settings",
            "Read");
        msg << QStringLiteral("org.freedesktop.appearance")
            << QStringLiteral("color-scheme");
        QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            // Portal returns variant<uint32>: 0=default, 1=dark, 2=light
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
    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",  controller.deviceModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",  controller.buttonModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",  controller.actionModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel", controller.profileModel());
#else
    engine.rootContext()->setContextProperty("DeviceModel",  controller.deviceModel());
    engine.rootContext()->setContextProperty("ButtonModel",  controller.buttonModel());
    engine.rootContext()->setContextProperty("ActionModel",  controller.actionModel());
    engine.rootContext()->setContextProperty("ProfileModel", controller.profileModel());
#endif

    qCInfo(lcApp) << "Loading QML...";
    engine.load(QUrl(QStringLiteral("qrc:/Logitune/qml/Main.qml")));
    qCInfo(lcApp) << "QML loaded, root objects:" << engine.rootObjects().size();

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcApp) << "QML failed to load — no root objects";
        return -1;
    }

    // Set theme
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *theme = engine.singletonInstance<QObject*>("Logitune", "Theme"))
        theme->setProperty("dark", isDark);
#else
    {
        int typeId = qmlTypeId("Logitune", 1, 0, "Theme");
        if (auto *theme = engine.singletonInstance<QObject*>(typeId))
            theme->setProperty("dark", isDark);
    }
#endif

    controller.startMonitoring();

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
