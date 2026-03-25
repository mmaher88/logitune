#include "DeviceModel.h"
#include <QDebug>

namespace logitune {

DeviceModel::DeviceModel(QObject *parent)
    : QObject(parent)
{
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

QString DeviceModel::connectionType() const
{
    return m_dm ? m_dm->connectionType() : QString();
}

int DeviceModel::currentDPI() const
{
    return m_currentDPI;
}

bool DeviceModel::smartShiftEnabled() const
{
    return m_smartShiftEnabled;
}

int DeviceModel::smartShiftThreshold() const
{
    return m_smartShiftThreshold;
}

QString DeviceModel::activeProfileName() const
{
    return m_activeProfileName;
}

void DeviceModel::setDPI(int value)
{
    qDebug() << "[DeviceModel] setDPI requested:" << value;
    // Actual HID++ DPI push flows through DeviceManager/FeatureDispatcher
}

void DeviceModel::setSmartShift(bool enabled, int threshold)
{
    qDebug() << "[DeviceModel] setSmartShift requested: enabled=" << enabled << "threshold=" << threshold;
    // Actual HID++ SmartShift push flows through DeviceManager/FeatureDispatcher
}

void DeviceModel::resetAllProfiles()
{
    qDebug() << "[DeviceModel] resetAllProfiles requested";
    // Delegate to ProfileEngine to wipe and reload default profiles
}

void DeviceModel::setCurrentDPI(int dpi)
{
    if (m_currentDPI == dpi)
        return;
    m_currentDPI = dpi;
    emit currentDPIChanged();
}

void DeviceModel::setSmartShiftState(bool enabled, int threshold)
{
    bool changed = false;
    if (m_smartShiftEnabled != enabled) {
        m_smartShiftEnabled = enabled;
        emit smartShiftEnabledChanged();
        changed = true;
    }
    if (m_smartShiftThreshold != threshold) {
        m_smartShiftThreshold = threshold;
        emit smartShiftThresholdChanged();
        changed = true;
    }
    Q_UNUSED(changed);
}

void DeviceModel::setActiveProfileName(const QString &name)
{
    if (m_activeProfileName == name)
        return;
    m_activeProfileName = name;
    emit activeProfileNameChanged();
}

} // namespace logitune
