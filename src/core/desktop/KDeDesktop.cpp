#include "desktop/KDeDesktop.h"
#include "logging/LogManager.h"
#include "actions/ActionPresetRegistry.h"
#include <optional>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace logitune {

KDeDesktop::KDeDesktop(QObject *parent)
    : LinuxDesktopBase(parent)
{
}

void KDeDesktop::start()
{
    // Connect to the KWin script engine D-Bus interface to detect window changes.
    // KWin exposes window activity via org.kde.KWin on the session bus.
    m_kwin = new QDBusInterface(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QDBusConnection::sessionBus(),
        this);

    if (!m_kwin->isValid()) {
        m_available = false;
        return;
    }

    m_available = true;

    // KWin 5.x emits activeClientChanged, KDE 6 removed it.
    // Try the signal first, fall back to polling.
    bool connected = QDBusConnection::sessionBus().connect(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("activeClientChanged"),
        this,
        SLOT(onActiveWindowChanged()));

    // KDE 6: activeClientChanged connects but never fires. Always use poll fallback.
    if (true) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(500);
        connect(m_pollTimer, &QTimer::timeout, this, &KDeDesktop::pollActiveWindow);
        m_pollTimer->start();
    }
}

bool KDeDesktop::available() const
{
    return m_available;
}

QString KDeDesktop::desktopName() const
{
    return QStringLiteral("KDE");
}

QStringList KDeDesktop::detectedCompositors() const
{
    QStringList compositors;
    const QString desktop = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (desktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive))
        compositors << QStringLiteral("KWin");
    return compositors;
}

void KDeDesktop::onActiveWindowChanged()
{
    if (!m_kwin || !m_kwin->isValid())
        return;

    // Ask KWin for the active client's details.
    // We call org.kde.KWin.activeClient() to get the window id, then query
    // org.kde.KWin.Client (the active window's interface) for resourceClass / caption.
    QDBusMessage reply = m_kwin->call(QStringLiteral("activeClient"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;

    // The active client is returned as an object path or int depending on KWin version.
    // For KWin >= 5.23 it is an object path under /org/kde/KWin/Clients/<id>.
    QString wmClass;
    QString title;

    QVariant clientVariant = reply.arguments().first();
    QString clientPath = clientVariant.toString();

    if (!clientPath.isEmpty()) {
        QDBusInterface clientIface(
            QStringLiteral("org.kde.KWin"),
            clientPath,
            QStringLiteral("org.kde.KWin.Window"),
            QDBusConnection::sessionBus());

        if (clientIface.isValid()) {
            wmClass = clientIface.property("resourceClass").toString();
            title   = clientIface.property("caption").toString();
        }
    }

    emit activeWindowChanged(wmClass, title);
}

void KDeDesktop::pollActiveWindow()
{
    // KDE 6: no D-Bus method for active window. Use a persistent KWin script
    // that writes active window info to a temp file on workspace.activeWindow change.
    static bool scriptInstalled = false;
    static const QString dataFile = QDir::tempPath() + QStringLiteral("/logitune_active_window");

    if (!scriptInstalled) {
        // Register our D-Bus service so KWin script can call us back
        QDBusConnection::sessionBus().registerService(QStringLiteral("com.logitune.app"));
        QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/FocusWatcher"), this,
            QDBusConnection::ExportAllSlots);

        // Create a KWin script that watches for window focus changes
        // and calls back to our D-Bus service
        QString scriptPath = QDir::tempPath() + QStringLiteral("/logitune_focus_watcher.js");
        QFile f(scriptPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(
                "function update() {\n"
                "    var c = workspace.activeWindow;\n"
                "    if (c) {\n"
                "        callDBus('com.logitune.app', '/FocusWatcher',\n"
                "                 'local.logitune.logitune.KDeDesktop', 'focusChanged',\n"
                "                 c.resourceClass, c.caption,\n"
                "                 c.desktopFileName || '');\n"
                "    }\n"
                "}\n"
                "workspace.windowActivated.connect(update);\n"
                "update();\n"
            );
            f.close();
        }

        QDBusMessage loadMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting"),
            QStringLiteral("loadScript"));
        loadMsg << scriptPath << QStringLiteral("logitune_focus_watcher");
        QDBusConnection::sessionBus().call(loadMsg);

        QDBusMessage startMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting"),
            QStringLiteral("start"));
        QDBusConnection::sessionBus().call(startMsg);

        scriptInstalled = true;

        // Stop the timer — we'll get callbacks instead
        if (m_pollTimer) {
            m_pollTimer->stop();
            m_pollTimer->deleteLater();
            m_pollTimer = nullptr;
        }
    }
}

void KDeDesktop::focusChanged(const QString &resourceClass, const QString &title,
                              const QString &desktopFileName)
{
    // Resolve to canonical app ID: the .desktop file completeBaseName.
    // 1. desktopFileName from KWin (most reliable, set by well-behaved apps)
    // 2. Look up .desktop file by resourceClass match (handles apps like Zoom
    //    where resourceClass="zoom" but .desktop is "us.zoom.Zoom.desktop")
    // 3. resourceClass as final fallback
    QString appId;
    if (!desktopFileName.isEmpty()) {
        appId = desktopFileName;
    } else {
        appId = resolveDesktopFile(resourceClass);
    }

    if (appId == m_lastWmClass) return;
    m_lastWmClass = appId;
    emit activeWindowChanged(appId, title);
}

void KDeDesktop::blockGlobalShortcuts(bool block)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QStringLiteral("blockGlobalShortcuts"));
    msg << block;
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

QString KDeDesktop::variantKey() const
{
    return QStringLiteral("kde");
}

std::optional<ButtonAction> KDeDesktop::resolveNamedAction(const QString &id) const
{
    if (!m_registry)
        return std::nullopt;

    const QJsonObject variant = m_registry->variantData(id, QStringLiteral("kde"));
    if (variant.isEmpty())
        return std::nullopt;

    // kglobalaccel: invoke a named action by DBus. Encoded as a five-field
    // comma-separated payload: service,path,interface,method,arg
    // The DBus fields cannot contain commas by construction; the action name
    // (arg) must not contain commas either (no escape is supported). Every
    // kglobalaccel action we ship is comma-free.
    if (variant.contains(QStringLiteral("kglobalaccel"))) {
        const QJsonObject spec = variant.value(QStringLiteral("kglobalaccel")).toObject();
        const QString component = spec.value(QStringLiteral("component")).toString();
        const QString name = spec.value(QStringLiteral("name")).toString();
        if (component.isEmpty() || name.isEmpty())
            return std::nullopt;

        const QString payload =
            QStringLiteral("org.kde.kglobalaccel,/component/") + component +
            QStringLiteral(",org.kde.kglobalaccel.Component,invokeShortcut,") + name;
        return ButtonAction{ButtonAction::DBus, payload};
    }

    // app-launch: simpler, same transport as the existing AppLaunch type.
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
