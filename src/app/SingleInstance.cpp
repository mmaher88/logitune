#include "SingleInstance.h"

#include <QElapsedTimer>
#include <QLocalSocket>
#include <QThread>

namespace logitune {

SingleInstance::SingleInstance(const QString &lockPath, const QString &serverName)
    : m_lockFile(lockPath), m_serverName(serverName)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (auto *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            auto receive = [this, socket]() {
                if (socket->canReadLine()) {
                    if (socket->readLine() == "show\n")
                        emit showRequested();
                    socket->disconnectFromServer();
                } else if (socket->bytesAvailable() > 64) {
                    socket->disconnectFromServer();
                }
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            receive();
        }
    });
}

std::optional<int> SingleInstance::start()
{
    if (m_lockFile.tryLock(100)) {
        // Only the lock owner may remove a socket left by a crashed instance.
        QLocalServer::removeServer(m_serverName);
        if (!m_server.listen(m_serverName))
            return 1;
        return std::nullopt;
    }

    // A concurrent first launch may own the lock before its socket is ready.
    QElapsedTimer timeout;
    timeout.start();
    do {
        QLocalSocket socket;
        socket.connectToServer(m_serverName);
        if (socket.waitForConnected(100)) {
            if (socket.write("show\n") != 5)
                return 1;
            if (socket.bytesToWrite() && !socket.waitForBytesWritten(1000))
                return 1;
            return 0;
        }
        QThread::msleep(50);
    } while (timeout.elapsed() < 2000);

    return 1;
}

} // namespace logitune
