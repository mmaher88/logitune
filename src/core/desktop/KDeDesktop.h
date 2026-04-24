#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QDBusInterface>
#include <QTimer>
#include <optional>

namespace logitune {

class KDeDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit KDeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;

public slots:
    // Called by KWin focus watcher script via D-Bus
    void focusChanged(const QString &resourceClass, const QString &title,
                      const QString &desktopFileName = QString());

private slots:
    void onActiveWindowChanged();
    void pollActiveWindow();

private:
    QDBusInterface *m_kwin = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_lastWmClass;
    bool m_available = false;
};

} // namespace logitune
