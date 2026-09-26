#pragma once
#include "RadialMenuConfig.h"
#include "CursorPositionProvider.h"
#include <QPointer>
#include <QObject>
#include <QRect>
#include <QWindow>

namespace logitune {
class RadialMenuController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool inFolder READ inFolder NOTIFY itemsChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY itemsChanged)
    Q_PROPERTY(int sizeIndex READ sizeIndex WRITE setSizeIndex NOTIFY sizeChanged)
    Q_PROPERTY(int overlaySize READ overlaySize NOTIFY sizeChanged)
    Q_PROPERTY(int adjustmentSteps READ adjustmentSteps NOTIFY changed)
    Q_PROPERTY(int adjustmentIndex READ adjustmentIndex NOTIFY changed)
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(int selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY changed)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition NOTIFY changed)
    Q_PROPERTY(QPoint cursorPosition READ cursorPosition NOTIFY changed)
public:
    explicit RadialMenuController(QObject *parent = nullptr);
    bool visible() const { return m_visible; }
    int selected() const { return m_selected; }
    QVariantList items() const;
    bool inFolder() const { return m_folder >= 0; }
    QString folderName() const;
    int sizeIndex() const { return m_sizeIndex; }
    int overlaySize() const { return m_sizeIndex == 0 ? 320 : m_sizeIndex == 2 ? 460 : 380; }
    void setSizeIndex(int index);
    int adjustmentSteps() const { return m_adjustmentSteps; }
    int adjustmentIndex() const { return m_adjustmentIndex; }
    Q_INVOKABLE void back();
    Q_INVOKABLE bool adjust(int index, int steps);
    QRect screenGeometry() const { return m_screenGeometry; }
    QPoint overlayPosition() const { return m_overlayPosition; }
    QPoint cursorPosition() const { return m_cursorPosition; }
    static QPoint placement(const QPoint &cursor, const QRect &available,
                            const QSize &size, const QPoint &center);
    void begin(const RadialMenuConfig &config);
    bool isActive() const { return m_active; }
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void configureWindow(QWindow *window);
signals:
    void changed();
    void sizeChanged();
    void itemsChanged();
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
    bool m_layerShell = false;
    RadialMenuConfig m_config;
    QRect m_screenGeometry;
    bool m_active = false;
    bool m_visible = false;
    int m_selected = -1;
    int m_folder = -1;
    int m_sizeIndex = 1;
    int m_adjustmentSteps = 0;
    int m_adjustmentIndex = -1;
};
}
