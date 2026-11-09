#include "ProfileOrchestrator.h"
#include "ActionExecutor.h"
#include "ActiveDeviceResolver.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "ProfileEngine.h"
#include "interfaces/IDesktopIntegration.h"
#include "interfaces/IDevice.h"
#include "logging/LogManager.h"
#include "models/ActionModel.h"
#include "models/ButtonModel.h"
#include "models/DeviceModel.h"
#include "models/ProfileModel.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QVariantMap>

namespace logitune {

ProfileOrchestrator::ProfileOrchestrator(ProfileEngine *profileEngine,
                                         ActionExecutor *actionExecutor,
                                         ActiveDeviceResolver *selection,
                                         DeviceModel *deviceModel,
                                         ButtonModel *buttonModel,
                                         ActionModel *actionModel,
                                         ProfileModel *profileModel,
                                         IDesktopIntegration *desktop,
                                         QObject *parent)
    : QObject(parent)
    , m_profileEngine(profileEngine)
    , m_actionExecutor(actionExecutor)
    , m_selection(selection)
    , m_deviceModel(deviceModel)
    , m_buttonModel(buttonModel)
    , m_actionModel(actionModel)
    , m_profileModel(profileModel)
    , m_desktop(desktop)
{}

QString ProfileOrchestrator::activeSerial() const
{
    return m_selection ? m_selection->activeSerial() : QString();
}

void ProfileOrchestrator::onCurrentDeviceChanged(const IDevice *device)
{
    m_currentDevice = device;
}

void ProfileOrchestrator::onTransportSetupComplete(PhysicalDevice *device)
{
    if (!device) return;
    // Do not touch m_currentDevice here: it tracks the mouse shown in the
    // UI, and the mouse that just reconnected need not be that one.
    const QString serial = device->deviceSerial();
    const QString hwName = m_profileEngine->hardwareProfile(serial);
    Profile &p = m_profileEngine->cachedProfile(serial, hwName);
    qCDebug(lcApp) << "device transport ready, applying profile:" << hwName
                   << "to" << serial;
    applyProfileToHardware(device, p);
}

void ProfileOrchestrator::setupProfileForDevice(PhysicalDevice *device)
{
    if (!device) return;
    const IDevice *descriptor = device->descriptor();
    const bool isSelected = m_selection && m_selection->activeDevice() == device;

    const QString serial = device->deviceSerial();
    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    const QString profilesDir = configBase
        + QStringLiteral("/devices/") + serial
        + QStringLiteral("/profiles");

    QDir().mkpath(profilesDir);
    m_profileEngine->registerDevice(serial, profilesDir);
    qCDebug(lcApp) << "profile dir:" << profilesDir;

    const QString defaultConf = profilesDir + QStringLiteral("/default.conf");
    if (!QFile::exists(defaultConf)) {
        Profile seed;
        seed.name                = QStringLiteral("Default");
        seed.dpi                 = device->currentDPI();
        seed.smartShiftEnabled   = device->smartShiftEnabled();
        seed.smartShiftThreshold = device->smartShiftThreshold();
        seed.hiResScroll         = device->scrollHiRes();
        seed.scrollDirection     = device->scrollInvert()
            ? QStringLiteral("natural") : QStringLiteral("standard");
        seed.smoothScrolling     = !device->scrollRatchet();
        if (descriptor) {
            const auto controls = descriptor->controls();
            for (int i = 0;
                 i < static_cast<int>(controls.size()) &&
                 i < static_cast<int>(seed.buttons.size()); ++i) {
                const auto &ctrl = controls[i];
                if (ctrl.defaultActionType == "gesture-trigger")
                    seed.buttons[i] = {ButtonAction::GestureTrigger, {}};
                else if (ctrl.defaultActionType == "smartshift-toggle")
                    seed.buttons[i] = {ButtonAction::SmartShiftToggle, {}};
                else if (ctrl.defaultActionType == "dpi-cycle")
                    seed.buttons[i] = {ButtonAction::DpiCycle, {}};
            }
            const auto defaultGestures = descriptor->defaultGestures();
            for (auto it = defaultGestures.begin();
                 it != defaultGestures.end(); ++it) {
                seed.gestures[it.key()] = it.value();
            }
        }
        ProfileEngine::saveProfile(defaultConf, seed);
        m_profileEngine->registerDevice(serial, profilesDir);  // reload cache after seeding
        qCDebug(lcApp) << "created default profile at" << defaultConf;
    }

    QString hwName = m_profileEngine->hardwareProfile(serial);
    bool isFirstConnect = hwName.isEmpty();
    if (isFirstConnect) {
        hwName = QStringLiteral("default");
        m_profileEngine->setHardwareProfile(serial, hwName);
        m_profileEngine->setDisplayProfile(serial, hwName);
    }

    // The tab bar shows the selected mouse's app profiles only. A second
    // mouse connecting must not add its tabs to them; its own tabs appear
    // when the carousel selects it (AppRoot::onSelectionChanged).
    if (isSelected)
        rebuildProfileTabs(serial);

    Profile &p = m_profileEngine->cachedProfile(serial, hwName);
    qCDebug(lcApp) << "setupProfileForDevice: applying profile" << hwName
                   << "thumbWheelMode=" << p.thumbWheelMode << "to" << serial;
    applyProfileToHardware(device, p);
}

void ProfileOrchestrator::rebuildProfileTabs(const QString &serial)
{
    QMap<QString, QString> iconLookup;
    if (m_desktop) {
        const auto apps = m_desktop->runningApplications();
        for (const auto &app : apps) {
            const auto map = app.toMap();
            iconLookup[map[QStringLiteral("wmClass")].toString().toLower()]
                = map[QStringLiteral("icon")].toString();
        }
    }

    QList<ProfileModel::AppTab> tabs;
    const auto bindings = m_profileEngine->appBindings(serial);
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it)
        tabs.append({it.key(), it.value(), iconLookup.value(it.key().toLower())});

    m_profileModel->resetProfiles(tabs,
                                  m_profileEngine->displayProfile(serial),
                                  m_profileEngine->hardwareProfile(serial));
}

void ProfileOrchestrator::onUserButtonChanged(int buttonId,
                                              const QString &actionName,
                                              const QString &actionType)
{
    Q_UNUSED(actionName)

    saveCurrentProfile();

    const QString serial = activeSerial();
    if (serial.isEmpty()) return;
    if (m_profileEngine->displayProfile(serial) != m_profileEngine->hardwareProfile(serial))
        return;

    if (!m_currentDevice) return;
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;

    const auto controls = m_currentDevice->controls();
    for (const auto &ctrl : controls) {
        if (ctrl.buttonIndex == buttonId) {
            if (ctrl.controlId == 0) return;
            bool needsDivert = (actionType != "default");
            bool needsRawXY = (actionType == "gesture-trigger");
            session->divertButton(ctrl.controlId, needsDivert, needsRawXY);
            return;
        }
    }
}

void ProfileOrchestrator::onDpiChangedByButton(const QString &serial, int dpi)
{
    applyHardwareChange(serial, [dpi](Profile &p) { p.dpi = dpi; });
}

void ProfileOrchestrator::onSmartShiftChangedByButton(const QString &serial,
                                                      bool enabled, int threshold)
{
    applyHardwareChange(serial, [enabled, threshold](Profile &p) {
        p.smartShiftEnabled   = enabled;
        p.smartShiftThreshold = threshold;
    });
}

void ProfileOrchestrator::onTabSwitched(const QString &profileName)
{
    const QString serial = activeSerial();
    if (serial.isEmpty()) return;
    m_profileEngine->setDisplayProfile(serial, profileName);
}

void ProfileOrchestrator::onDisplayProfileChanged(const QString &serial, const Profile &profile)
{
    if (serial != activeSerial())
        return;

    m_deviceModel->setActiveProfileName(profile.name);

    m_deviceModel->setDisplayValues(
        profile.dpi, profile.smartShiftEnabled, profile.smartShiftThreshold,
        profile.hiResScroll, profile.scrollDirection == "natural",
        profile.thumbWheelMode, profile.thumbWheelInvert);

    restoreButtonModelFromProfile(profile);
}

void ProfileOrchestrator::onWindowFocusChanged(const QString &wmClass, const QString & /*title*/)
{
    qCDebug(lcFocus) << "focus:" << wmClass;
    static const QStringList kIgnored = {
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("kwin_wayland"),
        QStringLiteral("kwin_x11"),
        QStringLiteral("plasmashell"),
        QStringLiteral("org.kde.krunner"),
        QStringLiteral("gnome-shell"),
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("org.gnome.Shell.Extensions"),
    };
    for (const auto &ig : kIgnored) {
        if (wmClass.compare(ig, Qt::CaseInsensitive) == 0)
            return;
    }

    m_deviceModel->setActiveWmClass(wmClass);

    // Every connected mouse follows focus with its own app profiles, not
    // just the one shown in the UI.
    const QString selectedSerial = activeSerial();
    const QList<PhysicalDevice *> devices = m_deviceModel->devices();
    for (PhysicalDevice *device : devices) {
        const QString serial = device->deviceSerial();
        const QString profileName = m_profileEngine->profileForApp(serial, wmClass);
        if (profileName == m_profileEngine->hardwareProfile(serial))
            continue;

        Profile &p = m_profileEngine->cachedProfile(serial, profileName);
        m_profileEngine->setHardwareProfile(serial, profileName);
        applyProfileToHardware(device, p);
        if (serial == selectedSerial)
            m_profileModel->setHwActiveByProfileName(profileName);
    }
}

void ProfileOrchestrator::restoreButtonModelFromProfile(const Profile &p)
{
    if (!m_currentDevice) return;
    const auto controls = m_currentDevice->controls();

    QList<ButtonAssignment> assignments;
    for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
        const auto &ctrl = controls[i];
        if (ctrl.controlId == 0) {
            static const QMap<QString, QString> kWheelNames = {
                {"scroll", "Horizontal scroll"}, {"zoom", "Zoom in/out"},
                {"volume", "Volume control"}, {"none", "No action"}
            };
            QString wheelName = kWheelNames.value(p.thumbWheelMode, p.thumbWheelMode);
            QString wheelType = (p.thumbWheelMode == "scroll") ? "default" : "wheel-mode";
            assignments.append({wheelName, wheelType, ctrl.controlId});
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
        case ButtonAction::DpiCycle:
            aType = QStringLiteral("dpi-cycle");
            aName = QStringLiteral("DPI cycle");
            break;
        case ButtonAction::Keystroke:
            aType = QStringLiteral("keystroke");
            aName = m_actionModel->buttonActionToName(ba);
            break;
        case ButtonAction::AppLaunch:
            aType = QStringLiteral("app-launch");
            aName = m_actionModel->buttonActionToName(ba);
            break;
        case ButtonAction::PresetRef:
            aType = QStringLiteral("preset");
            aName = m_actionModel->buttonActionToName(ba);
            break;
        case ButtonAction::Media: {
            // Legacy ButtonAction::Media (from older serialized profiles
            // using the "media:..." form) surfaces in the UI as a regular
            // keystroke entry; the dispatcher still injects the keystroke
            // payload through the same code path.
            aType = QStringLiteral("keystroke");
            ButtonAction asKs{ButtonAction::Keystroke, ba.payload};
            aName = m_actionModel->buttonActionToName(asKs);
            break;
        }
        default:
            aType = QStringLiteral("default");
            aName = ctrl.defaultName;
            break;
        }
        assignments.append({aName, aType, ctrl.controlId});
    }

    m_buttonModel->loadFromProfile(assignments);

    // Translate the profile's per-direction ButtonAction map into the
    // GestureEntry map the DeviceModel exposes to QML. The display name is
    // resolved via ActionModel::buttonActionToName so it works for both
    // PresetRef ("show-desktop" -> "Show desktop") and Keystroke
    // ("Super+D" -> "Show desktop") payloads.
    QMap<QString, GestureEntry> gestureMap;
    for (auto it = p.gestures.begin(); it != p.gestures.end(); ++it) {
        if (it->second.type == ButtonAction::Default)
            continue;
        QString name = m_actionModel->buttonActionToName(it->second);
        gestureMap[it->first] = GestureEntry{name, it->second};
    }
    m_deviceModel->loadGesturesFromProfile(gestureMap);
}

void ProfileOrchestrator::applyProfileToHardware(PhysicalDevice *device, const Profile &p)
{
    DeviceSession *session = device ? device->primary() : nullptr;
    const IDevice *descriptor = device ? device->descriptor() : nullptr;
    if (!session || !descriptor) {
        // Never bail silently: a skipped apply leaves the mouse in its
        // firmware defaults, and without this line the log looks healthy.
        qCDebug(lcApp) << "cannot apply profile" << p.name << "to"
                       << (device ? device->deviceSerial() : QStringLiteral("<none>"))
                       << "- no transport or descriptor";
        return;
    }

    session->flushCommandProcessor();

    session->touchResponseTime();
    const auto controls = descriptor->controls();
    const int buttonCount = controls.size();

    session->setDPI(p.dpi);
    session->setSmartShift(p.smartShiftEnabled, p.smartShiftThreshold);
    session->setScrollConfig(p.hiResScroll,
                             p.scrollDirection == QStringLiteral("natural"));
    session->setThumbWheelMode(p.thumbWheelMode, p.thumbWheelInvert);

    for (int i = 0; i < buttonCount; ++i) {
        const auto &ctrl = controls[i];
        if (ctrl.controlId == 0) continue;
        if (!ctrl.configurable) continue;
        const auto &ba = (static_cast<std::size_t>(i) < p.buttons.size())
            ? p.buttons[static_cast<std::size_t>(i)]
            : ButtonAction{ButtonAction::Default, {}};
        bool needsDivert = (ba.type != ButtonAction::Default);
        bool needsRawXY = (ba.type == ButtonAction::GestureTrigger);
        if (needsDivert)
            qCDebug(lcApp) << "diverting button" << i << "CID" << Qt::hex << ctrl.controlId;
        session->divertButton(ctrl.controlId, needsDivert, needsRawXY);
    }

    emit profileApplied(device->deviceSerial());
}

void ProfileOrchestrator::saveCurrentProfile()
{
    const QString serial = activeSerial();
    if (serial.isEmpty()) return;

    const QString name = m_profileEngine->displayProfile(serial);
    if (name.isEmpty()) return;

    Profile &p = m_profileEngine->cachedProfile(serial, name);
    if (p.name.isEmpty()) p.name = name;

    if (m_currentDevice) {
        const auto controls = m_currentDevice->controls();
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (controls[i].controlId == 0) continue;
            if (static_cast<std::size_t>(i) < p.buttons.size())
                p.buttons[static_cast<std::size_t>(i)] = m_actionModel->buttonEntryToAction(
                    m_buttonModel->actionTypeForButton(i),
                    m_buttonModel->actionNameForButton(i));
        }
    }

    for (const auto &dir : {"up", "down", "left", "right", "click"}) {
        const auto ba = m_deviceModel->gestureAction(dir);
        if (ba.type != ButtonAction::Default)
            p.gestures[dir] = ba;
        else
            p.gestures.erase(dir);
    }

    m_profileEngine->saveProfileToDisk(serial, name);
}

void ProfileOrchestrator::pushDisplayValues(const Profile &p)
{
    m_deviceModel->setDisplayValues(
        p.dpi, p.smartShiftEnabled, p.smartShiftThreshold,
        p.hiResScroll, p.scrollDirection == "natural", p.thumbWheelMode,
        p.thumbWheelInvert);
}

} // namespace logitune
