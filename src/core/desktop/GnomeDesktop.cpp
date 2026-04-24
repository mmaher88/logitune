#include "desktop/GnomeDesktop.h"
#include "actions/ActionPresetRegistry.h"
#include "logging/LogManager.h"
#include <optional>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QProcess>
#include <QRegularExpression>
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
    // Primary signal: any AppIndicator implementation (rgcjonas'
    // appindicatorsupport, Ubuntu's ubuntu-appindicators, etc.) registers
    // org.kde.StatusNotifierWatcher on the session bus. If present, the
    // tray icon will render regardless of which extension backs it.
    //
    // Previous implementation checked a specific uuid via GetExtensionInfo
    // and incorrectly reported "not installed" on Ubuntu GNOME, which ships
    // ubuntu-appindicators@ubuntu.com preinstalled.
    const bool watcherRegistered = QDBusConnection::sessionBus()
        .interface()
        ->isServiceRegistered(QStringLiteral("org.kde.StatusNotifierWatcher"));

    if (watcherRegistered) {
        m_appIndicatorStatus = AppIndicatorActive;
        qCInfo(lcFocus) << "StatusNotifier watcher active; tray icons supported";
        return;
    }

    // Watcher is absent. Distinguish "installed but disabled" from
    // "not installed at all" so the banner shows the right remediation
    // (enable vs install). Probe the two known community uuids.
    static const QStringList kKnownUuids = {
        QStringLiteral("appindicatorsupport@rgcjonas.gmail.com"),
        QStringLiteral("ubuntu-appindicators@ubuntu.com"),
    };

    for (const QString &uuid : kKnownUuids) {
        QDBusMessage infoMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.gnome.Shell.Extensions"),
            QStringLiteral("/org/gnome/Shell/Extensions"),
            QStringLiteral("org.gnome.Shell.Extensions"),
            QStringLiteral("GetExtensionInfo"));
        infoMsg << uuid;
        QDBusMessage infoReply = QDBusConnection::sessionBus()
            .call(infoMsg, QDBus::Block, 2000);

        if (infoReply.type() != QDBusMessage::ReplyMessage
            || infoReply.arguments().isEmpty())
            continue;

        const QVariantMap info = qdbus_cast<QVariantMap>(
            infoReply.arguments().first());
        if (info.isEmpty())
            continue;

        m_appIndicatorStatus = AppIndicatorDisabled;
        qCWarning(lcFocus) << "AppIndicator extension" << uuid
                           << "installed but not enabled -- run: "
                              "gnome-extensions enable" << uuid;
        return;
    }

    m_appIndicatorStatus = AppIndicatorNotInstalled;
    qCWarning(lcFocus) << "No StatusNotifier watcher and no known AppIndicator "
                          "extension installed -- tray icon will not be visible."
                       << "Install: gnome-shell-extension-appindicator";
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

QString GnomeDesktop::variantKey() const
{
    return QStringLiteral("gnome");
}

// ---------------------------------------------------------------------------
// gsettings output to keystroke transform
// ---------------------------------------------------------------------------

QString GnomeDesktop::gsettingsToKeystroke(const QString &gsettingsOutput)
{
    // gsettings returns GLib variant strings like "['<Super>d']" or
    // "['<Primary><Alt>Left', '<Super>D']". Find the first non-empty
    // single-quoted binding.
    static const QRegularExpression kBindingRe(QStringLiteral("'([^']*)'"));
    QRegularExpressionMatchIterator it = kBindingRe.globalMatch(gsettingsOutput);

    QString binding;
    while (it.hasNext()) {
        const QString candidate = it.next().captured(1);
        if (!candidate.isEmpty()) {
            binding = candidate;
            break;
        }
    }
    if (binding.isEmpty())
        return {};

    // Rewrite <Super>, <Primary>, <Control>, <Ctrl>, <Alt>, <Shift>, <Meta>
    // into the Logitune format: Super+, Ctrl+, Alt+, Shift+, Meta+.
    static const QRegularExpression kModifierRe(QStringLiteral("<([^>]+)>"));
    QString out;
    int pos = 0;
    auto mit = kModifierRe.globalMatch(binding);
    while (mit.hasNext()) {
        auto m = mit.next();
        const QString mod = m.captured(1);
        QString norm;
        if (mod.compare(QStringLiteral("Primary"), Qt::CaseInsensitive) == 0 ||
            mod.compare(QStringLiteral("Control"), Qt::CaseInsensitive) == 0 ||
            mod.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Ctrl");
        } else if (mod.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Alt");
        } else if (mod.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Shift");
        } else if (mod.compare(QStringLiteral("Super"), Qt::CaseInsensitive) == 0 ||
                   mod.compare(QStringLiteral("Meta"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Super");
        } else {
            norm = mod;
        }
        if (!out.isEmpty()) out += QLatin1Char('+');
        out += norm;
        pos = m.capturedEnd();
    }

    const QString tail = binding.mid(pos).trimmed();
    if (!tail.isEmpty()) {
        if (!out.isEmpty()) out += QLatin1Char('+');
        out += tail;
    }
    return out;
}

// ---------------------------------------------------------------------------
// resolveNamedAction
// ---------------------------------------------------------------------------

std::optional<ButtonAction> GnomeDesktop::resolveNamedAction(const QString &id) const
{
    if (!m_registry)
        return std::nullopt;

    const QJsonObject variant = m_registry->variantData(id, QStringLiteral("gnome"));
    if (variant.isEmpty())
        return std::nullopt;

    // gsettings: look up the live binding and translate.
    if (variant.contains(QStringLiteral("gsettings"))) {
        const QJsonObject spec = variant.value(QStringLiteral("gsettings")).toObject();
        const QString schema = spec.value(QStringLiteral("schema")).toString();
        const QString key = spec.value(QStringLiteral("key")).toString();
        if (schema.isEmpty() || key.isEmpty())
            return std::nullopt;

        QString raw;
        if (m_reader) {
            raw = m_reader(schema, key);
        } else {
            QProcess p;
            p.start(QStringLiteral("gsettings"),
                    {QStringLiteral("get"), schema, key});
            if (!p.waitForFinished(1000))
                return std::nullopt;
            raw = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }

        const QString combo = gsettingsToKeystroke(raw);
        if (combo.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::Keystroke, combo};
    }

    // app-launch: direct passthrough.
    if (variant.contains(QStringLiteral("app-launch"))) {
        const QJsonObject spec = variant.value(QStringLiteral("app-launch")).toObject();
        const QString binary = spec.value(QStringLiteral("binary")).toString();
        if (binary.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::AppLaunch, binary};
    }

    return std::nullopt;
}

} // namespace logitune
