#pragma once
#include "interfaces/IDevice.h"
#include <memory>
#include <vector>
#include <QString>

namespace logitune {

class DeviceRegistry {
public:
    DeviceRegistry();

    // First descriptor listing `pid`. Some descriptors share a PID (MX Vertical
    // and MX Vertical for Business); use findByPidAndName() to pick between them.
    const IDevice* findByPid(uint16_t pid) const;
    // Descriptor whose name equals `name` (case-insensitive), else the one
    // with the longest name contained in `name`.
    const IDevice* findByName(const QString &name) const;
    // Descriptor listing `pid`, using `name` to choose when several share the
    // PID. Falls back to findByName() when no descriptor lists `pid`.
    const IDevice* findByPidAndName(uint16_t pid, const QString &name) const;
    void registerDevice(std::unique_ptr<IDevice> device);
    const std::vector<std::unique_ptr<IDevice>>& devices() const;
    void reloadAll();
    const IDevice* findBySourcePath(const QString &dirPath) const;
    bool reload(const QString &dirPath);

    static QString systemDevicesDir();
    static QString cacheDevicesDir();
    static QString userDevicesDir();

private:
    void loadDirectory(const QString &dir);
    std::vector<std::unique_ptr<IDevice>> m_devices;
};

} // namespace logitune
