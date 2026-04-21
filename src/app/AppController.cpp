#include "AppController.h"
#include "devices/JsonDevice.h"
#include "models/EditorModel.h"
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

    m_actionFilterModel = std::make_unique<ActionFilterModel>(&m_deviceModel, this);
    m_actionFilterModel->setSourceModel(&m_actionModel);
}

AppController::~AppController() = default;

void AppController::init()
{
    m_deviceModel.setDesktopIntegration(m_desktop);
    qCInfo(lcApp) << "creating UinputInjector...";
    qCInfo(lcApp) << "init uinput...";
    if (!m_injector->init()) {
        qCWarning(lcApp) << "UinputInjector: uinput init failed (no /dev/uinput access?). Keystrokes will not be injected.";
    }

    QMap<QString, QPair<QString, QString>> defaultGestures;
    defaultGestures["down"]  = qMakePair(QStringLiteral("Show desktop"),          QStringLiteral("Super+D"));
    defaultGestures["left"]  = qMakePair(QStringLiteral("Switch desktop left"),   QStringLiteral("Ctrl+Super+Left"));
    defaultGestures["right"] = qMakePair(QStringLiteral("Switch desktop right"),  QStringLiteral("Ctrl+Super+Right"));
    defaultGestures["click"] = qMakePair(QStringLiteral("Task switcher"),         QStringLiteral("Super+W"));
    m_deviceModel.loadGesturesFromProfile(defaultGestures);

    wireSignals();
}

void AppController::startMonitoring(bool simulateAll, bool editMode)
{
    if (editMode) {
        m_editorModel = std::make_unique<EditorModel>(&m_registry, &m_deviceModel, true, this);
        qCInfo(lcApp) << "--edit: editor mode active";

        auto syncActive = [this]() {
            if (!m_editorModel)
                return;
            const IDevice *dev = m_deviceModel.activeDevice();
            if (auto *jd = dynamic_cast<const JsonDevice *>(dev))
                m_editorModel->setActiveDevicePath(jd->sourcePath());
            else
                m_editorModel->setActiveDevicePath(QString());
        };
        connect(&m_deviceModel, &DeviceModel::selectedChanged, this, syncActive);
        syncActive();
    }
    if (simulateAll) {
        // --simulate-all: populate the carousel with one fake session per
        // descriptor currently loaded in DeviceRegistry instead of scanning
        // udev for real hardware. Useful for visually walking through every
        // community descriptor at once without owning the physical mice.
        qCInfo(lcApp) << "--simulate-all: seeding carousel from registry";
        m_deviceManager.simulateAllFromRegistry();
        m_desktop->start();
        return;
    }

    m_deviceManager.start();
    m_desktop->start();
    m_deviceFetcher.fetchManifest();
}

// Signal wiring --------------------------------------------------------------

void AppController::wireSignals()
{
    connect(&m_buttonModel, &ButtonModel::userActionChanged,
            this, &AppController::onUserButtonChanged);

    connect(m_desktop, &IDesktopIntegration::activeWindowChanged,
            this, &AppController::onWindowFocusChanged);

    connect(&m_profileModel, &ProfileModel::profileSwitched,
            this, &AppController::onTabSwitched);

    connect(&m_profileEngine, &ProfileEngine::deviceDisplayProfileChanged,
            this, &AppController::onDisplayProfileChanged);

    connect(&m_deviceModel, &DeviceModel::selectedChanged,
            this, &AppController::onSelectedDeviceChanged);

    // Physical device lifecycle — one per unique serial, survives transport
    // switches. Per-transport events are routed through PhysicalDevice
    // (which fans in signals from all its transports).
    connect(&m_deviceManager, &DeviceManager::physicalDeviceAdded,
            this, &AppController::onPhysicalDeviceAdded);
    connect(&m_deviceManager, &DeviceManager::physicalDeviceRemoved,
            this, &AppController::onPhysicalDeviceRemoved);

    connect(&m_deviceModel, &DeviceModel::userGestureChanged,
        [this](const QString &, const QString &, const QString &) {
            saveCurrentProfile();
        });

    connect(&m_profileModel, &ProfileModel::profileAdded, this,
            [this](const QString &wmClass, const QString &profileName) {
        const QString serial = selectedSerial();
        if (!serial.isEmpty())
            m_profileEngine.createProfileForApp(serial, wmClass, profileName);
    });
    connect(&m_profileModel, &ProfileModel::profileRemoved, this,
            [this](const QString &wmClass) {
        const QString serial = selectedSerial();
        if (!serial.isEmpty())
            m_profileEngine.removeAppProfile(serial, wmClass);
    });

    connect(&m_deviceFetcher, &DeviceFetcher::descriptorsUpdated,
            this, [this]() {
        m_registry.reloadAll();
    });

    connect(&m_deviceManager, &DeviceManager::unknownDeviceDetected,
            &m_deviceFetcher, &DeviceFetcher::fetchForPid);

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

// Physical device lifecycle -------------------------------------------------

void AppController::onPhysicalDeviceAdded(PhysicalDevice *device)
{
    if (!device) return;

    const QString deviceId = device->deviceSerial();
    const bool wasEmpty = m_deviceModel.count() == 0;
    m_deviceModel.addPhysicalDevice(device);

    // Route per-device input events. PhysicalDevice fans in signals from
    // all its underlying transports, so we connect once here regardless of
    // how many hidraws back it.
    connect(device, &PhysicalDevice::gestureRawXY, this,
            [this, deviceId](int16_t dx, int16_t dy) {
        auto &state = m_perDeviceState[deviceId];
        if (state.gestureActive) {
            state.gestureAccumX += dx;
            state.gestureAccumY += dy;
        }
    });
    connect(device, &PhysicalDevice::divertedButtonPressed, this,
            [this](uint16_t controlId, bool pressed) {
        onDivertedButtonPressed(controlId, pressed);
    });
    connect(device, &PhysicalDevice::thumbWheelRotation, this,
            [this](int delta) {
        onThumbWheelRotation(delta);
    });

    // Re-apply profile whenever any transport finishes (re-)enumeration.
    // PhysicalDevice emits this from its setupComplete fan-in, which
    // covers both first-time attach and post-reconnect re-enumerate.
    connect(device, &PhysicalDevice::transportSetupComplete, this,
            [this, device]() {
        m_currentDevice = device->descriptor();
        const QString serial = device->deviceSerial();
        Profile &p = m_profileEngine.cachedProfile(serial,
                                                   m_profileEngine.hardwareProfile(serial));
        qCDebug(lcApp) << "device transport ready, applying profile:"
                        << m_profileEngine.hardwareProfile(serial);
        applyProfileToHardware(p);
    });

    if (wasEmpty && m_deviceModel.count() == 1)
        m_deviceModel.setSelectedIndex(0);

    setupProfileForDevice(device);
}

void AppController::onPhysicalDeviceRemoved(PhysicalDevice *device)
{
    const QString deviceId = device ? device->deviceSerial() : QString();
    m_deviceModel.removePhysicalDevice(device);
    if (!deviceId.isEmpty())
        m_perDeviceState.remove(deviceId);

    if (m_deviceModel.count() > 0 && m_deviceModel.selectedIndex() < 0)
        m_deviceModel.setSelectedIndex(0);
}

// Carousel selection changed. Refresh the UI from the newly-selected
// device's cached profile. No file I/O, no seeding, no hardware apply —
// one-time device provisioning happens in onPhysicalDeviceAdded.
void AppController::onSelectedDeviceChanged()
{
    auto *device = selectedDevice();
    if (!device) return;

    m_currentDevice = device->descriptor();

    const QString serial = device->deviceSerial();
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;  // device not yet fully set up

    const Profile &p = m_profileEngine.cachedProfile(serial, name);
    restoreButtonModelFromProfile(p);
    pushDisplayValues(p);
}

void AppController::setupProfileForDevice(PhysicalDevice *device)
{
    m_currentDevice = device->descriptor();

    const QString serial = device->deviceSerial();
    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    const QString profilesDir = configBase
        + QStringLiteral("/devices/") + serial
        + QStringLiteral("/profiles");

    QDir().mkpath(profilesDir);
    m_profileEngine.registerDevice(serial, profilesDir);
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
        if (m_currentDevice) {
            const auto controls = m_currentDevice->controls();
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
            const auto defaultGestures = m_currentDevice->defaultGestures();
            for (auto it = defaultGestures.begin();
                 it != defaultGestures.end(); ++it) {
                seed.gestures[it.key()] = it.value();
            }
        }
        ProfileEngine::saveProfile(defaultConf, seed);
        m_profileEngine.registerDevice(serial, profilesDir);  // reload cache after seeding
        qCDebug(lcApp) << "created default profile at" << defaultConf;
    }

    const QString bindingsFile = profilesDir + QStringLiteral("/app-bindings.conf");
    if (QFile::exists(bindingsFile)) {
        const auto bindings = ProfileEngine::loadAppBindings(bindingsFile);
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

    QString hwName = m_profileEngine.hardwareProfile(serial);
    bool isFirstConnect = hwName.isEmpty();
    if (isFirstConnect) {
        hwName = QStringLiteral("default");
        m_profileModel.setHwActiveIndex(0);
        m_profileEngine.setHardwareProfile(serial, hwName);
        m_profileEngine.setDisplayProfile(serial, hwName);
    }

    Profile &p = m_profileEngine.cachedProfile(serial, hwName);
    qCDebug(lcApp) << "setupProfileForDevice: applying profile" << hwName
                   << "thumbWheelMode=" << p.thumbWheelMode;
    applyProfileToHardware(p);
}

PhysicalDevice *AppController::selectedDevice() const
{
    int idx = m_deviceModel.selectedIndex();
    const auto &devices = m_deviceModel.devices();
    if (idx >= 0 && idx < devices.size())
        return devices[idx];
    return nullptr;
}

DeviceSession *AppController::selectedSession() const
{
    auto *d = selectedDevice();
    return d ? d->primary() : nullptr;
}

QString AppController::selectedSerial() const
{
    auto *d = selectedDevice();
    return d ? d->deviceSerial() : QString();
}

// Slot implementations -------------------------------------------------------

void AppController::onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType)
{
    Q_UNUSED(actionName)

    saveCurrentProfile();

    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    if (m_profileEngine.displayProfile(serial) != m_profileEngine.hardwareProfile(serial))
        return;

    if (!m_currentDevice) return;
    auto *session = selectedSession();
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

void AppController::onTabSwitched(const QString &profileName)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    m_profileEngine.setDisplayProfile(serial, profileName);
}

void AppController::onDisplayProfileChanged(const QString &serial, const Profile &profile)
{
    if (serial != selectedSerial())
        return;

    m_deviceModel.setActiveProfileName(profile.name);

    m_deviceModel.setDisplayValues(
        profile.dpi, profile.smartShiftEnabled, profile.smartShiftThreshold,
        profile.hiResScroll, profile.scrollDirection == "natural",
        profile.thumbWheelMode, profile.thumbWheelInvert);

    restoreButtonModelFromProfile(profile);
}

void AppController::onWindowFocusChanged(const QString &wmClass, const QString & /*title*/)
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

    m_deviceModel.setActiveWmClass(wmClass);

    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;

    QString profileName = m_profileEngine.profileForApp(serial, wmClass);
    if (profileName == m_profileEngine.hardwareProfile(serial))
        return;

    Profile &p = m_profileEngine.cachedProfile(serial, profileName);
    m_profileEngine.setHardwareProfile(serial, profileName);
    applyProfileToHardware(p);
    m_profileModel.setHwActiveByProfileName(profileName);
}

void AppController::restoreButtonModelFromProfile(const Profile &p)
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
        assignments.append({aName, aType, ctrl.controlId});
    }

    m_buttonModel.loadFromProfile(assignments);

    QMap<QString, QPair<QString, QString>> gestureMap;
    for (auto it = p.gestures.begin(); it != p.gestures.end(); ++it) {
        if (it->second.type == ButtonAction::Keystroke && !it->second.payload.isEmpty()) {
            QString name = it->second.payload;
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
    auto *session = selectedSession();
    if (!session || !m_currentDevice) return;

    session->flushCommandQueue();

    // Reset per-device thumb accumulator
    auto &state = m_perDeviceState[session->deviceId()];
    state.thumbAccum = 0;

    session->touchResponseTime();
    const auto controls = m_currentDevice->controls();
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
}

void AppController::saveCurrentProfile()
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;

    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;

    Profile &p = m_profileEngine.cachedProfile(serial, name);
    if (p.name.isEmpty()) p.name = name;

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

    for (const auto &dir : {"up", "down", "left", "right", "click"}) {
        QString ks = m_deviceModel.gestureKeystroke(dir);
        if (!ks.isEmpty())
            p.gestures[dir] = {ButtonAction::Keystroke, ks};
    }

    m_profileEngine.saveProfileToDisk(serial, name);
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
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(serial, name);
    p.dpi = value;
    m_profileEngine.saveProfileToDisk(serial, name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile(serial)) {
        auto *session = selectedSession();
        if (session) session->setDPI(value);
    }
}

void AppController::onSmartShiftChangeRequested(bool enabled, int threshold)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(serial, name);
    p.smartShiftEnabled = enabled;
    p.smartShiftThreshold = threshold;
    m_profileEngine.saveProfileToDisk(serial, name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile(serial)) {
        auto *session = selectedSession();
        if (session) session->setSmartShift(enabled, threshold);
    }
}

void AppController::onScrollConfigChangeRequested(bool hiRes, bool invert)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(serial, name);
    p.hiResScroll = hiRes;
    p.scrollDirection = invert ? QStringLiteral("natural") : QStringLiteral("standard");
    m_profileEngine.saveProfileToDisk(serial, name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile(serial)) {
        auto *session = selectedSession();
        if (session) session->setScrollConfig(hiRes, invert);
    }
}

void AppController::onThumbWheelModeChangeRequested(const QString &mode)
{
    auto *session = selectedSession();
    if (session) {
        auto &state = m_perDeviceState[session->deviceId()];
        state.thumbAccum = 0;
    }
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const QString name = m_profileEngine.displayProfile(serial);
    qCDebug(lcApp) << "thumbWheelMode requested:" << mode << "for profile:" << name;
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(serial, name);
    p.thumbWheelMode = mode;
    m_profileEngine.saveProfileToDisk(serial, name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile(serial) && session)
        session->setThumbWheelMode(mode, p.thumbWheelInvert);
}

void AppController::onThumbWheelInvertChangeRequested(bool invert)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;
    Profile &p = m_profileEngine.cachedProfile(serial, name);
    p.thumbWheelInvert = invert;
    m_profileEngine.saveProfileToDisk(serial, name);
    pushDisplayValues(p);
    if (name == m_profileEngine.hardwareProfile(serial)) {
        auto *session = selectedSession();
        if (session) session->setThumbWheelMode(p.thumbWheelMode, invert);
    }
}

void AppController::onDivertedButtonPressed(uint16_t controlId, bool pressed)
{
    auto *session = selectedSession();
    if (!session) return;
    auto &state = m_perDeviceState[session->deviceId()];

    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    const Profile &hwProfile = m_profileEngine.cachedProfile(
        serial, m_profileEngine.hardwareProfile(serial));

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
            m_actionExecutor.injectKeystroke(it->second.payload);
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
        m_actionExecutor.injectKeystroke(ba.payload);
    } else if (ba.type == ButtonAction::GestureTrigger) {
        state.gestureAccumX = 0;
        state.gestureAccumY = 0;
        state.gestureActive = true;
        state.gestureControlId = controlId;
    } else if (ba.type == ButtonAction::AppLaunch && !ba.payload.isEmpty()) {
        m_actionExecutor.launchApp(ba.payload);
    }
}

void AppController::onThumbWheelRotation(int delta)
{
    auto *session = selectedSession();
    if (!session) return;
    auto &state = m_perDeviceState[session->deviceId()];

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
        return QString();
    if (ba.type == ButtonAction::GestureTrigger)
        return QStringLiteral("Gestures");
    if (ba.type == ButtonAction::Keystroke) {
        int count = m_actionModel.rowCount();
        for (int i = 0; i < count; ++i) {
            QModelIndex mi = m_actionModel.index(i);
            if (m_actionModel.data(mi, ActionModel::ActionTypeRole).toString() == QStringLiteral("keystroke") &&
                m_actionModel.data(mi, ActionModel::PayloadRole).toString() == ba.payload) {
                return m_actionModel.data(mi, ActionModel::NameRole).toString();
            }
        }
        return ba.payload;
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
    if (actionType == QStringLiteral("dpi-cycle"))
        return {ButtonAction::DpiCycle, {}};
    if (actionType == QStringLiteral("media-controls")) {
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
            payload = actionName;
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
