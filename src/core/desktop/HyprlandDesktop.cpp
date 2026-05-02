#include "desktop/HyprlandDesktop.h"
#include "actions/ActionPresetRegistry.h"
#include "logging/LogManager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QTimer>

namespace logitune {

namespace {
constexpr int kSocketTimeoutMs = 1000;
constexpr int kReconnectIntervalMs = 1000;

bool hasUnsupportedModifiers(int modmask)
{
    constexpr int supported =
        0x01 |  // Shift
        0x04 |  // Ctrl
        0x08 |  // Alt
        0x40;   // Super
    return (modmask & ~supported) != 0;
}
} // namespace

HyprlandDesktop::HyprlandDesktop(QObject *parent)
    : LinuxDesktopBase(parent)
{
}

void HyprlandDesktop::start()
{
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        m_reconnectTimer->setInterval(kReconnectIntervalMs);
        connect(m_reconnectTimer, &QTimer::timeout,
                this, &HyprlandDesktop::connectEventSocket);
    }

    if (eventSocketPath().isEmpty()) {
        qCWarning(lcFocus) << "Hyprland: missing IPC socket path; focus tracking disabled";
        m_available = false;
        return;
    }

    connectEventSocket();
}

bool HyprlandDesktop::available() const
{
    return m_available;
}

QString HyprlandDesktop::desktopName() const
{
    return QStringLiteral("Hyprland");
}

QStringList HyprlandDesktop::detectedCompositors() const
{
    QStringList compositors;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString desktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (desktop.contains(QStringLiteral("Hyprland"), Qt::CaseInsensitive)
        || !env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE")).isEmpty()) {
        compositors << QStringLiteral("Hyprland");
    }
    return compositors;
}

void HyprlandDesktop::blockGlobalShortcuts(bool)
{
    // Hyprland does not provide a stable compositor-wide shortcut-block API.
    // Keystroke capture remains functional; global bindings may still fire.
}

QString HyprlandDesktop::variantKey() const
{
    return QStringLiteral("hyprland");
}

std::optional<ButtonAction> HyprlandDesktop::resolveNamedAction(const QString &id) const
{
    if (!m_registry)
        return std::nullopt;

    const QJsonObject variant = m_registry->variantData(id, QStringLiteral("hyprland"));
    if (variant.isEmpty())
        return std::nullopt;

    if (variant.contains(QStringLiteral("hyprland-bind")))
        return resolveHyprlandBind(variant.value(QStringLiteral("hyprland-bind")).toObject());

    if (variant.contains(QStringLiteral("app-launch"))) {
        const QJsonObject spec = variant.value(QStringLiteral("app-launch")).toObject();
        const QString binary = spec.value(QStringLiteral("binary")).toString();
        if (binary.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::AppLaunch, binary};
    }

    return std::nullopt;
}

void HyprlandDesktop::connectEventSocket()
{
    const QString path = eventSocketPath();
    if (path.isEmpty()) {
        m_available = false;
        return;
    }

    if (!m_eventSocket) {
        m_eventSocket = new QLocalSocket(this);
        connect(m_eventSocket, &QLocalSocket::readyRead,
                this, &HyprlandDesktop::onEventReadyRead);
        connect(m_eventSocket, &QLocalSocket::connected, this, [this]() {
            m_available = true;
            qCInfo(lcFocus) << "Hyprland desktop integration started";
        });
        connect(m_eventSocket, &QLocalSocket::disconnected,
                this, &HyprlandDesktop::scheduleReconnect);
        connect(m_eventSocket, &QLocalSocket::errorOccurred, this,
                [this](QLocalSocket::LocalSocketError) {
                    m_available = false;
                    scheduleReconnect();
                });
    }

    if (m_eventSocket->state() != QLocalSocket::UnconnectedState)
        m_eventSocket->abort();

    m_eventBuffer.clear();
    m_eventSocket->connectToServer(path);
}

void HyprlandDesktop::scheduleReconnect()
{
    m_available = false;
    if (m_reconnectTimer && !m_reconnectTimer->isActive())
        m_reconnectTimer->start();
}

void HyprlandDesktop::onEventReadyRead()
{
    if (!m_eventSocket)
        return;

    m_eventBuffer += m_eventSocket->readAll();

    int newline = -1;
    while ((newline = m_eventBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_eventBuffer.left(newline).trimmed();
        m_eventBuffer.remove(0, newline + 1);
        handleEventLine(line);
    }
}

void HyprlandDesktop::handleEventLine(const QByteArray &line)
{
    static const QByteArray prefix = "activewindow>>";
    if (!line.startsWith(prefix))
        return;

    const QByteArray payload = line.mid(prefix.size());
    const int comma = payload.indexOf(',');
    const QString resourceClass = QString::fromUtf8(
        comma >= 0 ? payload.left(comma) : payload).trimmed();
    const QString title = comma >= 0
        ? QString::fromUtf8(payload.mid(comma + 1)).trimmed()
        : QString();

    if (resourceClass.isEmpty())
        return;

    const QString appId = resolveDesktopFile(resourceClass);
    if (appId == m_lastAppId)
        return;

    m_lastAppId = appId;
    emit activeWindowChanged(appId, title);
}

QString HyprlandDesktop::socketDir() const
{
    if (!m_socketDirOverride.isEmpty())
        return m_socketDirOverride;

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString runtimeDir = env.value(QStringLiteral("XDG_RUNTIME_DIR"));
    const QString signature = env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"));
    if (runtimeDir.isEmpty() || signature.isEmpty())
        return {};

    return runtimeDir + QStringLiteral("/hypr/") + signature;
}

QString HyprlandDesktop::eventSocketPath() const
{
    const QString dir = socketDir();
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/.socket2.sock");
}

QString HyprlandDesktop::commandSocketPath() const
{
    const QString dir = socketDir();
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/.socket.sock");
}

QByteArray HyprlandDesktop::readHyprlandCommand(const QByteArray &command) const
{
    const QString path = commandSocketPath();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return {};

    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(kSocketTimeoutMs))
        return {};

    socket.write(command);
    if (!socket.waitForBytesWritten(kSocketTimeoutMs))
        return {};

    QByteArray out;
    if (socket.waitForReadyRead(kSocketTimeoutMs))
        out += socket.readAll();
    while (socket.waitForReadyRead(25))
        out += socket.readAll();

    socket.disconnectFromServer();
    return out;
}

QByteArray HyprlandDesktop::readBindsJson() const
{
    if (m_bindReader)
        return m_bindReader();
    return readHyprlandCommand(QByteArrayLiteral("j/binds"));
}

std::optional<ButtonAction> HyprlandDesktop::resolveHyprlandBind(
    const QJsonObject &spec) const
{
    const QString dispatcher = spec.value(QStringLiteral("dispatcher")).toString();
    const bool requiresArg = spec.contains(QStringLiteral("arg"));
    const QString arg = spec.value(QStringLiteral("arg")).toString();
    if (dispatcher.isEmpty())
        return std::nullopt;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(readBindsJson(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return std::nullopt;

    const QJsonArray binds = doc.array();
    for (const QJsonValue &value : binds) {
        const QJsonObject bind = value.toObject();
        if (bind.value(QStringLiteral("mouse")).toBool())
            continue;
        if (!bind.value(QStringLiteral("submap")).toString().isEmpty())
            continue;
        if (bind.value(QStringLiteral("dispatcher")).toString()
                .compare(dispatcher, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (requiresArg && bind.value(QStringLiteral("arg")).toString() != arg)
            continue;

        const QString combo = bindingToKeystroke(
            bind.value(QStringLiteral("modmask")).toInt(),
            bind.value(QStringLiteral("key")).toString());
        if (!combo.isEmpty())
            return ButtonAction{ButtonAction::Keystroke, combo};
    }

    return std::nullopt;
}

QString HyprlandDesktop::bindingToKeystroke(int modmask, const QString &key)
{
    if (key.isEmpty() || key.startsWith(QStringLiteral("mouse:"), Qt::CaseInsensitive)
        || hasUnsupportedModifiers(modmask)) {
        return {};
    }

    QStringList parts;
    if (modmask & 0x04) parts << QStringLiteral("Ctrl");
    if (modmask & 0x01) parts << QStringLiteral("Shift");
    if (modmask & 0x08) parts << QStringLiteral("Alt");
    if (modmask & 0x40) parts << QStringLiteral("Super");

    const QString normalized = normalizeKeyName(key);
    if (normalized.isEmpty())
        return {};

    parts << normalized;
    return parts.join(QLatin1Char('+'));
}

QString HyprlandDesktop::normalizeKeyName(const QString &key)
{
    const QString trimmed = key.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.length() == 1)
        return trimmed;

    const QString lower = trimmed.toLower();
    if (lower == QStringLiteral("return")) return QStringLiteral("Enter");
    if (lower == QStringLiteral("enter")) return QStringLiteral("Enter");
    if (lower == QStringLiteral("escape")) return QStringLiteral("Escape");
    if (lower == QStringLiteral("esc")) return QStringLiteral("Escape");
    if (lower == QStringLiteral("delete")) return QStringLiteral("Delete");
    if (lower == QStringLiteral("space")) return QStringLiteral("Space");
    if (lower == QStringLiteral("tab")) return QStringLiteral("Tab");
    if (lower == QStringLiteral("left")) return QStringLiteral("Left");
    if (lower == QStringLiteral("right")) return QStringLiteral("Right");
    if (lower == QStringLiteral("up")) return QStringLiteral("Up");
    if (lower == QStringLiteral("down")) return QStringLiteral("Down");
    if (lower == QStringLiteral("print") || lower == QStringLiteral("printscreen"))
        return QStringLiteral("Print");
    if (lower.startsWith(QLatin1Char('f'))) {
        bool ok = false;
        const int n = lower.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 12)
            return QStringLiteral("F%1").arg(n);
    }

    return {};
}

} // namespace logitune
