#include "SystemColorScheme.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

namespace logitune {

namespace {
const QString kService   = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath      = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kInterface = QStringLiteral("org.freedesktop.portal.Settings");
const QString kNamespace = QStringLiteral("org.freedesktop.appearance");
const QString kKey       = QStringLiteral("color-scheme");
}

SystemColorScheme::SystemColorScheme(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(kService, kPath, kInterface,
        QStringLiteral("SettingChanged"), this,
        SLOT(onSettingChanged(QString,QString,QDBusVariant)));
}

std::optional<bool> SystemColorScheme::readDark() const
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                      QStringLiteral("Read"));
    msg << kNamespace << kKey;
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return std::nullopt;
    m_lastDark = darkFromPortalValue(reply.arguments().first());
    return m_lastDark;
}

std::optional<bool> SystemColorScheme::darkFromPortalValue(const QVariant &value)
{
    QVariant v = value;
    while (v.userType() == qMetaTypeId<QDBusVariant>())
        v = v.value<QDBusVariant>().variant();
    bool ok = false;
    const uint scheme = v.toUInt(&ok);
    if (!ok)
        return std::nullopt;
    return scheme == 1;
}

void SystemColorScheme::onSettingChanged(const QString &ns, const QString &key,
                                         const QDBusVariant &value)
{
    if (ns != kNamespace || key != kKey)
        return;
    // GNOME sends each switch twice, so repeats are dropped.
    auto dark = darkFromPortalValue(value.variant());
    if (!dark || dark == m_lastDark)
        return;
    m_lastDark = dark;
    emit darkChanged(*dark);
}

} // namespace logitune
