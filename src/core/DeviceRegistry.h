#pragma once
#include "interfaces/IDevice.h"
#include <memory>
#include <vector>
#include <QString>

namespace logitune {

class DeviceRegistry {
public:
    DeviceRegistry();

    const IDevice* findByPid(uint16_t pid) const;
    const IDevice* findByName(const QString &name) const;
    void registerDevice(std::unique_ptr<IDevice> device);
    const std::vector<std::unique_ptr<IDevice>>& devices() const;
    void reloadAll();

    static QString systemDevicesDir();
    static QString cacheDevicesDir();
    static QString userDevicesDir();

private:
    void loadDirectory(const QString &dir);
    std::vector<std::unique_ptr<IDevice>> m_devices;
};

} // namespace logitune
