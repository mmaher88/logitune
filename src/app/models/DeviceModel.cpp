#include "DeviceModel.h"
#include "interfaces/IDevice.h"
#include "devices/JsonDevice.h"
#include "desktop/GnomeDesktop.h"
#include "DistroDetector.h"
#include "logging/LogManager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
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
    return m_devices.size();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};

    auto *device = m_devices[index.row()];

    switch (role) {
    case DeviceIdRole:
        return device->deviceSerial();
    case DeviceNameRole:
        return device->deviceName();
    case FrontImageRole: {
        if (!device->descriptor()) return QString();
        QString path = device->descriptor()->frontImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    case BatteryLevelRole:
        return device->batteryLevel();
    case BatteryChargingRole:
        return device->batteryCharging();
    case ConnectionTypeRole:
        return device->connectionType();
    case StatusRole: {
        if (!device->descriptor())
            return QStringLiteral("unknown");
        auto* json = dynamic_cast<const JsonDevice*>(device->descriptor());
        if (!json) return QStringLiteral("beta");
        switch (json->status()) {
        case JsonDevice::Status::Verified: return QStringLiteral("verified");
        case JsonDevice::Status::Beta:     return QStringLiteral("beta");
        }
        return QStringLiteral("beta");
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
    return m_devices.size();
}

int DeviceModel::selectedIndex() const
{
    return m_selectedIndex;
}

void DeviceModel::setSelectedIndex(int index)
{
    if (index < -1 || index >= m_devices.size())
        return;
    if (m_selectedIndex == index)
        return;

    int oldIndex = m_selectedIndex;
    m_selectedIndex = index;

    // Update isSelected role for old and new
    if (oldIndex >= 0 && oldIndex < m_devices.size())
        emit dataChanged(this->index(oldIndex), this->index(oldIndex), {IsSelectedRole});
    if (index >= 0 && index < m_devices.size())
        emit dataChanged(this->index(index), this->index(index), {IsSelectedRole});

    m_hasDisplayValues = false;
    emit selectedChanged();
    emit selectedBatteryChanged();
    emit settingsReloaded();
    emit deviceConnectedChanged();
}

QString DeviceModel::selectedDeviceId() const
{
    auto *d = selectedDevice();
    return d ? d->deviceSerial() : QString();
}

// ---------------------------------------------------------------------------
// Physical device management
// ---------------------------------------------------------------------------

void DeviceModel::addPhysicalDevice(PhysicalDevice *device)
{
    if (!device)
        return;

    // Track last-seen connection state per device so we can distinguish
    // "attribute update" (battery tick etc.) from "connection transition"
    // (connect/disconnect). Shared_ptr so the state survives capture by
    // the lambda and persists across multiple invocations.
    auto lastConnected = std::make_shared<bool>(false);
    connect(device, &PhysicalDevice::stateChanged, this,
            [this, device, lastConnected]() {
        const bool now = device->isConnected();
        const bool was = *lastConnected;
        *lastConnected = now;

        if (now && !was) {
            // disconnected -> connected: show the row
            insertRow(device);
        } else if (!now && was) {
            // connected -> disconnected: hide the row. PhysicalDevice is
            // still alive in DeviceManager; AppController keeps per-device
            // state. We just don't paint a carousel card for an offline
            // mouse.
            removeRow(device);
        } else {
            // state change while the visibility is unchanged: refresh
            refreshRow(device);
        }
    });

    // Per-property relay hooks. DeviceModel caches the last values pushed
    // in by setDisplayValues (profile load) and short-circuits the getters
    // to that cache whenever m_hasDisplayValues is true. If the hardware
    // state changes out from under the cache (button press, on-device
    // toggle, or a setter emitting optimistic echo), we must clear the
    // cache so the next getter call falls through to the live session.
    // Only the selected device's events should clear the cache; events
    // from alternates are ignored.
    connect(device, &PhysicalDevice::smartShiftChanged, this,
            [this, device](bool, int) {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit smartShiftEnabledChanged();
        emit smartShiftThresholdChanged();
    });
    connect(device, &PhysicalDevice::scrollConfigChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit scrollConfigChanged();
    });
    connect(device, &PhysicalDevice::thumbWheelModeChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit thumbWheelModeChanged();
    });
    connect(device, &PhysicalDevice::currentDPIChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit currentDPIChanged();
    });

    // If the device is already connected at the time of addition, insert
    // its row immediately. Otherwise we wait for the first connect
    // transition in the stateChanged handler.
    if (device->isConnected()) {
        *lastConnected = true;
        insertRow(device);
    }
}

void DeviceModel::insertRow(PhysicalDevice *device)
{
    if (m_devices.contains(device))
        return;

    // Determine insertion position from saved order.
    QStringList savedOrder = loadDeviceOrder();
    int insertAt = m_devices.size();
    if (!savedOrder.isEmpty()) {
        int savedPos = savedOrder.indexOf(device->deviceSerial());
        if (savedPos >= 0) {
            insertAt = 0;
            for (int i = 0; i < m_devices.size(); ++i) {
                int existingPos = savedOrder.indexOf(m_devices[i]->deviceSerial());
                if (existingPos < 0 || existingPos < savedPos)
                    insertAt = i + 1;
            }
        }
    }

    beginInsertRows(QModelIndex(), insertAt, insertAt);
    m_devices.insert(insertAt, device);
    endInsertRows();
    emit countChanged();

    // Auto-select the first device. Also emit deviceConnectedChanged
    // because going from 0 devices to 1 is a "connected" transition for
    // whatever QML is bound to the selected-device flat properties.
    if (m_devices.size() == 1) {
        m_selectedIndex = 0;
        emit selectedChanged();
        emit selectedBatteryChanged();
        emit settingsReloaded();
        emit deviceConnectedChanged();
    }
}

void DeviceModel::removeRow(PhysicalDevice *device)
{
    const int row = rowForDevice(device);
    if (row < 0) return;

    const bool wasSelected = (row == m_selectedIndex);
    beginRemoveRows(QModelIndex(), row, row);
    m_devices.removeAt(row);
    endRemoveRows();

    if (m_selectedIndex >= m_devices.size())
        m_selectedIndex = m_devices.size() - 1;
    if (m_devices.isEmpty())
        m_selectedIndex = -1;

    emit countChanged();
    if (wasSelected) {
        emit selectedChanged();
        emit selectedBatteryChanged();
        emit settingsReloaded();
        emit deviceConnectedChanged();
    }
}

void DeviceModel::refreshRow(PhysicalDevice *device)
{
    const int row = rowForDevice(device);
    if (row < 0) return;
    emit dataChanged(index(row), index(row));
    if (row == m_selectedIndex) {
        emit selectedChanged();
        emit selectedBatteryChanged();
        emit settingsReloaded();
    }
}

void DeviceModel::moveDevice(int from, int to)
{
    if (from < 0 || from >= m_devices.size()) return;
    if (to < 0 || to >= m_devices.size()) return;
    if (from == to) return;

    int destRow = to > from ? to + 1 : to;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);
    m_devices.move(from, to);
    endMoveRows();

    if (m_selectedIndex == from)
        m_selectedIndex = to;
    else if (from < m_selectedIndex && to >= m_selectedIndex)
        m_selectedIndex--;
    else if (from > m_selectedIndex && to <= m_selectedIndex)
        m_selectedIndex++;

    saveDeviceOrder();
}

void DeviceModel::saveDeviceOrder() const
{
    QJsonArray order;
    for (auto *device : m_devices)
        order.append(device->deviceSerial());

    QJsonObject root;
    root["order"] = order;

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                   + "/device-order.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

QStringList DeviceModel::loadDeviceOrder() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                   + "/device-order.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QJsonArray order = QJsonDocument::fromJson(f.readAll()).object()["order"].toArray();
    QStringList result;
    for (const auto &v : order)
        result.append(v.toString());
    return result;
}

void DeviceModel::removePhysicalDevice(PhysicalDevice *device)
{
    // If the device is currently visible in the carousel, remove its row.
    // If it wasn't visible (was hidden because offline), there's nothing
    // to do — the stateChanged lambda will auto-disconnect when the
    // PhysicalDevice is destroyed.
    if (rowForDevice(device) >= 0)
        removeRow(device);
}

bool DeviceModel::hasDeviceId(const QString &deviceId) const
{
    for (auto *d : m_devices)
        if (d->deviceSerial() == deviceId)
            return true;
    return false;
}

const QList<PhysicalDevice *> &DeviceModel::devices() const
{
    return m_devices;
}

int DeviceModel::rowForDevice(PhysicalDevice *device) const
{
    for (int i = 0; i < m_devices.size(); ++i)
        if (m_devices[i] == device)
            return i;
    return -1;
}

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

PhysicalDevice *DeviceModel::selectedDevice() const
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_devices.size())
        return m_devices[m_selectedIndex];
    return nullptr;
}

const IDevice *DeviceModel::activeDevice() const
{
    auto *s = selectedDevice();
    return s ? s->descriptor() : nullptr;
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
    auto *s = selectedDevice();
    return s ? s->isConnected() : false;
}

QString DeviceModel::deviceName() const
{
    auto *s = selectedDevice();
    return s ? s->deviceName() : QString();
}

int DeviceModel::batteryLevel() const
{
    auto *s = selectedDevice();
    return s ? s->batteryLevel() : 0;
}

bool DeviceModel::batteryCharging() const
{
    auto *s = selectedDevice();
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
    auto *s = selectedDevice();
    return s ? s->connectionType() : QString();
}

int DeviceModel::currentDPI() const
{
    if (m_hasDisplayValues) return m_displayDpi;
    auto *s = selectedDevice();
    return s ? s->currentDPI() : 1000;
}

int DeviceModel::minDPI() const
{
    auto *s = selectedDevice();
    return s ? s->minDPI() : 200;
}

int DeviceModel::maxDPI() const
{
    auto *s = selectedDevice();
    return s ? s->maxDPI() : 8000;
}

int DeviceModel::dpiStep() const
{
    auto *s = selectedDevice();
    return s ? s->dpiStep() : 50;
}

bool DeviceModel::smartShiftEnabled() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftEnabled;
    auto *s = selectedDevice();
    return s ? s->smartShiftEnabled() : true;
}

int DeviceModel::smartShiftThreshold() const
{
    if (m_hasDisplayValues) return m_displaySmartShiftThreshold;
    auto *s = selectedDevice();
    return s ? s->smartShiftThreshold() : 128;
}

QString DeviceModel::activeProfileName() const
{
    return m_activeProfileName;
}

QString DeviceModel::frontImage() const
{
    auto *s = selectedDevice();
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
    auto *s = selectedDevice();
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
    auto *s = selectedDevice();
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
    auto *s = selectedDevice();
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
            entry[QStringLiteral("buttonLabel")]    = it->displayName.isEmpty() ? it->defaultName : it->displayName;
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
    auto *s = selectedDevice();
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
        entry[QStringLiteral("kind")]            = hs.kind;
        result.append(entry);
    }
    return result;
}

QVariantList DeviceModel::controlDescriptors() const
{
    QVariantList result;
    auto *s = selectedDevice();
    if (!s || !s->descriptor())
        return result;

    const auto controls = s->descriptor()->controls();
    for (const auto &ctrl : controls) {
        QVariantMap entry;
        entry[QStringLiteral("buttonId")]      = ctrl.buttonIndex;
        entry[QStringLiteral("buttonName")]    = ctrl.defaultName;
        entry[QStringLiteral("displayName")]   = ctrl.displayName;
        entry[QStringLiteral("actionDefault")] = ctrl.defaultActionType;
        entry[QStringLiteral("configurable")]  = ctrl.configurable;
        result.append(entry);
    }
    return result;
}

QVariantList DeviceModel::easySwitchSlotPositions() const
{
    QVariantList result;
    auto *s = selectedDevice();
    if (!s || !s->descriptor())
        return result;

    const auto positions = s->descriptor()->easySwitchSlotPositions();
    for (const auto &pos : positions) {
        QVariantMap entry;
        entry[QStringLiteral("xPct")] = pos.xPct;
        entry[QStringLiteral("yPct")] = pos.yPct;
        entry[QStringLiteral("label")] = pos.label;
        result.append(entry);
    }
    return result;
}

bool DeviceModel::smoothScrollSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().smoothScroll;
    return true;
}

bool DeviceModel::thumbWheelSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().thumbWheel;
    return true;
}

bool DeviceModel::smartShiftSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().smartShift;
    return true;
}

bool DeviceModel::adjustableDpiSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().adjustableDpi;
    return true;
}

bool DeviceModel::reprogControlsSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().reprogControls;
    return true;
}

QString DeviceModel::deviceSerial() const
{
    auto *s = selectedDevice();
    return s ? s->deviceSerial() : QStringLiteral("Unknown");
}

QString DeviceModel::firmwareVersion() const
{
    auto *s = selectedDevice();
    if (s && !s->firmwareVersion().isEmpty())
        return s->firmwareVersion();
    return QStringLiteral("Unknown");
}

int DeviceModel::activeSlot() const
{
    auto *s = selectedDevice();
    if (!s || !s->isConnected())
        return -1;
    int host = s->currentHost();
    if (host >= 0)
        return host + 1;
    return -1;
}

bool DeviceModel::isSlotPaired(int slot) const
{
    auto *s = selectedDevice();
    if (!s) return false;
    return s->isHostPaired(slot - 1);
}

void DeviceModel::refreshFromActiveDevice()
{
    emit selectedChanged();
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
    emit currentDPIChanged();
    emit smartShiftEnabledChanged();
    emit smartShiftThresholdChanged();
    emit scrollConfigChanged();
    emit thumbWheelModeChanged();
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
    auto *s = selectedDevice();
    return s ? s->scrollHiRes() : false;
}

bool DeviceModel::scrollInvert() const
{
    if (m_hasDisplayValues) return m_displayScrollInvert;
    auto *s = selectedDevice();
    return s ? s->scrollInvert() : false;
}

void DeviceModel::setScrollConfig(bool hiRes, bool invert)
{
    emit scrollConfigChangeRequested(hiRes, invert);
}

QString DeviceModel::thumbWheelMode() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelMode;
    auto *s = selectedDevice();
    return s ? s->thumbWheelMode() : "scroll";
}

bool DeviceModel::thumbWheelInvert() const
{
    if (m_hasDisplayValues) return m_displayThumbWheelInvert;
    auto *s = selectedDevice();
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

QString DeviceModel::appIndicatorInstallCommand() const
{
    switch (logitune::util::detectDistroFamily()) {
    case logitune::util::DistroFamily::Arch:
        return QStringLiteral("sudo pacman -S gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Debian:
        return QStringLiteral("sudo apt install gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Fedora:
        return QStringLiteral("sudo dnf install gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Unknown:
        return QStringLiteral("Install gnome-shell-extension-appindicator via your package manager.");
    }
    Q_UNREACHABLE();
}

void DeviceModel::setActiveWmClass(const QString &wmClass)
{
    if (m_activeWmClass == wmClass) return;
    m_activeWmClass = wmClass;
    emit activeWmClassChanged();
}

QString DeviceModel::deviceStatus() const
{
    auto *s = selectedDevice();
    if (!s || !s->descriptor())
        return QStringLiteral("unknown");
    auto* json = dynamic_cast<const JsonDevice*>(s->descriptor());
    if (!json)
        return QStringLiteral("beta");
    switch (json->status()) {
    case JsonDevice::Status::Verified: return QStringLiteral("verified");
    case JsonDevice::Status::Beta:     return QStringLiteral("beta");
    }
    return QStringLiteral("beta");
}

} // namespace logitune
