#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QDBusInterface>
#include <QTimer>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <optional>

namespace logitune {

class ActionPresetRegistry;

class KDeDesktop : public LinuxDesktopBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logitune.KDeDesktop")
public:
    explicit KDeDesktop(QObject *parent = nullptr);
    ~KDeDesktop() override;

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;
    bool supportsActionRing() const override {
#ifdef LOGITUNE_LAYER_SHELL
        return true;
#else
        return false;
#endif
    }
    void requestActionRingPlacement(int requestId, const QSize &windowSize,
                                    const QPoint &anchor) override;
    void cancelActionRingPlacement(int requestId) override;

    static QPoint actionRingPlacement(const QPoint &cursor, const QRect &available,
                                      const QSize &windowSize, const QPoint &anchor);

    /// Test seam: point to a registry the resolver should query.
    /// Takes non-owning pointer; the registry must outlive this object.
    void setPresetRegistry(const ActionPresetRegistry *registry) {
        m_registry = registry;
    }

public slots:
    // Called by KWin focus watcher script via D-Bus
    void focusChanged(const QString &resourceClass, const QString &title,
                      const QString &desktopFileName = QString());
    void actionRingCursorPosition(int requestId, int x, int y);

private slots:
    void onActiveWindowChanged();
    void pollActiveWindow();
    void actionRingRequestFailed();

private:
    QDBusInterface *m_kwin = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_lastWmClass;
    bool m_available = false;
    const ActionPresetRegistry *m_registry = nullptr;
    QTimer m_actionRingTimeout;
    QString m_actionRingObjectPath;
    QString m_actionRingScriptName;
    int m_actionRingRequest = -1;
    QSize m_actionRingSize;
    QPoint m_actionRingAnchor;
    bool m_actionRingObjectRegistered = false;
};

} // namespace logitune
