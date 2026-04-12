#include "DeviceRegistry.h"
#include "devices/JsonDevice.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QStandardPaths>

namespace logitune {

DeviceRegistry::DeviceRegistry() {
    loadDirectory(systemDevicesDir());
    loadDirectory(cacheDevicesDir());
    loadDirectory(userDevicesDir());
    qCInfo(lcDevice) << "DeviceRegistry: loaded" << m_devices.size() << "devices";
}

void DeviceRegistry::loadDirectory(const QString &dir) {
    QDir d(dir);
    if (!d.exists()) return;
    for (const auto &entry : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto device = JsonDevice::load(d.filePath(entry));
        if (device)
            registerDevice(std::move(device));
    }
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

QString DeviceRegistry::systemDevicesDir() {
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto &p : paths) {
        QString dir = p + "/logitune/devices";
        if (QDir(dir).exists())
            return dir;
    }
    return "/usr/share/logitune/devices";
}

QString DeviceRegistry::cacheDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
           + "/logitune/devices";
}

QString DeviceRegistry::userDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/logitune/devices";
}

} // namespace logitune
