#include "DeviceRegistry.h"
#include "devices/MxMaster2sDescriptor.h"
#include "devices/MxMaster3sDescriptor.h"
#include "devices/MxMaster4Descriptor.h"

namespace logitune {

DeviceRegistry::DeviceRegistry() {
    registerDevice(std::make_unique<MxMaster2sDescriptor>());
    registerDevice(std::make_unique<MxMaster3sDescriptor>());
    registerDevice(std::make_unique<MxMaster4Descriptor>());
}

const IDevice* DeviceRegistry::findByPid(uint16_t pid) const {
    for (const auto &dev : m_devices) {
        if (dev->matchesPid(pid))
            return dev.get();
    }
    return nullptr;
}

const IDevice* DeviceRegistry::findByName(const QString &name) const {
    for (const auto &dev : m_devices) {
        if (name.contains(dev->deviceName(), Qt::CaseInsensitive))
            return dev.get();
    }
    return nullptr;
}

void DeviceRegistry::registerDevice(std::unique_ptr<IDevice> device) {
    m_devices.push_back(std::move(device));
}

const std::vector<std::unique_ptr<IDevice>>& DeviceRegistry::devices() const {
    return m_devices;
}

} // namespace logitune
