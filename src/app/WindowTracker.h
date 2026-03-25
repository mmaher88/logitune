#pragma once
#include <QObject>
#include <QDBusInterface>

namespace logitune {

class WindowTracker : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start() = 0;
    virtual bool available() const = 0;
signals:
    void activeWindowChanged(const QString &wmClass, const QString &title);
};

class KWinWindowTracker : public WindowTracker {
    Q_OBJECT
public:
    explicit KWinWindowTracker(QObject *parent = nullptr);
    void start() override;
    bool available() const override;
private slots:
    void onActiveWindowChanged();
private:
    QDBusInterface *m_kwin = nullptr;
    bool m_available = false;
};

} // namespace logitune
