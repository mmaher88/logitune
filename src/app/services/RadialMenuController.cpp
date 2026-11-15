#include "RadialMenuController.h"
#include "interfaces/IDesktopIntegration.h"
#include <QGuiApplication>
#include <QMargins>
#include <QScreen>
#include <QWindow>

#ifdef LOGITUNE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace logitune {

RadialMenuController::RadialMenuController(IDesktopIntegration *desktop, QObject *parent)
    : QObject(parent), m_desktop(desktop)
{
    if (!m_desktop) return;
    connect(m_desktop, &IDesktopIntegration::actionRingPlacementReady, this,
            [this](int requestId, const QPoint &position, const QRect &screenGeometry,
                   const QPoint &cursorPosition) {
        if (!m_active || requestId != m_requestId) return;
        m_overlayPosition = position;
        m_screenGeometry = screenGeometry;
        m_cursorPosition = cursorPosition;
        if (m_window) {
            m_window->resize(kOverlaySize, kOverlaySize);
            if (QScreen *screen = QGuiApplication::screenAt(cursorPosition))
                m_window->setScreen(screen);
#ifdef LOGITUNE_LAYER_SHELL
            if (m_layerShell) {
                if (QScreen *screen = m_window->screen()) {
                    const QPoint local = position - screen->geometry().topLeft();
                    LayerShellQt::Window::get(m_window)->setMargins(
                        QMargins(local.x(), local.y(), 0, 0));
                }
            }
#endif
            m_window->setPosition(position);
        }
        m_visible = true;
        emit changed();
    });
    connect(m_desktop, &IDesktopIntegration::actionRingPlacementFailed, this,
            [this](int requestId) {
        if (m_active && requestId == m_requestId) cancel();
    });
}

QVariantList RadialMenuController::items() const
{
    QVariantList result;
    result.reserve(m_config.items.size());
    for (const RadialMenuItem &item : m_config.items) {
        result.push_back(QVariantMap{{QStringLiteral("label"), item.label},
                                     {QStringLiteral("action"), item.action.serialize()}});
    }
    return result;
}

void RadialMenuController::begin(const RadialMenuConfig &config)
{
    close(false);
    if (!m_desktop || !m_desktop->supportsActionRing() || !config.isValid()) return;
    m_config = config;
    m_active = true;
    emit itemsChanged();
    ++m_requestId;
    m_desktop->requestActionRingPlacement(
        m_requestId, QSize(kOverlaySize, kOverlaySize),
        QPoint(kOverlaySize / 2, 165));
}

void RadialMenuController::activate(int index)
{
    if (!m_visible || index < 0 || index >= m_config.items.size()) return;
    const ButtonAction action = m_config.items.at(index).action;
    close(false);
    emit triggered(action);
}

void RadialMenuController::cancel()
{
    close(true);
}

void RadialMenuController::close(bool notifyDismissed)
{
    if (m_desktop && m_active)
        m_desktop->cancelActionRingPlacement(m_requestId);
    const bool changedState = m_active || m_visible;
    m_active = false;
    m_visible = false;
    if (changedState) emit changed();
    if (changedState && notifyDismissed) emit dismissed();
}

void RadialMenuController::configureWindow(QWindow *window)
{
    m_window = window;
#ifdef LOGITUNE_LAYER_SHELL
    if (window && m_desktop && m_desktop->supportsActionRing()
        && QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        auto *layer = LayerShellQt::Window::get(window);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                          | LayerShellQt::Window::AnchorLeft);
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setScope(QStringLiteral("logitune-action-ring"));
        m_layerShell = true;
    }
#else
    Q_UNUSED(window)
#endif
}

} // namespace logitune
