#include "ActiveDeviceResolver.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "models/DeviceModel.h"

namespace logitune {

ActiveDeviceResolver::ActiveDeviceResolver(DeviceModel *deviceModel, QObject *parent)
    : QObject(parent)
    , m_deviceModel(deviceModel)
{}

PhysicalDevice *ActiveDeviceResolver::activeDevice() const
{
    if (!m_deviceModel) return nullptr;
    const int idx = m_deviceModel->selectedIndex();
    const auto &devices = m_deviceModel->devices();
    if (idx < 0 || idx >= devices.size()) return nullptr;
    return devices[idx];
}

DeviceSession *ActiveDeviceResolver::activeSession() const
{
    auto *d = activeDevice();
    return d ? d->primary() : nullptr;
}

QString ActiveDeviceResolver::activeSerial() const
{
    auto *d = activeDevice();
    return d ? d->deviceSerial() : QString();
}

void ActiveDeviceResolver::onSelectionIndexChanged()
{
    emit selectionChanged();
}

} // namespace logitune
