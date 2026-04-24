#include "AppRoot.h"
#include "devices/JsonDevice.h"
#include "models/EditorModel.h"
#include "desktop/KDeDesktop.h"
#include "desktop/GnomeDesktop.h"
#include "desktop/GenericDesktop.h"
#include "input/UinputInjector.h"
#include "logging/LogManager.h"
#include <QProcessEnvironment>

namespace logitune {

// Construction / init --------------------------------------------------------

std::unique_ptr<IDesktopIntegration> AppRoot::makeOwnedDesktop(IDesktopIntegration *provided)
{
    if (provided) return {};
    const QString xdgDesktop = QProcessEnvironment::systemEnvironment()
                                   .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (xdgDesktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive))
        return std::make_unique<KDeDesktop>();
    if (xdgDesktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive))
        return std::make_unique<GnomeDesktop>();
    return std::make_unique<GenericDesktop>();
}

std::unique_ptr<IInputInjector> AppRoot::makeOwnedInjector(IInputInjector *provided)
{
    if (provided) return {};
    return std::make_unique<UinputInjector>();
}

AppRoot::AppRoot(QObject *parent)
    : AppRoot(nullptr, nullptr, parent)
{
}

AppRoot::AppRoot(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent)
    : QObject(parent)
    , m_ownedDesktop(makeOwnedDesktop(desktop))
    , m_ownedInjector(makeOwnedInjector(injector))
    , m_desktop(desktop ? desktop : m_ownedDesktop.get())
    , m_injector(injector ? injector : m_ownedInjector.get())
    , m_deviceManager(&m_registry)
    , m_deviceResolver(&m_deviceModel, this)
    , m_deviceCommandHandler(&m_deviceResolver, this)
    , m_actionExecutor(nullptr)
    , m_buttonDispatcher(&m_profileEngine, &m_actionExecutor, &m_deviceResolver,
                         m_desktop, this)
    , m_profileOrchestrator(&m_profileEngine, &m_actionExecutor, &m_deviceResolver,
                            &m_deviceModel, &m_buttonModel, &m_actionModel,
                            &m_profileModel, m_desktop, this)
{
    m_actionExecutor.setInjector(m_injector);

    m_actionFilterModel = std::make_unique<ActionFilterModel>(&m_deviceModel, this);
    m_actionFilterModel->setSourceModel(&m_actionModel);
}

AppRoot::~AppRoot() = default;

void AppRoot::init()
{
    m_deviceModel.setDesktopIntegration(m_desktop);
    qCInfo(lcApp) << "creating UinputInjector...";
    qCInfo(lcApp) << "init uinput...";
    if (!m_injector->init()) {
        qCWarning(lcApp) << "UinputInjector: uinput init failed (no /dev/uinput access?). Keystrokes will not be injected.";
    }

    QMap<QString, QPair<QString, QString>> defaultGestures;
    defaultGestures["down"]  = qMakePair(QStringLiteral("Show desktop"),          QStringLiteral("Super+D"));
    defaultGestures["left"]  = qMakePair(QStringLiteral("Switch desktop left"),   QStringLiteral("Ctrl+Super+Left"));
    defaultGestures["right"] = qMakePair(QStringLiteral("Switch desktop right"),  QStringLiteral("Ctrl+Super+Right"));
    defaultGestures["click"] = qMakePair(QStringLiteral("Task switcher"),         QStringLiteral("Super+W"));
    m_deviceModel.loadGesturesFromProfile(defaultGestures);

    wireSignals();
}

void AppRoot::startMonitoring(bool simulateAll, bool editMode)
{
    if (editMode) {
        m_editorModel = std::make_unique<EditorModel>(&m_registry, &m_deviceModel, true, this);
        qCInfo(lcApp) << "--edit: editor mode active";

        auto syncActive = [this]() {
            if (!m_editorModel)
                return;
            const IDevice *dev = m_deviceModel.activeDevice();
            if (auto *jd = dynamic_cast<const JsonDevice *>(dev))
                m_editorModel->setActiveDevicePath(jd->sourcePath());
            else
                m_editorModel->setActiveDevicePath(QString());
        };
        connect(&m_deviceModel, &DeviceModel::selectedChanged, this, syncActive);
        syncActive();
    }
    if (simulateAll) {
        // --simulate-all: populate the carousel with one fake session per
        // descriptor currently loaded in DeviceRegistry instead of scanning
        // udev for real hardware. Useful for visually walking through every
        // community descriptor at once without owning the physical mice.
        qCInfo(lcApp) << "--simulate-all: seeding carousel from registry";
        m_deviceManager.simulateAllFromRegistry();
        m_desktop->start();
        return;
    }

    m_deviceManager.start();
    m_desktop->start();
    m_deviceFetcher.fetchManifest();
}

// Signal wiring --------------------------------------------------------------

void AppRoot::wireSignals()
{
    // User edits in the UI (button reassignments, focus-follow from desktop,
    // tab selection, engine-driven display refresh) route to the orchestrator.
    connect(&m_buttonModel, &ButtonModel::userActionChanged,
            &m_profileOrchestrator, &ProfileOrchestrator::onUserButtonChanged);

    connect(m_desktop, &IDesktopIntegration::activeWindowChanged,
            &m_profileOrchestrator, &ProfileOrchestrator::onWindowFocusChanged);

    connect(&m_profileModel, &ProfileModel::profileSwitched,
            &m_profileOrchestrator, &ProfileOrchestrator::onTabSwitched);

    connect(&m_profileEngine, &ProfileEngine::deviceDisplayProfileChanged,
            &m_profileOrchestrator, &ProfileOrchestrator::onDisplayProfileChanged);

    connect(&m_deviceModel, &DeviceModel::selectedChanged,
            &m_deviceResolver, &ActiveDeviceResolver::onSelectionIndexChanged);
    connect(&m_deviceResolver, &ActiveDeviceResolver::selectionChanged,
            this, &AppRoot::onSelectionChanged);

    // Physical device lifecycle — one per unique serial, survives transport
    // switches. Per-transport events are routed through PhysicalDevice
    // (which fans in signals from all its transports).
    connect(&m_deviceManager, &DeviceManager::physicalDeviceAdded,
            this, &AppRoot::onPhysicalDeviceAdded);
    connect(&m_deviceManager, &DeviceManager::physicalDeviceRemoved,
            this, &AppRoot::onPhysicalDeviceRemoved);

    // Gesture keystroke edits in the UI. saveCurrentProfile re-serializes
    // the displayed profile; the orchestrator's own userChangedSomething
    // subscription covers point/scroll tweaks from DeviceCommandHandler.
    connect(&m_deviceModel, &DeviceModel::userGestureChanged,
            &m_profileOrchestrator,
            [this](const QString &, const QString &, const QString &) {
                m_profileOrchestrator.saveCurrentProfile();
            });

    connect(&m_profileModel, &ProfileModel::profileAdded, this,
            [this](const QString &wmClass, const QString &profileName) {
        const QString serial = m_deviceResolver.activeSerial();
        if (!serial.isEmpty())
            m_profileEngine.createProfileForApp(serial, wmClass, profileName);
    });
    connect(&m_profileModel, &ProfileModel::profileRemoved, this,
            [this](const QString &wmClass) {
        const QString serial = m_deviceResolver.activeSerial();
        if (!serial.isEmpty())
            m_profileEngine.removeAppProfile(serial, wmClass);
    });

    connect(&m_deviceFetcher, &DeviceFetcher::descriptorsUpdated,
            this, [this]() {
        m_registry.reloadAll();
    });

    connect(&m_deviceManager, &DeviceManager::unknownDeviceDetected,
            &m_deviceFetcher, &DeviceFetcher::fetchForPid);

    // DeviceCommandHandler emits userChangedSomething after any HID++ mutation
    // from point/scroll controls. The orchestrator persists the displayed
    // profile in response (it's already pushed into the cache by the
    // applyDisplayedChange bridge below).
    connect(&m_deviceCommandHandler, &DeviceCommandHandler::userChangedSomething,
            &m_profileOrchestrator, &ProfileOrchestrator::saveCurrentProfile);

    // Cross-service bridge: after any hardware apply, reset the dispatcher's
    // thumb accumulator for that serial; when the active IDevice changes,
    // the dispatcher must refresh its own pointer.
    connect(&m_profileOrchestrator, &ProfileOrchestrator::profileApplied,
            &m_buttonDispatcher, &ButtonActionDispatcher::onProfileApplied);
    connect(&m_profileOrchestrator, &ProfileOrchestrator::currentDeviceChanged,
            &m_buttonDispatcher, &ButtonActionDispatcher::onCurrentDeviceChanged);

    // DeviceModel *ChangeRequested -> cache + disk + UI + (if active)
    // hardware. The orchestrator owns the guard logic; AppRoot
    // only supplies the mutator / forwarder pair for each control.
    connect(&m_deviceModel, &DeviceModel::dpiChangeRequested, this,
        [this](int value) {
            m_profileOrchestrator.applyDisplayedChange(
                [&](Profile &p) { p.dpi = value; },
                [&]{ m_deviceCommandHandler.requestDpi(value); });
        });
    connect(&m_deviceModel, &DeviceModel::smartShiftChangeRequested, this,
        [this](bool enabled, int threshold) {
            m_profileOrchestrator.applyDisplayedChange(
                [&](Profile &p) {
                    p.smartShiftEnabled = enabled;
                    p.smartShiftThreshold = threshold;
                },
                [&]{ m_deviceCommandHandler.requestSmartShift(enabled, threshold); });
        });
    connect(&m_deviceModel, &DeviceModel::scrollConfigChangeRequested, this,
        [this](bool hiRes, bool invert) {
            m_profileOrchestrator.applyDisplayedChange(
                [&](Profile &p) {
                    p.hiResScroll = hiRes;
                    p.scrollDirection = invert ? QStringLiteral("natural")
                                                : QStringLiteral("standard");
                },
                [&]{ m_deviceCommandHandler.requestScrollConfig(hiRes, invert); });
        });
    connect(&m_deviceModel, &DeviceModel::thumbWheelModeChangeRequested, this,
        [this](const QString &mode) {
            m_profileOrchestrator.applyDisplayedChange(
                [&](Profile &p) { p.thumbWheelMode = mode; },
                [&]{ m_deviceCommandHandler.requestThumbWheelMode(mode); });
        });
    connect(&m_deviceModel, &DeviceModel::thumbWheelInvertChangeRequested, this,
        [this](bool invert) {
            m_profileOrchestrator.applyDisplayedChange(
                [&](Profile &p) { p.thumbWheelInvert = invert; },
                [&]{ m_deviceCommandHandler.requestThumbWheelInvert(invert); });
        });
}

// Physical device lifecycle -------------------------------------------------

void AppRoot::onPhysicalDeviceAdded(PhysicalDevice *device)
{
    if (!device) return;

    const bool wasEmpty = m_deviceModel.count() == 0;
    m_deviceModel.addPhysicalDevice(device);

    // Route per-device input events. PhysicalDevice fans in signals from
    // all its underlying transports, so we connect once here regardless of
    // how many hidraws back it.
    connect(device, &PhysicalDevice::gestureRawXY,
            &m_buttonDispatcher, &ButtonActionDispatcher::onGestureRaw);
    connect(device, &PhysicalDevice::divertedButtonPressed,
            &m_buttonDispatcher, &ButtonActionDispatcher::onDivertedButtonPressed);
    connect(device, &PhysicalDevice::thumbWheelRotation,
            &m_buttonDispatcher, &ButtonActionDispatcher::onThumbWheelRotation);

    // Re-apply profile whenever any transport finishes (re-)enumeration.
    // PhysicalDevice emits this from its setupComplete fan-in, which
    // covers both first-time attach and post-reconnect re-enumerate.
    connect(device, &PhysicalDevice::transportSetupComplete,
            &m_profileOrchestrator,
            [this, device]() { m_profileOrchestrator.onTransportSetupComplete(device); });

    if (wasEmpty && m_deviceModel.count() == 1)
        m_deviceModel.setSelectedIndex(0);

    m_profileOrchestrator.setupProfileForDevice(device);
}

void AppRoot::onPhysicalDeviceRemoved(PhysicalDevice *device)
{
    const QString deviceId = device ? device->deviceSerial() : QString();
    m_deviceModel.removePhysicalDevice(device);
    if (!deviceId.isEmpty())
        m_buttonDispatcher.onDeviceRemoved(deviceId);

    if (m_deviceModel.count() > 0 && m_deviceModel.selectedIndex() < 0)
        m_deviceModel.setSelectedIndex(0);
}

// Carousel selection changed. Refresh the UI from the newly-selected
// device's cached profile. No file I/O, no seeding, no hardware apply —
// one-time device provisioning happens in onPhysicalDeviceAdded.
void AppRoot::onSelectionChanged()
{
    auto *device = m_deviceResolver.activeDevice();
    if (!device) return;

    // Tell the orchestrator about the new IDevice; it in turn emits
    // currentDeviceChanged which is wired to the dispatcher in wireSignals().
    m_profileOrchestrator.onCurrentDeviceChanged(device->descriptor());

    const QString serial = device->deviceSerial();
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;  // device not yet fully set up

    // Replay the display-refresh path through the orchestrator's slot.
    // This mirrors what deviceDisplayProfileChanged would do if emitted,
    // but selection change alone doesn't produce that engine-level signal.
    const Profile &p = m_profileEngine.cachedProfile(serial, name);
    m_profileOrchestrator.onDisplayProfileChanged(serial, p);
}

} // namespace logitune
