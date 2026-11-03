#include "ActionsRingService.h"
#include "ActionExecutor.h"
#include "ActiveDeviceResolver.h"
#include "DeviceManager.h"
#include "ProfileEngine.h"
#include "ProfileOrchestrator.h"
#include "desktop/HyprlandCursor.h"
#include "interfaces/IDesktopIntegration.h"
#include "logging/LogManager.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QTimer>
#include <algorithm>
#include <memory>
#ifdef LOGITUNE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace logitune {

ActionsRingService::ActionsRingService(ProfileEngine *profiles, DeviceManager *devices,
    ActionExecutor *executor, ActiveDeviceResolver *selection, IDesktopIntegration *desktop,
    ProfileOrchestrator *orchestrator, QObject *parent)
    : QObject(parent), m_profiles(profiles), m_devices(devices), m_executor(executor),
      m_selection(selection), m_desktop(desktop), m_orchestrator(orchestrator)
{
    m_clock.start();
}

ActionsRingService::~ActionsRingService() { destroyOverlays(); }

void ActionsRingService::shutdown() { m_pendingAction = {}; destroyOverlays(); }

QVariantList ActionsRingService::entries() const
{
    const auto serial = m_selection->activeSerial();
    return ringSlotsToJson(m_profiles->cachedProfile(serial, m_profiles->displayProfile(serial)).ringSlots).toVariantList();
}

int ActionsRingService::size() const
{
    const auto serial = m_selection->activeSerial();
    return m_profiles->cachedProfile(serial, m_profiles->displayProfile(serial)).ringSize;
}

void ActionsRingService::setSize(int size)
{
    if (size != 80 && size != 100 && size != 150) return;
    m_orchestrator->applyDisplayedChange([size](Profile &p) { p.ringSize = size; }, [] {});
    emit editorChanged();
}

void ActionsRingService::setEntries(const QVariantList &entries)
{
    const auto parsed = ringSlotsFromJson(QJsonArray::fromVariantList(entries));
    m_orchestrator->applyDisplayedChange([&](Profile &p) { p.ringSlots = parsed; }, [] {});
    emit editorChanged();
}

void ActionsRingService::trigger(const QString &serial, bool pressed)
{
    if (!pressed && serial != m_serial) return;
    const auto result = pressed ? m_trigger.press(m_clock.elapsed()) : m_trigger.release(m_clock.elapsed());
    if (result == ActionsRingTrigger::Open) open(serial);
    else if (result == ActionsRingTrigger::Close) dismiss();
    else if (result == ActionsRingTrigger::Select) {
        if (m_activeRing) QMetaObject::invokeMethod(m_activeRing, "selectHovered");
        else finished();
    }
}

void ActionsRingService::toggle() { trigger(m_selection->activeSerial(), true); }

static QVariantMap themeOverrides()
{
    QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (config.isEmpty()) config = QDir::homePath() + "/.config";
    QFile file(config + "/Logitune/actions-ring-theme.json");
    if (!file.open(QIODevice::ReadOnly)) return {};
    auto colors = QJsonDocument::fromJson(file.readAll()).object();
    if (colors.contains("colors")) colors = colors.value("colors").toObject();
    QVariantMap result;
    for (const auto &key : {"surface", "surfaceRaised", "text", "accent", "onAccent", "border", "error"}) {
        const auto color = colors.value(QLatin1String(key)).toString();
        if (QColor::isValidColorName(color)) result.insert(QLatin1String(key), color);
    }
    return result;
}

void ActionsRingService::open(const QString &serial)
{
    if (!m_engine) { m_trigger.closed(); return; }
    m_serial = serial;
    m_pendingAction = {};
    const Profile profile = serial.isEmpty() ? Profile{} : m_profiles->cachedProfile(serial, m_profiles->hardwareProfile(serial));
    auto cursor = hyprlandCursor();
    if (cursor) {
        const auto screens = QGuiApplication::screens();
        const bool matched = std::any_of(screens.begin(), screens.end(),
            [&](QScreen *screen) { return screen->name() == cursor->screenName; });
        if (!matched) cursor.reset();
    }
    for (auto *screen : QGuiApplication::screens()) {
        if (cursor && cursor->screenName != screen->name()) continue;
        auto *view = new QQuickView(m_engine, nullptr);
        view->setScreen(screen);
        view->setColor(Qt::transparent);
        view->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setInitialProperties({{"slotList", ringSlotsToJson(profile.ringSlots).toVariantList()},
            {"launcher", QVariant::fromValue(this)}, {"themeOverrides", themeOverrides()},
            {"sizeScale", profile.ringSize / 100.0}, {"screenName", screen->name()}});
        view->setSource(QUrl("qrc:/Logitune/qml/components/actionsring/Ring.qml"));
        if (view->status() != QQuickView::Ready) { delete view; continue; }
        bool layerShell = false;
#ifdef LOGITUNE_LAYER_SHELL
        if (QGuiApplication::platformName().startsWith("wayland")) {
            auto *layer = LayerShellQt::Window::get(view);
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorBottom,
                               LayerShellQt::Window::AnchorLeft, LayerShellQt::Window::AnchorRight});
            layer->setExclusiveZone(-1);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
            layer->setScope(QStringLiteral("logitune-actions-ring"));
            layer->setWantsToBeOnActiveScreen(false);
            layer->setScreen(screen);
            layerShell = true;
        }
#endif
        m_overlays.append(view);
        if (cursor) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = connect(view, &QQuickWindow::frameSwapped, this, [this, view, cursor, connection] {
                disconnect(*connection);
                if (!m_overlays.contains(view)) return;
                QMetaObject::invokeMethod(view->rootObject(), "place",
                    Q_ARG(QVariant, cursor->positionOnScreen.x()), Q_ARG(QVariant, cursor->positionOnScreen.y()));
            }, Qt::QueuedConnection);
        }
        if (layerShell) view->show();
        else { view->setGeometry(screen->geometry()); view->showFullScreen(); }
        view->requestActivate();
    }
    if (m_overlays.isEmpty()) m_trigger.closed();
    else qCInfo(lcApp) << "Actions Ring opened" << serial << "overlays" << m_overlays.size();
}

void ActionsRingService::placed(QObject *ring)
{
    m_activeRing = ring;
    for (auto *view : m_overlays) if (view->rootObject() != ring) view->hide();
}

void ActionsRingService::haptic(const QString &kind)
{
    if (kind == "hover") m_devices->playHaptic(m_serial, 0x04);
}

void ActionsRingService::run(const QString &action)
{
    auto parsed = ButtonAction::parse(action);
    if (parsed.type == ButtonAction::PresetRef) {
        const auto resolved = m_desktop ? m_desktop->resolveNamedAction(parsed.payload) : std::nullopt;
        if (!resolved) return;
        parsed = *resolved;
    }
    if (parsed.type != ButtonAction::Keystroke && parsed.type != ButtonAction::Media
        && parsed.type != ButtonAction::AppLaunch && parsed.type != ButtonAction::DBus) return;
    m_pendingAction = parsed;
    m_devices->playHaptic(m_serial, 0x01);
}

void ActionsRingService::dismiss()
{
    if (m_activeRing) QMetaObject::invokeMethod(m_activeRing, "dismiss");
    else finished();
}

void ActionsRingService::destroyOverlays()
{
    qDeleteAll(m_overlays);
    m_overlays.clear();
    m_activeRing = nullptr;
    m_trigger.closed();
    if (m_engine) m_engine->collectGarbage();
}

void ActionsRingService::finished()
{
    // A QML animation calls this; destroy its window after that callback returns.
    QTimer::singleShot(0, this, [this] {
        const auto action = m_pendingAction;
        m_pendingAction = {};
        destroyOverlays();
        qCInfo(lcApp) << "Actions Ring closed";
        if (action.type != ButtonAction::Default)
            QTimer::singleShot(30, m_executor, [executor = m_executor, action] { executor->executeAction(action); });
    });
}
}
