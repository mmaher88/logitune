#include "ButtonActionDispatcher.h"
#include "ActionExecutor.h"
#include "ActiveDeviceResolver.h"
#include "DeviceSession.h"
#include "ProfileEngine.h"
#include "interfaces/IDesktopIntegration.h"
#include "interfaces/IDevice.h"
#include "logging/LogManager.h"
#include <cstdlib>

namespace logitune {

namespace {

QString directionForAxis(int lockedAxis, int signedAccum)
{
    if (lockedAxis == 1)
        return signedAccum > 0 ? QStringLiteral("right") : QStringLiteral("left");
    return signedAccum > 0 ? QStringLiteral("down") : QStringLiteral("up");
}

} // namespace

ButtonActionDispatcher::ButtonActionDispatcher(ProfileEngine *profileEngine,
                                               ActionExecutor *actionExecutor,
                                               ActiveDeviceResolver *selection,
                                               IDesktopIntegration *desktop,
                                               QObject *parent)
    : QObject(parent)
    , m_profileEngine(profileEngine)
    , m_actionExecutor(actionExecutor)
    , m_selection(selection)
    , m_desktop(desktop)
{}

bool ButtonActionDispatcher::isContinuousGestureAction(const ButtonAction &action)
{
    if (action.type != ButtonAction::Keystroke && action.type != ButtonAction::Media)
        return false;
    return action.payload == QLatin1String("VolumeUp")
        || action.payload == QLatin1String("VolumeDown")
        || action.payload == QLatin1String("BrightnessUp")
        || action.payload == QLatin1String("BrightnessDown");
}

void ButtonActionDispatcher::fireGestureAction(const ButtonAction &action)
{
    ButtonAction toFire = action;
    if (toFire.type == ButtonAction::PresetRef) {
        if (!m_desktop) {
            qCWarning(lcApp) << "gesture preset" << toFire.payload
                             << "requested but desktop integration is null";
            return;
        }
        auto resolved = m_desktop->resolveNamedAction(toFire.payload);
        if (!resolved.has_value()) {
            qCWarning(lcApp) << "gesture preset" << toFire.payload
                             << "not resolvable on" << m_desktop->variantKey();
            return;
        }
        toFire = *resolved;
    }
    m_actionExecutor->executeAction(toFire);
}

void ButtonActionDispatcher::onDeviceRemoved(const QString &serial)
{
    m_state.remove(serial);
}

void ButtonActionDispatcher::onProfileApplied(const QString &serial)
{
    auto it = m_state.find(serial);
    if (it != m_state.end()) it->thumbAccum = 0;
}

void ButtonActionDispatcher::onCurrentDeviceChanged(const IDevice *device)
{
    m_currentDevice = device;
}

void ButtonActionDispatcher::onGestureRaw(int16_t dx, int16_t dy)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    auto &state = m_state[session->deviceId()];
    if (!state.gestureActive) return;

    state.gestureAccumX += dx;
    state.gestureAccumY += dy;

    const QString serial = m_selection->activeSerial();
    if (serial.isEmpty()) return;
    const Profile &hwProfile = m_profileEngine->cachedProfile(
        serial, m_profileEngine->hardwareProfile(serial));

    if (state.gestureLockedAxis == 0) {
        if (std::abs(state.gestureAccumX) < kGestureStepThreshold
            && std::abs(state.gestureAccumY) < kGestureStepThreshold)
            return;
        state.gestureLockedAxis =
            std::abs(state.gestureAccumX) >= std::abs(state.gestureAccumY) ? 1 : 2;
        state.gestureStepAccum =
            state.gestureLockedAxis == 1 ? state.gestureAccumX : state.gestureAccumY;
    } else {
        state.gestureStepAccum += state.gestureLockedAxis == 1 ? dx : dy;
    }

    if (state.gestureStepAccum == 0) return;

    const QString probeDir =
        directionForAxis(state.gestureLockedAxis, state.gestureStepAccum);
    auto it = hwProfile.gestures.find(probeDir);
    if (it == hwProfile.gestures.end() || it->second.type == ButtonAction::Default)
        return;

    ButtonAction probe = it->second;
    if (probe.type == ButtonAction::PresetRef) {
        if (!m_desktop) return;
        auto resolved = m_desktop->resolveNamedAction(probe.payload);
        if (!resolved.has_value()) return;
        probe = *resolved;
    }
    if (!isContinuousGestureAction(probe)) return;

    if (std::abs(state.gestureStepAccum) < kGestureStepThreshold) return;

    int steps = state.gestureStepAccum / kGestureStepThreshold;
    state.gestureStepAccum %= kGestureStepThreshold;

    for (int i = 0; i < std::abs(steps); ++i) {
        const QString stepDir =
            directionForAxis(state.gestureLockedAxis, steps);
        auto sit = hwProfile.gestures.find(stepDir);
        if (sit == hwProfile.gestures.end()
            || sit->second.type == ButtonAction::Default)
            continue;
        fireGestureAction(sit->second);
        state.gestureFiredContinuous = true;
    }
}

void ButtonActionDispatcher::onDivertedButtonPressed(uint16_t controlId, bool pressed)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    auto &state = m_state[session->deviceId()];

    const QString serial = m_selection->activeSerial();
    if (serial.isEmpty()) return;
    const Profile &hwProfile = m_profileEngine->cachedProfile(
        serial, m_profileEngine->hardwareProfile(serial));

    if (!pressed && state.gestureActive
        && (controlId == 0 || controlId == state.gestureControlId)) {
        const bool alreadyFired = state.gestureFiredContinuous;
        state.gestureActive = false;
        state.gestureFiredContinuous = false;
        state.gestureLockedAxis = 0;
        state.gestureStepAccum = 0;

        if (alreadyFired) return;

        int dx = state.gestureAccumX;
        int dy = state.gestureAccumY;

        QString dir;
        if (std::abs(dx) > kGestureThreshold || std::abs(dy) > kGestureThreshold) {
            if (std::abs(dx) > std::abs(dy))
                dir = dx > 0 ? "right" : "left";
            else
                dir = dy > 0 ? "down" : "up";
        } else {
            dir = "click";
        }

        auto it = hwProfile.gestures.find(dir);
        if (it == hwProfile.gestures.end() || it->second.type == ButtonAction::Default)
            return;

        fireGestureAction(it->second);
        return;
    }

    if (!pressed) return;

    if (!m_currentDevice) return;
    int idx = -1;
    for (const auto &ctrl : m_currentDevice->controls()) {
        if (ctrl.controlId == controlId) {
            idx = ctrl.buttonIndex;
            break;
        }
    }
    if (idx < 0) return;

    const auto &ba = (static_cast<std::size_t>(idx) < hwProfile.buttons.size())
        ? hwProfile.buttons[static_cast<std::size_t>(idx)]
        : ButtonAction{ButtonAction::Default, {}};

    qCDebug(lcApp) << "button" << idx << "action type=" << ba.type << "payload=" << ba.payload;

    if (ba.type == ButtonAction::Default) return;

    if (ba.type == ButtonAction::SmartShiftToggle) {
        bool current = session->smartShiftEnabled();
        session->setSmartShift(!current, session->smartShiftThreshold());
        emit smartShiftChangedByButton(!current, session->smartShiftThreshold());
    } else if (ba.type == ButtonAction::DpiCycle) {
        session->cycleDpi();
        emit dpiChangedByButton(session->currentDPI());
    } else if ((ba.type == ButtonAction::Keystroke || ba.type == ButtonAction::Media)
               && !ba.payload.isEmpty()) {
        m_actionExecutor->injectKeystroke(ba.payload);
    } else if (ba.type == ButtonAction::GestureTrigger) {
        state.gestureAccumX = 0;
        state.gestureAccumY = 0;
        state.gestureStepAccum = 0;
        state.gestureFiredContinuous = false;
        state.gestureLockedAxis = 0;
        state.gestureActive = true;
        state.gestureControlId = controlId;
    } else if (ba.type == ButtonAction::AppLaunch && !ba.payload.isEmpty()) {
        m_actionExecutor->launchApp(ba.payload);
    } else if (ba.type == ButtonAction::PresetRef && !ba.payload.isEmpty()) {
        if (!m_desktop) {
            qCWarning(lcApp) << "preset action requested but desktop integration is null"
                             << ba.payload;
            return;
        }
        auto resolved = m_desktop->resolveNamedAction(ba.payload);
        if (!resolved.has_value()) {
            qCWarning(lcApp) << "preset" << ba.payload
                             << "not resolvable on" << m_desktop->variantKey();
            return;
        }
        m_actionExecutor->executeAction(*resolved);
    }
}

void ButtonActionDispatcher::onThumbWheelRotation(int delta)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    auto &state = m_state[session->deviceId()];

    const QString &mode = session->thumbWheelMode();
    int normalized = delta * session->thumbWheelDefaultDirection();
    qCDebug(lcInput) << "thumbWheel raw=" << delta << "normalized=" << normalized
                      << "mode=" << mode << "invert=" << session->thumbWheelInvert();
    state.thumbAccum += normalized;

    if (std::abs(state.thumbAccum) < kThumbThreshold)
        return;

    int steps = state.thumbAccum / kThumbThreshold;
    state.thumbAccum %= kThumbThreshold;

    qCDebug(lcInput) << "thumbWheel steps=" << steps << "accum=" << state.thumbAccum;
    for (int i = 0; i < std::abs(steps); ++i) {
        if (mode == "scroll") {
            int dir = steps > 0 ? 1 : -1;
            qCDebug(lcInput) << "thumbWheel action: HScroll" << dir;
            m_actionExecutor->injectHorizontalScroll(dir);
        } else if (mode == "volume") {
            QString key = steps > 0 ? "VolumeUp" : "VolumeDown";
            qCDebug(lcInput) << "thumbWheel action:" << key;
            m_actionExecutor->injectKeystroke(key);
        } else if (mode == "zoom") {
            int dir = steps > 0 ? 1 : -1;
            qCDebug(lcInput) << "thumbWheel action: CtrlScroll" << dir;
            m_actionExecutor->injectCtrlScroll(dir);
        }
    }
}

} // namespace logitune
