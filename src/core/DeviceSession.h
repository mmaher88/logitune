#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/HidrawDevice.h"
#include "hidpp/Transport.h"
#include "hidpp/FeatureDispatcher.h"
#include "hidpp/CommandQueue.h"
#include "hidpp/capabilities/BatteryCapability.h"
#include "hidpp/capabilities/SmartShiftCapability.h"
#include "hidpp/capabilities/ReprogControlsCapability.h"
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
#include <optional>

namespace logitune::test { class AppControllerFixture; }

namespace logitune {

class DeviceRegistry;
class IDevice;

class DeviceSession : public QObject {
    Q_OBJECT
    friend class test::AppControllerFixture;

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

    // Setters (write to device via command queue)
    void setDPI(int value);
    void setSmartShift(bool enabled, int threshold);
    void setScrollConfig(bool hiRes, bool invert);
    void divertButton(uint16_t controlId, bool divert, bool rawXY = false);
    void setThumbWheelMode(const QString &mode, bool invert = false);
    void flushCommandQueue();
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

    std::unique_ptr<hidpp::HidrawDevice> m_device;
    std::unique_ptr<hidpp::Transport> m_transport;
    std::unique_ptr<hidpp::FeatureDispatcher> m_features;
    std::unique_ptr<hidpp::CommandQueue> m_commandQueue;

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
