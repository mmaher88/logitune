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
    connect(dm, &DeviceManager::currentDPIChanged,
            this, &DeviceModel::currentDPIChanged);
    connect(dm, &DeviceManager::smartShiftChanged,
            this, [this]() {
                emit smartShiftEnabledChanged();
                emit smartShiftThresholdChanged();
            });
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
    return m_dm ? m_dm->currentDPI() : m_currentDPI;
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
    return m_dm ? m_dm->smartShiftEnabled() : m_smartShiftEnabled;
}

int DeviceModel::smartShiftThreshold() const
{
    return m_dm ? m_dm->smartShiftThreshold() : m_smartShiftThreshold;
}

QString DeviceModel::activeProfileName() const
{
    return m_activeProfileName;
}

void DeviceModel::setDPI(int value)
{
    if (m_dm) m_dm->setDPI(value);
}

void DeviceModel::setSmartShift(bool enabled, int threshold)
{
    if (m_dm) m_dm->setSmartShift(enabled, threshold);
}

void DeviceModel::resetAllProfiles()
{
    qDebug() << "[DeviceModel] resetAllProfiles requested";
}

void DeviceModel::setCurrentDPI(int dpi)
{
    if (m_currentDPI == dpi) return;
    m_currentDPI = dpi;
    emit currentDPIChanged();
}

void DeviceModel::setSmartShiftState(bool enabled, int threshold)
{
    if (m_smartShiftEnabled != enabled) {
        m_smartShiftEnabled = enabled;
        emit smartShiftEnabledChanged();
    }
    if (m_smartShiftThreshold != threshold) {
        m_smartShiftThreshold = threshold;
        emit smartShiftThresholdChanged();
    }
}

void DeviceModel::setActiveProfileName(const QString &name)
{
    if (m_activeProfileName == name) return;
    m_activeProfileName = name;
    emit activeProfileNameChanged();
}

} // namespace logitune
