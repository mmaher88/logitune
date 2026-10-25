#pragma once
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
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

    /// Map travel + snap percent to an integer workspace offset from origin.
    static int workspaceSnapTarget(double progress, int snapPercent);

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
    /// HID++ 0x2150 rotation_status Stop — end workspace scrub on thumb lift.
    void onThumbWheelReleased();
    void onProfileApplied(const QString &serial);
    void onCurrentDeviceChanged(const IDevice *device);

private:
    struct PerDeviceState {
        int gestureAccumX = 0;
        int gestureAccumY = 0;
        int thumbAccum = 0;
        bool gestureActive = false;
        uint16_t gestureControlId = 0;
        bool workspaceGestureActive = false;
        /// Accumulated thumb travel in workspace units (1.0 = one workspace).
        double workspaceProgress = 0.0;
    };
    static constexpr int kGestureThreshold = 50;
    static constexpr int kThumbThreshold = 15;
    /// Fallback if Stop is missed; must be long so pause-with-thumb-down holds.
    static constexpr int kWorkspaceSafetyMs = 5000;
    /// Must match UNITS_PER_WORKSPACE in the GNOME workspace extension.
    static constexpr double kWorkspaceUnitsPerWorkspace = 150.0;
    /// Ease duration when finishing scrub to the threshold target on release.
    static constexpr int kWorkspaceFinishMs = 250;

    void handleWorkspaceThumbWheel(const QString &serial, PerDeviceState &state,
                                   int normalized);
    void endWorkspaceGesture(const QString &serial);
    void cancelWorkspaceFinish(const QString &serial);
    void finishWorkspaceGesture(const QString &serial, int snapPercent);
    QTimer *workspaceSafetyTimer(const QString &serial);

    ProfileEngine   *m_profileEngine;
    ActionExecutor  *m_actionExecutor;
    ActiveDeviceResolver *m_selection;
    IDesktopIntegration *m_desktop = nullptr;
    const IDevice   *m_currentDevice = nullptr;
    QMap<QString, PerDeviceState> m_state;
    QMap<QString, QTimer*> m_workspaceSafetyTimers;
    QMap<QString, QPointer<QVariantAnimation>> m_workspaceFinishAnims;
    /// Overridable for unit tests (0 = finish synchronously).
    int m_workspaceFinishDurationMs = kWorkspaceFinishMs;
};

} // namespace logitune
