#pragma once
#include "DeviceFetcher.h"
#include "DeviceManager.h"
#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
#include "interfaces/IDesktopIntegration.h"
#include "interfaces/IInputInjector.h"
#include "ProfileEngine.h"
#include "ActionExecutor.h"
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
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

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    AppController(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent = nullptr);

    void init();
    void startMonitoring();

    friend class test::AppControllerFixture;

    DeviceModel    *deviceModel()    { return &m_deviceModel; }
    ButtonModel    *buttonModel()    { return &m_buttonModel; }
    ActionModel    *actionModel()    { return &m_actionModel; }
    ProfileModel   *profileModel()   { return &m_profileModel; }
    SettingsModel  *settingsModel()  { return &m_settingsModel; }

private slots:
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const Profile &profile);
    void onSessionAdded(const QString &deviceId);
    void onSessionRemoved(const QString &deviceId);
    void onGestureRawXY(int16_t dx, int16_t dy);
    void onDivertedButtonPressed(uint16_t controlId, bool pressed);
    void onThumbWheelRotation(int delta);
    void onDpiChangeRequested(int value);
    void onSmartShiftChangeRequested(bool enabled, int threshold);
    void onScrollConfigChangeRequested(bool hiRes, bool invert);
    void onThumbWheelModeChangeRequested(const QString &mode);
    void onThumbWheelInvertChangeRequested(bool invert);

private:
    void wireSignals();
    void saveCurrentProfile();
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);
    void applyProfileToHardware(const Profile &p);
    void setupProfileForSession(DeviceSession *session);
    DeviceSession* selectedSession() const;
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
    ProfileModel   m_profileModel;
    ProfileEngine  m_profileEngine;
    ActionExecutor m_actionExecutor;

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
