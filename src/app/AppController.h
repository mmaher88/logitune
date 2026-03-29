#pragma once
#include "DeviceManager.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
#include "ProfileEngine.h"
#include "ActionExecutor.h"
#include "input/UinputInjector.h"
#include "desktop/KDeDesktop.h"
#include "models/DeviceModel.h"
#include "models/ButtonModel.h"
#include "models/ActionModel.h"
#include "models/ProfileModel.h"
#include <QObject>
#include <QTimer>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace logitune {

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);

    /// Create all subsystems, wire signals, set gesture defaults.
    /// Call this once after construction but before QML loads.
    void init();

    /// Start device monitoring and window tracking.
    /// Call after QML is loaded.
    void startMonitoring();

    // Accessors for QML singleton registration
    DeviceModel  *deviceModel()  { return &m_deviceModel; }
    ButtonModel  *buttonModel()  { return &m_buttonModel; }
    ActionModel  *actionModel()  { return &m_actionModel; }
    ProfileModel *profileModel() { return &m_profileModel; }

private slots:
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const Profile &profile);
    void onDeviceSetupComplete();
    void onGestureRawXY(int16_t dx, int16_t dy);
    void onDivertedButtonPressed(uint16_t controlId, bool pressed);
    void onThumbWheelRotation(int delta);
    void onDpiChangeRequested(int value);
    void onSmartShiftChangeRequested(bool enabled, int threshold);
    void onScrollConfigChangeRequested(bool hiRes, bool invert);
    void onThumbWheelModeChangeRequested(const QString &mode);

private:
    void wireSignals();
    void saveCurrentProfile();
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);
    void applyProfileToHardware(const Profile &p);
    QString buttonActionToName(const ButtonAction &ba) const;
    ButtonAction buttonEntryToAction(const QString &actionType, const QString &actionName) const;

    // Subsystems
    DeviceRegistry m_registry;
    DeviceManager  m_deviceManager;
    DeviceModel    m_deviceModel;
    ButtonModel    m_buttonModel;
    ActionModel    m_actionModel;
    ProfileModel   m_profileModel;
    KDeDesktop     m_windowTracker;
    ProfileEngine  m_profileEngine;
    UinputInjector m_injector;
    ActionExecutor m_actionExecutor;

    // Active device descriptor (set on connect)
    const IDevice *m_currentDevice = nullptr;

    // Gesture state
    int      m_gestureTotalDx = 0;
    int      m_gestureTotalDy = 0;
    bool     m_gestureActive  = false;
    uint16_t m_gestureControlId = 0;
    static constexpr int kGestureThreshold = 50;

    // Thumb wheel accumulator
    int m_thumbAccum = 0;
    static constexpr int kThumbThreshold = 15;

};

} // namespace logitune
