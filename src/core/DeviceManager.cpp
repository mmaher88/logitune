#include "DeviceManager.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
#include "logging/LogManager.h"

#include <QDateTime>
#include <QFile>
#include <QMap>
#include <QSocketNotifier>
#include <QTimer>

#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

namespace logitune {

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

bool DeviceManager::isReceiver(uint16_t pid)
{
    return pid == hidpp::kPidBoltReceiver || pid == hidpp::kPidUnifyReceiver;
}

uint8_t DeviceManager::deviceIndexForDirect()
{
    return hidpp::kDeviceIndexDirect;
}

uint8_t DeviceManager::deviceIndexForReceiver(int slot)
{
    return static_cast<uint8_t>(slot);
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DeviceManager::DeviceManager(DeviceRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
}

DeviceManager::~DeviceManager()
{
    m_sessions.clear();

    if (m_receiverNotifier) {
        m_receiverNotifier->setEnabled(false);
        delete m_receiverNotifier;
        m_receiverNotifier = nullptr;
    }
    m_receiverDevice.reset();

    if (m_udevNotifier) {
        m_udevNotifier->setEnabled(false);
        delete m_udevNotifier;
        m_udevNotifier = nullptr;
    }
    if (m_udevMon) {
        udev_monitor_unref(m_udevMon);
        m_udevMon = nullptr;
    }
    if (m_udev) {
        udev_unref(m_udev);
        m_udev = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Session access
// ---------------------------------------------------------------------------

const std::vector<std::unique_ptr<DeviceSession>>& DeviceManager::sessions() const
{
    return m_sessions;
}

DeviceSession* DeviceManager::sessionById(const QString &id) const
{
    for (auto &s : m_sessions) {
        if (s->deviceId() == id)
            return s.get();
    }
    return nullptr;
}

DeviceSession* DeviceManager::sessionByPid(uint16_t pid) const
{
    for (auto &s : m_sessions) {
        if (s->devicePid() == pid)
            return s.get();
    }
    return nullptr;
}

QList<PhysicalDevice *> DeviceManager::physicalDevices() const
{
    QList<PhysicalDevice *> result;
    for (const auto &p : m_physicalDevices)
        result.append(p.second.get());
    return result;
}

PhysicalDevice *DeviceManager::physicalDeviceBySerial(const QString &serial) const
{
    auto it = m_physicalDevices.find(serial);
    return it != m_physicalDevices.end() ? it->second.get() : nullptr;
}

const IDevice* DeviceManager::activeDevice() const
{
    if (!m_sessions.empty())
        return m_sessions.front()->descriptor();
    return nullptr;
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

void DeviceManager::start()
{
    m_udev = udev_new();
    if (!m_udev) {
        qCWarning(lcDevice) << "failed to create udev context";
        return;
    }

    m_udevMon = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_udevMon) {
        qCWarning(lcDevice) << "failed to create udev monitor";
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(m_udevMon, "hidraw", nullptr);
    udev_monitor_enable_receiving(m_udevMon);

    int fd = udev_monitor_get_fd(m_udevMon);
    m_udevNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_udevNotifier, &QSocketNotifier::activated, this, &DeviceManager::onUdevReady);

    scanExistingDevices();
}

// ---------------------------------------------------------------------------
// scanExistingDevices()
// ---------------------------------------------------------------------------

void DeviceManager::scanExistingDevices()
{
    qCDebug(lcDevice) << "scanning existing hidraw devices...";

    struct udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    if (!enumerate)
        return;

    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;
    int count = 0;

    udev_list_entry_foreach(entry, devices) {
        const char *syspath = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(m_udev, syspath);
        if (!dev)
            continue;

        count++;
        const char *devNode = udev_device_get_devnode(dev);

        struct udev_device *hiddev = udev_device_get_parent_with_subsystem_devtype(
            dev, "hid", nullptr);

        bool isLogitech = false;
        uint16_t productId = 0;
        if (hiddev) {
            const char *vid = udev_device_get_property_value(hiddev, "HID_ID");
            if (vid) {
                unsigned bus = 0, vendorId = 0, pid = 0;
                if (sscanf(vid, "%x:%x:%x", &bus, &vendorId, &pid) == 3) {
                    isLogitech = (vendorId == hidpp::kVendorLogitech);
                    productId = static_cast<uint16_t>(pid);
                }
            }
        }

        if (isLogitech && devNode) {
            qCDebug(lcDevice) << "found Logitech device:" << devNode
                              << "PID:" << Qt::hex << productId
                              << (isReceiver(productId) ? "(receiver)" : "(direct)");
            probeDevice(QString::fromUtf8(devNode));
        }

        udev_device_unref(dev);
    }

    qCDebug(lcDevice) << "scan complete:" << count << "hidraw devices,"
                      << m_sessions.size() << "sessions active";
    udev_enumerate_unref(enumerate);
}

// ---------------------------------------------------------------------------
// onUdevReady()
// ---------------------------------------------------------------------------

void DeviceManager::onUdevReady()
{
    struct udev_device *dev = udev_monitor_receive_device(m_udevMon);
    if (!dev)
        return;

    const char *action  = udev_device_get_action(dev);
    const char *devNode = udev_device_get_devnode(dev);

    if (action && devNode) {
        onUdevEvent(QString::fromUtf8(action), QString::fromUtf8(devNode));
    }

    udev_device_unref(dev);
}

// ---------------------------------------------------------------------------
// onUdevEvent()
// ---------------------------------------------------------------------------

void DeviceManager::onUdevEvent(const QString &action, const QString &devNode)
{
    if (action == QLatin1String("add")) {
        if (!m_availableTransports.contains(devNode))
            m_availableTransports.append(devNode);

        QTimer::singleShot(1000, this, [this, devNode]() {
            probeDevice(devNode);
        });
    } else if (action == QLatin1String("remove")) {
        m_availableTransports.removeAll(devNode);
        removeSessionByDevNode(devNode);
    }
}

// ---------------------------------------------------------------------------
// removeSessionByDevNode()
// ---------------------------------------------------------------------------

void DeviceManager::removeSessionByDevNode(const QString &devNode)
{
    auto it = m_devNodeToSession.find(devNode);
    if (it == m_devNodeToSession.end())
        return;

    DeviceSession *target = it.value();
    m_devNodeToSession.erase(it);

    const QString serial = target->deviceId();

    // Detach this transport from its PhysicalDevice. If it was the last
    // transport for that device, destroy the PhysicalDevice (after emitting
    // physicalDeviceRemoved so listeners can drop their references while
    // the pointer is still valid).
    auto pdIt = m_physicalDevices.find(serial);
    if (pdIt != m_physicalDevices.end()) {
        PhysicalDevice *pd = pdIt->second.get();
        const bool empty = pd->detachTransport(target);
        if (empty) {
            emit physicalDeviceRemoved(pd);
            m_physicalDevices.erase(pdIt);
        }
    }

    for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
        if (sit->get() == target) {
            (*sit)->disconnectCleanup();
            m_sessions.erase(sit);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// probeDevice()
// ---------------------------------------------------------------------------

void DeviceManager::probeDevice(const QString &devNode)
{
    // Skip if we already have a session from this devNode
    if (m_devNodeToSession.contains(devNode))
        return;

    qCDebug(lcDevice) << "probing" << devNode;

    // Check report descriptor for HID++
    {
        QString hidrawName = devNode.mid(devNode.lastIndexOf('/') + 1);
        QString sysfsDesc = QString("/sys/class/hidraw/%1/device/report_descriptor").arg(hidrawName);
        QFile descFile(sysfsDesc);
        if (descFile.open(QIODevice::ReadOnly)) {
            QByteArray desc = descFile.readAll();
            bool hasHidpp = false;
            for (int i = 0; i + 1 < desc.size(); ++i) {
                if (static_cast<uint8_t>(desc[i]) == 0x85 && static_cast<uint8_t>(desc[i + 1]) == 0x11) {
                    hasHidpp = true;
                    break;
                }
            }
            if (!hasHidpp) {
                qCDebug(lcDevice) << devNode << "no HID++ report ID in descriptor, skipping";
                return;
            }
            qCDebug(lcDevice) << devNode << "has HID++ report ID";
        }
    }

    auto device = std::make_unique<hidpp::HidrawDevice>(devNode);
    if (!device->open()) {
        qCDebug(lcDevice) << "cannot open" << devNode;
        return;
    }

    auto info = device->info();
    qCDebug(lcDevice) << "opened" << devNode << "vendor:" << Qt::hex << info.vendorId << "product:" << info.productId;
    if (info.vendorId != hidpp::kVendorLogitech)
        return;

    uint16_t pid = info.productId;
    uint8_t deviceIndex = 0xFF;
    QString connType;

    if (isReceiver(pid)) {
        connType = (pid == hidpp::kPidBoltReceiver) ? QStringLiteral("Bolt")
                                                    : QStringLiteral("Unifying");

        bool found = false;
        for (int slot = 1; slot <= 6; ++slot) {
            uint8_t ping[20] = {};
            ping[0] = 0x11;
            ping[1] = static_cast<uint8_t>(slot);
            ping[2] = 0x00;
            ping[3] = 0x0A;

            qCDebug(lcDevice) << "pinging slot" << slot << "on" << devNode;
            int written = device->writeReport(std::span<const uint8_t>(ping, 20));
            if (written < 0)
                continue;

            auto resp = device->readReport(2000);
            if (resp.empty())
                continue;

            if (resp.size() >= 4 && resp[1] == static_cast<uint8_t>(slot)) {
                bool isError = false;
                if (resp.size() >= 7 && resp[2] == 0x8F) {
                    isError = (resp[5] != 0x00);
                }
                if (resp.size() >= 7 && resp[0] == 0x11 && resp[2] == 0xFF) {
                    isError = true;
                }

                if (!isError) {
                    deviceIndex = static_cast<uint8_t>(slot);
                    found = true;
                    qCDebug(lcDevice) << "found device at slot" << slot
                                      << "(response:" << QByteArray(reinterpret_cast<const char*>(resp.data()),
                                                                     static_cast<int>(resp.size())).toHex() << ")";
                    break;
                }
            }

            device->readReport(50);
        }

        if (!found) {
            qCDebug(lcDevice) << "no device on any slot of" << devNode << "— keeping receiver open for DJ notifications";
            m_receiverDevice = std::move(device);
            if (m_receiverNotifier) {
                delete m_receiverNotifier;
                m_receiverNotifier = nullptr;
            }
            m_receiverNotifier = new QSocketNotifier(m_receiverDevice->fd(), QSocketNotifier::Read, this);
            connect(m_receiverNotifier, &QSocketNotifier::activated, this, [this, devNode]() {
                if (!m_receiverDevice) return;
                auto bytes = m_receiverDevice->readReport(0);
                if (bytes.empty()) {
                    if (m_receiverNotifier) {
                        m_receiverNotifier->setEnabled(false);
                        delete m_receiverNotifier;
                        m_receiverNotifier = nullptr;
                    }
                    m_receiverDevice.reset();
                    return;
                }

                if (bytes.size() >= 3 && bytes[1] >= 1 && bytes[1] <= 6) {
                    uint8_t slot = bytes[1];
                    qCDebug(lcDevice) << "receiver got data from slot" << slot
                                      << "— device arrived on receiver";
                    if (m_receiverNotifier) {
                        m_receiverNotifier->setEnabled(false);
                        delete m_receiverNotifier;
                        m_receiverNotifier = nullptr;
                    }
                    m_receiverDevice.reset();
                    QTimer::singleShot(500, this, [this, devNode]() {
                        probeDevice(devNode);
                    });
                }
            });
            return;
        }
    } else {
        // Check if this is a known direct device
        bool isDirect = m_registry && m_registry->findByPid(pid) != nullptr;
        if (!isDirect)
            return;
        deviceIndex = deviceIndexForDirect();
        connType = QStringLiteral("Bluetooth");
    }

    if (!m_availableTransports.contains(devNode))
        m_availableTransports.append(devNode);

    // Create session
    auto session = std::make_unique<DeviceSession>(
        std::move(device), deviceIndex, connType, m_registry, this);

    // Set up hidraw notification routing
    auto *sessionPtr = session.get();
    auto *notifier = new QSocketNotifier(sessionPtr->device()->fd(), QSocketNotifier::Read, sessionPtr);
    connect(notifier, &QSocketNotifier::activated, sessionPtr, [sessionPtr]() {
        if (!sessionPtr->device()) return;
        auto bytes = sessionPtr->device()->readReport(0);
        if (bytes.empty())
            return;
        QString hex;
        for (size_t i = 0; i < bytes.size() && i < 20; ++i)
            hex += QString("%1 ").arg(bytes[i], 2, 16, QChar('0'));
        qCDebug(lcHidpp) << "raw hidraw:" << hex.trimmed();

        auto report = hidpp::Report::parse(bytes);
        if (!report)
            return;
        sessionPtr->handleNotification(*report);
    });

    // Forward unknownDeviceDetected from session
    connect(sessionPtr, &DeviceSession::unknownDeviceDetected,
            this, &DeviceManager::unknownDeviceDetected);

    // Enumerate features and read initial state
    sessionPtr->enumerateAndSetup();

    QString serial = sessionPtr->deviceId();
    m_devNodeToSession[devNode] = sessionPtr;
    m_sessions.push_back(std::move(session));

    // Attach transport to its PhysicalDevice (create if first seen).
    auto pdIt = m_physicalDevices.find(serial);
    if (pdIt == m_physicalDevices.end()) {
        auto pd = std::make_unique<PhysicalDevice>(serial, this);
        pd->attachTransport(sessionPtr);
        PhysicalDevice *pdPtr = pd.get();
        m_physicalDevices.emplace(serial, std::move(pd));
        emit physicalDeviceAdded(pdPtr);
    } else {
        pdIt->second->attachTransport(sessionPtr);
    }
}

} // namespace logitune
