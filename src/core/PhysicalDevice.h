#pragma once
#include <QList>
#include <QObject>
#include <QString>

namespace logitune {

class DeviceSession;
class IDevice;

// A single physical mouse, identified by its HID++ unit serial. Aggregates
// one or more DeviceSession transports (e.g. Bolt receiver + direct
// Bluetooth) behind a single identity. Delegates read access to the
// currently-active (primary) transport and picks which transport should
// be primary based on connection state.
//
// Lifetime is managed by DeviceManager: created when the first transport
// for a serial arrives, destroyed when the last transport is detached.
// All transport pointers are non-owning — DeviceManager owns the
// DeviceSession objects.
class PhysicalDevice : public QObject {
    Q_OBJECT
public:
    explicit PhysicalDevice(const QString &serial, QObject *parent = nullptr);

    QString serial() const { return m_serial; }

    // The currently-active transport. Commands should be routed here.
    // Null if this device has no transports attached (should not happen
    // during normal operation since DeviceManager destroys empty
    // PhysicalDevices).
    DeviceSession *primary() const { return m_primary; }

    // All currently-known transports (primary + alternates).
    QList<DeviceSession *> transports() const;
    int transportCount() const;

    // Attach a transport. If there's no current primary, or the current
    // primary is offline and this one is connected, this becomes primary.
    // Otherwise it's tracked as an alternate for failover.
    void attachTransport(DeviceSession *session);

    // Detach a specific transport. If it was primary and an alternate
    // exists, the alternate is promoted. Returns true if no transports
    // remain (caller should destroy this PhysicalDevice).
    bool detachTransport(DeviceSession *session);

    // Read-only delegation to the primary transport. Returns defaults
    // when no primary or primary not yet enumerated.
    bool isConnected() const;
    QString deviceName() const;
    QString connectionType() const;
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
    QString deviceSerial() const;
    QString firmwareVersion() const;
    const IDevice *descriptor() const;

signals:
    // Fires whenever anything observable changes: primary swap, session
    // enumerate complete, session disconnect, battery level change, etc.
    // DeviceModel hooks this to emit QAbstractListModel::dataChanged.
    void stateChanged();

    // Fires on setupComplete from any transport (fresh enumerate or
    // reconnect re-enumerate). AppController hooks this to re-apply
    // the current profile after reconnects.
    void transportSetupComplete();

    // Forwarded transport events. Emitted from whichever session produced
    // them; consumers shouldn't need to know or care about multi-transport.
    void gestureRawXY(int16_t dx, int16_t dy);
    void divertedButtonPressed(uint16_t controlId, bool pressed);
    void thumbWheelRotation(int delta);

private:
    void promoteBest();
    void connectSessionSignals(DeviceSession *session);
    void disconnectSessionSignals(DeviceSession *session);

    QString m_serial;
    DeviceSession *m_primary = nullptr;
    QList<DeviceSession *> m_alternates;
};

} // namespace logitune
