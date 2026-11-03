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
                    const auto command = socket->readLine();
                    if (command == "show\n")
                        emit showRequested();
                    else if (command == "quit\n")
                        emit quitRequested();
                    else if (command == "ring\n")
                        emit ringRequested();
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

std::optional<int> SingleInstance::start(Command command)
{
    if (m_lockFile.tryLock(100)) {
        if (command == Command::Quit) {
            m_lockFile.unlock();
            return 0;
        }
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
            const QByteArray message = command == Command::Quit ? "quit\n"
                : command == Command::Ring ? "ring\n" : "show\n";
            if (socket.write(message) != message.size())
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
