#pragma once
#include "RadialMenuConfig.h"
#include "CursorPositionProvider.h"
#include <QPointer>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QRect>
#include <QWindow>

namespace logitune {
class RadialMenuController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(int selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY changed)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition NOTIFY changed)
    Q_PROPERTY(QPoint cursorPosition READ cursorPosition NOTIFY changed)
public:
    explicit RadialMenuController(QObject *parent = nullptr);
    bool visible() const { return m_visible; }
    int selected() const { return m_selected; }
    QVariantList items() const { return m_config.items; }
    QRect screenGeometry() const { return m_screenGeometry; }
    QPoint overlayPosition() const { return m_overlayPosition; }
    QPoint cursorPosition() const { return m_cursorPosition; }
    static QPoint placement(const QPoint &cursor, const QRect &available,
                            const QSize &size, const QPoint &center);
    void begin(const RadialMenuConfig &config);
    void move(int dx, int dy);
    void release();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void configureWindow(QWindow *window);
signals:
    void changed();
    void triggered(const logitune::ButtonAction &action);
private:
    void open();
    void positionAt(const QPoint &cursor);
    CursorPositionProvider m_cursorProvider;
    QPointer<QWindow> m_window;
    QPoint m_overlayPosition;
    QPoint m_cursorPosition;
    int m_generation = 0;
    bool m_positionPending = false;
    bool m_openRequested = false;
    bool m_layerShell = false;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    RadialMenuConfig m_config;
    QRect m_screenGeometry;
    bool m_active = false;
    bool m_visible = false;
    int m_selected = -1;
    double m_x = 0, m_y = 0;
};
}
