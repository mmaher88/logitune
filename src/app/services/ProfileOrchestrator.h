#pragma once
#include "ProfileEngine.h"
#include <QObject>
#include <QString>

namespace logitune {

class ActionExecutor;
class ActionModel;
class ButtonModel;
class DeviceModel;
class ActiveDeviceResolver;
class IDesktopIntegration;
class IDevice;
class PhysicalDevice;
class ProfileModel;

/// Owns the save / apply / push / window-focus flow. Holds no device
/// state of its own beyond the current IDevice pointer; reads from
/// models and engines, writes to them, and emits profileApplied(serial)
/// after every hardware apply.
///
/// Two kinds of target, never mixed:
///  - Device-originated work (setup, reconnect, focus switching, button-
///    initiated changes) names the mouse it is for and writes to that
///    mouse's own transport and profiles.
///  - UI edits act on the mouse selected in the carousel (activeSerial /
///    m_currentDevice), because that is the mouse the user is looking at.
///
/// Zero `connect()` calls in the .cpp — AppRoot wires the inbound
/// signals (from models, ProfileEngine, desktop, DeviceCommandHandler) and the
/// outbound signals (to ButtonActionDispatcher).
class ProfileOrchestrator : public QObject {
    Q_OBJECT
public:
    ProfileOrchestrator(ProfileEngine *profileEngine,
                        ActionExecutor *actionExecutor,
                        ActiveDeviceResolver *selection,
                        DeviceModel *deviceModel,
                        ButtonModel *buttonModel,
                        ActionModel *actionModel,
                        ProfileModel *profileModel,
                        IDesktopIntegration *desktop,
                        QObject *parent = nullptr);

    void setupProfileForDevice(PhysicalDevice *device);

    /// Writes \p p to \p device's active transport, using that device's own
    /// descriptor. Does nothing (and logs why) if the device has no
    /// transport or descriptor yet.
    void applyProfileToHardware(PhysicalDevice *device, const Profile &p);

    /// Templated bridge for the five DeviceModel *ChangeRequested signals.
    /// Mutates the displayed profile's cached copy, persists it, refreshes
    /// the UI, and only forwards to hardware if the displayed profile is
    /// the one currently active on the device.
    template<typename Mutator, typename HardwareForward>
    void applyDisplayedChange(Mutator mutator, HardwareForward hardwareForward) {
        const QString serial = activeSerial();
        if (serial.isEmpty()) return;
        const QString name = m_profileEngine->displayProfile(serial);
        if (name.isEmpty()) return;
        Profile &p = m_profileEngine->cachedProfile(serial, name);
        mutator(p);
        m_profileEngine->saveProfileToDisk(serial, name);
        pushDisplayValues(p);
        if (name == m_profileEngine->hardwareProfile(serial))
            hardwareForward();
    }

    /// Mirror of applyDisplayedChange for changes that originate at the
    /// device (a diverted button cycling DPI or toggling SmartShift). The
    /// button acted on the hardware, so the change belongs to the HARDWARE
    /// profile — not whichever profile the user happens to be viewing — and
    /// no hardware forward is needed because the device is already in the
    /// new state. \p serial is the mouse whose button was pressed, which is
    /// not necessarily the one on screen; the UI is refreshed only when it
    /// is, and only when the viewed profile is the active one.
    template<typename Mutator>
    void applyHardwareChange(const QString &serial, Mutator mutator) {
        if (serial.isEmpty()) return;
        const QString name = m_profileEngine->hardwareProfile(serial);
        if (name.isEmpty()) return;
        Profile &p = m_profileEngine->cachedProfile(serial, name);
        mutator(p);
        m_profileEngine->saveProfileToDisk(serial, name);
        if (serial == activeSerial() && name == m_profileEngine->displayProfile(serial))
            pushDisplayValues(p);
    }

public slots:
    void saveCurrentProfile();
    void onDpiChangedByButton(const QString &serial, int dpi);
    void onSmartShiftChangedByButton(const QString &serial, bool enabled, int threshold);
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const QString &serial, const Profile &profile);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTransportSetupComplete(PhysicalDevice *device);

    /// The descriptor of the mouse selected in the UI. Used only for UI
    /// edits and for rebuilding the button list on screen.
    void onCurrentDeviceChanged(const IDevice *device);

signals:
    /// Emitted after applyProfileToHardware finishes. ButtonActionDispatcher
    /// subscribes to reset its thumbAccum for the given serial.
    void profileApplied(const QString &serial);

private:
    QString activeSerial() const;
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);

    ProfileEngine       *m_profileEngine;
    ActionExecutor      *m_actionExecutor;
    ActiveDeviceResolver     *m_selection;
    DeviceModel         *m_deviceModel;
    ButtonModel         *m_buttonModel;
    ActionModel         *m_actionModel;
    ProfileModel        *m_profileModel;
    IDesktopIntegration *m_desktop;
    const IDevice       *m_currentDevice = nullptr;
};

} // namespace logitune
