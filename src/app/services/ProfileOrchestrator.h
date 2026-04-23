#pragma once
#include "ButtonAction.h"
#include "ProfileEngine.h"
#include <QObject>
#include <QString>

namespace logitune::test {
class AppControllerFixture;
class ProfileOrchestratorFixture;
}

namespace logitune {

class ActionExecutor;
class ActionModel;
class ButtonModel;
class DeviceModel;
class DeviceSelection;
class IDesktopIntegration;
class IDevice;
class PhysicalDevice;
class ProfileModel;

/// Owns the save / apply / push / window-focus flow. Holds no device
/// state of its own beyond the current IDevice pointer; reads from
/// models and engines, writes to them, and emits profileApplied(serial)
/// after every hardware apply.
///
/// Zero `connect()` calls in the .cpp — AppController wires the inbound
/// signals (from models, ProfileEngine, desktop, DeviceCommands) and the
/// outbound signals (to ButtonActionDispatcher).
class ProfileOrchestrator : public QObject {
    Q_OBJECT
public:
    ProfileOrchestrator(ProfileEngine *profileEngine,
                        ActionExecutor *actionExecutor,
                        DeviceSelection *selection,
                        DeviceModel *deviceModel,
                        ButtonModel *buttonModel,
                        ActionModel *actionModel,
                        ProfileModel *profileModel,
                        IDesktopIntegration *desktop,
                        QObject *parent = nullptr);

    void setupProfileForDevice(PhysicalDevice *device);
    void applyProfileToHardware(const Profile &p);

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

    friend class test::AppControllerFixture;
    friend class test::ProfileOrchestratorFixture;

public slots:
    void saveCurrentProfile();
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const QString &serial, const Profile &profile);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTransportSetupComplete(PhysicalDevice *device);
    void onCurrentDeviceChanged(const IDevice *device);

signals:
    /// Emitted after applyProfileToHardware finishes. ButtonActionDispatcher
    /// subscribes to reset its thumbAccum for the given serial.
    void profileApplied(const QString &serial);

    /// Emitted on every m_currentDevice update. ButtonActionDispatcher
    /// subscribes to keep its own IDevice pointer in sync.
    void currentDeviceChanged(const IDevice *device);

private:
    QString activeSerial() const;
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);
    // Temporary — Task 5 moves these to ActionModel.
    QString buttonActionToName(const ButtonAction &ba) const;
    ButtonAction buttonEntryToAction(const QString &actionType, const QString &actionName) const;

    ProfileEngine       *m_profileEngine;
    ActionExecutor      *m_actionExecutor;
    DeviceSelection     *m_selection;
    DeviceModel         *m_deviceModel;
    ButtonModel         *m_buttonModel;
    ActionModel         *m_actionModel;
    ProfileModel        *m_profileModel;
    IDesktopIntegration *m_desktop;
    const IDevice       *m_currentDevice = nullptr;
};

} // namespace logitune
