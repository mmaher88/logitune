#include "RadialMenuController.h"
#include <QGuiApplication>
#include <QCursor>
#include <QScreen>
#include <QSettings>
#include <algorithm>
#ifdef LOGITUNE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace logitune {
RadialMenuController::RadialMenuController(QObject *parent) : QObject(parent)
{
    m_sizeIndex = std::clamp(QSettings().value("ActionRing/size", 1).toInt(), 0, 2);
    connect(&m_cursorProvider, &CursorPositionProvider::positionReady, this,
            [this](int generation, const QPoint &position) {
        if (!m_active || generation != m_generation) return;
        m_positionPending = false;
        positionAt(position);
        open();
    });
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
    const QSize size(overlaySize(), overlaySize());
    // The ring center is 25 logical pixels above the window center (caption below).
    m_overlayPosition = placement(cursor, m_screenGeometry, size,
                                  QPoint(size.width() / 2, qRound(size.height() * 165.0 / 380.0)));
    if (m_window) {
        m_window->resize(size);
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
    emit itemsChanged();
    m_active = true;
    m_positionPending = true;
    m_cursorProvider.request(++m_generation);
}
void RadialMenuController::open()
{
    if (!m_active) return;
    if (m_positionPending) return;
    m_visible = true;
    emit changed();
}
void RadialMenuController::select(int index)
{
    if (!m_visible) return;
    const int selected = index >= 0 && index < items().size() ? index : -1;
    if (m_selected == selected) return;
    m_selected = selected;
    emit changed();
}
QVariantList RadialMenuController::items() const
{
    if (m_folder >= 0)
        return RadialMenuConfig::padded(m_config.items[m_folder].toMap().value("children").toList(), 9);
    return RadialMenuConfig::padded(m_config.items);
}
QString RadialMenuController::folderName() const
{
    return m_folder >= 0 ? m_config.items[m_folder].toMap().value("label").toString() : QString();
}
void RadialMenuController::setSizeIndex(int index)
{
    if (index < 0 || index > 2 || m_sizeIndex == index) return;
    m_sizeIndex = index;
    QSettings().setValue("ActionRing/size", index);
    emit sizeChanged();
    if (m_visible) positionAt(m_cursorPosition);
}
void RadialMenuController::back()
{
    if (!m_visible) return;
    if (m_folder < 0) { cancel(); return; }
    m_folder = -1;
    m_selected = -1;
    m_adjustmentIndex = -1;
    m_adjustmentSteps = 0;
    emit itemsChanged();
    emit changed();
}
bool RadialMenuController::adjust(int index, int steps)
{
    const auto entries = items();
    if (!m_visible || index < 0 || index >= entries.size()) return false;
    const auto entry = entries[index].toMap();
    if (entry.value("kind") != "adjustment") return false;
    if (!steps) return true;
    steps = std::clamp(steps, -10, 10);
    if (m_adjustmentIndex != index) m_adjustmentSteps = 0;
    m_adjustmentIndex = index;
    m_adjustmentSteps = std::clamp(m_adjustmentSteps + steps, -1000, 1000);
    const QString key = entry.value("parameter") == "brightness"
        ? (steps > 0 ? "BrightnessUp" : "BrightnessDown")
        : (steps > 0 ? "VolumeUp" : "VolumeDown");
    emit changed();
    for (int i = 0; i < std::abs(steps) && m_visible; ++i)
        emit triggered({ButtonAction::Keystroke, key});
    return true;
}
void RadialMenuController::activate(int index)
{
    const auto entries = items();
    if (!m_visible || index < 0 || index >= entries.size()) return;
    const auto entry = entries[index].toMap();
    const auto kind = entry.value("kind").toString();
    if (kind == "empty") return;
    if (kind == "folder") {
        m_folder = index;
        m_selected = -1;
        m_adjustmentIndex = -1;
        m_adjustmentSteps = 0;
        emit itemsChanged();
        emit changed();
        return;
    }
    if (kind == "adjustment") { select(index); return; }
    const auto action = ButtonAction::parse(entry.value("action").toString());
    cancel(); // Close the pointer surface before injecting into the focused application.
    emit triggered(action);
}
void RadialMenuController::cancel()
{
    m_cursorProvider.cancel();
    m_positionPending = false;
    m_active = false;
    m_visible = false;
    m_selected = -1;
    m_folder = -1;
    m_adjustmentIndex = -1;
    m_adjustmentSteps = 0;
    emit itemsChanged();
    emit changed();
}
}
