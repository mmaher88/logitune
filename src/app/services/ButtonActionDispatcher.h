#pragma once
#include <QMap>
#include <QObject>
#include <QString>
#include <cstdint>

namespace logitune::test {
class AppRootFixture;
class ButtonActionDispatcherFixture;
}

namespace logitune {

class ActionExecutor;
class PhysicalDevice;
class ProfileEngine;
class IDesktopIntegration;

/// Turns raw HID++ input events (gestureRawXY, divertedButtonPressed,
/// thumbWheelRotation) into high-level actions (SmartShift toggle, DPI
/// cycle, keystroke injection, gesture direction, app launch).
///
/// Every event names the mouse it came from. Mappings are read from that
/// mouse's hardware profile, its own descriptor resolves the control ID,
/// device writes go to its own transport, and gesture / thumb wheel
/// accumulators are kept per mouse. Which mouse is selected in the UI is
/// irrelevant here.
class ButtonActionDispatcher : public QObject {
    Q_OBJECT
public:
    ButtonActionDispatcher(ProfileEngine *profileEngine,
                           ActionExecutor *actionExecutor,
                           IDesktopIntegration *desktop = nullptr,
                           QObject *parent = nullptr);

    void onDeviceRemoved(const QString &serial);

    friend class test::AppRootFixture;
    friend class test::ButtonActionDispatcherFixture;

signals:
    /// Emitted after a diverted button changes a device setting, so the
    /// orchestrator can persist it into that mouse's active profile.
    /// Notifying explicitly (rather than having the orchestrator listen to
    /// DeviceSession's change signals) keeps a profile *application* from
    /// echoing back and rewriting the profile it just applied.
    void dpiChangedByButton(const QString &serial, int dpi);
    void smartShiftChangedByButton(const QString &serial, bool enabled, int threshold);

public slots:
    void onGestureRaw(PhysicalDevice *device, int16_t dx, int16_t dy);
    void onDivertedButtonPressed(PhysicalDevice *device, uint16_t controlId, bool pressed);
    void onThumbWheelRotation(PhysicalDevice *device, int delta);
    void onProfileApplied(const QString &serial);

private:
    struct PerDeviceState {
        int gestureAccumX = 0;
        int gestureAccumY = 0;
        int thumbAccum = 0;
        bool gestureActive = false;
        uint16_t gestureControlId = 0;
    };
    static constexpr int kGestureThreshold = 50;
    static constexpr int kThumbThreshold = 15;

    ProfileEngine   *m_profileEngine;
    ActionExecutor  *m_actionExecutor;
    IDesktopIntegration *m_desktop = nullptr;
    QMap<QString, PerDeviceState> m_state;  // keyed by PhysicalDevice serial
};

} // namespace logitune
