#pragma once
#include "DeviceFetcher.h"
#include "DeviceManager.h"
#include "PhysicalDevice.h"
#include "DeviceRegistry.h"
#include "interfaces/IDesktopIntegration.h"
#include "interfaces/IInputInjector.h"
#include "ProfileEngine.h"
#include "ActionExecutor.h"
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
#include "models/ActionFilterModel.h"
#include "models/ActionModel.h"
#include "models/ProfileModel.h"
#include "models/SettingsModel.h"
#include "services/DeviceSelection.h"
#include "services/DeviceCommands.h"
#include "services/ButtonActionDispatcher.h"
#include "services/ProfileOrchestrator.h"
#include <QObject>
#include <memory>

namespace logitune::test { class AppRootFixture; }

namespace logitune {

class EditorModel;

/// Composition root for the Logitune application.
///
/// Owns long-lived singletons (ViewModels, services, engines), wires the
/// signal graph between them at startup, and attaches PhysicalDevice
/// instances into the graph at runtime. Exposes ViewModels via accessors
/// for QML registration in main.cpp.
///
/// This class does not implement user-facing behavior. Profile flow lives
/// in ProfileOrchestrator, input interpretation in ButtonActionDispatcher,
/// hardware command relays in DeviceCommands, and active-device resolution
/// in DeviceSelection. If you find yourself adding a method here that
/// responds to a user event or mutates application state, it belongs in
/// a service instead.
class AppRoot : public QObject {
    Q_OBJECT
public:
    explicit AppRoot(QObject *parent = nullptr);
    AppRoot(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent = nullptr);
    ~AppRoot() override;

    void init();

    /// Start the device monitor.
    ///
    /// When @p simulateAll is true, the app skips udev + HID++ entirely
    /// and instead seeds the carousel with one fake session per descriptor
    /// currently loaded in DeviceRegistry. Used by the `--simulate-all`
    /// CLI flag for visually inspecting every community descriptor
    /// without needing physical hardware.
    void startMonitoring(bool simulateAll = false, bool editMode = false);

    friend class test::AppRootFixture;

    DeviceModel    *deviceModel()    { return &m_deviceModel; }
    ButtonModel    *buttonModel()    { return &m_buttonModel; }
    ActionModel    *actionModel()    { return &m_actionModel; }
    ActionFilterModel *actionFilterModel() { return m_actionFilterModel.get(); }
    ProfileModel   *profileModel()   { return &m_profileModel; }
    SettingsModel  *settingsModel()  { return &m_settingsModel; }
    EditorModel    *editorModel() const { return m_editorModel.get(); }

private slots:
    void onPhysicalDeviceAdded(PhysicalDevice *device);
    void onPhysicalDeviceRemoved(PhysicalDevice *device);

private:
    void wireSignals();
    void onSelectionChanged();

    static std::unique_ptr<IDesktopIntegration> makeOwnedDesktop(IDesktopIntegration *provided);
    static std::unique_ptr<IInputInjector>      makeOwnedInjector(IInputInjector *provided);

    // Subsystems. Declaration order is the construction order for
    // subobjects, so things that are handed as pointers into other
    // subobjects' ctors must be declared first. m_ownedDesktop /
    // m_desktop / m_injector precede the services that reference them
    // (m_profileOrchestrator needs the desktop pointer).
    std::unique_ptr<IDesktopIntegration> m_ownedDesktop;
    std::unique_ptr<IInputInjector>      m_ownedInjector;
    IDesktopIntegration *m_desktop  = nullptr;
    IInputInjector      *m_injector = nullptr;

    DeviceRegistry m_registry;
    DeviceManager  m_deviceManager;
    DeviceFetcher  m_deviceFetcher;
    DeviceModel    m_deviceModel;
    SettingsModel  m_settingsModel;
    ButtonModel    m_buttonModel;
    ActionModel    m_actionModel;
    std::unique_ptr<ActionFilterModel> m_actionFilterModel;
    ProfileModel   m_profileModel;
    DeviceSelection m_deviceSelection;
    DeviceCommands  m_deviceCommands;
    ProfileEngine  m_profileEngine;
    ActionExecutor m_actionExecutor;
    ButtonActionDispatcher m_buttonDispatcher;
    ProfileOrchestrator    m_profileOrchestrator;

    std::unique_ptr<EditorModel>         m_editorModel;
};

} // namespace logitune
