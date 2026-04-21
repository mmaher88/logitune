#pragma once
#include "DeviceFetcher.h"
#include "DeviceManager.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
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
#include <QObject>
#include <QTimer>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace logitune::test { class AppControllerFixture; }

namespace logitune {

class EditorModel;

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    AppController(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent = nullptr);
    ~AppController() override;

    void init();

    /// Start the device monitor.
    ///
    /// When @p simulateAll is true, the app skips udev + HID++ entirely
    /// and instead seeds the carousel with one fake session per descriptor
    /// currently loaded in DeviceRegistry. Used by the `--simulate-all`
    /// CLI flag for visually inspecting every community descriptor
    /// without needing physical hardware.
    void startMonitoring(bool simulateAll = false, bool editMode = false);

    friend class test::AppControllerFixture;

    DeviceModel    *deviceModel()    { return &m_deviceModel; }
    ButtonModel    *buttonModel()    { return &m_buttonModel; }
    ActionModel    *actionModel()    { return &m_actionModel; }
    ActionFilterModel *actionFilterModel() { return m_actionFilterModel.get(); }
    ProfileModel   *profileModel()   { return &m_profileModel; }
    SettingsModel  *settingsModel()  { return &m_settingsModel; }
    EditorModel    *editorModel() const { return m_editorModel.get(); }

private slots:
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const QString &serial, const Profile &profile);
    void onPhysicalDeviceAdded(PhysicalDevice *device);
    void onPhysicalDeviceRemoved(PhysicalDevice *device);
    void onDivertedButtonPressed(uint16_t controlId, bool pressed);
    void onThumbWheelRotation(int delta);
    void onDpiChangeRequested(int value);
    void onSmartShiftChangeRequested(bool enabled, int threshold);
    void onScrollConfigChangeRequested(bool hiRes, bool invert);
    void onThumbWheelModeChangeRequested(const QString &mode);
    void onThumbWheelInvertChangeRequested(bool invert);
    void onSelectedDeviceChanged();

private:
    void wireSignals();
    void saveCurrentProfile();
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);
    void applyProfileToHardware(const Profile &p);
    void setupProfileForDevice(PhysicalDevice *device);
    PhysicalDevice *selectedDevice() const;
    DeviceSession *selectedSession() const;  // convenience — selectedDevice()->primary()
    QString selectedSerial() const;  // PhysicalDevice::deviceSerial() of the selected device, or empty
    QString buttonActionToName(const ButtonAction &ba) const;
    ButtonAction buttonEntryToAction(const QString &actionType, const QString &actionName) const;

    // Subsystems
    DeviceRegistry m_registry;
    DeviceManager  m_deviceManager;
    DeviceFetcher  m_deviceFetcher;
    DeviceModel    m_deviceModel;
    SettingsModel  m_settingsModel;
    ButtonModel    m_buttonModel;
    ActionModel    m_actionModel;
    std::unique_ptr<ActionFilterModel> m_actionFilterModel;
    ProfileModel   m_profileModel;
    ProfileEngine  m_profileEngine;
    ActionExecutor m_actionExecutor;

    std::unique_ptr<EditorModel>         m_editorModel;
    std::unique_ptr<IDesktopIntegration> m_ownedDesktop;
    std::unique_ptr<IInputInjector>      m_ownedInjector;
    IDesktopIntegration *m_desktop  = nullptr;
    IInputInjector      *m_injector = nullptr;

    // Active device descriptor (set on connect)
    const IDevice *m_currentDevice = nullptr;

    // Per-device gesture/thumb state
    struct PerDeviceState {
        int gestureAccumX = 0;
        int gestureAccumY = 0;
        int thumbAccum = 0;
        bool gestureActive = false;
        uint16_t gestureControlId = 0;
    };
    QMap<QString, PerDeviceState> m_perDeviceState;

    static constexpr int kGestureThreshold = 50;
    static constexpr int kThumbThreshold = 15;
};

} // namespace logitune
