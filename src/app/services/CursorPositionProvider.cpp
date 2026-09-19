#include "CursorPositionProvider.h"
#include "logging/LogManager.h"
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QGuiApplication>
#include <QPointer>
#include <QTemporaryFile>
#include <memory>

namespace logitune {
namespace {
QDBusMessage scriptingCall(const QString &method) {
    return QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting",
                                         "org.kde.kwin.Scripting", method);
}
void unload(const QString &name) {
    if (name.isEmpty()) return;
    auto message = scriptingCall("unloadScript");
    message << name;
    QDBusConnection::sessionBus().asyncCall(message);
}
}
CursorPositionProvider::CursorPositionProvider(QObject *parent) : QObject(parent)
{
    static int instance = 0;
    m_objectPath = QStringLiteral("/Logitune/CursorPosition%1").arg(++instance);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(200);
    connect(&m_timeout, &QTimer::timeout, this, &CursorPositionProvider::fallback);
}
CursorPositionProvider::~CursorPositionProvider()
{
    cancel();
    if (m_registered) QDBusConnection::sessionBus().unregisterObject(m_objectPath);
}
void CursorPositionProvider::cancel()
{
    m_timeout.stop();
    m_generation = -1;
    unload(m_scriptName);
    m_scriptName.clear();
}
void CursorPositionProvider::fallback()
{
    if (m_generation < 0) return;
    const int generation = m_generation;
    cancel();
    emit positionReady(generation, QCursor::pos());
}
void CursorPositionProvider::receivePosition(int generation, int x, int y)
{
    if (generation != m_generation) return;
    qCDebug(lcApp) << "Radial menu cursor from KWin:" << QPoint(x, y);
    cancel();
    emit positionReady(generation, QPoint(x, y));
}
void CursorPositionProvider::request(int generation)
{
    cancel();
    m_generation = generation;
    const bool kde = qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive);
    if (!kde || !QGuiApplication::platformName().startsWith("wayland")) {
        fallback();
        return;
    }
    auto bus = QDBusConnection::sessionBus();
    if (!m_registered)
        m_registered = bus.registerObject(m_objectPath, this, QDBusConnection::ExportAllSlots);
    if (!m_registered) { fallback(); return; }

    // Unique service + object names keep test instances and concurrent app processes separate.
    m_scriptName = QStringLiteral("logitune_cursor_%1_%2_%3")
                       .arg(QCoreApplication::applicationPid()).arg(m_objectPath.section('/', -1)).arg(generation);
    const QString scriptName = m_scriptName;
    auto script = std::make_shared<QTemporaryFile>(QDir::tempPath() + "/logitune-cursor-XXXXXX.js");
    if (!script->open()) { fallback(); return; }
    const QByteArray source = QStringLiteral(
        "var p = workspace.cursorPos;\n"
        "callDBus('%1', '%2', 'com.logitune.CursorPosition', 'receivePosition', %3, p.x, p.y);\n")
        .arg(bus.baseService(), m_objectPath).arg(generation).toUtf8();
    if (script->write(source) != source.size() || !script->flush()) { fallback(); return; }

    auto load = scriptingCall("loadScript");
    load << script->fileName() << scriptName;
    m_timeout.start();
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(load, 200), QCoreApplication::instance());
    QPointer<CursorPositionProvider> self(this);
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
            [self, watcher, generation, scriptName, script]() {
        const QDBusPendingReply<int> reply = *watcher;
        watcher->deleteLater();
        if (!self || self->m_generation != generation) { unload(scriptName); return; }
        if (reply.isError() || reply.value() < 0) { self->fallback(); return; }
        auto run = QDBusMessage::createMethodCall("org.kde.KWin",
            QStringLiteral("/Scripting/Script%1").arg(reply.value()), "org.kde.kwin.Script", "run");
        // Keep the temporary script alive until KWin has read and executed it.
        auto *running = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(run, 200),
                                                  QCoreApplication::instance());
        connect(running, &QDBusPendingCallWatcher::finished, running,
                [self, running, generation, scriptName, script]() {
            const QDBusPendingReply<> result = *running;
            running->deleteLater();
            if (!self || self->m_generation != generation) { unload(scriptName); return; }
            if (result.isError()) self->fallback();
        });
    });
}
}
