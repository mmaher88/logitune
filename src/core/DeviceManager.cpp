#include "DeviceManager.h"
#include "hidpp/features/Battery.h"
#include "hidpp/features/DeviceName.h"
#include "hidpp/features/AdjustableDPI.h"
#include "hidpp/features/SmartShift.h"
#include "hidpp/features/HiResWheel.h"
#include "hidpp/features/ReprogControls.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QtConcurrent>
#include <QSocketNotifier>

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

bool DeviceManager::isDirectDevice(uint16_t pid)
{
    return pid == hidpp::kPidMxMaster3s;
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

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

DeviceManager::~DeviceManager()
{
    // Undivert all buttons before shutdown to restore native behavior
    if (m_connected && m_features && m_transport &&
        m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4)) {
        static const uint16_t kAllButtons[] = { 0x0052, 0x0053, 0x0056, 0x00C3, 0x00C4 };
        for (uint16_t cid : kAllButtons) {
            auto params = hidpp::features::ReprogControls::buildSetDivert(cid, false);
            m_features->call(m_transport.get(), m_deviceIndex,
                             hidpp::FeatureId::ReprogControlsV4,
                             hidpp::features::ReprogControls::kFnSetControlReporting,
                             std::span<const uint8_t>(params));
        }
    }

    stopIoThread();

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
        qWarning() << "DeviceManager: failed to create udev context";
        return;
    }

    m_udevMon = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_udevMon) {
        qWarning() << "DeviceManager: failed to create udev monitor";
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
    qDebug() << "[DeviceManager] scanning existing hidraw devices...";

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
            qDebug() << "[DeviceManager] found Logitech device:" << devNode
                     << "PID:" << Qt::hex << productId
                     << (isReceiver(productId) ? "(receiver)" : "(direct)");
            if (devNode && !m_connected) {
                probeDevice(QString::fromUtf8(devNode));
            }
        }

        udev_device_unref(dev);
    }

    qDebug() << "[DeviceManager] scan complete:" << count << "hidraw devices," << (m_connected ? "connected" : "no device connected");
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

        if (!m_connected)
            probeDevice(devNode);
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
    qDebug() << "[DeviceManager] probing" << devNode;

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
                qDebug() << "[DeviceManager]" << devNode << "no HID++ report ID in descriptor, skipping";
                return;
            }
            qDebug() << "[DeviceManager]" << devNode << "has HID++ report ID";
        }
    }

    auto device = std::make_unique<hidpp::HidrawDevice>(devNode);
    if (!device->open()) {
        qDebug() << "[DeviceManager] cannot open" << devNode;
        return;
    }

    auto info = device->info();
    qDebug() << "[DeviceManager] opened" << devNode << "vendor:" << Qt::hex << info.vendorId << "product:" << info.productId;
    if (info.vendorId != hidpp::kVendorLogitech) {
        return;
    }

    uint16_t pid = info.productId;
    uint8_t deviceIndex = 0xFF;
    QString connType;

    if (isReceiver(pid)) {
        // Bolt receiver PID -> connection type "Bolt"; Unifying -> "Bolt" is debatable
        // but per spec: Bolt = 0xc548, Unifying = 0xc52b. We label both as "Bolt" for
        // simplicity since MX Master 3S only supports Bolt among receivers.
        connType = (pid == hidpp::kPidBoltReceiver) ? QStringLiteral("Bolt")
                                                    : QStringLiteral("Bolt");

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

            qDebug() << "[DeviceManager] pinging slot" << slot << "on" << devNode;
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
                    qDebug() << "[DeviceManager] found device at slot" << slot
                             << "(response:" << QByteArray(reinterpret_cast<const char*>(resp.data()),
                                                           static_cast<int>(resp.size())).toHex() << ")";
                    break;
                }
            }

            // Drain any extra data (notifications etc)
            device->readReport(50);
        }

        if (!found) {
            qDebug() << "[DeviceManager] no device found on any slot of" << devNode;
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

    // Enumerate features and read initial state
    enumerateAndSetup();

    // Kernel-driven notification: QSocketNotifier fires when hidraw has data
    if (!m_hidrawNotifier) {
        m_hidrawNotifier = new QSocketNotifier(m_device->fd(), QSocketNotifier::Read, this);
        connect(m_hidrawNotifier, &QSocketNotifier::activated, this, [this]() {
            // tryLock: if a settings write holds the mutex, skip — data stays in fd buffer
            // and the notifier will fire again when the mutex is released
            if (!m_hidrawMutex.tryLock())
                return;
            auto bytes = m_device->readReport(0); // non-blocking, data already available
            m_hidrawMutex.unlock();
            if (bytes.empty())
                return;
            auto report = hidpp::Report::parse(bytes);
            if (!report)
                return;
            handleNotification(*report);
        });
    }
    m_hidrawNotifier->setEnabled(true);
}

// ---------------------------------------------------------------------------
// enumerateAndSetup()
// ---------------------------------------------------------------------------

void DeviceManager::enumerateAndSetup()
{
    qDebug() << "[DeviceManager] enumerateAndSetup: deviceIndex=" << Qt::hex << m_deviceIndex;
    m_features = std::make_unique<hidpp::FeatureDispatcher>();

    bool ok = m_features->enumerate(m_transport.get(), m_deviceIndex);
    if (!ok) {
        qWarning() << "[DeviceManager] feature enumeration failed";
    } else {
        qDebug() << "[DeviceManager] feature enumeration succeeded";
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

    qDebug() << "[DeviceManager] device name:" << name;

    if (name.isEmpty())
        name = QStringLiteral("Logitech Device");

    // Stable device identifier from product ID + name hash
    QString serial = QString::number(m_device->info().productId, 16)
                     + QStringLiteral("-") + QString::number(qHash(name), 16);
    qDebug() << "[DeviceManager] device id:" << serial;

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
            qDebug() << "[DeviceManager] DPI range:" << m_minDPI << "-" << m_maxDPI
                     << "step:" << m_dpiStep;
        }
        // Get current DPI
        auto dpiResp = m_features->call(m_transport.get(), m_deviceIndex,
                                        hidpp::FeatureId::AdjustableDPI,
                                        hidpp::features::AdjustableDPI::kFnGetSensorDpi,
                                        std::array<uint8_t, 1>{0x00});
        if (dpiResp.has_value()) {
            m_currentDPI = hidpp::features::AdjustableDPI::parseCurrentDPI(*dpiResp);
            qDebug() << "[DeviceManager] current DPI:" << m_currentDPI;
        }
    }

    // Read SmartShift
    if (m_features->hasFeature(hidpp::FeatureId::SmartShift)) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::SmartShift,
                                     hidpp::features::SmartShift::kFnGetConfig);
        if (resp.has_value()) {
            auto cfg = hidpp::features::SmartShift::parseConfig(*resp);
            m_smartShiftEnabled = cfg.enabled;
            m_smartShiftThreshold = cfg.threshold;
            qDebug() << "[DeviceManager] SmartShift:" << (m_smartShiftEnabled ? "ON" : "OFF")
                     << "threshold:" << m_smartShiftThreshold;
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
            qDebug() << "[DeviceManager] Scroll: hiRes=" << m_scrollHiRes << "invert=" << m_scrollInvert;
        }
        auto ratchetResp = m_features->call(m_transport.get(), m_deviceIndex,
                                            hidpp::FeatureId::HiResWheel,
                                            hidpp::features::HiResWheel::kFnGetRatchetSwitch);
        if (ratchetResp.has_value()) {
            m_scrollRatchet = hidpp::features::HiResWheel::parseRatchetSwitch(*ratchetResp);
            qDebug() << "[DeviceManager] Scroll: ratchet=" << m_scrollRatchet;
        }
    }

    // Undivert ALL buttons to ensure clean native state on startup.
    // Previous sessions may have left diversions active on the device.
    if (m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4)) {
        static const uint16_t kAllButtons[] = { 0x0052, 0x0053, 0x0056, 0x00C3, 0x00C4 };
        for (uint16_t cid : kAllButtons) {
            auto params = hidpp::features::ReprogControls::buildSetDivert(cid, false);
            m_features->call(m_transport.get(), m_deviceIndex,
                             hidpp::FeatureId::ReprogControlsV4,
                             hidpp::features::ReprogControls::kFnSetControlReporting,
                             std::span<const uint8_t>(params));
        }
        qDebug() << "[DeviceManager] all buttons undiverted (clean native state)";
    }

    // Undivert thumb wheel (restore native horizontal scroll)
    if (m_features->hasFeature(hidpp::FeatureId::ThumbWheel)) {
        std::array<uint8_t, 2> twParams = {0x00, 0x00}; // divert=false, invert=false
        m_features->call(m_transport.get(), m_deviceIndex,
                         hidpp::FeatureId::ThumbWheel, 0x02,
                         std::span<const uint8_t>(twParams));
        m_thumbWheelMode = "scroll";
        qDebug() << "[DeviceManager] thumb wheel set to native scroll";
    }

    // Update state and emit signals
    bool nameChanged    = (m_deviceName != name);
    bool levelChanged   = (m_batteryLevel != battLevel);
    bool chargeChanged  = (m_batteryCharging != battCharging);
    bool typeChanged    = false; // connectionType already set before this call

    qDebug() << "[DeviceManager] battery:" << battLevel << "% charging:" << battCharging;

    m_deviceName     = name;
    m_deviceSerial   = serial;
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

    emit deviceSetupComplete();
}

// ---------------------------------------------------------------------------
// disconnectDevice()
// ---------------------------------------------------------------------------

void DeviceManager::disconnectDevice()
{
    stopIoThread();

    m_features.reset();
    m_transport.reset();
    m_device.reset();
    m_deviceIndex = 0xFF;

    bool wasConnected = m_connected;
    m_connected      = false;
    m_deviceName     = QString();
    m_deviceSerial   = QString();
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
    qDebug() << "[DeviceManager] notification: featureIndex=" << Qt::hex << report.featureIndex
             << "functionId=" << report.functionId;

    // Track last response time for sleep/wake detection
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    checkSleepWake();
    m_lastResponseTime = now;

    // Battery notification (feature index matches BatteryUnified)
    if (m_features && m_features->hasFeature(hidpp::FeatureId::BatteryUnified)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::BatteryUnified);
        if (idx.has_value() && report.featureIndex == *idx) {
            qDebug() << "[DeviceManager] battery raw params:"
                     << Qt::hex << report.params[0] << report.params[1] << report.params[2] << report.params[3];
            auto status = hidpp::features::Battery::parseStatus(report);
            qDebug() << "[DeviceManager] battery notification:" << status.level << "% charging:" << status.charging;
            bool levelChanged   = (m_batteryLevel != status.level);
            bool chargeChanged  = (m_batteryCharging != status.charging);
            m_batteryLevel   = status.level;
            m_batteryCharging = status.charging;
            if (levelChanged)  emit batteryLevelChanged();
            if (chargeChanged) emit batteryChargingChanged();
            return;
        }
    }

    // ReprogControls (diverted button) notification
    if (m_features && m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::ReprogControlsV4);
        if (idx.has_value() && report.featureIndex == *idx) {
            qDebug() << "[DeviceManager] ReprogControls raw params:"
                     << Qt::hex << report.params[0] << report.params[1]
                     << report.params[2] << report.params[3] << report.params[4];
            uint16_t controlId = (static_cast<uint16_t>(report.params[0]) << 8)
                                 | report.params[1];
            // In ReprogControls v4 diverted events, CID != 0 means button is pressed,
            // CID == 0 means all buttons released
            bool pressed = (controlId != 0);
            emit divertedButtonPressed(controlId, pressed);
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

    // GestureV2 notification
    if (m_features && m_features->hasFeature(hidpp::FeatureId::GestureV2)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::GestureV2);
        if (idx.has_value() && report.featureIndex == *idx) {
            int dx = static_cast<int8_t>(report.params[0]);
            int dy = static_cast<int8_t>(report.params[1]);
            bool released = (report.params[2] != 0);
            emit gestureEvent(dx, dy, released);
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
        qDebug() << "DeviceManager: device woke from sleep, re-enumerating features";
        // Re-enumerate features and re-read state after wake
        if (m_transport && m_device && m_device->isOpen()) {
            enumerateAndSetup();
        }
        emit deviceWoke();
    }
}

// ---------------------------------------------------------------------------
// I/O thread management
// ---------------------------------------------------------------------------

void DeviceManager::startIoThread()
{
    if (m_ioThread.isRunning())
        return;

    hidpp::Transport *transport = m_transport.get();
    connect(&m_ioThread, &QThread::started, transport, &hidpp::Transport::run,
            Qt::DirectConnection);

    m_ioThread.start();
}

void DeviceManager::stopIoThread()
{
    if (!m_ioThread.isRunning())
        return;

    if (m_transport)
        m_transport->stop();

    m_ioThread.quit();
    m_ioThread.wait(3000);
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
uint16_t DeviceManager::deviceVid() const { return m_deviceVid; }
uint16_t DeviceManager::devicePid() const { return m_devicePid; }

// ---------------------------------------------------------------------------
// setDPI() — change mouse DPI via HID++
// ---------------------------------------------------------------------------

void DeviceManager::setDPI(int value)
{
    if (!m_connected || !m_features || !m_transport)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::AdjustableDPI))
        return;

    value = qBound(m_minDPI, value, m_maxDPI);
    value = (value / m_dpiStep) * m_dpiStep;

    // Optimistic UI update
    m_currentDPI = value;
    emit currentDPIChanged();

    // Run HID++ write on background thread, mutex blocks notifier during write+read
    auto *transport = m_transport.get();
    auto *features = m_features.get();
    uint8_t devIdx = m_deviceIndex;
    QMutex *mutex = &m_hidrawMutex;

    QtConcurrent::run([transport, features, devIdx, value, mutex]() {
        QMutexLocker lock(mutex);
        auto params = hidpp::features::AdjustableDPI::buildSetDPI(value);
        features->call(transport, devIdx,
                       hidpp::FeatureId::AdjustableDPI,
                       hidpp::features::AdjustableDPI::kFnSetSensorDpi,
                       params);
        qDebug() << "[DeviceManager] DPI set to" << value;
    });
}

// ---------------------------------------------------------------------------
// setSmartShift() — change SmartShift via HID++
// ---------------------------------------------------------------------------

void DeviceManager::setSmartShift(bool enabled, int threshold)
{
    if (!m_connected || !m_features || !m_transport)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::SmartShift))
        return;

    threshold = qBound(1, threshold, 255);

    m_smartShiftEnabled = enabled;
    m_smartShiftThreshold = threshold;
    emit smartShiftChanged();

    auto *transport = m_transport.get();
    auto *features = m_features.get();
    uint8_t devIdx = m_deviceIndex;
    QMutex *mutex = &m_hidrawMutex;

    QtConcurrent::run([transport, features, devIdx, enabled, threshold, mutex]() {
        QMutexLocker lock(mutex);
        auto params = hidpp::features::SmartShift::buildSetConfig(enabled, threshold);
        features->call(transport, devIdx,
                       hidpp::FeatureId::SmartShift,
                       hidpp::features::SmartShift::kFnSetConfig,
                       std::span<const uint8_t>(params));
        qDebug() << "[DeviceManager] SmartShift set:" << enabled << "threshold:" << threshold;
    });
}

void DeviceManager::setScrollConfig(bool hiRes, bool invert)
{
    if (!m_connected || !m_features || !m_transport)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::HiResWheel))
        return;

    m_scrollHiRes = hiRes;
    m_scrollInvert = invert;
    emit scrollConfigChanged();

    auto *transport = m_transport.get();
    auto *features = m_features.get();
    uint8_t devIdx = m_deviceIndex;
    uint8_t currentMode = m_scrollModeByte;
    QMutex *mutex = &m_hidrawMutex;

    QtConcurrent::run([transport, features, devIdx, hiRes, invert, currentMode, mutex]() {
        QMutexLocker lock(mutex);
        auto params = hidpp::features::HiResWheel::buildSetWheelMode(currentMode, hiRes, invert);
        features->call(transport, devIdx,
                       hidpp::FeatureId::HiResWheel,
                       hidpp::features::HiResWheel::kFnSetWheelMode,
                       std::span<const uint8_t>(params));
        qDebug() << "[DeviceManager] Scroll set: hiRes=" << hiRes << "invert=" << invert;
    });
}

void DeviceManager::divertButton(uint16_t controlId, bool divert)
{
    if (!m_connected || !m_features || !m_transport)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::ReprogControlsV4))
        return;

    auto *transport = m_transport.get();
    auto *features = m_features.get();
    uint8_t devIdx = m_deviceIndex;
    QMutex *mutex = &m_hidrawMutex;

    QtConcurrent::run([transport, features, devIdx, controlId, divert, mutex]() {
        QMutexLocker lock(mutex);
        auto params = hidpp::features::ReprogControls::buildSetDivert(controlId, divert);
        features->call(transport, devIdx,
                       hidpp::FeatureId::ReprogControlsV4,
                       hidpp::features::ReprogControls::kFnSetControlReporting,
                       std::span<const uint8_t>(params));
        qDebug() << "[DeviceManager] button" << Qt::hex << controlId
                 << (divert ? "diverted" : "undiverted");
    });
}

QString DeviceManager::thumbWheelMode() const { return m_thumbWheelMode; }

void DeviceManager::setThumbWheelMode(const QString &mode)
{
    if (!m_connected || !m_features || !m_transport)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::ThumbWheel))
        return;

    bool divert = (mode != "scroll"); // "scroll" = native, anything else = diverted
    m_thumbWheelMode = mode;
    emit thumbWheelModeChanged();

    auto *transport = m_transport.get();
    auto *features = m_features.get();
    uint8_t devIdx = m_deviceIndex;
    QMutex *mutex = &m_hidrawMutex;

    QtConcurrent::run([transport, features, devIdx, divert, mutex]() {
        QMutexLocker lock(mutex);
        // ThumbWheel setReporting: params[0]=divert, params[1]=invert
        std::array<uint8_t, 2> params = {
            static_cast<uint8_t>(divert ? 0x01 : 0x00),
            0x00 // no invert
        };
        features->call(transport, devIdx,
                       hidpp::FeatureId::ThumbWheel,
                       0x02, // setReporting
                       std::span<const uint8_t>(params));
        qDebug() << "[DeviceManager] thumb wheel" << (divert ? "diverted" : "native");
    });
}

} // namespace logitune
