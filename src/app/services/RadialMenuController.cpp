#include "RadialMenuController.h"
#include <QGuiApplication>
#include <QCursor>
#include <QScreen>
#ifdef LOGITUNE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace logitune {
RadialMenuController::RadialMenuController(QObject *parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &RadialMenuController::open);
}
void RadialMenuController::configureWindow(QWindow *window)
{
#ifdef LOGITUNE_LAYER_SHELL
    // GNOME does not implement the layer-shell protocol. Keep the Qt fallback there.
    const bool gnome = qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME", Qt::CaseInsensitive);
    if (window && !gnome && QGuiApplication::platformName().startsWith("wayland")) {
        auto *layer = LayerShellQt::Window::get(window);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::AnchorNone);
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setScope("logitune-radial-menu");
        if (layer->metaObject()->indexOfProperty("wantsToBeOnActiveScreen") >= 0)
            layer->setProperty("wantsToBeOnActiveScreen", true);
        else
            layer->setProperty("screenConfiguration", 1); // compositor chooses active output
    }
#else
    Q_UNUSED(window)
#endif
}

void RadialMenuController::begin(const RadialMenuConfig &config)
{
    cancel();
    if (!config.isValid()) return;
    m_config = config;
    m_active = true;
    m_elapsed.start();
    auto *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) m_screenGeometry = screen->availableGeometry();
    m_timer.start();
}
void RadialMenuController::open()
{
    if (!m_active) return;
    m_visible = true;
    emit changed();
}
void RadialMenuController::move(int dx, int dy)
{
    if (!m_active) return;
    m_x += dx;
    m_y += dy;
    m_selected = RadialMenuConfig::selection(m_x, m_y, m_config.items.size());
    // A deliberate movement opens immediately; a stationary hold opens after 250 ms.
    if (m_selected >= 0 && !m_visible) { m_timer.stop(); open(); }
    else emit changed();
}
void RadialMenuController::release()
{
    if (!m_active) return;
    QString action;
    if (!m_visible && m_elapsed.elapsed() < 250)
        action = m_config.click.value("action").toString();
    else if (m_selected >= 0)
        action = m_config.items[m_selected].toMap().value("action").toString();
    cancel(); // Hide before injecting into the original focused application.
    if (!action.isEmpty()) emit triggered(ButtonAction::parse(action));
}
void RadialMenuController::cancel()
{
    m_timer.stop();
    m_active = false;
    m_visible = false;
    m_selected = -1;
    m_x = m_y = 0;
    emit changed();
}
}
