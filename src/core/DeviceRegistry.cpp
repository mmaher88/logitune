#include "DeviceRegistry.h"
#include "devices/JsonDevice.h"
#include "logging/LogManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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
        if (name.compare(dev->deviceName(), Qt::CaseInsensitive) == 0)
            return dev.get();
    }
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

void DeviceRegistry::reloadAll()
{
    m_devices.clear();
    loadDirectory(systemDevicesDir());
    loadDirectory(cacheDevicesDir());
    loadDirectory(userDevicesDir());
    qCInfo(lcDevice) << "DeviceRegistry: reloaded" << m_devices.size() << "devices";
}

QString DeviceRegistry::systemDevicesDir() {
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto &p : paths) {
        QString dir = p + "/logitune/devices";
        if (QDir(dir).exists())
            return dir;
    }
    if (QCoreApplication::instance()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString devDir = appDir + "/../../../devices";
        if (QDir(devDir).exists())
            return QDir(devDir).canonicalPath();
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

const IDevice* DeviceRegistry::findBySourcePath(const QString &dirPath) const {
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    for (const auto &dev : m_devices) {
        if (auto *jd = dynamic_cast<const JsonDevice*>(dev.get()))
            if (jd->sourcePath() == canonical)
                return dev.get();
    }
    return nullptr;
}

bool DeviceRegistry::reload(const QString &dirPath) {
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    for (auto &dev : m_devices) {
        if (auto *jd = dynamic_cast<JsonDevice*>(dev.get())) {
            if (jd->sourcePath() == canonical)
                return jd->refresh();
        }
    }
    return false;
}

} // namespace logitune
