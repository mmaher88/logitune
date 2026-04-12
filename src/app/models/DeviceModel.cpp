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
    : QAbstractListModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractListModel
// ---------------------------------------------------------------------------

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_sessions.size();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sessions.size())
        return {};

    auto *session = m_sessions[index.row()];

    switch (role) {
    case DeviceIdRole:
        return session->deviceId();
    case DeviceNameRole:
        return session->deviceName();
    case FrontImageRole: {
        if (!session->descriptor()) return QString();
        QString path = session->descriptor()->frontImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    case BatteryLevelRole:
        return session->batteryLevel();
    case BatteryChargingRole:
        return session->batteryCharging();
    case ConnectionTypeRole:
        return session->connectionType();
    case StatusRole: {
        if (!session->descriptor())
            return QStringLiteral("unknown");
        auto* json = dynamic_cast<const JsonDevice*>(session->descriptor());
        if (!json) return QStringLiteral("implemented");
        switch (json->status()) {
        case JsonDevice::Status::Implemented:       return QStringLiteral("implemented");
        case JsonDevice::Status::CommunityVerified: return QStringLiteral("community-verified");
        case JsonDevice::Status::CommunityLocal:    return QStringLiteral("community-local");
        case JsonDevice::Status::Placeholder:       return QStringLiteral("placeholder");
        }
        return QStringLiteral("unknown");
    }
    case IsSelectedRole:
        return index.row() == m_selectedIndex;
    }

    return {};
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    return {
        {DeviceIdRole,        "deviceId"},
        {DeviceNameRole,      "deviceName"},
        {FrontImageRole,      "frontImage"},
        {BatteryLevelRole,    "batteryLevel"},
        {BatteryChargingRole, "batteryCharging"},
        {ConnectionTypeRole,  "connectionType"},
        {StatusRole,          "status"},
        {IsSelectedRole,      "isSelected"},
    };
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

int DeviceModel::count() const
{
    return m_sessions.size();
}

int DeviceModel::selectedIndex() const
{
    return m_selectedIndex;
}

void DeviceModel::setSelectedIndex(int index)
{
    if (index < -1 || index >= m_sessions.size())
        return;
    if (m_selectedIndex == index)
        return;

    int oldIndex = m_selectedIndex;
    m_selectedIndex = index;

    // Update isSelected role for old and new
    if (oldIndex >= 0 && oldIndex < m_sessions.size())
        emit dataChanged(this->index(oldIndex), this->index(oldIndex), {IsSelectedRole});
    if (index >= 0 && index < m_sessions.size())
        emit dataChanged(this->index(index), this->index(index), {IsSelectedRole});

    m_hasDisplayValues = false;
    emit selectedChanged();
    emit selectedBatteryChanged();
    emit settingsReloaded();
}

QString DeviceModel::selectedDeviceId() const
{
    auto *s = selectedSession();
    return s ? s->deviceId() : QString();
}

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------

void DeviceModel::addSession(DeviceSession *session)
{
    int row = m_sessions.size();
    beginInsertRows(QModelIndex(), row, row);
    m_sessions.append(session);
    endInsertRows();
    emit countChanged();
}

void DeviceModel::removeSession(const QString &deviceId)
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i]->deviceId() == deviceId) {
            beginRemoveRows(QModelIndex(), i, i);
            m_sessions.removeAt(i);
            endRemoveRows();

            if (m_selectedIndex >= m_sessions.size())
                m_selectedIndex = m_sessions.size() - 1;
            if (m_sessions.isEmpty())
                m_selectedIndex = -1;

            emit countChanged();
            emit selectedChanged();
            return;
        }
    }
}

const QList<DeviceSession*>& DeviceModel::sessions() const
{
    return m_sessions;
}

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

DeviceSession* DeviceModel::selectedSession() const
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_sessions.size())
        return m_sessions[m_selectedIndex];
    return nullptr;
}

// ---------------------------------------------------------------------------
// Desktop integration
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Selected device property getters
// ---------------------------------------------------------------------------

bool DeviceModel::deviceConnected() const
{
    auto *s = selectedSession();
    return s ? s->isConnected() : false;
}

QString DeviceModel::deviceName() const
{
    auto *s = selectedSession();
    return s ? s->deviceName() : QString();
}

int DeviceModel::batteryLevel() const
{
    auto *s = selectedSession();
    return s ? s->batteryLevel() : 0;
}

bool DeviceModel::batteryCharging() const
{
    auto *s = selectedSession();
    return s ? s->batteryCharging() : false;
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
    auto *s = selectedSession();
    return s ? s->connectionType() : QString();
}

int DeviceModel::currentDPI() const
{
    if (m_hasDisplayValues) return m_displayDpi;
    auto *s = selectedSession();
    return s ? s->currentDPI() : 1000;
}

int DeviceModel::minDPI() const
{
    auto *s = selectedSession();
    return s ? s->minDPI() : 200;
}

int DeviceModel::maxDPI() const
{
    auto *s = selectedSession();
    return s ? s->maxDPI() : 8000;
}

int DeviceModel::dpiStep() const
{
    auto *s = selectedSession();
    return s ? s->dpiStep() : 50;
}

bool DeviceModel::smartShiftEnabled() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftEnabled;
    auto *s = selectedSession();
    return s ? s->smartShiftEnabled() : true;
}

int DeviceModel::smartShiftThreshold() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftThreshold;
    auto *s = selectedSession();
    return s ? s->smartShiftThreshold() : 128;
}

QString DeviceModel::activeProfileName() const
{
    return m_activeProfileName;
}

QString DeviceModel::frontImage() const
{
    auto *s = selectedSession();
    if (s && s->descriptor()) {
        QString path = s->descriptor()->frontImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::sideImage() const
{
    auto *s = selectedSession();
    if (s && s->descriptor()) {
        QString path = s->descriptor()->sideImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::backImage() const
{
    auto *s = selectedSession();
    if (s && s->descriptor()) {
        QString path = s->descriptor()->backImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QVariantList DeviceModel::buttonHotspots() const
{
    QVariantList result;
    auto *s = selectedSession();
    if (!s || !s->descriptor())
        return result;

    const auto hotspots = s->descriptor()->buttonHotspots();
    const auto controls = s->descriptor()->controls();

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

        auto it = controlMap.find(hs.buttonIndex);
        if (it != controlMap.end()) {
            entry[QStringLiteral("buttonLabel")]    = it->defaultName;
            entry[QStringLiteral("actionDefault")]  = it->defaultName;
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
    auto *s = selectedSession();
    if (!s || !s->descriptor())
        return result;

    const auto hotspots = s->descriptor()->scrollHotspots();
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
    auto *s = selectedSession();
    if (!s || !s->descriptor())
        return result;

    const auto controls = s->descriptor()->controls();
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
    auto *s = selectedSession();
    if (!s || !s->descriptor())
        return result;

    const auto positions = s->descriptor()->easySwitchSlotPositions();
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
    auto *s = selectedSession();
    if (s && s->descriptor())
        return s->descriptor()->features().smoothScroll;
    return true;
}

QString DeviceModel::deviceSerial() const
{
    auto *s = selectedSession();
    return s ? s->deviceSerial() : QStringLiteral("Unknown");
}

QString DeviceModel::firmwareVersion() const
{
    auto *s = selectedSession();
    if (s && !s->firmwareVersion().isEmpty())
        return s->firmwareVersion();
    return QStringLiteral("Unknown");
}

int DeviceModel::activeSlot() const
{
    auto *s = selectedSession();
    if (!s || !s->isConnected())
        return -1;
    int host = s->currentHost();
    if (host >= 0)
        return host + 1;
    return -1;
}

bool DeviceModel::isSlotPaired(int slot) const
{
    auto *s = selectedSession();
    if (!s) return false;
    return s->isHostPaired(slot - 1);
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
    auto *s = selectedSession();
    return s ? s->scrollHiRes() : false;
}

bool DeviceModel::scrollInvert() const
{
    if (m_hasDisplayValues) return m_displayScrollInvert;
    auto *s = selectedSession();
    return s ? s->scrollInvert() : false;
}

void DeviceModel::setScrollConfig(bool hiRes, bool invert)
{
    emit scrollConfigChangeRequested(hiRes, invert);
}

QString DeviceModel::thumbWheelMode() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelMode;
    auto *s = selectedSession();
    return s ? s->thumbWheelMode() : "scroll";
}

bool DeviceModel::thumbWheelInvert() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelInvert;
    auto *s = selectedSession();
    return s ? s->thumbWheelInvert() : false;
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
    emit gestureChanged();
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
        return QString();
    auto *gnome = qobject_cast<const GnomeDesktop*>(m_desktop);
    if (!gnome) return QString();
    switch (gnome->appIndicatorStatus()) {
    case GnomeDesktop::AppIndicatorNotInstalled:
        return QStringLiteral("not-installed");
    case GnomeDesktop::AppIndicatorDisabled:
        return QStringLiteral("disabled");
    default:
        return QString();
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
    auto *s = selectedSession();
    if (!s || !s->descriptor())
        return QStringLiteral("unknown");
    auto* json = dynamic_cast<const JsonDevice*>(s->descriptor());
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
