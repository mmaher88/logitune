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
    SingleInstance(const QString &lockPath, const QString &serverName);

    // An exit code means this launch must not initialize the devices or UI.
    std::optional<int> start();

signals:
    void showRequested();

private:
    QLockFile m_lockFile;
    QLocalServer m_server;
    QString m_serverName;
};

} // namespace logitune
