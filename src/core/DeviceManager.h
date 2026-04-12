#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/HidrawDevice.h"
#include "hidpp/Transport.h"
#include "hidpp/FeatureDispatcher.h"
#include "hidpp/CommandQueue.h"
#include "hidpp/capabilities/BatteryCapability.h"
#include "hidpp/capabilities/SmartShiftCapability.h"
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
#include <optional>

struct udev;
struct udev_monitor;

namespace logitune::test { class AppControllerFixture; }

namespace logitune {

class DeviceRegistry;
class IDevice;

class DeviceManager : public QObject {
    Q_OBJECT
    friend class test::AppControllerFixture;
    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY batteryChargingChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(int currentDPI READ currentDPI NOTIFY currentDPIChanged)
    Q_PROPERTY(int minDPI READ minDPI CONSTANT)
    Q_PROPERTY(int maxDPI READ maxDPI CONSTANT)
    Q_PROPERTY(int dpiStep READ dpiStep CONSTANT)
    Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY smartShiftChanged)
    Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY smartShiftChanged)
    Q_PROPERTY(bool scrollHiRes READ scrollHiRes NOTIFY scrollConfigChanged)
    Q_PROPERTY(bool scrollInvert READ scrollInvert NOTIFY scrollConfigChanged)
    Q_PROPERTY(bool scrollRatchet READ scrollRatchet NOTIFY scrollConfigChanged)

public:
    explicit DeviceManager(DeviceRegistry *registry, QObject *parent = nullptr);
    ~DeviceManager();

    void start();

    // Static helpers (testable without hardware)
    static bool isReceiver(uint16_t pid);
    bool isDirectDevice(uint16_t pid) const;
    static uint8_t deviceIndexForDirect();
    static uint8_t deviceIndexForReceiver(int slot);

    // Active device descriptor (available after connect)
    const IDevice* activeDevice() const;

    // Properties
    bool deviceConnected() const;
    QString deviceName() const;
    int batteryLevel() const;
    bool batteryCharging() const;
    QString connectionType() const;
    int currentDPI() const;
    int minDPI() const;
    int maxDPI() const;
    int dpiStep() const;
    bool smartShiftEnabled() const;
    int smartShiftThreshold() const;

    // Set device settings (calls HID++ directly)
    Q_INVOKABLE void setDPI(int value);
    Q_INVOKABLE void setSmartShift(bool enabled, int threshold);
    Q_INVOKABLE void setScrollConfig(bool hiRes, bool invert);
    Q_INVOKABLE void divertButton(uint16_t controlId, bool divert, bool rawXY = false);
    Q_INVOKABLE void setThumbWheelMode(const QString &mode, bool invert = false);
    void flushCommandQueue();   // discard pending commands (call before applying new profile)
    void touchResponseTime();  // prevent false sleep/wake detection during intentional writes
    bool scrollHiRes() const;
    bool scrollInvert() const;
    bool scrollRatchet() const;
    QString thumbWheelMode() const;
    bool thumbWheelInvert() const;
    int thumbWheelDefaultDirection() const;  // -1 or +1

    // Device identity (available after connect)
    QString deviceSerial() const;
    QString firmwareVersion() const;
    uint16_t deviceVid() const;
    uint16_t devicePid() const;

    // Easy-Switch host info
    int currentHost() const;   // 0-based, -1 if unknown
    int hostCount() const;
    bool isHostPaired(int host) const;

    // Access to internals for other components
    hidpp::FeatureDispatcher *features() const;
    hidpp::Transport *transport() const;
    uint8_t deviceIndex() const;

signals:
    void deviceConnectedChanged();
    void deviceSetupComplete();  // emitted after enumerateAndSetup() finishes successfully
    void deviceNameChanged();
    void batteryLevelChanged();
    void batteryChargingChanged();
    void connectionTypeChanged();
    void currentDPIChanged();
    void smartShiftChanged();
    void scrollConfigChanged();
    void thumbWheelModeChanged();
    void thumbWheelRotation(int delta); // emitted when diverted, raw rotation
    void deviceDisconnected();
    void transportSwitched(const QString &newType);
    void divertedButtonPressed(uint16_t controlId, bool pressed);
    void gestureRawXY(int16_t dx, int16_t dy);  // raw mouse deltas from diverted gesture button
    void deviceWoke();
    void unknownDeviceDetected(uint16_t pid);

private slots:
    void onUdevReady();

private:
    void scanExistingDevices();
    void onUdevEvent(const QString &action, const QString &devNode);
    void probeDevice(const QString &devNode);
    void disconnectDevice();
    void enumerateAndSetup();

public:
    // Public for testing — called from QSocketNotifier lambda
    void handleNotification(const hidpp::Report &report);

private:

    // Sleep/wake
    void checkSleepWake();
    qint64 m_lastResponseTime = 0;
    bool m_enumerating = false;

    // Reconnect debounce
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_batteryPollTimer = nullptr;

    // Transport failover
    QStringList m_availableTransports;

    DeviceRegistry *m_registry = nullptr;
    const IDevice *m_activeDevice = nullptr;

    std::unique_ptr<hidpp::HidrawDevice> m_device;
    std::unique_ptr<hidpp::HidrawDevice> m_receiverDevice;  // kept open for transport switching
    std::unique_ptr<hidpp::Transport> m_transport;
    std::unique_ptr<hidpp::FeatureDispatcher> m_features;
    std::unique_ptr<hidpp::CommandQueue> m_commandQueue;

    // Resolved capability dispatches — set once at enumerateAndSetup
    std::optional<hidpp::capabilities::BatteryVariant>    m_batteryDispatch;
    std::optional<hidpp::capabilities::SmartShiftVariant> m_smartShiftDispatch;

    QSocketNotifier *m_hidrawNotifier = nullptr;
    QSocketNotifier *m_receiverNotifier = nullptr;  // listens for DJ device-arrival when no device on slots

    struct udev *m_udev = nullptr;
    struct udev_monitor *m_udevMon = nullptr;
    QSocketNotifier *m_udevNotifier = nullptr;
    uint8_t m_deviceIndex = 0xFF;

    // Cached state
    bool m_connected = false;
    QString m_deviceName;
    QString m_deviceSerial;
    QString m_firmwareVersion;
    uint16_t m_deviceVid = 0;
    uint16_t m_devicePid = 0;
    int m_batteryLevel = 0;
    bool m_batteryCharging = false;
    QString m_connectionType;
    int m_currentDPI = 0;
    int m_minDPI = 200;
    int m_maxDPI = 8000;
    int m_dpiStep = 50;
    bool m_smartShiftEnabled = false;
    int m_smartShiftThreshold = 0;
    bool m_scrollHiRes = false;
    bool m_scrollInvert = false;
    bool m_scrollRatchet = true;
    uint8_t m_scrollModeByte = 0;
    QString m_thumbWheelMode = "scroll"; // "scroll", "zoom", "volume"
    bool m_thumbWheelInvert = false;
    int m_thumbWheelDefaultDirection = -1;  // -1 = positive when left (MX Master 3S default)
    int m_currentHost = -1;   // Easy-Switch: 0-based, -1 if unknown
    int m_hostCount = 0;
    QVector<bool> m_hostPaired;
};

} // namespace logitune
