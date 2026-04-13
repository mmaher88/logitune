#pragma once
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include <QMap>
#include <QObject>
#include <QSocketNotifier>
#include <QStringList>
#include <map>
#include <memory>
#include <vector>

struct udev;
struct udev_monitor;

namespace logitune {

class DeviceRegistry;

class DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit DeviceManager(DeviceRegistry *registry, QObject *parent = nullptr);
    ~DeviceManager();

    void start();

    // Static helpers
    static bool isReceiver(uint16_t pid);
    static uint8_t deviceIndexForDirect();
    static uint8_t deviceIndexForReceiver(int slot);

    // Session access (transport layer)
    const std::vector<std::unique_ptr<DeviceSession>>& sessions() const;
    DeviceSession* sessionById(const QString &id) const;
    DeviceSession* sessionByPid(uint16_t pid) const;

    // Physical device access (logical layer, one per unique serial).
    QList<PhysicalDevice *> physicalDevices() const;
    PhysicalDevice *physicalDeviceBySerial(const QString &serial) const;

    // Backward compat — returns first session's descriptor
    const IDevice* activeDevice() const;

signals:
    // Physical-device lifecycle. One PhysicalDevice per unique HID++ serial.
    // A transport switch (Bolt -> BT) does NOT trigger these — it's handled
    // internally by PhysicalDevice attaching/detaching transports. These
    // only fire when a device is first seen or completely goes away.
    void physicalDeviceAdded(PhysicalDevice *device);
    void physicalDeviceRemoved(PhysicalDevice *device);

    void unknownDeviceDetected(uint16_t pid);

private slots:
    void onUdevReady();

private:
    void scanExistingDevices();
    void onUdevEvent(const QString &action, const QString &devNode);
    void probeDevice(const QString &devNode);
    void removeSessionByDevNode(const QString &devNode);

    DeviceRegistry *m_registry = nullptr;
    std::vector<std::unique_ptr<DeviceSession>> m_sessions;

    // Owns PhysicalDevice wrappers keyed by serial. Created lazily on
    // first transport attach, destroyed when the last transport detaches.
    // std::map (not QMap) because QMap requires copyable value types and
    // unique_ptr is move-only.
    std::map<QString, std::unique_ptr<PhysicalDevice>> m_physicalDevices;

    // Track which specific session came from which devNode.
    QMap<QString, DeviceSession *> m_devNodeToSession;

    // Transport failover
    QStringList m_availableTransports;

    // Receiver monitoring (when no device on slots)
    std::unique_ptr<hidpp::HidrawDevice> m_receiverDevice;
    QSocketNotifier *m_receiverNotifier = nullptr;

    struct udev *m_udev = nullptr;
    struct udev_monitor *m_udevMon = nullptr;
    QSocketNotifier *m_udevNotifier = nullptr;
};

} // namespace logitune
