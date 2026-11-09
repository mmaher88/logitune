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
class ActiveDeviceResolver;
class ProfileEngine;
class IDevice;
class IDesktopIntegration;

/// Turns raw HID++ input events (gestureRawXY, divertedButtonPressed,
/// thumbWheelRotation) into high-level actions (SmartShift toggle, DPI
/// cycle, keystroke injection, gesture direction, app launch).
///
/// Owns the per-device gesture + thumb wheel accumulator state.
class ButtonActionDispatcher : public QObject {
    Q_OBJECT
public:
    ButtonActionDispatcher(ProfileEngine *profileEngine,
                           ActionExecutor *actionExecutor,
                           ActiveDeviceResolver *selection,
                           IDesktopIntegration *desktop = nullptr,
                           QObject *parent = nullptr);

    void onDeviceRemoved(const QString &serial);

    friend class test::AppRootFixture;
    friend class test::ButtonActionDispatcherFixture;

signals:
    /// Emitted after a diverted button changes a device setting, so the
    /// orchestrator can persist it into the active profile. Notifying
    /// explicitly (rather than having the orchestrator listen to
    /// DeviceSession's change signals) keeps a profile *application* from
    /// echoing back and rewriting the profile it just applied.
    void dpiChangedByButton(int dpi);
    void smartShiftChangedByButton(bool enabled, int threshold);

public slots:
    void onGestureRaw(int16_t dx, int16_t dy);
    void onDivertedButtonPressed(uint16_t controlId, bool pressed);
    void onThumbWheelRotation(int delta);
    void onProfileApplied(const QString &serial);
    void onCurrentDeviceChanged(const IDevice *device);

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
    ActiveDeviceResolver *m_selection;
    IDesktopIntegration *m_desktop = nullptr;
    const IDevice   *m_currentDevice = nullptr;
    QMap<QString, PerDeviceState> m_state;
};

} // namespace logitune
