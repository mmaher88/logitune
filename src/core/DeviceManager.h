#pragma once
#include "DeviceSession.h"
#include <QMap>
#include <QObject>
#include <QSocketNotifier>
#include <QStringList>
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

    // Session access
    const std::vector<std::unique_ptr<DeviceSession>>& sessions() const;
    DeviceSession* sessionById(const QString &id) const;
    DeviceSession* sessionByPid(uint16_t pid) const;

    // Backward compat — returns first session's descriptor
    const IDevice* activeDevice() const;

signals:
    void sessionAdded(const QString &deviceId);
    void sessionRemoved(const QString &deviceId);
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

    // Track which devNode each session came from
    QMap<QString, QString> m_devNodeToSessionId;

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
