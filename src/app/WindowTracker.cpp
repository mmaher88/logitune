#include "WindowTracker.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

namespace logitune {

KWinWindowTracker::KWinWindowTracker(QObject *parent)
    : WindowTracker(parent)
{
}

void KWinWindowTracker::start()
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
        qWarning() << "KWinWindowTracker: org.kde.KWin D-Bus service not available";
        m_available = false;
        return;
    }

    m_available = true;

    // KWin emits activeClientChanged on /KWin via org.kde.KWin
    bool connected = QDBusConnection::sessionBus().connect(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("activeClientChanged"),
        this,
        SLOT(onActiveWindowChanged()));

    if (!connected) {
        qWarning() << "KWinWindowTracker: failed to connect to activeClientChanged signal";
    }
}

bool KWinWindowTracker::available() const
{
    return m_available;
}

void KWinWindowTracker::onActiveWindowChanged()
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

} // namespace logitune
