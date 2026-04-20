#include "desktop/GnomeDesktop.h"
#include "logging/LogManager.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QProcess>
#include <QStandardPaths>

namespace logitune {

static constexpr auto kExtUuid = "logitune-focus@logitune.com";

GnomeDesktop::GnomeDesktop(QObject *parent)
    : LinuxDesktopBase(parent)
{
}

void GnomeDesktop::start()
{
    // Only support Wayland sessions
    const QString sessionType = QProcessEnvironment::systemEnvironment()
                                    .value(QStringLiteral("XDG_SESSION_TYPE"));
    if (sessionType != QStringLiteral("wayland")) {
        qCInfo(lcFocus) << "GNOME: not a Wayland session, focus tracking disabled";
        m_available = false;
        return;
    }

    // Check GNOME Shell is running
    QDBusMessage ping = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    ping << QStringLiteral("org.gnome.Shell") << QStringLiteral("ShellVersion");
    QDBusMessage reply = QDBusConnection::sessionBus().call(ping, QDBus::Block, 1000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcFocus) << "GNOME Shell not reachable on D-Bus";
        m_available = false;
        return;
    }

    // Probe the AppIndicator extension before touching our own focus extension.
    // The two extensions are independent: a fresh install of logitune-focus
    // requires a shell reload to register, and we still want the tray-icon
    // banner to reflect reality on the user's first launch.
    detectAppIndicatorStatus();

    // Install and enable the extension
    if (!ensureExtensionInstalled()) {
        qCWarning(lcFocus) << "GNOME: failed to install/enable extension";
        m_available = false;
        return;
    }

    // Register our D-Bus service so the extension can call us
    QDBusConnection::sessionBus().registerService(QStringLiteral("com.logitune.app"));
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/FocusWatcher"), this,
        QDBusConnection::ExportAllSlots);

    m_available = true;
    qCInfo(lcFocus) << "GNOME desktop integration started";
}

void GnomeDesktop::detectAppIndicatorStatus()
{
    static const QString kAppIndicatorUuid =
        QStringLiteral("appindicatorsupport@rgcjonas.gmail.com");

    QDBusMessage infoMsg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("/org/gnome/Shell/Extensions"),
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("GetExtensionInfo"));
    infoMsg << kAppIndicatorUuid;
    QDBusMessage infoReply = QDBusConnection::sessionBus().call(infoMsg, QDBus::Block, 2000);

    bool installed = false;
    QVariantMap info;
    if (infoReply.type() == QDBusMessage::ReplyMessage && !infoReply.arguments().isEmpty()) {
        info = qdbus_cast<QVariantMap>(infoReply.arguments().first());
        installed = !info.isEmpty();
    }

    if (!installed) {
        m_appIndicatorStatus = AppIndicatorNotInstalled;
        qCWarning(lcFocus) << "AppIndicator extension not installed -- tray icon will not be visible."
                           << "Install: gnome-shell-extension-appindicator";
        return;
    }

    // GNOME Shell extension states: 1=ENABLED, 2=DISABLED, 3=ERROR, 4=OUT_OF_DATE, 5=DOWNLOADING, 6=INITIALIZED
    int state = info.value(QStringLiteral("state")).toInt();
    if (state == 1) {
        m_appIndicatorStatus = AppIndicatorActive;
        qCInfo(lcFocus) << "AppIndicator extension active";
    } else {
        m_appIndicatorStatus = AppIndicatorDisabled;
        qCWarning(lcFocus) << "AppIndicator extension installed but not enabled (state:" << state << ")"
                           << "-- run: gnome-extensions enable" << kAppIndicatorUuid;
    }
}

bool GnomeDesktop::ensureExtensionInstalled()
{
    int major = detectShellMajorVersion();
    if (major < 42) {
        qCWarning(lcFocus) << "GNOME Shell version" << major << "not supported (need 42+)";
        return false;
    }

    QString variant = (major >= 45) ? QStringLiteral("v45") : QStringLiteral("v42");

    // Check system-wide install first
    QString systemDir = QStringLiteral("/usr/share/gnome-shell/extensions/")
                        + QLatin1String(kExtUuid);
    QString userDir = QDir::homePath()
                      + QStringLiteral("/.local/share/gnome-shell/extensions/")
                      + QLatin1String(kExtUuid);

    bool systemComplete = QFile::exists(systemDir + QStringLiteral("/extension.js"))
                       && QFile::exists(systemDir + QStringLiteral("/metadata.json"));

    if (systemComplete) {
        // System install has root extension.js — Shell loads from there.
        // Remove any stale user-dir copy that might override with wrong interface name.
        if (QFile::exists(userDir + QStringLiteral("/extension.js"))) {
            QFile::remove(userDir + QStringLiteral("/extension.js"));
            QFile::remove(userDir + QStringLiteral("/metadata.json"));
            qCInfo(lcFocus) << "Removed stale user extension dir (system install is complete)";
        }
    } else if (QFile::exists(systemDir + QStringLiteral("/metadata.json"))) {
        // System install exists but no root extension.js — copy correct variant to user dir
        QDir().mkpath(userDir);
        QFile::copy(systemDir + QStringLiteral("/metadata.json"),
                    userDir + QStringLiteral("/metadata.json"));
        QFile::copy(systemDir + "/" + variant + QStringLiteral("/extension.js"),
                    userDir + QStringLiteral("/extension.js"));
        qCInfo(lcFocus) << "Copied" << variant << "extension.js to" << userDir;
    } else {
        qCWarning(lcFocus) << "Extension not found at" << systemDir;
        return false;
    }

    // Enable via D-Bus (GNOME 3.36+)
    QDBusMessage enableMsg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("/org/gnome/Shell/Extensions"),
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("EnableExtension"));
    enableMsg << QString::fromLatin1(kExtUuid);
    QDBusReply<bool> enableReply = QDBusConnection::sessionBus().call(enableMsg, QDBus::Block, 2000);
    if (enableReply.isValid() && enableReply.value()) {
        qCInfo(lcFocus) << "GNOME extension enabled";
    } else {
        // Try CLI fallback
        QProcess proc;
        proc.start(QStringLiteral("gnome-extensions"),
                   {QStringLiteral("enable"), QString::fromLatin1(kExtUuid)});
        proc.waitForFinished(3000);
        if (proc.exitCode() != 0) {
            qCWarning(lcFocus) << "Failed to enable extension:" << proc.readAllStandardError();
            return false;
        }
        qCInfo(lcFocus) << "GNOME extension enabled via CLI";
    }

    return true;
}

int GnomeDesktop::detectShellMajorVersion()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg << QStringLiteral("org.gnome.Shell") << QStringLiteral("ShellVersion");
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        QString version = reply.arguments().first().value<QDBusVariant>().variant().toString();
        qCInfo(lcFocus) << "GNOME Shell version:" << version;
        return version.section('.', 0, 0).toInt();
    }
    return 0;
}

bool GnomeDesktop::available() const
{
    return m_available;
}

QString GnomeDesktop::desktopName() const
{
    return QStringLiteral("GNOME");
}

QStringList GnomeDesktop::detectedCompositors() const
{
    QStringList compositors;
    const QString desktop = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (desktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive))
        compositors << QStringLiteral("Mutter");
    return compositors;
}

void GnomeDesktop::focusChanged(const QString &appId, const QString &title)
{
    QString resolved = appId;
    if (!appId.contains('.'))
        resolved = resolveDesktopFile(appId);

    if (resolved == m_lastAppId) return;
    m_lastAppId = resolved;
    emit activeWindowChanged(resolved, title);
}

void GnomeDesktop::blockGlobalShortcuts(bool block)
{
    QString js = block
        ? QStringLiteral("global.stage.set_key_focus(null); "
                          "Main.layoutManager._startingUp = true;")
        : QStringLiteral("Main.layoutManager._startingUp = false;");

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("Eval"));
    msg << js;
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

} // namespace logitune
