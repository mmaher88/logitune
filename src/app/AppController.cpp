#include "AppController.h"
#include "desktop/KDeDesktop.h"
#include "desktop/GnomeDesktop.h"
#include "desktop/GenericDesktop.h"
#include "input/UinputInjector.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

namespace logitune {

// Construction / init --------------------------------------------------------

AppController::AppController(QObject *parent)
    : AppController(nullptr, nullptr, parent)
{
}

AppController::AppController(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent)
    : QObject(parent)
    , m_deviceManager(&m_registry)
    , m_actionExecutor(nullptr)
{
    if (desktop) {
        m_desktop = desktop;
    } else {
        const QString xdgDesktop = QProcessEnvironment::systemEnvironment()
                                       .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
        if (xdgDesktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive))
            m_ownedDesktop = std::make_unique<KDeDesktop>();
        else if (xdgDesktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive))
            m_ownedDesktop = std::make_unique<GnomeDesktop>();
        else
            m_ownedDesktop = std::make_unique<GenericDesktop>();
        m_desktop = m_ownedDesktop.get();
    }

    if (injector) {
        m_injector = injector;
    } else {
        m_ownedInjector = std::make_unique<UinputInjector>();
        m_injector = m_ownedInjector.get();
    }

    m_actionExecutor.setInjector(m_injector);
}

void AppController::init()
{
    // Wire DeviceModel to DeviceManager
    m_deviceModel.setDeviceManager(&m_deviceManager);
    m_deviceModel.setDesktopIntegration(m_desktop);
    // Init uinput
    qCInfo(lcApp) << "creating UinputInjector...";
    qCInfo(lcApp) << "init uinput...";
    if (!m_injector->init()) {
        qCWarning(lcApp) << "UinputInjector: uinput init failed (no /dev/uinput access?). Keystrokes will not be injected.";
    }

    // Gesture defaults (matches logid.cfg) — use programmatic path, not user path
    QMap<QString, QPair<QString, QString>> defaultGestures;
    defaultGestures["down"]  = qMakePair(QStringLiteral("Show desktop"),          QStringLiteral("Super+D"));
    defaultGestures["left"]  = qMakePair(QStringLiteral("Switch desktop left"),   QStringLiteral("Ctrl+Super+Left"));
    defaultGestures["right"] = qMakePair(QStringLiteral("Switch desktop right"),  QStringLiteral("Ctrl+Super+Right"));
    defaultGestures["click"] = qMakePair(QStringLiteral("Task switcher"),         QStringLiteral("Super+W"));
    m_deviceModel.loadGesturesFromProfile(defaultGestures);

    wireSignals();
}

void AppController::startMonitoring()
{
    m_deviceManager.start();
    m_desktop->start();
}

// Signal wiring --------------------------------------------------------------

void AppController::wireSignals()
{
    // 0. User-initiated button change -> save + divert/undivert
    connect(&m_buttonModel, &ButtonModel::userActionChanged,
            this, &AppController::onUserButtonChanged);

    // 1. Window focus change -> apply profile to hardware + update UI
    connect(m_desktop, &IDesktopIntegration::activeWindowChanged,
            this, &AppController::onWindowFocusChanged);

    // 2. Tab click -> show cached profile in UI (no hardware writes)
    connect(&m_profileModel, &ProfileModel::profileSwitched,
            this, &AppController::onTabSwitched);

    // 3. Display profile changed -> update UI models
    connect(&m_profileEngine, &ProfileEngine::displayProfileChanged,
            this, &AppController::onDisplayProfileChanged);

    // 4. Device setup complete -> configure per-device profile dir, load/create default profile
    connect(&m_deviceManager, &DeviceManager::deviceSetupComplete,
            this, &AppController::onDeviceSetupComplete);

    // 5. Save gestures to profile when changed in UI (user-initiated only)
    connect(&m_deviceModel, &DeviceModel::userGestureChanged,
        [this](const QString &, const QString &, const QString &) {
            saveCurrentProfile();  // saves gesture assignments to displayed profile
        });

    // 6. Profile creation/removal from UI -> ProfileEngine (disk persistence)
    connect(&m_profileModel, &ProfileModel::profileAdded,
            &m_profileEngine, &ProfileEngine::createProfileForApp);
    connect(&m_profileModel, &ProfileModel::profileRemoved,
            &m_profileEngine, &ProfileEngine::removeAppProfile);

    // 7. Raw XY deltas for gesture accumulation
    connect(&m_deviceManager, &DeviceManager::gestureRawXY,
            this, &AppController::onGestureRawXY);

    // 8. Diverted button press -> gesture + action dispatch
    connect(&m_deviceManager, &DeviceManager::divertedButtonPressed,
            this, &AppController::onDivertedButtonPressed);

    // 9. Thumb wheel rotation -> mode action
    connect(&m_deviceManager, &DeviceManager::thumbWheelRotation,
            this, &AppController::onThumbWheelRotation);

    // 10. DeviceModel change requests -> update cache + save + conditionally write to hardware
    connect(&m_deviceModel, &DeviceModel::dpiChangeRequested,
            this, &AppController::onDpiChangeRequested);
    connect(&m_deviceModel, &DeviceModel::smartShiftChangeRequested,
            this, &AppController::onSmartShiftChangeRequested);
    connect(&m_deviceModel, &DeviceModel::scrollConfigChangeRequested,
            this, &AppController::onScrollConfigChangeRequested);
    connect(&m_deviceModel, &DeviceModel::thumbWheelModeChangeRequested,
            this, &AppController::onThumbWheelModeChangeRequested);
    connect(&m_deviceModel, &DeviceModel::thumbWheelInvertChangeRequested,
            this, &AppController::onThumbWheelInvertChangeRequested);
}

// Slot implementations -------------------------------------------------------

void AppController::onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType)
{
    Q_UNUSED(actionName)

    // Save to displayed profile cache
    saveCurrentProfile();

    // Apply to hardware only if editing the active profile
    if (m_profileEngine.displayProfile() != m_profileEngine.hardwareProfile())
        return;

    if (!m_currentDevice) return;
    const auto controls = m_currentDevice->controls();
    for (const auto &ctrl : controls) {
        if (ctrl.buttonIndex == buttonId) {
            if (ctrl.controlId == 0) return; // thumb wheel
            bool needsDivert = (actionType != "default");
            bool needsRawXY = (actionType == "gesture-trigger");
            m_deviceManager.divertButton(ctrl.controlId, needsDivert, needsRawXY);
            return;
        }
    }
}

void AppController::onTabSwitched(const QString &profileName)
{
    m_profileEngine.setDisplayProfile(profileName);
}

void AppController::onDisplayProfileChanged(const Profile &profile)
{
    m_deviceModel.setActiveProfileName(profile.name);

    // Push all profile values to DeviceModel so QML reads from profile, not hardware
    m_deviceModel.setDisplayValues(
        profile.dpi, profile.smartShiftEnabled, profile.smartShiftThreshold,
        profile.hiResScroll, profile.scrollDirection == "natural", profile.thumbWheelMode,
        profile.thumbWheelInvert);

    restoreButtonModelFromProfile(profile);

}

void AppController::onWindowFocusChanged(const QString &wmClass, const QString & /*title*/)
{
    qCDebug(lcFocus) << "focus:" << wmClass;
    // Ignore desktop shell components — they steal focus transiently
    // and shouldn't trigger profile switches
    static const QStringList kIgnored = {
        // KDE
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("kwin_wayland"),
        QStringLiteral("kwin_x11"),
        QStringLiteral("plasmashell"),
        QStringLiteral("org.kde.krunner"),
        // GNOME
        QStringLiteral("gnome-shell"),
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("org.gnome.Shell.Extensions"),
    };
    for (const auto &ig : kIgnored) {
        if (wmClass.compare(ig, Qt::CaseInsensitive) == 0)
            return;
    }

    m_deviceModel.setActiveWmClass(wmClass);

    QString profileName = m_profileEngine.profileForApp(wmClass);
    if (profileName == m_profileEngine.hardwareProfile())
        return;

    Profile &p = m_profileEngine.cachedProfile(profileName);
    m_profileEngine.setHardwareProfile(profileName);
    applyProfileToHardware(p);
    m_profileModel.setHwActiveByProfileName(profileName);
}

void AppController::onDeviceSetupComplete()
{
    m_currentDevice = m_deviceManager.activeDevice();

    const QString configBase =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString profilesDir = configBase
        + QStringLiteral("/devices/")
        + m_deviceManager.deviceSerial()
        + QStringLiteral("/profiles");

    QDir().mkpath(profilesDir);
    m_profileEngine.setDeviceConfigDir(profilesDir);
    qCDebug(lcApp) << "profile dir:" << profilesDir;

    const QString defaultConf = profilesDir + QStringLiteral("/default.conf");
    if (!QFile::exists(defaultConf)) {
        // First connect - seed default profile from current device state
        Profile seed;
        seed.name                = QStringLiteral("Default");
        seed.dpi                 = m_deviceManager.currentDPI();
        seed.smartShiftEnabled   = m_deviceManager.smartShiftEnabled();
        seed.smartShiftThreshold = m_deviceManager.smartShiftThreshold();
        seed.hiResScroll         = m_deviceManager.scrollHiRes();
        seed.scrollDirection     = m_deviceManager.scrollInvert()
            ? QStringLiteral("natural") : QStringLiteral("standard");
        seed.smoothScrolling     = !m_deviceManager.scrollRatchet();
        // Seed buttons from device descriptor defaults
        if (m_currentDevice) {
            const auto controls = m_currentDevice->controls();
            for (int i = 0; i < static_cast<int>(controls.size()) && i < static_cast<int>(seed.buttons.size()); ++i) {
                const auto &ctrl = controls[i];
                if (ctrl.defaultActionType == "gesture-trigger")
                    seed.buttons[i] = {ButtonAction::GestureTrigger, {}};
                else if (ctrl.defaultActionType == "smartshift-toggle")
                    seed.buttons[i] = {ButtonAction::SmartShiftToggle, {}};
            }
            // Seed gestures from device descriptor defaults
            const auto defaultGestures = m_currentDevice->defaultGestures();
            for (auto it = defaultGestures.begin(); it != defaultGestures.end(); ++it) {
                seed.gestures[it.key()] = it.value();
            }
        }
        ProfileEngine::saveProfile(defaultConf, seed);
        // Reload cache so the seed is available
        m_profileEngine.setDeviceConfigDir(profilesDir);
        qCDebug(lcApp) << "created default profile at" << defaultConf;
    }

    // Populate ProfileModel from saved app bindings
    const QString bindingsFile = profilesDir + QStringLiteral("/app-bindings.conf");
    if (QFile::exists(bindingsFile)) {
        const auto bindings = ProfileEngine::loadAppBindings(bindingsFile);

        // Build wmClass -> icon lookup from installed apps
        QMap<QString, QString> iconLookup;
        const auto apps = m_desktop->runningApplications();
        for (const auto &app : apps) {
            auto map = app.toMap();
            iconLookup[map[QStringLiteral("wmClass")].toString().toLower()]
                = map[QStringLiteral("icon")].toString();
        }

        for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
            QString icon = iconLookup.value(it.key().toLower());
            m_profileModel.restoreProfile(it.key(), it.value(), icon);
        }
    }

    // On first connect, start with default profile.
    // On wake/reconnect, re-apply whatever was active on hardware.
    QString hwName = m_profileEngine.hardwareProfile();
    bool isFirstConnect = hwName.isEmpty();

    if (isFirstConnect) {
        hwName = QStringLiteral("default");
        m_profileModel.setHwActiveIndex(0);
        m_profileEngine.setHardwareProfile(hwName);
        m_profileEngine.setDisplayProfile(hwName);
    }

    Profile &p = m_profileEngine.cachedProfile(hwName);
    qCDebug(lcApp) << "onDeviceSetupComplete: applying profile" << hwName
                    << "thumbWheelMode=" << p.thumbWheelMode;
    applyProfileToHardware(p);
}

void AppController::restoreButtonModelFromProfile(const Profile &p)
{
    if (!m_currentDevice) return;
    const auto controls = m_currentDevice->controls();

    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
        const auto &ctrl = controls[i];
        if (ctrl.controlId == 0) {
            // Thumb wheel virtual entry — label from profile mode
            static const QMap<QString, QString> kWheelNames = {
                {"scroll", "Horizontal scroll"}, {"zoom", "Zoom in/out"},
                {"volume", "Volume control"}, {"none", "No action"}
            };
            QString wheelName = kWheelNames.value(p.thumbWheelMode, p.thumbWheelMode);
            QString wheelType = (p.thumbWheelMode == "scroll") ? "default" : "wheel-mode";
            buttons.append({wheelName, wheelType});
            continue;
        }
        const auto &ba = (static_cast<std::size_t>(i) < p.buttons.size())
            ? p.buttons[static_cast<std::size_t>(i)]
            : ButtonAction{ButtonAction::Default, {}};

        QString aType, aName;
        switch (ba.type) {
        case ButtonAction::Default:
            aType = QStringLiteral("default");
            aName = ctrl.defaultName;
            break;
        case ButtonAction::GestureTrigger:
            aType = QStringLiteral("gesture-trigger");
            aName = QStringLiteral("Gestures");
            break;
        case ButtonAction::SmartShiftToggle:
            aType = QStringLiteral("smartshift-toggle");
            aName = QStringLiteral("Shift wheel mode");
            break;
        case ButtonAction::Keystroke:
            aType = QStringLiteral("keystroke");
            aName = buttonActionToName(ba);
            break;
        case ButtonAction::AppLaunch:
            aType = QStringLiteral("app-launch");
            aName = buttonActionToName(ba);
            break;
        case ButtonAction::Media: {
            aType = QStringLiteral("media-controls");
            static const QHash<QString, QString> kMediaNames = {
                {"Play", "Play/Pause"}, {"Next", "Next track"},
                {"Previous", "Previous track"}, {"Stop", "Stop"},
                {"Mute", "Mute"}, {"VolumeUp", "Volume up"},
                {"VolumeDown", "Volume down"},
            };
            aName = kMediaNames.value(ba.payload, ba.payload);
            break;
        }
        default:
            aType = QStringLiteral("default");
            aName = ctrl.defaultName;
            break;
        }
        buttons.append({aName, aType});
    }

    m_buttonModel.loadFromProfile(buttons);

    // Restore gesture actions from profile (programmatic — no save trigger)
    QMap<QString, QPair<QString, QString>> gestureMap;
    for (auto it = p.gestures.begin(); it != p.gestures.end(); ++it) {
        if (it->second.type == ButtonAction::Keystroke && !it->second.payload.isEmpty()) {
            QString name = it->second.payload;
            // Reverse lookup display name from ActionModel
            int count = m_actionModel.rowCount();
            for (int j = 0; j < count; ++j) {
                QModelIndex mi = m_actionModel.index(j);
                if (m_actionModel.data(mi, ActionModel::PayloadRole).toString() == it->second.payload) {
                    name = m_actionModel.data(mi, ActionModel::NameRole).toString();
                    break;
                }
            }
            gestureMap[it->first] = qMakePair(name, it->second.payload);
        }
    }
    m_deviceModel.loadGesturesFromProfile(gestureMap);
}

void AppController::applyProfileToHardware(const Profile &p)
{
    if (!m_currentDevice) return;
    // Discard any pending commands from a previous profile switch so they
    // don't interleave with (or delay) commands for the new profile.
    m_deviceManager.flushCommandQueue();
    // Reset thumb wheel accumulator to prevent stale rotation from the
    // previous profile's mode from bleeding into the new mode's actions.
    m_thumbAccum = 0;
    // Refresh response timestamp so HID++ responses from these writes
    // don't trigger a false sleep/wake re-enumeration.
    m_deviceManager.touchResponseTime();
    const auto controls = m_currentDevice->controls();
    const int buttonCount = controls.size();

    // Apply DPI, SmartShift, scroll, thumb wheel FIRST — these are the most
    // user-visible settings and must survive rapid focus switches.  Button
    // diversions are queued after; they're less time-critical.
    m_deviceManager.setDPI(p.dpi);
    m_deviceManager.setSmartShift(p.smartShiftEnabled, p.smartShiftThreshold);
    m_deviceManager.setScrollConfig(p.hiResScroll,
                                    p.scrollDirection == QStringLiteral("natural"));
    m_deviceManager.setThumbWheelMode(p.thumbWheelMode, p.thumbWheelInvert);

    // Apply button diversions to hardware via HID++
    for (int i = 0; i < buttonCount; ++i) {
        const auto &ctrl = controls[i];
        if (ctrl.controlId == 0) continue; // virtual entry (thumb wheel) -- no ReprogControls divert
        if (!ctrl.configurable) continue;  // non-divertable (e.g. left/right click)
        const auto &ba = (static_cast<std::size_t>(i) < p.buttons.size())
            ? p.buttons[static_cast<std::size_t>(i)]
            : ButtonAction{ButtonAction::Default, {}};
        bool needsDivert = (ba.type != ButtonAction::Default);
        bool needsRawXY = (ba.type == ButtonAction::GestureTrigger);
        if (needsDivert)
            qCDebug(lcApp) << "diverting button" << i << "CID" << Qt::hex << ctrl.controlId;
        m_deviceManager.divertButton(ctrl.controlId, needsDivert, needsRawXY);
    }
}

void AppController::saveCurrentProfile()
{
    // Save buttons + gestures from UI models to the displayed profile
    QString name = m_profileEngine.displayProfile();
    if (name.isEmpty()) return;

    Profile &p = m_profileEngine.cachedProfile(name);
    if (p.name.isEmpty()) p.name = name;

    // Save buttons (skip thumb wheel virtual entry, stored as thumbWheelMode)
    if (m_currentDevice) {
        const auto controls = m_currentDevice->controls();
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (controls[i].controlId == 0) continue;
            if (static_cast<std::size_t>(i) < p.buttons.size())
                p.buttons[static_cast<std::size_t>(i)] = buttonEntryToAction(
                    m_buttonModel.actionTypeForButton(i),
                    m_buttonModel.actionNameForButton(i));
        }
    }

    // Save gestures
    for (const auto &dir : {"up", "down", "left", "right", "click"}) {
        QString ks = m_deviceModel.gestureKeystroke(dir);
        if (!ks.isEmpty())
            p.gestures[dir] = {ButtonAction::Keystroke, ks};
    }

    m_profileEngine.saveProfileToDisk(name);
}

void AppController::pushDisplayValues(const Profile &p)
{
    m_deviceModel.setDisplayValues(
        p.dpi, p.smartShiftEnabled, p.smartShiftThreshold,
        p.hiResScroll, p.scrollDirection == "natural", p.thumbWheelMode,
        p.thumbWheelInvert);
}

void AppController::onDpiChangeRequested(int value)
{
    QString name = m_profileEngine.displayProfile();
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(name);
    p.dpi = value;
    m_profileEngine.saveProfileToDisk(name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile())
        m_deviceManager.setDPI(value);
}

void AppController::onSmartShiftChangeRequested(bool enabled, int threshold)
{
    QString name = m_profileEngine.displayProfile();
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(name);
    p.smartShiftEnabled = enabled;
    p.smartShiftThreshold = threshold;
    m_profileEngine.saveProfileToDisk(name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile())
        m_deviceManager.setSmartShift(enabled, threshold);
}

void AppController::onScrollConfigChangeRequested(bool hiRes, bool invert)
{
    QString name = m_profileEngine.displayProfile();
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(name);
    p.hiResScroll = hiRes;
    p.scrollDirection = invert ? QStringLiteral("natural") : QStringLiteral("standard");
    m_profileEngine.saveProfileToDisk(name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile())
        m_deviceManager.setScrollConfig(hiRes, invert);
}

void AppController::onThumbWheelModeChangeRequested(const QString &mode)
{
    m_thumbAccum = 0;  // reset accumulator on mode change
    QString name = m_profileEngine.displayProfile();
    qCDebug(lcApp) << "thumbWheelMode requested:" << mode << "for profile:" << name;
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(name);
    p.thumbWheelMode = mode;
    m_profileEngine.saveProfileToDisk(name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile())
        m_deviceManager.setThumbWheelMode(mode, p.thumbWheelInvert);
}

void AppController::onThumbWheelInvertChangeRequested(bool invert)
{
    QString name = m_profileEngine.displayProfile();
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(name);
    p.thumbWheelInvert = invert;
    m_profileEngine.saveProfileToDisk(name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile())
        m_deviceManager.setThumbWheelMode(p.thumbWheelMode, invert);
}

void AppController::onGestureRawXY(int16_t dx, int16_t dy)
{
    if (m_gestureActive) {
        m_gestureTotalDx += dx;
        m_gestureTotalDy += dy;
    }
}

void AppController::onDivertedButtonPressed(uint16_t controlId, bool pressed)
{
    // Read actions from the hardware profile (not ButtonModel, which holds the displayed profile)
    const Profile &hwProfile = m_profileEngine.cachedProfile(m_profileEngine.hardwareProfile());

    // Resolve gesture on all-released (controlId=0) or on the gesture button's own CID.
    // Don't resolve on other buttons being released while gesture is held.
    if (!pressed && m_gestureActive
        && (controlId == 0 || controlId == m_gestureControlId)) {
        m_gestureActive = false;
        int dx = m_gestureTotalDx;
        int dy = m_gestureTotalDy;

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
            m_actionExecutor.injectKeystroke(it->second.payload);
        }
        return;
    }

    if (!pressed) return; // ignore other release events

    // Look up button index from device descriptor
    if (!m_currentDevice) return;
    int idx = -1;
    for (const auto &ctrl : m_currentDevice->controls()) {
        if (ctrl.controlId == controlId) {
            idx = ctrl.buttonIndex;
            break;
        }
    }
    if (idx < 0) return;

    // Read action from hardware profile cache
    const auto &ba = (static_cast<std::size_t>(idx) < hwProfile.buttons.size())
        ? hwProfile.buttons[static_cast<std::size_t>(idx)]
        : ButtonAction{ButtonAction::Default, {}};

    qCDebug(lcApp) << "button" << idx << "action type=" << ba.type << "payload=" << ba.payload;

    if (ba.type == ButtonAction::Default) return;

    if (ba.type == ButtonAction::SmartShiftToggle) {
        bool current = m_deviceManager.smartShiftEnabled();
        m_deviceManager.setSmartShift(!current, m_deviceManager.smartShiftThreshold());
    } else if ((ba.type == ButtonAction::Keystroke || ba.type == ButtonAction::Media)
               && !ba.payload.isEmpty()) {
        m_actionExecutor.injectKeystroke(ba.payload);
    } else if (ba.type == ButtonAction::GestureTrigger) {
        m_gestureTotalDx = 0;
        m_gestureTotalDy = 0;
        m_gestureActive = true;
        m_gestureControlId = controlId;
    } else if (ba.type == ButtonAction::AppLaunch && !ba.payload.isEmpty()) {
        m_actionExecutor.launchApp(ba.payload);
    }
}

void AppController::onThumbWheelRotation(int delta)
{
    const QString &mode = m_deviceManager.thumbWheelMode();
    // Normalize: multiply by defaultDirection so clockwise = positive.
    // defaultDirection is -1 on MX Master 3S (positive when left), so this
    // flips the sign to make clockwise = positive.
    int normalized = delta * m_deviceManager.thumbWheelDefaultDirection();
    qCDebug(lcInput) << "thumbWheel raw=" << delta << "normalized=" << normalized
                      << "mode=" << mode << "invert=" << m_deviceManager.thumbWheelInvert();
    m_thumbAccum += normalized;

    if (std::abs(m_thumbAccum) < kThumbThreshold)
        return; // not enough rotation yet

    int steps = m_thumbAccum / kThumbThreshold;
    m_thumbAccum %= kThumbThreshold;

    qCDebug(lcInput) << "thumbWheel steps=" << steps << "accum=" << m_thumbAccum;
    for (int i = 0; i < std::abs(steps); ++i) {
        if (mode == "scroll") {
            int dir = steps > 0 ? 1 : -1;
            qCDebug(lcInput) << "thumbWheel action: HScroll" << dir;
            m_actionExecutor.injectHorizontalScroll(dir);
        } else if (mode == "volume") {
            QString key = steps > 0 ? "VolumeUp" : "VolumeDown";
            qCDebug(lcInput) << "thumbWheel action:" << key;
            m_actionExecutor.injectKeystroke(key);
        } else if (mode == "zoom") {
            int dir = steps > 0 ? 1 : -1;
            qCDebug(lcInput) << "thumbWheel action: CtrlScroll" << dir;
            m_actionExecutor.injectCtrlScroll(dir);
        }
    }
}

// Helper implementations -----------------------------------------------------

QString AppController::buttonActionToName(const ButtonAction &ba) const
{
    if (ba.type == ButtonAction::Default)
        return QString();  // caller uses buttonName default
    if (ba.type == ButtonAction::GestureTrigger)
        return QStringLiteral("Gestures");
    if (ba.type == ButtonAction::Keystroke) {
        // Reverse-map payload -> display name via ActionModel
        int count = m_actionModel.rowCount();
        for (int i = 0; i < count; ++i) {
            QModelIndex mi = m_actionModel.index(i);
            if (m_actionModel.data(mi, ActionModel::ActionTypeRole).toString() == QStringLiteral("keystroke") &&
                m_actionModel.data(mi, ActionModel::PayloadRole).toString() == ba.payload) {
                return m_actionModel.data(mi, ActionModel::NameRole).toString();
            }
        }
        return ba.payload; // fallback: use payload directly
    }
    return ba.payload;
}

ButtonAction AppController::buttonEntryToAction(const QString &actionType, const QString &actionName) const
{
    if (actionType == QStringLiteral("default"))
        return {ButtonAction::Default, {}};
    if (actionType == QStringLiteral("gesture-trigger"))
        return {ButtonAction::GestureTrigger, {}};
    if (actionType == QStringLiteral("smartshift-toggle"))
        return {ButtonAction::SmartShiftToggle, {}};
    if (actionType == QStringLiteral("media-controls")) {
        // actionName is the friendly name; map to media key code
        static const QHash<QString, QString> mediaKeys = {
            {"Play/Pause",     "Play"},
            {"Next track",     "Next"},
            {"Previous track", "Previous"},
            {"Stop",           "Stop"},
            {"Mute",           "Mute"},
            {"Volume up",      "VolumeUp"},
            {"Volume down",    "VolumeDown"},
        };
        return {ButtonAction::Media, mediaKeys.value(actionName, "Play")};
    }
    if (actionType == QStringLiteral("keystroke")) {
        QString payload = m_actionModel.payloadForName(actionName);
        if (payload.isEmpty() && actionName != QStringLiteral("Keyboard shortcut"))
            payload = actionName; // actionName might already be a keystroke
        return {ButtonAction::Keystroke, payload};
    }
    if (actionType == QStringLiteral("app-launch")) {
        QString payload = m_actionModel.payloadForName(actionName);
        if (payload.isEmpty()) payload = actionName;
        return {ButtonAction::AppLaunch, payload};
    }
    return {ButtonAction::Default, {}};
}

} // namespace logitune
