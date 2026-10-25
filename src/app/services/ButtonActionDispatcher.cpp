#include "ButtonActionDispatcher.h"
#include "ActionExecutor.h"
#include "ActiveDeviceResolver.h"
#include "DeviceSession.h"
#include "ProfileEngine.h"
#include "interfaces/IDesktopIntegration.h"
#include "interfaces/IDevice.h"
#include "logging/LogManager.h"
#include <QEasingCurve>
#include <cmath>
#include <cstdlib>
#include <memory>

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
    cancelWorkspaceFinish(serial);
    if (auto *timer = m_workspaceSafetyTimers.take(serial)) {
        timer->stop();
        timer->deleteLater();
    }
    auto it = m_state.find(serial);
    if (it != m_state.end() && it->workspaceGestureActive) {
        it->workspaceGestureActive = false;
        if (m_desktop)
            m_desktop->endInteractiveWorkspaceSwitch(50);
    }
    m_state.remove(serial);
}

void ButtonActionDispatcher::cancelWorkspaceFinish(const QString &serial)
{
    if (auto anim = m_workspaceFinishAnims.take(serial)) {
        anim->stop();
        anim->deleteLater();
    }
}

QTimer *ButtonActionDispatcher::workspaceSafetyTimer(const QString &serial)
{
    auto it = m_workspaceSafetyTimers.find(serial);
    if (it != m_workspaceSafetyTimers.end())
        return *it;

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(kWorkspaceSafetyMs);
    QObject::connect(timer, &QTimer::timeout, this, [this, serial]() {
        endWorkspaceGesture(serial);
    });
    m_workspaceSafetyTimers.insert(serial, timer);
    return timer;
}

int ButtonActionDispatcher::workspaceSnapTarget(double progress, int snapPercent)
{
    const double thresh = qBound(snapPercent, 10, 90) / 100.0;
    if (std::abs(progress) < 1e-9)
        return 0;
    const int dir = progress > 0.0 ? 1 : -1;
    const double crossed = std::abs(progress);
    const double whole = std::floor(crossed);
    const double frac = crossed - whole;
    if (whole < 1.0) {
        // Still within the first workspace of travel from origin.
        return (crossed >= thresh) ? dir : 0;
    }
    const int steps = static_cast<int>(whole) + (frac >= thresh ? 1 : 0);
    return dir * steps;
}

void ButtonActionDispatcher::finishWorkspaceGesture(const QString &serial,
                                                    int snapPercent)
{
    auto it = m_state.find(serial);
    if (it == m_state.end())
        return;
    it->workspaceGestureActive = false;
    it->workspaceProgress = 0.0;
    m_workspaceFinishAnims.remove(serial);
    if (m_desktop)
        m_desktop->endInteractiveWorkspaceSwitch(snapPercent);
}

void ButtonActionDispatcher::endWorkspaceGesture(const QString &serial)
{
    auto it = m_state.find(serial);
    if (it == m_state.end() || !it->workspaceGestureActive)
        return;
    if (m_workspaceFinishAnims.contains(serial))
        return; // already easing to the snap target

    if (auto *timer = m_workspaceSafetyTimers.value(serial, nullptr))
        timer->stop();
    if (!m_desktop) {
        it->workspaceGestureActive = false;
        it->workspaceProgress = 0.0;
        return;
    }

    int snapPercent = 50;
    if (m_profileEngine && m_profileEngine->hasDevice(serial)) {
        const QString hw = m_profileEngine->hardwareProfile(serial);
        snapPercent = qBound(
            m_profileEngine->cachedProfile(serial, hw).workspaceSnapPercent, 10, 90);
    }

    const double from = it->workspaceProgress;
    const double to = static_cast<double>(workspaceSnapTarget(from, snapPercent));
    const double distance = std::abs(to - from);

    // Already on the snap point — just close the Shell gesture.
    if (distance < 0.001) {
        finishWorkspaceGesture(serial, snapPercent);
        return;
    }

    // Ease Updates toward the threshold target, then End — so release
    // animates instead of jumping (Shell End only closest-snaps).
    auto *anim = new QVariantAnimation(this);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setDuration(std::max(0, m_workspaceFinishDurationMs));
    anim->setEasingCurve(QEasingCurve::OutCubic);

    auto last = std::make_shared<double>(from);
    QObject::connect(anim, &QVariantAnimation::valueChanged, this,
                     [this, serial, last](const QVariant &value) {
                         if (!m_desktop)
                             return;
                         const double now = value.toDouble();
                         const int delta = static_cast<int>(std::lround(
                             (now - *last) * kWorkspaceUnitsPerWorkspace));
                         *last = now;
                         if (delta == 0)
                             return;
                         auto st = m_state.find(serial);
                         if (st != m_state.end())
                             st->workspaceProgress = now;
                         m_desktop->updateInteractiveWorkspaceSwitch(delta);
                     });
    QObject::connect(anim, &QVariantAnimation::finished, this,
                     [this, serial, snapPercent, anim]() {
                         anim->deleteLater();
                         finishWorkspaceGesture(serial, snapPercent);
                     });

    m_workspaceFinishAnims.insert(serial, anim);
    anim->start();
}

void ButtonActionDispatcher::handleWorkspaceThumbWheel(const QString &serial,
                                                       PerDeviceState &state,
                                                       int normalized)
{
    if (m_desktop && m_desktop->supportsInteractiveWorkspaceSwitch()) {
        // Ignore wheel input while the release ease is running.
        if (m_workspaceFinishAnims.contains(serial))
            return;
        if (!state.workspaceGestureActive) {
            m_desktop->beginInteractiveWorkspaceSwitch();
            state.workspaceGestureActive = true;
            state.workspaceProgress = 0.0;
        }
        state.workspaceProgress +=
            static_cast<double>(normalized) / kWorkspaceUnitsPerWorkspace;
        m_desktop->updateInteractiveWorkspaceSwitch(normalized);
        // Hold scrub while thumb is down; only Stop (or safety timeout) ends.
        workspaceSafetyTimer(serial)->start();
        return;
    }

    // Fallback: one-shot switch-desktop presets per thumb threshold.
    state.thumbAccum += normalized;
    if (std::abs(state.thumbAccum) < kThumbThreshold)
        return;
    int steps = state.thumbAccum / kThumbThreshold;
    state.thumbAccum %= kThumbThreshold;
    if (!m_desktop)
        return;
    for (int i = 0; i < std::abs(steps); ++i) {
        const QString presetId = steps > 0
            ? QStringLiteral("switch-desktop-right")
            : QStringLiteral("switch-desktop-left");
        auto resolved = m_desktop->resolveNamedAction(presetId);
        if (!resolved.has_value()) {
            qCWarning(lcApp) << "workspace thumb wheel preset" << presetId
                             << "not resolvable on" << m_desktop->variantKey();
            continue;
        }
        m_actionExecutor->executeAction(*resolved);
    }
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
        if (it == hwProfile.gestures.end() || it->second.type == ButtonAction::Default)
            return;

        // Mirrors the PresetRef branch in the press path below: if the action
        // is a PresetRef, resolve via the desktop integration to a concrete
        // ButtonAction, then fire it via the executor. Otherwise fire the
        // stored action directly.
        ButtonAction toFire = it->second;
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

void ButtonActionDispatcher::onThumbWheelReleased()
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session)
        return;
    if (session->thumbWheelMode() != QLatin1String("workspace"))
        return;
    endWorkspaceGesture(session->deviceId());
}

void ButtonActionDispatcher::onThumbWheelRotation(int delta)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    const QString serial = session->deviceId();
    auto &state = m_state[serial];

    const QString &mode = session->thumbWheelMode();
    // Invert is applied by the device when diverted (setReporting) for
    // volume/zoom/scroll; workspace applies invert in software instead.
    int normalized = delta * session->thumbWheelDefaultDirection();
    if (mode == QLatin1String("workspace") && session->thumbWheelInvert())
        normalized = -normalized;
    qCDebug(lcInput) << "thumbWheel raw=" << delta << "normalized=" << normalized
                      << "mode=" << mode << "invert=" << session->thumbWheelInvert();

    if (mode == QLatin1String("workspace")) {
        handleWorkspaceThumbWheel(serial, state, normalized);
        return;
    }

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
