#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <QString>
#include <optional>

namespace logitune {

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    enum class Command { Show, Quit, Ring };
    SingleInstance(const QString &lockPath, const QString &serverName);

    // An exit code means this launch must not initialize the devices or UI.
    std::optional<int> start(Command command = Command::Show);

signals:
    void showRequested();
    void quitRequested();
    void ringRequested();

private:
    QLockFile m_lockFile;
    QLocalServer m_server;
    QString m_serverName;
};

} // namespace logitune
