#include "RadialMenuController.h"
#include <QGuiApplication>
#include <QCursor>
#include <QScreen>
#include <algorithm>
#ifdef LOGITUNE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace logitune {
RadialMenuController::RadialMenuController(QObject *parent) : QObject(parent)
{
    connect(&m_cursorProvider, &CursorPositionProvider::positionReady, this,
            [this](int generation, const QPoint &position) {
        if (!m_active || generation != m_generation) return;
        m_positionPending = false;
        positionAt(position);
        if (m_openRequested) open();
    });
    m_timer.setSingleShot(true);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &RadialMenuController::open);
}
void RadialMenuController::configureWindow(QWindow *window)
{
    m_window = window;
#ifdef LOGITUNE_LAYER_SHELL
    // GNOME does not implement the layer-shell protocol. Keep the Qt fallback there.
    const bool gnome = qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME", Qt::CaseInsensitive);
    if (window && !gnome && QGuiApplication::platformName().startsWith("wayland")) {
        auto *layer = LayerShellQt::Window::get(window);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        m_layerShell = true;
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                          | LayerShellQt::Window::AnchorLeft);
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setScope("logitune-radial-menu");
        if (layer->metaObject()->indexOfProperty("wantsToBeOnActiveScreen") >= 0)
            layer->setProperty("wantsToBeOnActiveScreen", false);
        else
            layer->setProperty("screenConfiguration", 0); // use the output set on QWindow
    }
#else
    Q_UNUSED(window)
#endif
}

QPoint RadialMenuController::placement(const QPoint &cursor, const QRect &available,
                                       const QSize &size, const QPoint &center)
{
    const QPoint desired = cursor - center;
    return QPoint(std::clamp(desired.x(), available.left(),
                             std::max(available.left(), available.x() + available.width() - size.width())),
                  std::clamp(desired.y(), available.top(),
                             std::max(available.top(), available.y() + available.height() - size.height())));
}

void RadialMenuController::positionAt(const QPoint &cursor)
{
    auto *screen = QGuiApplication::screenAt(cursor);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    m_cursorPosition = cursor;
    m_screenGeometry = screen->availableGeometry();
    const QSize size = m_window ? m_window->size() : QSize(380, 380);
    // The ring center is 25 logical pixels above the window center (caption below).
    m_overlayPosition = placement(cursor, m_screenGeometry, size,
                                  QPoint(size.width() / 2, size.height() / 2 - 25));
    if (m_window) {
        m_window->setScreen(screen);
#ifdef LOGITUNE_LAYER_SHELL
        if (m_layerShell) {
            auto *layer = LayerShellQt::Window::get(m_window);
            if (layer->metaObject()->indexOfProperty("screen") >= 0)
                layer->setProperty("screen", QVariant::fromValue(screen));
            const QPoint local = m_overlayPosition - screen->geometry().topLeft();
            layer->setMargins(QMargins(local.x(), local.y(), 0, 0));
        }
#endif
        m_window->setPosition(m_overlayPosition);
    }
    emit changed();
}

void RadialMenuController::begin(const RadialMenuConfig &config)
{
    cancel();
    if (!config.isValid()) return;
    m_config = config;
    m_active = true;
    m_elapsed.start();
    m_positionPending = true;
    m_cursorProvider.request(++m_generation);
    m_timer.start();
}
void RadialMenuController::open()
{
    if (!m_active) return;
    m_openRequested = true;
    if (m_positionPending) return;
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
    if (!m_openRequested && !m_visible && m_elapsed.elapsed() < 250)
        action = m_config.click.value("action").toString();
    else if (m_visible && m_selected >= 0)
        action = m_config.items[m_selected].toMap().value("action").toString();
    cancel(); // Hide before injecting into the original focused application.
    if (!action.isEmpty()) emit triggered(ButtonAction::parse(action));
}
void RadialMenuController::cancel()
{
    m_timer.stop();
    m_cursorProvider.cancel();
    m_positionPending = false;
    m_openRequested = false;
    m_active = false;
    m_visible = false;
    m_selected = -1;
    m_x = m_y = 0;
    emit changed();
}
}
