#include "DeviceModel.h"
#include "interfaces/IDevice.h"
#include "devices/JsonDevice.h"
#include "desktop/GnomeDesktop.h"
#include "logging/LogManager.h"
#include <QTimer>
#include <QSettings>
#include <QVariantMap>

namespace logitune {

DeviceModel::DeviceModel(QObject *parent)
    : QObject(parent)
{
}

void DeviceModel::setDesktopIntegration(IDesktopIntegration *desktop)
{
    m_desktop = desktop;
}

void DeviceModel::blockGlobalShortcuts(bool block)
{
    if (m_desktop)
        m_desktop->blockGlobalShortcuts(block);
}

QVariantList DeviceModel::runningApplications() const
{
    if (m_desktop) {
        auto result = m_desktop->runningApplications();
        qCDebug(lcUi) << "runningApplications:" << result.size() << "apps";
        return result;
    }
    qCDebug(lcUi) << "runningApplications: no desktop integration";
    return {};
}

void DeviceModel::setDeviceManager(DeviceManager *dm)
{
    m_dm = dm;

    connect(dm, &DeviceManager::deviceConnectedChanged,
            this, &DeviceModel::deviceConnectedChanged);
    connect(dm, &DeviceManager::deviceNameChanged,
            this, &DeviceModel::deviceNameChanged);
    connect(dm, &DeviceManager::batteryLevelChanged,
            this, &DeviceModel::batteryLevelChanged);
    connect(dm, &DeviceManager::batteryChargingChanged,
            this, &DeviceModel::batteryChargingChanged);
    connect(dm, &DeviceManager::connectionTypeChanged,
            this, &DeviceModel::connectionTypeChanged);
}

bool DeviceModel::deviceConnected() const
{
    return m_dm ? m_dm->deviceConnected() : false;
}

QString DeviceModel::deviceName() const
{
    return m_dm ? m_dm->deviceName() : QString();
}

int DeviceModel::batteryLevel() const
{
    return m_dm ? m_dm->batteryLevel() : 0;
}

bool DeviceModel::batteryCharging() const
{
    return m_dm ? m_dm->batteryCharging() : false;
}

QString DeviceModel::batteryStatusText() const
{
    QString text = QStringLiteral("Battery: %1%").arg(batteryLevel());
    if (batteryCharging())
        text += QStringLiteral(" (charging)");
    return text;
}

QString DeviceModel::connectionType() const
{
    return m_dm ? m_dm->connectionType() : QString();
}

int DeviceModel::currentDPI() const
{
    if (m_hasDisplayValues) return m_displayDpi;
    return m_dm ? m_dm->currentDPI() : 1000;
}

int DeviceModel::minDPI() const
{
    return m_dm ? m_dm->minDPI() : 200;
}

int DeviceModel::maxDPI() const
{
    return m_dm ? m_dm->maxDPI() : 8000;
}

int DeviceModel::dpiStep() const
{
    return m_dm ? m_dm->dpiStep() : 50;
}

bool DeviceModel::smartShiftEnabled() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftEnabled;
    return m_dm ? m_dm->smartShiftEnabled() : true;
}

int DeviceModel::smartShiftThreshold() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftThreshold;
    return m_dm ? m_dm->smartShiftThreshold() : 128;
}

QString DeviceModel::activeProfileName() const
{
    return m_activeProfileName;
}

QString DeviceModel::frontImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->frontImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::sideImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->sideImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::backImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->backImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QVariantList DeviceModel::buttonHotspots() const
{
    QVariantList result;
    if (!m_dm || !m_dm->activeDevice())
        return result;

    const auto hotspots = m_dm->activeDevice()->buttonHotspots();
    const auto controls = m_dm->activeDevice()->controls();

    // Build a lookup from buttonIndex -> ControlDescriptor
    QMap<int, ControlDescriptor> controlMap;
    for (const auto &ctrl : controls)
        controlMap[ctrl.buttonIndex] = ctrl;

    for (const auto &hs : hotspots) {
        QVariantMap entry;
        entry[QStringLiteral("buttonId")]       = hs.buttonIndex;
        entry[QStringLiteral("hotspotXPct")]     = hs.xPct;
        entry[QStringLiteral("hotspotYPct")]     = hs.yPct;
        entry[QStringLiteral("side")]            = hs.side;
        entry[QStringLiteral("labelOffsetYPct")] = hs.labelOffsetYPct;

        // Merge control descriptor data
        auto it = controlMap.find(hs.buttonIndex);
        if (it != controlMap.end()) {
            entry[QStringLiteral("buttonLabel")]    = it->defaultName;
            entry[QStringLiteral("actionDefault")]  = it->defaultName;  // human-readable fallback
            entry[QStringLiteral("configurable")]   = it->configurable;
        } else {
            entry[QStringLiteral("buttonLabel")]    = QStringLiteral("Button %1").arg(hs.buttonIndex);
            entry[QStringLiteral("actionDefault")]  = QStringLiteral("default");
            entry[QStringLiteral("configurable")]   = false;
        }

        result.append(entry);
    }
    return result;
}

QVariantList DeviceModel::scrollHotspots() const
{
    QVariantList result;
    if (!m_dm || !m_dm->activeDevice())
        return result;

    const auto hotspots = m_dm->activeDevice()->scrollHotspots();
    for (const auto &hs : hotspots) {
        QVariantMap entry;
        entry[QStringLiteral("buttonIndex")]     = hs.buttonIndex;
        entry[QStringLiteral("xPct")]            = hs.xPct;
        entry[QStringLiteral("yPct")]            = hs.yPct;
        entry[QStringLiteral("side")]            = hs.side;
        entry[QStringLiteral("labelOffsetYPct")] = hs.labelOffsetYPct;
        result.append(entry);
    }
    return result;
}

QVariantList DeviceModel::controlDescriptors() const
{
    QVariantList result;
    if (!m_dm || !m_dm->activeDevice())
        return result;

    const auto controls = m_dm->activeDevice()->controls();
    for (const auto &ctrl : controls) {
        QVariantMap entry;
        entry[QStringLiteral("buttonId")]      = ctrl.buttonIndex;
        entry[QStringLiteral("buttonName")]    = ctrl.defaultName;
        entry[QStringLiteral("actionDefault")] = ctrl.defaultActionType;
        entry[QStringLiteral("configurable")]  = ctrl.configurable;
        result.append(entry);
    }
    return result;
}

QVariantList DeviceModel::easySwitchSlotPositions() const
{
    QVariantList result;
    if (!m_dm || !m_dm->activeDevice())
        return result;

    const auto positions = m_dm->activeDevice()->easySwitchSlotPositions();
    for (const auto &pos : positions) {
        QVariantMap entry;
        entry[QStringLiteral("xPct")] = pos.xPct;
        entry[QStringLiteral("yPct")] = pos.yPct;
        result.append(entry);
    }
    return result;
}

bool DeviceModel::smoothScrollSupported() const
{
    if (m_dm && m_dm->activeDevice())
        return m_dm->activeDevice()->features().smoothScroll;
    return true;
}

QString DeviceModel::deviceSerial() const
{
    if (m_dm)
        return m_dm->deviceSerial();
    return QStringLiteral("Unknown");
}

QString DeviceModel::firmwareVersion() const
{
    if (m_dm && !m_dm->firmwareVersion().isEmpty())
        return m_dm->firmwareVersion();
    return QStringLiteral("Unknown");
}

int DeviceModel::activeSlot() const
{
    if (!m_dm || !m_dm->deviceConnected())
        return -1;
    // Use ChangeHost feature (0x1814) for real Easy-Switch channel
    int host = m_dm->currentHost();
    if (host >= 0)
        return host + 1;  // 0-based → 1-based for display
    return -1;  // unknown
}

bool DeviceModel::isSlotPaired(int slot) const
{
    if (!m_dm) return false;
    return m_dm->isHostPaired(slot - 1);  // 1-based → 0-based
}

void DeviceModel::setDisplayValues(int dpi, bool smartShiftEnabled, int smartShiftThreshold,
                                    bool scrollHiRes, bool scrollInvert, const QString &thumbWheelMode,
                                    bool thumbWheelInvert)
{
    m_displayDpi = dpi;
    m_displaySmartShiftEnabled = smartShiftEnabled;
    m_displaySmartShiftThreshold = smartShiftThreshold;
    m_displayScrollHiRes = scrollHiRes;
    m_displayScrollInvert = scrollInvert;
    m_displayThumbWheelMode = thumbWheelMode;
    m_displayThumbWheelInvert = thumbWheelInvert;
    m_hasDisplayValues = true;
    emit settingsReloaded();
}

void DeviceModel::setDPI(int value)
{
    emit dpiChangeRequested(value);
}

void DeviceModel::setSmartShift(bool enabled, int threshold)
{
    emit smartShiftChangeRequested(enabled, threshold);
}

bool DeviceModel::scrollHiRes() const
{
    if (m_hasDisplayValues) return m_displayScrollHiRes;
    return m_dm ? m_dm->scrollHiRes() : false;
}

bool DeviceModel::scrollInvert() const
{
    if (m_hasDisplayValues) return m_displayScrollInvert;
    return m_dm ? m_dm->scrollInvert() : false;
}

void DeviceModel::setScrollConfig(bool hiRes, bool invert)
{
    emit scrollConfigChangeRequested(hiRes, invert);
}

QString DeviceModel::thumbWheelMode() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelMode;
    return m_dm ? m_dm->thumbWheelMode() : "scroll";
}

bool DeviceModel::thumbWheelInvert() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelInvert;
    return m_dm ? m_dm->thumbWheelInvert() : false;
}

void DeviceModel::setThumbWheelMode(const QString &mode)
{
    emit thumbWheelModeChangeRequested(mode);
}

void DeviceModel::setThumbWheelInvert(bool invert)
{
    emit thumbWheelInvertChangeRequested(invert);
}

void DeviceModel::setGestureAction(const QString &direction, const QString &actionName, const QString &keystroke)
{
    m_gestures[direction] = qMakePair(actionName, keystroke);
    emit gestureChanged();
    emit userGestureChanged(direction, actionName, keystroke);
}

void DeviceModel::loadGesturesFromProfile(const QMap<QString, QPair<QString, QString>> &gestures)
{
    m_gestures = gestures;
    emit gestureChanged();  // QML re-reads display, but no save trigger
}

QString DeviceModel::gestureActionName(const QString &direction) const
{
    auto it = m_gestures.find(direction);
    return it != m_gestures.end() ? it.value().first : QString();
}

QString DeviceModel::gestureKeystroke(const QString &direction) const
{
    auto it = m_gestures.find(direction);
    return it != m_gestures.end() ? it.value().second : QString();
}

void DeviceModel::resetAllProfiles()
{
    qCDebug(lcUi) << "resetAllProfiles requested";
}

void DeviceModel::setActiveProfileName(const QString &name)
{
    if (m_activeProfileName == name) return;
    m_activeProfileName = name;
    emit activeProfileNameChanged();
}

QString DeviceModel::activeWmClass() const
{
    return m_activeWmClass;
}

QString DeviceModel::gnomeTrayStatus() const
{
    if (!m_desktop || m_desktop->desktopName() != QStringLiteral("GNOME"))
        return QString(); // Not GNOME — no issue
    auto *gnome = qobject_cast<const GnomeDesktop*>(m_desktop);
    if (!gnome) return QString();
    switch (gnome->appIndicatorStatus()) {
    case GnomeDesktop::AppIndicatorNotInstalled:
        return QStringLiteral("not-installed");
    case GnomeDesktop::AppIndicatorDisabled:
        return QStringLiteral("disabled");
    default:
        return QString(); // Active or unknown — no action needed
    }
}

void DeviceModel::setActiveWmClass(const QString &wmClass)
{
    if (m_activeWmClass == wmClass) return;
    m_activeWmClass = wmClass;
    emit activeWmClassChanged();
}

QString DeviceModel::deviceStatus() const
{
    if (!m_dm || !m_dm->activeDevice())
        return QStringLiteral("unknown");
    auto* json = dynamic_cast<const JsonDevice*>(m_dm->activeDevice());
    if (!json)
        return QStringLiteral("implemented");
    switch (json->status()) {
    case JsonDevice::Status::Implemented:       return QStringLiteral("implemented");
    case JsonDevice::Status::CommunityVerified: return QStringLiteral("community-verified");
    case JsonDevice::Status::CommunityLocal:    return QStringLiteral("community-local");
    case JsonDevice::Status::Placeholder:       return QStringLiteral("placeholder");
    }
    return QStringLiteral("unknown");
}

} // namespace logitune
