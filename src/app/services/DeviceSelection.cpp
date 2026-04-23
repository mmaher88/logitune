#include "DeviceSelection.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "models/DeviceModel.h"

namespace logitune {

DeviceSelection::DeviceSelection(DeviceModel *deviceModel, QObject *parent)
    : QObject(parent)
    , m_deviceModel(deviceModel)
{}

PhysicalDevice *DeviceSelection::activeDevice() const
{
    if (!m_deviceModel) return nullptr;
    const int idx = m_deviceModel->selectedIndex();
    const auto &devices = m_deviceModel->devices();
    if (idx < 0 || idx >= devices.size()) return nullptr;
    return devices[idx];
}

DeviceSession *DeviceSelection::activeSession() const
{
    auto *d = activeDevice();
    return d ? d->primary() : nullptr;
}

QString DeviceSelection::activeSerial() const
{
    auto *d = activeDevice();
    return d ? d->deviceSerial() : QString();
}

void DeviceSelection::onSelectionIndexChanged()
{
    emit selectionChanged();
}

} // namespace logitune
