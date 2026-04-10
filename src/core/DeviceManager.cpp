#include "DeviceManager.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
#include "hidpp/features/Battery.h"
#include "hidpp/features/DeviceName.h"
#include "hidpp/features/AdjustableDPI.h"
#include "hidpp/features/SmartShift.h"
#include "hidpp/features/HiResWheel.h"
#include "hidpp/features/ReprogControls.h"
#include "hidpp/features/ThumbWheel.h"
#include "logging/LogManager.h"

#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QHash>
#include <QSocketNotifier>
#include <QTimer>

#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>

namespace logitune {

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

bool DeviceManager::isReceiver(uint16_t pid)
{
    return pid == hidpp::kPidBoltReceiver || pid == hidpp::kPidUnifyReceiver;
}

bool DeviceManager::isDirectDevice(uint16_t pid) const
{
    return m_registry && m_registry->findByPid(pid) != nullptr;
}

uint8_t DeviceManager::deviceIndexForDirect()
{
    return hidpp::kDeviceIndexDirect; // 0xFF
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
    // Undivert all buttons before shutdown to restore native behavior
    if (m_connected && m_features && m_transport &&
        m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4) && m_activeDevice) {
        for (const auto &ctrl : m_activeDevice->controls()) {
            if (ctrl.configurable && ctrl.controlId != 0) {
                auto params = hidpp::features::ReprogControls::buildSetDivert(ctrl.controlId, false);
                m_features->call(m_transport.get(), m_deviceIndex,
                                 hidpp::FeatureId::ReprogControlsV4,
                                 hidpp::features::ReprogControls::kFnSetControlReporting,
                                 std::span<const uint8_t>(params));
            }
        }
    }

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
// start()
// ---------------------------------------------------------------------------

void DeviceManager::start()
{
    // Initialize libudev
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

        // Get parent HID device for vendor/product info
        struct udev_device *hiddev = udev_device_get_parent_with_subsystem_devtype(
            dev, "hid", nullptr);

        bool isLogitech = false;
        uint16_t productId = 0;
        if (hiddev) {
            const char *vid = udev_device_get_property_value(hiddev, "HID_ID");
            if (vid) {
                // HID_ID is "BUS:VID:PID" in hex
                unsigned bus = 0, vendorId = 0, pid = 0;
                if (sscanf(vid, "%x:%x:%x", &bus, &vendorId, &pid) == 3) {
                    isLogitech = (vendorId == hidpp::kVendorLogitech);
                    productId = static_cast<uint16_t>(pid);
                }
            }
        }

        if (isLogitech) {
            qCDebug(lcDevice) << "found Logitech device:" << devNode
                              << "PID:" << Qt::hex << productId
                              << (isReceiver(productId) ? "(receiver)" : "(direct)");
            if (devNode && !m_connected) {
                probeDevice(QString::fromUtf8(devNode));
            }
        }

        udev_device_unref(dev);
    }

    qCDebug(lcDevice) << "scan complete:" << count << "hidraw devices," << (m_connected ? "connected" : "no device connected");
    udev_enumerate_unref(enumerate);
}

// ---------------------------------------------------------------------------
// onUdevReady() — slot called when udev fd becomes readable
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
        // Track available transports for failover
        if (!m_availableTransports.contains(devNode))
            m_availableTransports.append(devNode);

        if (!m_connected) {
            // Delay probe slightly — the device may still be initializing,
            // especially after a transport switch (Bolt -> Bluetooth).
            QTimer::singleShot(1000, this, [this, devNode]() {
                if (!m_connected)
                    probeDevice(devNode);
            });
        } else {
            // Already connected — but the new device might be the same mouse on a different transport.
            // Ping the current device to check if it's still alive.
            if (m_device && m_transport && m_features &&
                m_features->hasFeature(hidpp::FeatureId::Root)) {
                hidpp::Report ping;
                ping.reportId = hidpp::kLongReportId;
                ping.deviceIndex = m_deviceIndex;
                ping.featureIndex = 0x00;
                ping.functionId = 0;
                ping.softwareId = 0x0A;
                auto resp = m_transport->sendRequest(ping, 500);
                if (!resp.has_value()) {
                    qCDebug(lcDevice) << "current device unresponsive, switching to" << devNode;
                    disconnectDevice();
                    probeDevice(devNode);
                }
            }
        }
    } else if (action == QLatin1String("remove")) {
        m_availableTransports.removeAll(devNode);

        if (m_device && m_device->info().path == devNode) {
            // Current device disconnected — try failover
            QString failoverPath;
            for (const QString &path : m_availableTransports) {
                if (path != devNode) {
                    failoverPath = path;
                    break;
                }
            }

            disconnectDevice();

            if (!failoverPath.isEmpty()) {
                probeDevice(failoverPath);
                if (m_connected)
                    emit transportSwitched(m_connectionType);
            } else {
                emit deviceDisconnected();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// probeDevice()
// ---------------------------------------------------------------------------

void DeviceManager::probeDevice(const QString &devNode)
{
    qCDebug(lcDevice) << "probing" << devNode;

    // Check report descriptor via sysfs BEFORE opening the device.
    // Opening the wrong hidraw interface poisons writes to sibling interfaces.
    {
        // devNode is e.g. "/dev/hidraw8" → sysfs path is "/sys/class/hidraw/hidraw8/device/report_descriptor"
        QString hidrawName = devNode.mid(devNode.lastIndexOf('/') + 1); // "hidraw8"
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
    if (info.vendorId != hidpp::kVendorLogitech) {
        return;
    }

    uint16_t pid = info.productId;
    uint8_t deviceIndex = 0xFF;
    QString connType;

    if (isReceiver(pid)) {
        connType = (pid == hidpp::kPidBoltReceiver) ? QStringLiteral("Bolt")
                                                    : QStringLiteral("Unifying");

        // The sysfs descriptor check above already filtered to the correct interface.

        // Probe device indices 1-6 with raw write+read.
        // The Bolt receiver may respond with HID++ 1.0 (short) or 2.0 (long) reports.
        // We accept ANY response from the correct device index as "device present."
        bool found = false;
        for (int slot = 1; slot <= 6; ++slot) {
            // Build a HID++ 2.0 Root ping (long report)
            uint8_t ping[20] = {};
            ping[0] = 0x11;  // long report
            ping[1] = static_cast<uint8_t>(slot);
            ping[2] = 0x00;  // Root feature index
            ping[3] = 0x0A;  // functionId=0, softwareId=0x0A

            qCDebug(lcDevice) << "pinging slot" << slot << "on" << devNode;
            int written = device->writeReport(std::span<const uint8_t>(ping, 20));
            if (written < 0)
                continue;

            // Read response — accept short (7b) or long (20b), any format
            auto resp = device->readReport(2000);
            if (resp.empty())
                continue;

            // Check if the response is from this device index
            if (resp.size() >= 4 && resp[1] == static_cast<uint8_t>(slot)) {
                // Got a response from this slot — device is present
                // Check it's not an error indicating "no device"
                // HID++ 1.0 error: byte2=0x8F, byte4=0x00 means no error
                // HID++ 2.0 error: byte2=0xFF means error
                bool isError = false;
                if (resp.size() >= 7 && resp[2] == 0x8F) {
                    // HID++ 1.0 error: byte5 is error code
                    // 0x00 = no error, anything else = error (0x09 = device not present)
                    isError = (resp[5] != 0x00);
                }
                if (resp.size() >= 7 && resp[0] == 0x11 && resp[2] == 0xFF) {
                    isError = true; // HID++ 2.0 error
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

            // Drain any extra data (notifications etc)
            device->readReport(50);
        }

        if (!found) {
            qCDebug(lcDevice) << "no device on any slot of" << devNode << "— keeping receiver open for DJ notifications";
            // Keep receiver open separately so it survives if another transport connects
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
                    // fd error (device removed) — clean up
                    if (m_receiverNotifier) {
                        m_receiverNotifier->setEnabled(false);
                        delete m_receiverNotifier;
                        m_receiverNotifier = nullptr;
                    }
                    m_receiverDevice.reset();
                    return;
                }
                // Any HID++ traffic from a device index 1-6 means a device is present
                if (bytes.size() >= 3 && bytes[1] >= 1 && bytes[1] <= 6) {
                    uint8_t slot = bytes[1];
                    qCDebug(lcDevice) << "receiver got data from slot" << slot
                                      << "— device arrived on receiver, switching transport";
                    // Disable receiver notifier
                    if (m_receiverNotifier) {
                        m_receiverNotifier->setEnabled(false);
                        delete m_receiverNotifier;
                        m_receiverNotifier = nullptr;
                    }
                    m_receiverDevice.reset();
                    // Disconnect current transport (if any) and switch
                    if (m_connected)
                        disconnectDevice();
                    // Delay to let device finish sending wake-up notifications
                    QTimer::singleShot(500, this, [this, devNode]() {
                        probeDevice(devNode);
                    });
                }
            });
            return;
        }
    } else if (isDirectDevice(pid)) {
        deviceIndex = deviceIndexForDirect();
        connType = QStringLiteral("Bluetooth");
    } else {
        // Unknown Logitech device — skip
        return;
    }

    // Store device path for failover tracking
    if (!m_availableTransports.contains(devNode))
        m_availableTransports.append(devNode);

    // Take ownership
    m_device = std::move(device);
    m_deviceIndex = deviceIndex;
    m_connectionType = connType;

    // Create transport (owned by DeviceManager, lives on main thread initially)
    m_transport = std::make_unique<hidpp::Transport>(m_device.get(), nullptr);

    // Connect notification handler
    connect(m_transport.get(), &hidpp::Transport::notificationReceived,
            this, &DeviceManager::handleNotification);
    connect(m_transport.get(), &hidpp::Transport::deviceDisconnected,
            this, &DeviceManager::disconnectDevice);

    // Enumerate features and read initial state.
    // This emits deviceSetupComplete at the end, which triggers profile application.
    // The command queue is created inside enumerateAndSetup after features are populated.
    enumerateAndSetup();

    // Kernel-driven notification: QSocketNotifier fires when hidraw has data.
    // Always recreate — the fd changes on transport switch (Bolt -> BT).
    if (m_hidrawNotifier) {
        m_hidrawNotifier->setEnabled(false);
        delete m_hidrawNotifier;
        m_hidrawNotifier = nullptr;
    }
    m_hidrawNotifier = new QSocketNotifier(m_device->fd(), QSocketNotifier::Read, this);
    connect(m_hidrawNotifier, &QSocketNotifier::activated, this, [this]() {
            if (!m_device) return;
            auto bytes = m_device->readReport(0);
            if (bytes.empty())
                return;
            // Log raw bytes for protocol debugging
            QString hex;
            for (size_t i = 0; i < bytes.size() && i < 20; ++i)
                hex += QString("%1 ").arg(bytes[i], 2, 16, QChar('0'));
            qCDebug(lcHidpp) << "raw hidraw:" << hex.trimmed();

            auto report = hidpp::Report::parse(bytes);
            if (!report)
                return;
            handleNotification(*report);
        });
    m_hidrawNotifier->setEnabled(true);
}

// ---------------------------------------------------------------------------
// enumerateAndSetup()
// ---------------------------------------------------------------------------

void DeviceManager::enumerateAndSetup()
{
    if (m_enumerating) return;
    m_enumerating = true;
    qCDebug(lcDevice) << "enumerateAndSetup: deviceIndex=" << Qt::hex << m_deviceIndex;
    m_features = std::make_unique<hidpp::FeatureDispatcher>();

    bool ok = m_features->enumerate(m_transport.get(), m_deviceIndex);
    if (!ok) {
        qCWarning(lcDevice) << "feature enumeration failed";
        m_enumerating = false;
        return;
    } else {
        qCDebug(lcDevice) << "feature enumeration succeeded";
    }

    // Read device name
    QString name;
    if (m_features->hasFeature(hidpp::FeatureId::DeviceName)) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::DeviceName,
                                     hidpp::features::DeviceName::kFnGetNameLength);
        if (resp.has_value()) {
            int nameLen = hidpp::features::DeviceName::parseNameLength(*resp);
            for (int offset = 0; offset < nameLen; offset += 13) {
                // HID++ name chunk params: [offset_hi, offset_lo]
                uint8_t params[2] = {
                    static_cast<uint8_t>((offset >> 8) & 0xFF),
                    static_cast<uint8_t>(offset & 0xFF)
                };
                auto chunkResp = m_features->call(m_transport.get(), m_deviceIndex,
                                                  hidpp::FeatureId::DeviceName,
                                                  hidpp::features::DeviceName::kFnGetName,
                                                  params);
                if (chunkResp.has_value())
                    name += hidpp::features::DeviceName::parseNameChunk(*chunkResp);
            }
            name = name.left(nameLen);
        }
    }

    qCDebug(lcDevice) << "device name:" << name;

    if (name.isEmpty())
        name = QStringLiteral("Logitech Device");

    // Read serial number and firmware version from DeviceInfo (0x0003).
    // If feature enumeration missed it, try a direct Root query with retry.
    QString serial;
    QString firmwareVersion;
    qCDebug(lcDevice) << "hasDeviceInfo:" << m_features->hasFeature(hidpp::FeatureId::DeviceInfo)
                      << "hasFeatureSet:" << m_features->hasFeature(hidpp::FeatureId::FeatureSet);
    if (!m_features->hasFeature(hidpp::FeatureId::DeviceInfo)) {
        if (m_features->hasFeature(hidpp::FeatureId::FeatureSet)) {
            // getCount first
            auto countResp = m_features->call(m_transport.get(), m_deviceIndex,
                                               hidpp::FeatureId::FeatureSet, 0x00);
            int count = countResp.has_value() ? countResp->params[0] : 0;
            qCDebug(lcDevice) << "FeatureSet: device has" << count << "features";

            for (int i = 1; i <= count; ++i) {
                uint8_t idxParam[1] = {static_cast<uint8_t>(i)};
                auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                              hidpp::FeatureId::FeatureSet, 0x01, idxParam);
                if (!resp.has_value()) continue;
                uint16_t fid = (resp->params[0] << 8) | resp->params[1];
                if (fid == 0x0003) {
                    m_features->setFeatureIndex(hidpp::FeatureId::DeviceInfo, static_cast<uint8_t>(i));
                    qCDebug(lcDevice) << "DeviceInfo (0x0003) found at index" << i << "(via FeatureSet)";
                    break;
                }
            }
        }
    }
    if (m_features->hasFeature(hidpp::FeatureId::DeviceInfo)) {
        // Function 0x00: getEntityCount
        auto countResp = m_features->call(m_transport.get(), m_deviceIndex,
                                           hidpp::FeatureId::DeviceInfo, 0x00);
        int entityCount = countResp.has_value() ? countResp->params[0] : 3;

        // Function 0x01: getFwInfo(entityIdx) — iterate to find main firmware (type 0)
        // Response: [type, prefix0..2, ver_major, ver_minor, build_hi, build_lo, ...]
        for (int entity = 0; entity < entityCount; ++entity) {
            uint8_t entityParam[1] = {static_cast<uint8_t>(entity)};
            auto fwResp = m_features->call(m_transport.get(), m_deviceIndex,
                                            hidpp::FeatureId::DeviceInfo, 0x01, entityParam);
            if (!fwResp.has_value()) continue;

            uint8_t type = fwResp->params[0]; // 0=firmware, 1=bootloader, 2=hardware
            QString prefix;
            for (int i = 1; i <= 3; ++i) {
                char c = static_cast<char>(fwResp->params[i]);
                if (c) prefix += QChar::fromLatin1(c);
            }
            int major = fwResp->params[4];
            int minor = fwResp->params[5];
            int build = (fwResp->params[6] << 8) | fwResp->params[7];
            QString ver = QStringLiteral("%1 %2.%3.%4")
                .arg(prefix)
                .arg(major, 2, 16, QChar('0'))
                .arg(minor, 2, 16, QChar('0'))
                .arg(build, 4, 16, QChar('0'))
                .toUpper();
            qCDebug(lcDevice) << "entity" << entity << "type" << type << "firmware:" << ver;

            if (type == 0) // Main firmware
                firmwareVersion = ver;
        }

        // Function 0x02: getDeviceInfo — unit ID (serial), model ID
        // Response: [unitId0..3, transport0..1, modelId0..5, ...]
        auto infoResp = m_features->call(m_transport.get(), m_deviceIndex,
                                          hidpp::FeatureId::DeviceInfo, 0x02);
        if (infoResp.has_value()) {
            // Unit ID: ASCII chars in params (feature 0x0003 V4+), read until null
            QByteArray unitIdBytes;
            for (int i = 0; i < infoResp->paramLength; ++i) {
                char c = static_cast<char>(infoResp->params[i]);
                if (!c) break;
                unitIdBytes += c;
            }
            serial = QString::fromLatin1(unitIdBytes).toUpper();
            qCDebug(lcDevice) << "serial (unit ID):" << serial;
        }
    }

    // Fallback: hash of name for stable profile directory naming
    if (serial.isEmpty()) {
        serial = QString::number(qHash(name), 16);
        qCDebug(lcDevice) << "device id (hash fallback):" << serial;
    }

    // Read battery
    int battLevel = 0;
    bool battCharging = false;
    if (m_features->hasFeature(hidpp::FeatureId::BatteryUnified)) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::BatteryUnified,
                                     hidpp::features::Battery::kFnGetStatus);
        if (resp.has_value()) {
            auto status = hidpp::features::Battery::parseStatus(*resp);
            battLevel    = status.level;
            battCharging = status.charging;
        }
    }

    // Read DPI range and current value
    if (m_features->hasFeature(hidpp::FeatureId::AdjustableDPI)) {
        // Get DPI range
        auto rangeResp = m_features->call(m_transport.get(), m_deviceIndex,
                                          hidpp::FeatureId::AdjustableDPI,
                                          hidpp::features::AdjustableDPI::kFnGetSensorDpiList,
                                          std::array<uint8_t, 1>{0x00}); // sensor 0
        if (rangeResp.has_value()) {
            auto info = hidpp::features::AdjustableDPI::parseSensorDpiList(*rangeResp);
            m_minDPI = info.minDPI;
            m_maxDPI = info.maxDPI;
            m_dpiStep = info.stepDPI > 0 ? info.stepDPI : 50;
            qCDebug(lcDevice) << "DPI range:" << m_minDPI << "-" << m_maxDPI
                              << "step:" << m_dpiStep;
        }
        // Get current DPI
        auto dpiResp = m_features->call(m_transport.get(), m_deviceIndex,
                                        hidpp::FeatureId::AdjustableDPI,
                                        hidpp::features::AdjustableDPI::kFnGetSensorDpi,
                                        std::array<uint8_t, 1>{0x00});
        if (dpiResp.has_value()) {
            m_currentDPI = hidpp::features::AdjustableDPI::parseCurrentDPI(*dpiResp);
            qCDebug(lcDevice) << "current DPI:" << m_currentDPI;
        }
    }

    // Read SmartShift — try V1 (0x2110) first, fall back to Enhanced (0x2111)
    // V1: fn0=GetStatus, fn1=SetStatus
    // Enhanced: fn1=GetStatus, fn2=SetStatus (fn0 is getCapabilities)
    {
        hidpp::FeatureId ssFeature = hidpp::FeatureId::SmartShift;
        uint8_t ssGetFn = hidpp::features::SmartShift::kFnGetStatus;  // 0x00

        if (!m_features->hasFeature(hidpp::FeatureId::SmartShift) &&
            m_features->hasFeature(hidpp::FeatureId::SmartShiftEnhanced)) {
            ssFeature = hidpp::FeatureId::SmartShiftEnhanced;
            ssGetFn = 0x01;  // Enhanced uses fn1 for GetStatus
            qCDebug(lcDevice) << "using SmartShift Enhanced (0x2111)";
        }

        if (m_features->hasFeature(ssFeature)) {
            auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                         ssFeature, ssGetFn);
            if (resp.has_value()) {
                auto cfg = hidpp::features::SmartShift::parseConfig(*resp);
                m_smartShiftEnabled = cfg.isRatchet();
                m_smartShiftThreshold = cfg.autoDisengage;
                qCDebug(lcDevice) << "SmartShift: mode=" << cfg.mode
                                  << (m_smartShiftEnabled ? "(ratchet)" : "(freespin)")
                                  << "autoDisengage=" << m_smartShiftThreshold;
            }
        }
    }

    // Read scroll wheel config
    if (m_features->hasFeature(hidpp::FeatureId::HiResWheel)) {
        auto modeResp = m_features->call(m_transport.get(), m_deviceIndex,
                                         hidpp::FeatureId::HiResWheel,
                                         hidpp::features::HiResWheel::kFnGetWheelMode);
        if (modeResp.has_value()) {
            m_scrollModeByte = modeResp->params[0];
            auto cfg = hidpp::features::HiResWheel::parseWheelMode(*modeResp);
            m_scrollHiRes = cfg.hiRes;
            m_scrollInvert = cfg.invert;
            qCDebug(lcDevice) << "Scroll: hiRes=" << m_scrollHiRes << "invert=" << m_scrollInvert;
        }
        auto ratchetResp = m_features->call(m_transport.get(), m_deviceIndex,
                                            hidpp::FeatureId::HiResWheel,
                                            hidpp::features::HiResWheel::kFnGetRatchetSwitch);
        if (ratchetResp.has_value()) {
            m_scrollRatchet = hidpp::features::HiResWheel::parseRatchetSwitch(*ratchetResp);
            qCDebug(lcDevice) << "Scroll: ratchet=" << m_scrollRatchet;
        }
    }

    // Look up the device descriptor from the registry.
    // For direct connections the PID matches; for Bolt receiver the device name
    // (read via HID++ DeviceName feature) identifies the device behind the receiver.
    m_activeDevice = nullptr;
    if (m_registry) {
        m_activeDevice = m_registry->findByPid(m_device->info().productId);
        if (!m_activeDevice && !name.isEmpty())
            m_activeDevice = m_registry->findByName(name);
    }
    if (m_activeDevice)
        qCDebug(lcDevice) << "matched device descriptor:" << m_activeDevice->deviceName();
    else
        qCDebug(lcDevice) << "no device descriptor found for PID"
                          << Qt::hex << m_device->info().productId << "name:" << name;

    // Undivert ALL buttons to ensure clean native state on startup.
    // Previous sessions may have left diversions active on the device.
    if (m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4)) {
        if (m_activeDevice) {
            for (const auto &ctrl : m_activeDevice->controls()) {
                if (ctrl.configurable && ctrl.controlId != 0) {
                    auto params = hidpp::features::ReprogControls::buildSetDivert(ctrl.controlId, false);
                    m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::ReprogControlsV4,
                                     hidpp::features::ReprogControls::kFnSetControlReporting,
                                     std::span<const uint8_t>(params));
                }
            }
        }
        qCDebug(lcDevice) << "all buttons undiverted (clean native state)";
    }

    // Undivert thumb wheel (restore native horizontal scroll)
    if (m_features->hasFeature(hidpp::FeatureId::ThumbWheel)) {
        std::array<uint8_t, 2> twParams = {0x00, 0x00}; // divert=false, invert=false
        m_features->call(m_transport.get(), m_deviceIndex,
                         hidpp::FeatureId::ThumbWheel, 0x02,
                         std::span<const uint8_t>(twParams));
        m_thumbWheelMode = "scroll";

        // Read defaultDirection from ThumbWheel GetInfo (function 0x00, byte 4).
        // 0 = positive when left/back, 1 = positive when right/forward.
        // We use this to normalize rotation so clockwise = positive in software.
        auto twInfo = m_features->call(m_transport.get(), m_deviceIndex,
                                       hidpp::FeatureId::ThumbWheel, 0x00, {});
        if (twInfo.has_value()) {
            m_thumbWheelDefaultDirection = (twInfo->params[4] & 0x01) ? 1 : -1;
            qCDebug(lcDevice) << "thumb wheel defaultDirection:" << m_thumbWheelDefaultDirection;
        }
        qCDebug(lcDevice) << "thumb wheel set to native scroll";
    }

    // Update state and emit signals
    bool nameChanged    = (m_deviceName != name);
    bool levelChanged   = (m_batteryLevel != battLevel);
    bool chargeChanged  = (m_batteryCharging != battCharging);
    bool typeChanged    = true; // always notify — connectionType was set in probeDevice before signals were connected

    qCDebug(lcDevice) << "battery:" << battLevel << "% charging:" << battCharging;

    // Read current Easy-Switch host (ChangeHost 0x1814)
    int currentHost = -1;
    int hostCount = 0;
    if (m_features->hasFeature(hidpp::FeatureId::ChangeHost)) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::ChangeHost, 0x00); // GetHostInfo
        if (resp.has_value()) {
            hostCount = resp->params[0];
            currentHost = resp->params[1];
            qCDebug(lcDevice) << "Easy-Switch: host" << currentHost << "of" << hostCount;

            // Query cookies to determine which slots are paired
            auto cookieResp = m_features->call(m_transport.get(), m_deviceIndex,
                                               hidpp::FeatureId::ChangeHost, 0x02); // GetCookies
            m_hostPaired.clear();
            m_hostPaired.resize(hostCount, false);
            if (cookieResp.has_value()) {
                for (int i = 0; i < hostCount && i < 16; ++i) {
                    m_hostPaired[i] = (cookieResp->params[i] != 0);
                }
                qCDebug(lcDevice) << "Easy-Switch paired slots:" << m_hostPaired;
            }
        }
    }
    m_currentHost = currentHost;
    m_hostCount = hostCount;

    m_deviceName     = name;
    m_deviceSerial   = serial;
    m_firmwareVersion = firmwareVersion;
    m_deviceVid      = m_device->info().vendorId;
    m_devicePid      = m_device->info().productId;
    m_batteryLevel   = battLevel;
    m_batteryCharging = battCharging;

    m_connected = true;
    emit deviceConnectedChanged();

    if (nameChanged)    emit deviceNameChanged();
    if (levelChanged)   emit batteryLevelChanged();
    if (chargeChanged)  emit batteryChargingChanged();
    if (typeChanged)    emit connectionTypeChanged();

    // Record response time for sleep/wake tracking
    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();

    m_enumerating = false;

    // Create command queue now — features are populated, transport is ready.
    // Must be before deviceSetupComplete which triggers profile application.
    if (!m_commandQueue && m_features && m_transport) {
        m_commandQueue = std::make_unique<hidpp::CommandQueue>(
            m_features.get(), m_transport.get(), m_deviceIndex);
        m_commandQueue->start();
    }

    emit deviceSetupComplete();

    // Start periodic battery polling (60s) — the device doesn't always send
    // notifications when charging stops, so we need to poll.
    if (!m_batteryPollTimer) {
        m_batteryPollTimer = new QTimer(this);
        m_batteryPollTimer->setInterval(60000);
        connect(m_batteryPollTimer, &QTimer::timeout, this, [this]() {
            if (!m_connected || !m_features || !m_transport) return;
            if (!m_features->hasFeature(hidpp::FeatureId::BatteryUnified)) return;
            auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                          hidpp::FeatureId::BatteryUnified, 0x01);
            if (resp.has_value()) {
                auto status = hidpp::features::Battery::parseStatus(*resp);
                qCDebug(lcDevice) << "battery poll:" << status.level << "% charging:" << status.charging;
                bool levelChanged   = (m_batteryLevel != status.level);
                bool chargeChanged  = (m_batteryCharging != status.charging);
                m_batteryLevel   = status.level;
                m_batteryCharging = status.charging;
                if (levelChanged)  emit batteryLevelChanged();
                if (chargeChanged) emit batteryChargingChanged();
            }
        });
    }
    m_batteryPollTimer->start();
}

// ---------------------------------------------------------------------------
// disconnectDevice()
// ---------------------------------------------------------------------------

void DeviceManager::disconnectDevice()
{
    // Don't touch m_receiverNotifier/m_receiverDevice — they live independently
    // for transport switching detection

    // Stop battery polling
    if (m_batteryPollTimer)
        m_batteryPollTimer->stop();

    // Clean up hidraw notifier BEFORE destroying the device/transport
    // to prevent QSocketNotifier firing on an invalid fd.
    if (m_hidrawNotifier) {
        m_hidrawNotifier->setEnabled(false);
        delete m_hidrawNotifier;
        m_hidrawNotifier = nullptr;
    }

    if (m_commandQueue) {
        m_commandQueue->clear();
        m_commandQueue->stop();
        m_commandQueue.reset();
    }
    m_activeDevice = nullptr;
    m_features.reset();
    m_transport.reset();
    m_device.reset();
    m_deviceIndex = 0xFF;

    bool wasConnected = m_connected;
    m_connected      = false;
    m_deviceName     = QString();
    m_deviceSerial    = QString();
    m_firmwareVersion = QString();
    m_deviceVid      = 0;
    m_devicePid      = 0;
    m_batteryLevel   = 0;
    m_batteryCharging = false;
    m_connectionType  = QString();
    m_lastResponseTime = 0;

    if (wasConnected) {
        emit deviceConnectedChanged();
        emit deviceNameChanged();
        emit batteryLevelChanged();
        emit batteryChargingChanged();
        emit connectionTypeChanged();
    }
}

// ---------------------------------------------------------------------------
// handleNotification()
// ---------------------------------------------------------------------------

void DeviceManager::handleNotification(const hidpp::Report &report)
{
    // Responses echo softwareId != 0. Route to pending async callbacks.
    // Notifications from the device have softwareId=0.
    if (report.softwareId != 0) {
        if (m_features)
            m_features->handleResponse(report);
        return;
    }

    qCDebug(lcDevice) << "notification: featureIndex=" << Qt::hex << report.featureIndex
                      << "functionId=" << report.functionId;

    // HID++ 1.0 DeviceConnection notification (register 0x41) from Bolt/Unifying receiver.
    // Byte 4 (params[0]) bit 6: 0 = link established, 1 = link not established.
    if (report.featureIndex == 0x41) {
        bool linkEstablished = (report.params[0] & 0x40) == 0;
        qCDebug(lcDevice) << "DeviceConnection:" << (linkEstablished ? "connected" : "disconnected");
        if (!linkEstablished && m_connected) {
            // Soft disconnect — keep hidraw fd open for reconnect detection.
            // Only reset logical state, not the transport.
            if (m_commandQueue) {
                m_commandQueue->clear();
                m_commandQueue->stop();
                m_commandQueue.reset();
            }
            m_activeDevice = nullptr;
            m_features.reset();
            m_connected = false;
            m_batteryLevel = 0;
            m_batteryCharging = false;
            emit deviceConnectedChanged();
            emit batteryLevelChanged();
            emit deviceDisconnected();
        } else if (linkEstablished && !m_connected) {
            // Device reconnected on the same receiver — re-enumerate after delay.
            // Use 1500ms: the device sends multiple DJ notifications during boot
            // and HID++ calls fail with HwError if sent too early.
            qCDebug(lcDevice) << "device reconnected, re-enumerating (1500ms delay)";
            // Cancel any pending reconnect timer to avoid double enumeration
            if (m_reconnectTimer) {
                m_reconnectTimer->stop();
                delete m_reconnectTimer;
            }
            m_reconnectTimer = new QTimer(this);
            m_reconnectTimer->setSingleShot(true);
            connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
                m_reconnectTimer = nullptr;
                if (!m_connected && m_device && m_transport) {
                    // enumerateAndSetup creates features + command queue internally
                    enumerateAndSetup();
                }
            });
            m_reconnectTimer->start(1500);
        }
        return;
    }

    // Battery notification (feature index matches BatteryUnified)
    if (m_features && m_features->hasFeature(hidpp::FeatureId::BatteryUnified)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::BatteryUnified);
        if (idx.has_value() && report.featureIndex == *idx) {
            qCDebug(lcDevice) << "battery raw params:"
                              << Qt::hex << report.params[0] << report.params[1] << report.params[2] << report.params[3];
            auto status = hidpp::features::Battery::parseStatus(report);
            qCDebug(lcDevice) << "battery notification:" << status.level << "% charging:" << status.charging;
            bool levelChanged   = (m_batteryLevel != status.level);
            bool chargeChanged  = (m_batteryCharging != status.charging);
            m_batteryLevel   = status.level;
            m_batteryCharging = status.charging;
            if (levelChanged)  emit batteryLevelChanged();
            if (chargeChanged) emit batteryChargingChanged();
            return;
        }
    }

    // HiResWheel ratchet notification — physical SmartShift button toggled
    if (m_features && m_features->hasFeature(hidpp::FeatureId::HiResWheel)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::HiResWheel);
        if (idx.has_value() && report.featureIndex == *idx && report.functionId == 1) {
            // Ratchet switch event: params[0] = 0 (freespin) or 1 (ratchet)
            bool ratchet = (report.params[0] & 0x01) != 0;
            if (m_scrollRatchet != ratchet) {
                m_scrollRatchet = ratchet;
                // Ratchet ON = SmartShift active, Ratchet OFF = freespin
                m_smartShiftEnabled = ratchet;
                qCDebug(lcDevice) << "SmartShift button:" << (ratchet ? "ratchet" : "freespin");
                emit smartShiftChanged();
                emit scrollConfigChanged();
            }
            return;
        }
    }

    // SmartShift feature notification (V1 or Enhanced)
    for (auto ssId : {hidpp::FeatureId::SmartShift, hidpp::FeatureId::SmartShiftEnhanced}) {
        if (m_features && m_features->hasFeature(ssId)) {
            auto idx = m_features->featureIndex(ssId);
            if (idx.has_value() && report.featureIndex == *idx) {
                auto cfg = hidpp::features::SmartShift::parseConfig(report);
                bool newEnabled = cfg.isRatchet();
                if (m_smartShiftEnabled != newEnabled) {
                    m_smartShiftEnabled = newEnabled;
                    m_smartShiftThreshold = cfg.autoDisengage;
                    qCDebug(lcDevice) << "SmartShift toggled:"
                                      << (newEnabled ? "ratchet" : "freespin");
                    emit smartShiftChanged();
                }
                return;
            }
        }
    }

    // ReprogControls notifications (feature index matches ReprogControlsV4)
    if (m_features && m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::ReprogControlsV4);
        if (idx.has_value() && report.featureIndex == *idx) {
            if (report.functionId == 0) {
                // DivertedButtonEvent: params[0-1]=CID, CID=0 means released
                uint16_t controlId = (static_cast<uint16_t>(report.params[0]) << 8)
                                     | report.params[1];
                bool pressed = (controlId != 0);
                qCDebug(lcDevice) << "button event: CID" << Qt::hex << controlId
                                  << (pressed ? "pressed" : "released");
                emit divertedButtonPressed(controlId, pressed);
            } else if (report.functionId == 1) {
                // DivertedRawXYEvent: params[0-1]=dx, params[2-3]=dy (int16 BE)
                auto evt = hidpp::features::ReprogControls::parseDivertedRawXYEvent(report);
                emit gestureRawXY(evt.dx, evt.dy);
            }
            return;
        }
    }

    // ThumbWheel (diverted) notification
    if (m_features && m_features->hasFeature(hidpp::FeatureId::ThumbWheel)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::ThumbWheel);
        if (idx.has_value() && report.featureIndex == *idx) {
            // params[0-1] = rotation (int16_t, signed, big-endian)
            int16_t rotation = static_cast<int16_t>(
                (static_cast<uint16_t>(report.params[0]) << 8) | report.params[1]);
            emit thumbWheelRotation(rotation);
            return;
        }
    }

}

// ---------------------------------------------------------------------------
// checkSleepWake()
// ---------------------------------------------------------------------------

void DeviceManager::checkSleepWake()
{
    if (m_lastResponseTime == 0)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kSleepThresholdMs = 120000; // 2 minutes

    if ((now - m_lastResponseTime) > kSleepThresholdMs) {
        if (m_enumerating) return; // prevent re-entrant enumeration on wake
        m_enumerating = true; // block further wake attempts until timer fires
        qCDebug(lcDevice) << "device woke from sleep, deferring re-enumeration (500ms)";
        QTimer::singleShot(500, this, [this]() {
            m_enumerating = false;
            qCDebug(lcDevice) << "wake timer fired, re-enumerating features";
            if (m_transport && m_device && m_device->isOpen()) {
                enumerateAndSetup();
            }
            m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();
            emit deviceWoke();
        });
    }
}


// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

bool DeviceManager::deviceConnected() const { return m_connected; }
QString DeviceManager::deviceName() const { return m_deviceName; }
int DeviceManager::batteryLevel() const { return m_batteryLevel; }
bool DeviceManager::batteryCharging() const { return m_batteryCharging; }
QString DeviceManager::connectionType() const { return m_connectionType; }
int DeviceManager::currentDPI() const { return m_currentDPI; }
int DeviceManager::minDPI() const { return m_minDPI; }
int DeviceManager::maxDPI() const { return m_maxDPI; }
int DeviceManager::dpiStep() const { return m_dpiStep; }
bool DeviceManager::smartShiftEnabled() const { return m_smartShiftEnabled; }
int DeviceManager::smartShiftThreshold() const { return m_smartShiftThreshold; }

bool DeviceManager::scrollHiRes() const { return m_scrollHiRes; }
bool DeviceManager::scrollInvert() const { return m_scrollInvert; }
bool DeviceManager::scrollRatchet() const { return m_scrollRatchet; }

hidpp::FeatureDispatcher *DeviceManager::features() const { return m_features.get(); }
hidpp::Transport *DeviceManager::transport() const { return m_transport.get(); }
uint8_t DeviceManager::deviceIndex() const { return m_deviceIndex; }
QString DeviceManager::deviceSerial() const { return m_deviceSerial; }
QString DeviceManager::firmwareVersion() const { return m_firmwareVersion; }
uint16_t DeviceManager::deviceVid() const { return m_deviceVid; }
uint16_t DeviceManager::devicePid() const { return m_devicePid; }
const IDevice* DeviceManager::activeDevice() const { return m_activeDevice; }

// ---------------------------------------------------------------------------
// setDPI() — change mouse DPI via HID++
// ---------------------------------------------------------------------------

void DeviceManager::setDPI(int value)
{
    if (!m_connected || !m_features || !m_commandQueue) {
        qCDebug(lcDevice) << "setDPI: skipped (not connected)";
        return;
    }
    if (!m_features->hasFeature(hidpp::FeatureId::AdjustableDPI))
        return;

    value = qBound(m_minDPI, value, m_maxDPI);
    value = (value / m_dpiStep) * m_dpiStep;

    qCDebug(lcDevice) << "setDPI:" << value << "(was" << m_currentDPI << ") queue=" << m_commandQueue->pending();
    m_currentDPI = value;
    emit currentDPIChanged();

    auto params = hidpp::features::AdjustableDPI::buildSetDPI(value);
    m_commandQueue->enqueue(hidpp::FeatureId::AdjustableDPI,
                            hidpp::features::AdjustableDPI::kFnSetSensorDpi,
                            params);
}

// ---------------------------------------------------------------------------
// setSmartShift() — change SmartShift via HID++
// ---------------------------------------------------------------------------

void DeviceManager::setSmartShift(bool enabled, int threshold)
{
    if (!m_connected || !m_features || !m_commandQueue)
        return;

    // Resolve which feature to use: V1 (0x2110) or Enhanced (0x2111)
    hidpp::FeatureId ssFeature;
    uint8_t ssSetFn;
    if (m_features->hasFeature(hidpp::FeatureId::SmartShift)) {
        ssFeature = hidpp::FeatureId::SmartShift;
        ssSetFn = hidpp::features::SmartShift::kFnSetStatus;  // fn1
    } else if (m_features->hasFeature(hidpp::FeatureId::SmartShiftEnhanced)) {
        ssFeature = hidpp::FeatureId::SmartShiftEnhanced;
        ssSetFn = 0x02;  // Enhanced uses fn2 for SetStatus
    } else {
        return;
    }

    threshold = qBound(1, threshold, 255);

    m_smartShiftEnabled = enabled;
    m_smartShiftThreshold = threshold;
    emit smartShiftChanged();

    uint8_t mode = enabled ? 2 : 1;
    uint8_t ad = static_cast<uint8_t>(threshold);

    auto params = hidpp::features::SmartShift::buildSetConfig(mode, ad);
    m_commandQueue->enqueue(ssFeature, ssSetFn,
                            std::span<const uint8_t>(params));
    qCDebug(lcDevice) << "SmartShift set: feature=" << Qt::hex << static_cast<uint16_t>(ssFeature)
                      << "mode=" << mode << "autoDisengage=" << ad;
}

void DeviceManager::setScrollConfig(bool hiRes, bool invert)
{
    if (!m_connected || !m_features || !m_commandQueue)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::HiResWheel))
        return;

    m_scrollHiRes = hiRes;
    m_scrollInvert = invert;
    emit scrollConfigChanged();

    auto params = hidpp::features::HiResWheel::buildSetWheelMode(m_scrollModeByte, hiRes, invert);
    m_commandQueue->enqueue(hidpp::FeatureId::HiResWheel,
                            hidpp::features::HiResWheel::kFnSetWheelMode,
                            std::span<const uint8_t>(params));
}

void DeviceManager::divertButton(uint16_t controlId, bool divert, bool rawXY)
{
    if (!m_connected || !m_features || !m_commandQueue)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4))
        return;

    auto params = hidpp::features::ReprogControls::buildSetDivert(controlId, divert, rawXY);
    m_commandQueue->enqueue(hidpp::FeatureId::ReprogControlsV4,
                            hidpp::features::ReprogControls::kFnSetControlReporting,
                            std::span<const uint8_t>(params));
    qCDebug(lcDevice) << "button" << Qt::hex << controlId
                      << (divert ? "diverted" : "undiverted") << (rawXY ? "+rawXY" : "");
}

QString DeviceManager::thumbWheelMode() const { return m_thumbWheelMode; }
bool DeviceManager::thumbWheelInvert() const { return m_thumbWheelInvert; }
int DeviceManager::thumbWheelDefaultDirection() const { return m_thumbWheelDefaultDirection; }
int DeviceManager::currentHost() const { return m_currentHost; }
int DeviceManager::hostCount() const { return m_hostCount; }
bool DeviceManager::isHostPaired(int host) const {
    if (host < 0 || host >= m_hostPaired.size()) return false;
    return m_hostPaired[host];
}

void DeviceManager::flushCommandQueue()
{
    if (m_commandQueue)
        m_commandQueue->clear();
}

void DeviceManager::touchResponseTime()
{
    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();
}

void DeviceManager::setThumbWheelMode(const QString &mode, bool invert)
{
    qCDebug(lcDevice) << "setThumbWheelMode:" << mode << "invert=" << invert;
    m_thumbWheelMode = mode;
    m_thumbWheelInvert = invert;
    emit thumbWheelModeChanged();

    if (!m_connected || !m_features || !m_commandQueue)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::ThumbWheel))
        return;

    bool divert = (mode != "scroll");

    std::array<uint8_t, 2> twParams = {
        static_cast<uint8_t>(divert ? 0x01 : 0x00),
        static_cast<uint8_t>(invert ? 0x01 : 0x00)
    };
    m_commandQueue->enqueue(hidpp::FeatureId::ThumbWheel, 0x02,
                            std::span<const uint8_t>(twParams),
                            [divert, invert](const hidpp::Report &) {
                                qCDebug(lcDevice) << "thumb wheel setReporting confirmed:"
                                                   << "divert=" << divert << "invert=" << invert;
                            });
}

} // namespace logitune
