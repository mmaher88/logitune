#pragma once

#include "RadialMenuConfig.h"
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVariantList>

class QWindow;

namespace logitune {

class IDesktopIntegration;

class RadialMenuController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY changed)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition NOTIFY changed)
    Q_PROPERTY(QPoint cursorPosition READ cursorPosition NOTIFY changed)
public:
    explicit RadialMenuController(IDesktopIntegration *desktop,
                                  QObject *parent = nullptr);

    bool visible() const { return m_visible; }
    bool isActive() const { return m_active; }
    QVariantList items() const;
    QRect screenGeometry() const { return m_screenGeometry; }
    QPoint overlayPosition() const { return m_overlayPosition; }
    QPoint cursorPosition() const { return m_cursorPosition; }

    void begin(const RadialMenuConfig &config);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void configureWindow(QWindow *window);

signals:
    void changed();
    void itemsChanged();
    void triggered(const logitune::ButtonAction &action);
    void dismissed();

private:
    void close(bool notifyDismissed);
    static constexpr int kOverlaySize = 380;
    IDesktopIntegration *m_desktop = nullptr;
    QPointer<QWindow> m_window;
    RadialMenuConfig m_config;
    QRect m_screenGeometry;
    QPoint m_overlayPosition;
    QPoint m_cursorPosition;
    int m_requestId = 0;
    bool m_active = false;
    bool m_visible = false;
    bool m_layerShell = false;
};

} // namespace logitune
