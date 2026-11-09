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

namespace {

// Exact (case-insensitive) name match first. Otherwise the candidate with the
// longest name contained in `name`, so "MX Master 3S For Business" prefers
// "MX Master 3S" over "MX Master 3" regardless of load order.
const IDevice* bestNameMatch(const std::vector<const IDevice*> &candidates,
                             const QString &name) {
    if (name.isEmpty())
        return nullptr;
    for (const IDevice *dev : candidates) {
        if (name.compare(dev->deviceName(), Qt::CaseInsensitive) == 0)
            return dev;
    }
    const IDevice *best = nullptr;
    for (const IDevice *dev : candidates) {
        const QString devName = dev->deviceName();
        if (devName.isEmpty() || !name.contains(devName, Qt::CaseInsensitive))
            continue;
        if (!best || devName.size() > best->deviceName().size())
            best = dev;
    }
    return best;
}

} // namespace

const IDevice* DeviceRegistry::findByName(const QString &name) const {
    std::vector<const IDevice*> all;
    all.reserve(m_devices.size());
    for (const auto &dev : m_devices)
        all.push_back(dev.get());
    return bestNameMatch(all, name);
}

const IDevice* DeviceRegistry::findByPidAndName(uint16_t pid, const QString &name) const {
    std::vector<const IDevice*> byPid;
    for (const auto &dev : m_devices) {
        if (dev->matchesPid(pid))
            byPid.push_back(dev.get());
    }
    if (byPid.empty())
        return findByName(name);
    if (byPid.size() == 1)
        return byPid.front();
    if (const IDevice *named = bestNameMatch(byPid, name))
        return named;
    return byPid.front();
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
