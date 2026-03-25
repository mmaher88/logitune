#pragma once
#include "DeviceManager.h"
#include <QObject>
#include <qqmlintegration.h>

namespace logitune {

class DeviceModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY batteryChargingChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(int currentDPI READ currentDPI NOTIFY currentDPIChanged)
    Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY smartShiftEnabledChanged)
    Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY smartShiftThresholdChanged)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY activeProfileNameChanged)

public:
    explicit DeviceModel(QObject *parent = nullptr);

    void setDeviceManager(DeviceManager *dm);

    bool deviceConnected() const;
    QString deviceName() const;
    int batteryLevel() const;
    bool batteryCharging() const;
    QString connectionType() const;
    int currentDPI() const;
    bool smartShiftEnabled() const;
    int smartShiftThreshold() const;
    QString activeProfileName() const;

    Q_INVOKABLE void setDPI(int value);
    Q_INVOKABLE void setSmartShift(bool enabled, int threshold);
    Q_INVOKABLE void resetAllProfiles();

    // Called from main integration to sync profile state into the model
    void setCurrentDPI(int dpi);
    void setSmartShiftState(bool enabled, int threshold);
    void setActiveProfileName(const QString &name);

signals:
    void deviceConnectedChanged();
    void deviceNameChanged();
    void batteryLevelChanged();
    void batteryChargingChanged();
    void connectionTypeChanged();
    void currentDPIChanged();
    void smartShiftEnabledChanged();
    void smartShiftThresholdChanged();
    void activeProfileNameChanged();

private:
    DeviceManager *m_dm = nullptr;
    int m_currentDPI = 1000;
    bool m_smartShiftEnabled = true;
    int m_smartShiftThreshold = 128;
    QString m_activeProfileName;
};

} // namespace logitune
