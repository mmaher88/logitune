#include "desktop/GnomeDesktop.h"
#include "actions/ActionPresetRegistry.h"
#include "logging/LogManager.h"
#include <optional>
#include <QCoreApplication>
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
static constexpr auto kWorkspaceExtUuid = "logitune-workspace@logitune.com";

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

    // Install and enable the focus-watcher extension
    if (!ensureExtensionInstalled()) {
        qCWarning(lcFocus) << "GNOME: failed to install/enable extension";
        m_available = false;
        return;
    }

    // User-only workspace scrub extension (avoids colliding with a packaged
    // logitune-focus that Shell prefers from /usr/share).
    if (!ensureWorkspaceExtensionInstalled()) {
        qCWarning(lcFocus) << "GNOME: workspace extension install failed";
    }

    // Register our D-Bus service so the extension can call us
    QDBusConnection::sessionBus().registerService(QStringLiteral("com.logitune.app"));
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/FocusWatcher"), this,
        QDBusConnection::ExportAllSlots);

    // Probe workspace-gesture D-Bus exported by logitune-workspace.
    QDBusMessage probe = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/com/logitune/WorkspaceGesture"),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));
    QDBusMessage probeReply =
        QDBusConnection::sessionBus().call(probe, QDBus::Block, 1000);
    m_workspaceGestureAvailable = false;
    if (probeReply.type() == QDBusMessage::ReplyMessage) {
        const QString xml = probeReply.arguments().value(0).toString();
        m_workspaceGestureAvailable =
            xml.contains(QStringLiteral("com.logitune.WorkspaceGesture"));
    }
    if (!m_workspaceGestureAvailable) {
        qCWarning(lcFocus) << "GNOME: workspace gesture D-Bus not available yet "
                              "(enable logitune-workspace@logitune.com, or log "
                              "out/in after first install)";
    }

    m_available = true;
    qCInfo(lcFocus) << "GNOME desktop integration started"
                    << "workspaceGesture=" << m_workspaceGestureAvailable;
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

QString GnomeDesktop::findExtensionSourceDir(const QString &uuid) const
{
    QStringList sourceRoots;
    const QString appDir = QCoreApplication::applicationDirPath();
    sourceRoots << (appDir + QStringLiteral("/../share/gnome-shell/extensions/") + uuid);
    sourceRoots << (QStringLiteral("/usr/local/share/gnome-shell/extensions/") + uuid);
    sourceRoots << (QStringLiteral("/usr/share/gnome-shell/extensions/") + uuid);
    // Dev tree fallback when running an uninstalled build binary.
    sourceRoots << (appDir + QStringLiteral("/../../data/gnome-workspace-extension"));
    sourceRoots << (appDir + QStringLiteral("/../../data/gnome-extension"));
    sourceRoots << (QDir::homePath()
                    + QStringLiteral("/.local/share/logitune/gnome-workspace-extension"));

    // ~/.local/bin/logitune resolves ../share/... to the *user* extension install
    // dir. Never treat that as a source — syncing from it deletes then fails.
    const QString userInstallDir = QDir(QDir::homePath()
                                        + QStringLiteral("/.local/share/gnome-shell/extensions/")
                                        + uuid)
                                       .canonicalPath();

    for (const QString &root : sourceRoots) {
        const QString canonical = QDir(root).canonicalPath();
        if (canonical.isEmpty())
            continue;
        if (!userInstallDir.isEmpty() && canonical == userInstallDir)
            continue;
        if (QFile::exists(canonical + QStringLiteral("/metadata.json")))
            return canonical;
    }
    return {};
}

bool GnomeDesktop::syncExtensionToUserDir(const QString &uuid, const QString &sourceDir,
                                          const QString &variant)
{
    const QString userDir = QDir::homePath()
                            + QStringLiteral("/.local/share/gnome-shell/extensions/")
                            + uuid;
    QDir().mkpath(userDir);
    const QString variantJs = sourceDir + QStringLiteral("/") + variant
                              + QStringLiteral("/extension.js");
    const QString rootJs = sourceDir + QStringLiteral("/extension.js");
    const QString srcJs = QFile::exists(variantJs) ? variantJs : rootJs;
    if (!QFile::exists(sourceDir + QStringLiteral("/metadata.json"))
        || !QFile::exists(srcJs)) {
        qCWarning(lcFocus) << "Incomplete extension source for" << uuid << "in" << sourceDir;
        return false;
    }
    // Stage into temps first so a failed copy never leaves the user dir empty
    // (which previously broke the session-facing extension until reinstall).
    const QString tmpMeta = userDir + QStringLiteral("/.metadata.json.new");
    const QString tmpJs = userDir + QStringLiteral("/.extension.js.new");
    QFile::remove(tmpMeta);
    QFile::remove(tmpJs);
    if (!QFile::copy(sourceDir + QStringLiteral("/metadata.json"), tmpMeta)
        || !QFile::copy(srcJs, tmpJs)) {
        qCWarning(lcFocus) << "Failed to stage extension files for" << uuid;
        QFile::remove(tmpMeta);
        QFile::remove(tmpJs);
        return false;
    }
    QFile::remove(userDir + QStringLiteral("/metadata.json"));
    QFile::remove(userDir + QStringLiteral("/extension.js"));
    if (!QFile::rename(tmpMeta, userDir + QStringLiteral("/metadata.json"))
        || !QFile::rename(tmpJs, userDir + QStringLiteral("/extension.js"))) {
        qCWarning(lcFocus) << "Failed to install staged extension files for" << uuid;
        return false;
    }
    qCInfo(lcFocus) << "Synced" << uuid << variant << "from" << sourceDir << "to" << userDir;
    return true;
}

bool GnomeDesktop::enableShellExtension(const QString &uuid)
{
    QDBusMessage enableMsg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("/org/gnome/Shell/Extensions"),
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("EnableExtension"));
    enableMsg << uuid;
    QDBusReply<bool> enableReply =
        QDBusConnection::sessionBus().call(enableMsg, QDBus::Block, 2000);
    if (enableReply.isValid() && enableReply.value()) {
        qCInfo(lcFocus) << "GNOME extension enabled:" << uuid;
        return true;
    }
    QProcess proc;
    proc.start(QStringLiteral("gnome-extensions"), {QStringLiteral("enable"), uuid});
    proc.waitForFinished(3000);
    if (proc.exitCode() != 0) {
        qCWarning(lcFocus) << "Failed to enable" << uuid << ":" << proc.readAllStandardError();
        return false;
    }
    qCInfo(lcFocus) << "GNOME extension enabled via CLI:" << uuid;
    return true;
}

bool GnomeDesktop::ensureExtensionInstalled()
{
    int major = detectShellMajorVersion();
    if (major < 42) {
        qCWarning(lcFocus) << "GNOME Shell version" << major << "not supported (need 42+)";
        return false;
    }

    const QString variant = (major >= 45) ? QStringLiteral("v45") : QStringLiteral("v42");
    const QString systemDir = QStringLiteral("/usr/share/gnome-shell/extensions/")
                              + QLatin1String(kExtUuid);
    const bool systemComplete =
        QFile::exists(systemDir + QStringLiteral("/extension.js"))
        && QFile::exists(systemDir + QStringLiteral("/metadata.json"));

    // Packaged focus extension is fine for focus tracking; don't fight it.
    if (systemComplete)
        return enableShellExtension(QString::fromLatin1(kExtUuid));

    const QString sourceDir = findExtensionSourceDir(QString::fromLatin1(kExtUuid));
    if (sourceDir.isEmpty()) {
        qCWarning(lcFocus) << "Focus extension not found";
        return false;
    }
    if (!syncExtensionToUserDir(QString::fromLatin1(kExtUuid), sourceDir, variant))
        return false;
    return enableShellExtension(QString::fromLatin1(kExtUuid));
}

bool GnomeDesktop::ensureWorkspaceExtensionInstalled()
{
    int major = detectShellMajorVersion();
    if (major < 42)
        return false;

    const QString variant = (major >= 45) ? QStringLiteral("v45") : QStringLiteral("v42");
    const QString userDir = QDir::homePath()
                            + QStringLiteral("/.local/share/gnome-shell/extensions/")
                            + QLatin1String(kWorkspaceExtUuid);
    const bool userInstalled =
        QFile::exists(userDir + QStringLiteral("/metadata.json"))
        && QFile::exists(userDir + QStringLiteral("/extension.js"));

    QString sourceDir =
        findExtensionSourceDir(QString::fromLatin1(kWorkspaceExtUuid));
    // Uninstalled / local builds: data/gnome-workspace-extension next to the repo
    // (binary lives in build/src/app). Also accept a user share copy used when
    // the binary is installed to ~/.local/bin (no repo-relative path).
    if (sourceDir.isEmpty()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            QDir::homePath()
                + QStringLiteral("/.local/share/logitune/gnome-workspace-extension"),
            appDir + QStringLiteral("/../../../data/gnome-workspace-extension"),
            appDir + QStringLiteral("/../../data/gnome-workspace-extension"),
        };
        for (const QString &c : candidates) {
            const QString canonical = QDir(c).canonicalPath();
            if (QFile::exists(canonical + QStringLiteral("/metadata.json"))) {
                sourceDir = canonical;
                break;
            }
        }
    }

    // Already present in the user extension dir (e.g. prior manual install) —
    // just ensure it is enabled. Prefer this over syncing when source is missing.
    if (sourceDir.isEmpty()) {
        if (userInstalled)
            return enableShellExtension(QString::fromLatin1(kWorkspaceExtUuid));
        qCWarning(lcFocus) << "Workspace extension source not found";
        return false;
    }
    if (!syncExtensionToUserDir(QString::fromLatin1(kWorkspaceExtUuid), sourceDir, variant))
        return false;
    return enableShellExtension(QString::fromLatin1(kWorkspaceExtUuid));
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

bool GnomeDesktop::supportsInteractiveWorkspaceSwitch() const
{
    return m_available && m_workspaceGestureAvailable;
}

void GnomeDesktop::callWorkspaceGesture(const QString &method, const QVariantList &args)
{
    if (!supportsInteractiveWorkspaceSwitch())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/com/logitune/WorkspaceGesture"),
        QStringLiteral("com.logitune.WorkspaceGesture"),
        method);
    msg.setArguments(args);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

void GnomeDesktop::beginInteractiveWorkspaceSwitch()
{
    callWorkspaceGesture(QStringLiteral("BeginWorkspaceGesture"));
}

void GnomeDesktop::updateInteractiveWorkspaceSwitch(int delta)
{
    callWorkspaceGesture(QStringLiteral("UpdateWorkspaceGesture"), {delta});
}

void GnomeDesktop::endInteractiveWorkspaceSwitch(int /*snapPercent*/)
{
    // snapPercent is applied by ButtonActionDispatcher (progress padding)
    // before this call — End stays arg-less for live Shell module compat.
    callWorkspaceGesture(QStringLiteral("EndWorkspaceGesture"));
}

// ---------------------------------------------------------------------------
// gsettings output to keystroke transform
// ---------------------------------------------------------------------------

QString GnomeDesktop::gsettingsToKeystroke(const QString &gsettingsOutput)
{
    // gsettings returns GLib variant strings like "['<Super>d']" or
    // "['<Primary><Alt>Left', '<Super>D']". Find the first non-empty
    // single-quoted binding.
    // Note: this regex does not handle GVariant's \' escape. Real gsettings
    // keybinding values are alphanumeric or named keysyms (Print, F1, Left,
    // etc.), so apostrophes never appear inside a binding string.
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
    int pos = 0;  // End of last matched modifier; remaining suffix is the keysym.
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
                   mod.compare(QStringLiteral("Meta"),  Qt::CaseInsensitive) == 0 ||
                   mod.compare(QStringLiteral("Hyper"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Super");
        } else if (mod.compare(QStringLiteral("AltGr"), Qt::CaseInsensitive) == 0 ||
                   mod.compare(QStringLiteral("ISO_Level3_Shift"), Qt::CaseInsensitive) == 0) {
            pos = m.capturedEnd();
            continue;   // Skip; not representable in Logitune keystroke format
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
            if (!p.waitForFinished(3000)) {
                qCWarning(lcFocus) << "gsettings get" << schema << key
                                   << "did not finish; state:" << p.state();
                return std::nullopt;
            }
            if (p.exitCode() != 0) {
                qCWarning(lcFocus) << "gsettings failed for" << schema << key
                                   << ":" << p.readAllStandardError().trimmed();
                return std::nullopt;
            }
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
