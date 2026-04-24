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
    if (state.gestureActive) {
        state.gestureAccumX += dx;
        state.gestureAccumY += dy;
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
        state.gestureActive = false;
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
        if (it != hwProfile.gestures.end() && it->second.type == ButtonAction::Keystroke
            && !it->second.payload.isEmpty()) {
            m_actionExecutor->injectKeystroke(it->second.payload);
        }
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
    } else if (ba.type == ButtonAction::DpiCycle) {
        session->cycleDpi();
    } else if ((ba.type == ButtonAction::Keystroke || ba.type == ButtonAction::Media)
               && !ba.payload.isEmpty()) {
        m_actionExecutor->injectKeystroke(ba.payload);
    } else if (ba.type == ButtonAction::GestureTrigger) {
        state.gestureAccumX = 0;
        state.gestureAccumY = 0;
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
