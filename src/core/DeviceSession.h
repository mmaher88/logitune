#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/HidrawDevice.h"
#include "hidpp/Transport.h"
#include "hidpp/FeatureDispatcher.h"
#include "hidpp/CommandProcessor.h"
#include "hidpp/capabilities/BatteryCapability.h"
#include "hidpp/capabilities/SmartShiftCapability.h"
#include "hidpp/capabilities/ReprogControlsCapability.h"
#include "interfaces/IDevice.h"
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
#include <optional>
#include <vector>

namespace logitune::test { class AppRootFixture; }

namespace logitune {

class DeviceRegistry;

class DeviceSession : public QObject {
    Q_OBJECT
    friend class test::AppRootFixture;

public:
    DeviceSession(std::unique_ptr<hidpp::HidrawDevice> device,
                  uint8_t deviceIndex,
                  const QString &connectionType,
                  DeviceRegistry *registry,
                  QObject *parent = nullptr);
    ~DeviceSession();

    // Identity
    QString deviceId() const;
    const IDevice* descriptor() const;
    uint16_t devicePid() const;
    QString deviceSerial() const;
    QString deviceName() const;
    QString connectionType() const;
    bool isConnected() const;
    uint16_t deviceVid() const;
    QString firmwareVersion() const;
    // Extended model ID from DeviceInformation (0x0003) getDeviceInfo;
    // absent before enumeration or when the device doesn't report one.
    std::optional<uint8_t> extendedModelId() const;
    // The descriptor's images, or the reported variant's where it has its own.
    QString frontImagePath() const;
    QString sideImagePath() const;
    QString backImagePath() const;

    // State getters
    int batteryLevel() const;
    bool batteryCharging() const;
    int currentDPI() const;
    int minDPI() const;
    int maxDPI() const;
    int dpiStep() const;
    bool smartShiftEnabled() const;
    int smartShiftThreshold() const;
    bool scrollHiRes() const;
    bool scrollInvert() const;
    bool scrollRatchet() const;
    QString thumbWheelMode() const;
    bool thumbWheelInvert() const;
    int thumbWheelDefaultDirection() const;
    int currentHost() const;
    int hostCount() const;
    bool isHostPaired(int host) const;

    // Pure helpers exposed as static methods so they can be unit-tested
    // without constructing a session. See tests/test_dpi_cycle_ring.cpp.
    static std::vector<int> effectiveDpiRing(const std::vector<int> &curated,
                                             bool adjustableDpi,
                                             int minDpi, int maxDpi, int step);
    static int nextDpiInRing(const std::vector<int> &ring, int currentDpi);

    // Picks the descriptor for a device. hidrawPid is the PID of the hidraw
    // node; receiverWpid is the paired device's wireless PID when the node
    // is a receiver. See tests/test_device_session.cpp.
    static const IDevice *resolveDescriptor(const DeviceRegistry *registry,
                                            uint16_t hidrawPid,
                                            std::optional<uint16_t> receiverWpid,
                                            const QString &name);

    // The descriptor's variant for an extended model ID, or nullopt when
    // there is no descriptor, no ID, or no variant entry for it (a plain
    // retail unit). See tests/test_device_registry.cpp.
    static std::optional<DeviceVariant> resolveVariant(const IDevice *descriptor,
                                                       std::optional<uint8_t> extendedModelId);

    // Adopts the resolved descriptor and the variant the device reports.
    // The display name is the variant's, else the descriptor's, else
    // reportedName (the HID++ DeviceName) for an unknown device.
    // enumerateAndSetup calls this; tests call it to skip HID++ I/O.
    void applyDescriptor(const IDevice *descriptor,
                         std::optional<uint8_t> extendedModelId,
                         const QString &reportedName);

    Q_INVOKABLE void cycleDpi();

    // Setters (write to device via command processor)
    void setDPI(int value);
    void setSmartShift(bool enabled, int threshold);
    void setScrollConfig(bool hiRes, bool invert);
    void divertButton(uint16_t controlId, bool divert, bool rawXY = false);
    void setThumbWheelMode(const QString &mode, bool invert = false);
    void flushCommandProcessor();
    void touchResponseTime();

    // Notification handling (called by DeviceManager from hidraw notifier)
    void handleNotification(const hidpp::Report &report);

    // Enumeration (called once after construction)
    void enumerateAndSetup();

    // Access to internals
    hidpp::FeatureDispatcher *features() const;
    hidpp::Transport *transport() const;
    uint8_t deviceIndex() const;
    hidpp::HidrawDevice *device() const;

    // Test hooks — let tests inject state without a real HID++ conversation.
    // Not intended for production use; called exclusively from test fixtures.
    void setConnectedForTest(bool v) { m_connected = v; }
    void setDeviceNameForTest(const QString &n) { m_deviceName = n; }
    void setBatteryForTest(int level, bool charging) {
        m_batteryLevel = level;
        m_batteryCharging = charging;
    }

    // --simulate-all CLI flag entry point — wires a DeviceSession
    // into a "fake connected" state against a registry descriptor so
    // the UI renders its card without real hardware. Bypasses all
    // HID++ probing. Never called from production code paths.
    void applySimulation(const IDevice *dev, const QString &fakeSerial);

signals:
    void setupComplete();
    void disconnected();
    void batteryChanged(int level, bool charging);
    void smartShiftChanged(bool enabled, int threshold);
    void currentDPIChanged();
    void scrollConfigChanged();
    void thumbWheelModeChanged();
    void divertedButtonPressed(uint16_t controlId, bool pressed);
    void gestureRawXY(int16_t dx, int16_t dy);
    void thumbWheelRotation(int delta);
    void deviceWoke();
    void unknownDeviceDetected(uint16_t pid);

public:
    void disconnectCleanup();

private:
    void checkSleepWake();
    bool isDirectDevice(uint16_t pid) const;

    DeviceRegistry *m_registry = nullptr;
    const IDevice *m_activeDevice = nullptr;
    std::optional<uint8_t> m_extendedModelId;
    std::optional<DeviceVariant> m_variant;

    std::unique_ptr<hidpp::HidrawDevice> m_device;
    std::unique_ptr<hidpp::Transport> m_transport;
    std::unique_ptr<hidpp::FeatureDispatcher> m_features;
    std::unique_ptr<hidpp::CommandProcessor> m_commandProcessor;

    std::optional<hidpp::capabilities::BatteryVariant>         m_batteryDispatch;
    std::optional<hidpp::capabilities::SmartShiftVariant>      m_smartShiftDispatch;
    std::optional<hidpp::capabilities::ReprogControlsVariant>  m_reprogControlsDispatch;

    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_batteryPollTimer = nullptr;

    qint64 m_lastResponseTime = 0;
    bool m_enumerating = false;

    uint8_t m_deviceIndex = 0xFF;
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
    QString m_thumbWheelMode = "scroll";
    bool m_thumbWheelInvert = false;
    int m_thumbWheelDefaultDirection = 1;
    int m_currentHost = -1;
    int m_hostCount = 0;
    QVector<bool> m_hostPaired;
};

} // namespace logitune
