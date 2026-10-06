#pragma once
#include <QObject>
#include <QPoint>
#include <QTimer>

namespace logitune {
// Wayland does not expose other clients' pointer positions through QCursor.
// Query KWin only when opening the menu; do not track the pointer continuously.
class CursorPositionProvider : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logitune.CursorPosition")
public:
    explicit CursorPositionProvider(QObject *parent = nullptr);
    ~CursorPositionProvider() override;
    void request(int generation);
    void cancel();
signals:
    void positionReady(int generation, const QPoint &position);
public slots:
    void receivePosition(int generation, int x, int y);
private:
    void fallback();
    QString m_objectPath;
    QString m_scriptName;
    QTimer m_timeout;
    int m_generation = -1;
    bool m_registered = false;
};
}
