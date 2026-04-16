#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "interfaces/IDevice.h"
#include "hidpp/features/Battery.h"
#include "hidpp/features/DeviceName.h"
#include "hidpp/features/AdjustableDPI.h"
#include "hidpp/features/SmartShift.h"
#include "hidpp/features/HiResWheel.h"
#include "hidpp/features/ReprogControls.h"
#include "hidpp/features/ThumbWheel.h"
#include "hidpp/capabilities/Capabilities.h"
#include "logging/LogManager.h"

#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QHash>
#include <QSocketNotifier>
#include <QTimer>

namespace logitune {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DeviceSession::DeviceSession(std::unique_ptr<hidpp::HidrawDevice> device,
                             uint8_t deviceIndex,
                             const QString &connectionType,
                             DeviceRegistry *registry,
                             QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_device(std::move(device))
    , m_deviceIndex(deviceIndex)
    , m_connectionType(connectionType)
{
    m_transport = std::make_unique<hidpp::Transport>(m_device.get(), nullptr);
}

DeviceSession::~DeviceSession()
{
    if (m_connected && m_features && m_transport && m_activeDevice &&
        m_reprogControlsDispatch && m_reprogControlsDispatch->supportsDiversion) {
        for (const auto &ctrl : m_activeDevice->controls()) {
            if (ctrl.configurable && ctrl.controlId != 0) {
                auto params = hidpp::features::ReprogControls::buildSetDivert(ctrl.controlId, false);
                m_features->call(m_transport.get(), m_deviceIndex,
                                 m_reprogControlsDispatch->feature,
                                 hidpp::features::ReprogControls::kFnSetControlReporting,
                                 std::span<const uint8_t>(params));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// deviceId()
// ---------------------------------------------------------------------------

QString DeviceSession::deviceId() const
{
    // Identity must be stable across transport switches (Bolt receiver vs
    // direct Bluetooth) for the same physical device, so we use the
    // HID++ unit ID (serial) only. VID/PID differ per transport since
    // Bolt reports the receiver PID while BT reports the mouse PID.
    // Fall back to a VID-PID-empty placeholder before enumerate completes.
    if (!m_deviceSerial.isEmpty())
        return m_deviceSerial;
    return QStringLiteral("%1-%2")
        .arg(m_deviceVid, 4, 16, QLatin1Char('0'))
        .arg(m_devicePid, 4, 16, QLatin1Char('0'));
}

// ---------------------------------------------------------------------------
// enumerateAndSetup()
// ---------------------------------------------------------------------------

void DeviceSession::enumerateAndSetup()
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
    QString serial;
    QString firmwareVersion;
    qCDebug(lcDevice) << "hasDeviceInfo:" << m_features->hasFeature(hidpp::FeatureId::DeviceInfo)
                      << "hasFeatureSet:" << m_features->hasFeature(hidpp::FeatureId::FeatureSet);
    if (!m_features->hasFeature(hidpp::FeatureId::DeviceInfo)) {
        if (m_features->hasFeature(hidpp::FeatureId::FeatureSet)) {
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
        auto countResp = m_features->call(m_transport.get(), m_deviceIndex,
                                           hidpp::FeatureId::DeviceInfo, 0x00);
        int entityCount = countResp.has_value() ? countResp->params[0] : 3;

        for (int entity = 0; entity < entityCount; ++entity) {
            uint8_t entityParam[1] = {static_cast<uint8_t>(entity)};
            auto fwResp = m_features->call(m_transport.get(), m_deviceIndex,
                                            hidpp::FeatureId::DeviceInfo, 0x01, entityParam);
            if (!fwResp.has_value()) continue;

            uint8_t type = fwResp->params[0];
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

            if (type == 0)
                firmwareVersion = ver;
        }

        auto infoResp = m_features->call(m_transport.get(), m_deviceIndex,
                                          hidpp::FeatureId::DeviceInfo, 0x02);
        if (infoResp.has_value()) {
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

    if (serial.isEmpty()) {
        serial = QString::number(qHash(name), 16);
        qCDebug(lcDevice) << "device id (hash fallback):" << serial;
    }

    // Resolve variant dispatches
    m_batteryDispatch        = hidpp::capabilities::resolveCapability(
                                   m_features.get(), hidpp::capabilities::kBatteryVariants);
    m_smartShiftDispatch     = hidpp::capabilities::resolveCapability(
                                   m_features.get(), hidpp::capabilities::kSmartShiftVariants);
    m_reprogControlsDispatch = hidpp::capabilities::resolveCapability(
                                   m_features.get(), hidpp::capabilities::kReprogControlsVariants);
    if (m_reprogControlsDispatch) {
        qCDebug(lcDevice) << "ReprogControls: feature="
                          << Qt::hex << static_cast<uint16_t>(m_reprogControlsDispatch->feature)
                          << Qt::dec
                          << (m_reprogControlsDispatch->supportsDiversion
                                ? "(diversion supported)" : "(enumeration only)");
    }

    // Read battery
    int battLevel = 0;
    bool battCharging = false;
    if (m_batteryDispatch) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     m_batteryDispatch->feature,
                                     m_batteryDispatch->getFn);
        if (resp.has_value()) {
            auto status = m_batteryDispatch->parse(*resp);
            battLevel    = status.level;
            battCharging = status.charging;
            qCDebug(lcDevice) << "battery: feature="
                              << Qt::hex << static_cast<uint16_t>(m_batteryDispatch->feature)
                              << Qt::dec << "level=" << battLevel << "% charging=" << battCharging;
        }
    }

    // Read DPI range and current value
    if (m_features->hasFeature(hidpp::FeatureId::AdjustableDPI)) {
        auto rangeResp = m_features->call(m_transport.get(), m_deviceIndex,
                                          hidpp::FeatureId::AdjustableDPI,
                                          hidpp::features::AdjustableDPI::kFnGetSensorDpiList,
                                          std::array<uint8_t, 1>{0x00});
        if (rangeResp.has_value()) {
            auto info = hidpp::features::AdjustableDPI::parseSensorDpiList(*rangeResp);
            m_minDPI = info.minDPI;
            m_maxDPI = info.maxDPI;
            m_dpiStep = info.stepDPI > 0 ? info.stepDPI : 50;
            qCDebug(lcDevice) << "DPI range:" << m_minDPI << "-" << m_maxDPI
                              << "step:" << m_dpiStep;
        }
        auto dpiResp = m_features->call(m_transport.get(), m_deviceIndex,
                                        hidpp::FeatureId::AdjustableDPI,
                                        hidpp::features::AdjustableDPI::kFnGetSensorDpi,
                                        std::array<uint8_t, 1>{0x00});
        if (dpiResp.has_value()) {
            m_currentDPI = hidpp::features::AdjustableDPI::parseCurrentDPI(*dpiResp);
            qCDebug(lcDevice) << "current DPI:" << m_currentDPI;
        }
    }

    // Read SmartShift
    if (m_smartShiftDispatch) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     m_smartShiftDispatch->feature,
                                     m_smartShiftDispatch->getFn);
        if (resp.has_value()) {
            auto cfg = m_smartShiftDispatch->parseGet(*resp);
            m_smartShiftEnabled   = cfg.isRatchet();
            m_smartShiftThreshold = cfg.autoDisengage;
            qCDebug(lcDevice) << "SmartShift: feature="
                              << Qt::hex << static_cast<uint16_t>(m_smartShiftDispatch->feature)
                              << Qt::dec << "mode=" << cfg.mode
                              << (m_smartShiftEnabled ? "(ratchet)" : "(freespin)")
                              << "autoDisengage=" << m_smartShiftThreshold;
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

    // Look up device descriptor
    m_activeDevice = nullptr;
    if (m_registry) {
        m_activeDevice = m_registry->findByPid(m_device->info().productId);
        if (!m_activeDevice && !name.isEmpty())
            m_activeDevice = m_registry->findByName(name);
    }
    if (m_activeDevice)
        qCDebug(lcDevice) << "matched device descriptor:" << m_activeDevice->deviceName();
    else {
        qCDebug(lcDevice) << "no device descriptor found for PID"
                          << Qt::hex << m_device->info().productId << "name:" << name;
        emit unknownDeviceDetected(m_device->info().productId);
    }

    // Undivert ALL buttons for clean native state on startup.
    // Only V4 supports SetControlReporting; older variants can enumerate
    // buttons but can't divert them, so there's nothing to undo.
    if (m_reprogControlsDispatch && m_reprogControlsDispatch->supportsDiversion) {
        if (m_activeDevice) {
            for (const auto &ctrl : m_activeDevice->controls()) {
                if (ctrl.configurable && ctrl.controlId != 0) {
                    auto params = hidpp::features::ReprogControls::buildSetDivert(ctrl.controlId, false);
                    m_features->call(m_transport.get(), m_deviceIndex,
                                     m_reprogControlsDispatch->feature,
                                     hidpp::features::ReprogControls::kFnSetControlReporting,
                                     std::span<const uint8_t>(params));
                }
            }
        }
        qCDebug(lcDevice) << "all buttons undiverted (clean native state)";
    }

    // Undivert thumb wheel
    if (m_features->hasFeature(hidpp::FeatureId::ThumbWheel)) {
        std::array<uint8_t, 2> twParams = {0x00, 0x00};
        m_features->call(m_transport.get(), m_deviceIndex,
                         hidpp::FeatureId::ThumbWheel, 0x02,
                         std::span<const uint8_t>(twParams));
        m_thumbWheelMode = "scroll";

        auto twInfo = m_features->call(m_transport.get(), m_deviceIndex,
                                       hidpp::FeatureId::ThumbWheel, 0x00, {});
        if (twInfo.has_value()) {
            m_thumbWheelDefaultDirection = (twInfo->params[4] & 0x01) ? 1 : -1;
            qCDebug(lcDevice) << "thumb wheel defaultDirection:" << m_thumbWheelDefaultDirection;
        }
        qCDebug(lcDevice) << "thumb wheel set to native scroll";
    }

    // Update state and emit signals
    qCDebug(lcDevice) << "battery:" << battLevel << "% charging:" << battCharging;

    // Read current Easy-Switch host
    int currentHost = -1;
    int hostCount = 0;
    if (m_features->hasFeature(hidpp::FeatureId::ChangeHost)) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     hidpp::FeatureId::ChangeHost, 0x00);
        if (resp.has_value()) {
            hostCount = resp->params[0];
            currentHost = resp->params[1];
            qCDebug(lcDevice) << "Easy-Switch: host" << currentHost << "of" << hostCount;

            auto cookieResp = m_features->call(m_transport.get(), m_deviceIndex,
                                               hidpp::FeatureId::ChangeHost, 0x02);
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

    m_deviceName     = m_activeDevice ? m_activeDevice->deviceName() : name;
    m_deviceSerial   = serial;
    m_firmwareVersion = firmwareVersion;
    m_deviceVid      = m_device->info().vendorId;
    m_devicePid      = m_device->info().productId;
    m_batteryLevel   = battLevel;
    m_batteryCharging = battCharging;

    m_connected = true;

    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();
    m_enumerating = false;

    if (!m_commandQueue && m_features && m_transport) {
        m_commandQueue = std::make_unique<hidpp::CommandQueue>(
            m_features.get(), m_transport.get(), m_deviceIndex);
        m_commandQueue->start();
    }

    // Start periodic battery polling (60s)
    if (!m_batteryPollTimer) {
        m_batteryPollTimer = new QTimer(this);
        m_batteryPollTimer->setInterval(60000);
        connect(m_batteryPollTimer, &QTimer::timeout, this, [this]() {
            if (!m_connected || !m_features || !m_transport) return;
            if (!m_batteryDispatch) return;
            auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                          m_batteryDispatch->feature,
                                          m_batteryDispatch->getFn);
            if (resp.has_value()) {
                auto status = m_batteryDispatch->parse(*resp);
                qCDebug(lcDevice) << "battery poll:" << status.level << "% charging:" << status.charging;
                bool levelChanged   = (m_batteryLevel != status.level);
                bool chargeChanged  = (m_batteryCharging != status.charging);
                m_batteryLevel   = status.level;
                m_batteryCharging = status.charging;
                if (levelChanged || chargeChanged)
                    emit batteryChanged(m_batteryLevel, m_batteryCharging);
            }
        });
    }
    m_batteryPollTimer->start();

    emit setupComplete();
}

// ---------------------------------------------------------------------------
// applySimulation() — --simulate-all entry point
// ---------------------------------------------------------------------------

void DeviceSession::applySimulation(const IDevice *dev, const QString &fakeSerial)
{
    m_activeDevice = dev;
    m_deviceName = dev ? dev->deviceName() : QStringLiteral("Simulated");
    m_deviceSerial = fakeSerial;
    m_connected = true;

    // Populate a few visible-in-UI fields with plausible stubs so the
    // cards render without empty regions. These never hit real hardware.
    m_batteryLevel = 85;
    m_batteryCharging = false;
    if (dev) {
        m_minDPI = dev->minDpi();
        m_maxDPI = dev->maxDpi();
        m_dpiStep = dev->dpiStep();
        m_currentDPI = (m_minDPI + m_maxDPI) / 2;
    }
}

// ---------------------------------------------------------------------------
// disconnectCleanup()
// ---------------------------------------------------------------------------

void DeviceSession::disconnectCleanup()
{
    if (m_batteryPollTimer)
        m_batteryPollTimer->stop();

    if (m_commandQueue) {
        m_commandQueue->clear();
        m_commandQueue->stop();
        m_commandQueue.reset();
    }
    m_activeDevice = nullptr;
    m_batteryDispatch.reset();
    m_smartShiftDispatch.reset();
    m_reprogControlsDispatch.reset();
    m_features.reset();
    m_transport.reset();
    m_device.reset();
    m_deviceIndex = 0xFF;

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

    emit disconnected();
}

// ---------------------------------------------------------------------------
// handleNotification()
// ---------------------------------------------------------------------------

void DeviceSession::handleNotification(const hidpp::Report &report)
{
    if (report.softwareId != 0) {
        if (m_features)
            m_features->handleResponse(report);
        return;
    }

    qCDebug(lcDevice) << "notification: featureIndex=" << Qt::hex << report.featureIndex
                      << "functionId=" << report.functionId;

    // HID++ 1.0 DeviceConnection notification from Bolt/Unifying receiver
    if (report.featureIndex == 0x41) {
        bool linkEstablished = (report.params[0] & 0x40) == 0;
        qCDebug(lcDevice) << "DeviceConnection:" << (linkEstablished ? "connected" : "disconnected");
        if (!linkEstablished && m_connected) {
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
            emit batteryChanged(m_batteryLevel, m_batteryCharging);
            emit disconnected();
        } else if (linkEstablished && !m_connected) {
            qCDebug(lcDevice) << "device reconnected, re-enumerating (1500ms delay)";
            if (m_reconnectTimer) {
                m_reconnectTimer->stop();
                delete m_reconnectTimer;
            }
            m_reconnectTimer = new QTimer(this);
            m_reconnectTimer->setSingleShot(true);
            connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
                m_reconnectTimer = nullptr;
                if (!m_connected && m_device && m_transport) {
                    enumerateAndSetup();
                }
            });
            m_reconnectTimer->start(1500);
        }
        return;
    }

    // Battery notification
    if (m_batteryDispatch && m_features) {
        auto idx = m_features->featureIndex(m_batteryDispatch->feature);
        if (idx.has_value() && report.featureIndex == *idx) {
            qCDebug(lcDevice) << "battery raw params:"
                              << Qt::hex << report.params[0] << report.params[1] << report.params[2] << report.params[3];
            auto status = m_batteryDispatch->parse(report);
            qCDebug(lcDevice) << "battery notification:" << status.level << "% charging:" << status.charging;
            bool levelChanged  = (m_batteryLevel    != status.level);
            bool chargeChanged = (m_batteryCharging != status.charging);
            m_batteryLevel    = status.level;
            m_batteryCharging = status.charging;
            if (levelChanged || chargeChanged)
                emit batteryChanged(m_batteryLevel, m_batteryCharging);
            return;
        }
    }

    // HiResWheel ratchet notification
    if (m_features && m_features->hasFeature(hidpp::FeatureId::HiResWheel)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::HiResWheel);
        if (idx.has_value() && report.featureIndex == *idx && report.functionId == 1) {
            bool ratchet = (report.params[0] & 0x01) != 0;
            if (m_scrollRatchet != ratchet) {
                m_scrollRatchet = ratchet;
                m_smartShiftEnabled = ratchet;
                qCDebug(lcDevice) << "SmartShift button:" << (ratchet ? "ratchet" : "freespin");
                emit smartShiftChanged(m_smartShiftEnabled, m_smartShiftThreshold);
                emit scrollConfigChanged();
            }
            return;
        }
    }

    // SmartShift notification
    if (m_smartShiftDispatch && m_features) {
        auto idx = m_features->featureIndex(m_smartShiftDispatch->feature);
        if (idx.has_value() && report.featureIndex == *idx) {
            auto cfg = m_smartShiftDispatch->parseGet(report);
            bool newEnabled = cfg.isRatchet();
            if (m_smartShiftEnabled != newEnabled) {
                m_smartShiftEnabled   = newEnabled;
                m_smartShiftThreshold = cfg.autoDisengage;
                qCDebug(lcDevice) << "SmartShift toggled:"
                                  << (newEnabled ? "ratchet" : "freespin");
                emit smartShiftChanged(m_smartShiftEnabled, m_smartShiftThreshold);
            }
            return;
        }
    }

    // ReprogControls notifications — only V4 emits diverted button / raw XY
    // events, so other variants have nothing to route here.
    if (m_features && m_reprogControlsDispatch && m_reprogControlsDispatch->supportsDiversion) {
        auto idx = m_features->featureIndex(m_reprogControlsDispatch->feature);
        if (idx.has_value() && report.featureIndex == *idx) {
            if (report.functionId == 0) {
                uint16_t controlId = (static_cast<uint16_t>(report.params[0]) << 8)
                                     | report.params[1];
                bool pressed = (controlId != 0);
                qCDebug(lcDevice) << "button event: CID" << Qt::hex << controlId
                                  << (pressed ? "pressed" : "released");
                emit divertedButtonPressed(controlId, pressed);
            } else if (report.functionId == 1) {
                auto evt = hidpp::features::ReprogControls::parseDivertedRawXYEvent(report);
                emit gestureRawXY(evt.dx, evt.dy);
            }
            return;
        }
    }

    // ThumbWheel notification
    if (m_features && m_features->hasFeature(hidpp::FeatureId::ThumbWheel)) {
        auto idx = m_features->featureIndex(hidpp::FeatureId::ThumbWheel);
        if (idx.has_value() && report.featureIndex == *idx) {
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

void DeviceSession::checkSleepWake()
{
    if (m_lastResponseTime == 0)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kSleepThresholdMs = 120000;

    if ((now - m_lastResponseTime) > kSleepThresholdMs) {
        if (m_enumerating) return;
        m_enumerating = true;
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

bool DeviceSession::isConnected() const { return m_connected; }
QString DeviceSession::deviceName() const { return m_deviceName; }
int DeviceSession::batteryLevel() const { return m_batteryLevel; }
bool DeviceSession::batteryCharging() const { return m_batteryCharging; }
QString DeviceSession::connectionType() const { return m_connectionType; }
int DeviceSession::currentDPI() const { return m_currentDPI; }
int DeviceSession::minDPI() const { return m_minDPI; }
int DeviceSession::maxDPI() const { return m_maxDPI; }
int DeviceSession::dpiStep() const { return m_dpiStep; }
bool DeviceSession::smartShiftEnabled() const { return m_smartShiftEnabled; }
int DeviceSession::smartShiftThreshold() const { return m_smartShiftThreshold; }

bool DeviceSession::scrollHiRes() const { return m_scrollHiRes; }
bool DeviceSession::scrollInvert() const { return m_scrollInvert; }
bool DeviceSession::scrollRatchet() const { return m_scrollRatchet; }

hidpp::FeatureDispatcher *DeviceSession::features() const { return m_features.get(); }
hidpp::Transport *DeviceSession::transport() const { return m_transport.get(); }
uint8_t DeviceSession::deviceIndex() const { return m_deviceIndex; }
hidpp::HidrawDevice *DeviceSession::device() const { return m_device.get(); }
QString DeviceSession::deviceSerial() const { return m_deviceSerial; }
QString DeviceSession::firmwareVersion() const { return m_firmwareVersion; }
uint16_t DeviceSession::deviceVid() const { return m_deviceVid; }
uint16_t DeviceSession::devicePid() const { return m_devicePid; }
const IDevice* DeviceSession::descriptor() const { return m_activeDevice; }

// ---------------------------------------------------------------------------
// setDPI()
// ---------------------------------------------------------------------------

void DeviceSession::setDPI(int value)
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

    auto params = hidpp::features::AdjustableDPI::buildSetDPI(value);
    m_commandQueue->enqueue(hidpp::FeatureId::AdjustableDPI,
                            hidpp::features::AdjustableDPI::kFnSetSensorDpi,
                            params);
}

// ---------------------------------------------------------------------------
// setSmartShift()
// ---------------------------------------------------------------------------

void DeviceSession::setSmartShift(bool enabled, int threshold)
{
    if (!m_connected || !m_features || !m_commandQueue || !m_smartShiftDispatch)
        return;

    threshold = qBound(1, threshold, 255);

    m_smartShiftEnabled   = enabled;
    m_smartShiftThreshold = threshold;
    emit smartShiftChanged(m_smartShiftEnabled, m_smartShiftThreshold);

    uint8_t mode = enabled ? 2 : 1;
    uint8_t ad   = static_cast<uint8_t>(threshold);

    auto params = m_smartShiftDispatch->buildSet(mode, ad);
    m_commandQueue->enqueue(m_smartShiftDispatch->feature,
                            m_smartShiftDispatch->setFn,
                            std::span<const uint8_t>(params));
    qCDebug(lcDevice) << "SmartShift set: feature="
                      << Qt::hex << static_cast<uint16_t>(m_smartShiftDispatch->feature)
                      << Qt::dec << "mode=" << mode << "autoDisengage=" << ad;
}

void DeviceSession::setScrollConfig(bool hiRes, bool invert)
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

void DeviceSession::divertButton(uint16_t controlId, bool divert, bool rawXY)
{
    if (!m_connected || !m_features || !m_commandQueue)
        return;
    // Only V4 supports SetControlReporting. Older variants enumerate but
    // cannot divert — silently no-op so callers don't need to special-case.
    if (!m_reprogControlsDispatch || !m_reprogControlsDispatch->supportsDiversion)
        return;

    auto params = hidpp::features::ReprogControls::buildSetDivert(controlId, divert, rawXY);
    m_commandQueue->enqueue(m_reprogControlsDispatch->feature,
                            hidpp::features::ReprogControls::kFnSetControlReporting,
                            std::span<const uint8_t>(params));
    qCDebug(lcDevice) << "button" << Qt::hex << controlId
                      << (divert ? "diverted" : "undiverted") << (rawXY ? "+rawXY" : "");
}

QString DeviceSession::thumbWheelMode() const { return m_thumbWheelMode; }
bool DeviceSession::thumbWheelInvert() const { return m_thumbWheelInvert; }
int DeviceSession::thumbWheelDefaultDirection() const { return m_thumbWheelDefaultDirection; }
int DeviceSession::currentHost() const { return m_currentHost; }
int DeviceSession::hostCount() const { return m_hostCount; }
bool DeviceSession::isHostPaired(int host) const {
    if (host < 0 || host >= m_hostPaired.size()) return false;
    return m_hostPaired[host];
}

void DeviceSession::flushCommandQueue()
{
    if (m_commandQueue)
        m_commandQueue->clear();
}

void DeviceSession::touchResponseTime()
{
    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();
}

void DeviceSession::setThumbWheelMode(const QString &mode, bool invert)
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

bool DeviceSession::isDirectDevice(uint16_t pid) const
{
    return m_registry && m_registry->findByPid(pid) != nullptr;
}

} // namespace logitune
