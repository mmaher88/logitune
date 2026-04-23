#pragma once
#include <QMap>
#include <QObject>
#include <QString>
#include <cstdint>

namespace logitune::test {
class AppControllerFixture;
class ButtonActionDispatcherFixture;
}

namespace logitune {

class ActionExecutor;
class DeviceSelection;
class ProfileEngine;
class IDevice;

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
                           DeviceSelection *selection,
                           QObject *parent = nullptr);

    void onDeviceRemoved(const QString &serial);

    friend class test::AppControllerFixture;
    friend class test::ButtonActionDispatcherFixture;

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
    DeviceSelection *m_selection;
    const IDevice   *m_currentDevice = nullptr;
    QMap<QString, PerDeviceState> m_state;
};

} // namespace logitune
