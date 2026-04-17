#pragma once
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "interfaces/IDesktopIntegration.h"
#include <QAbstractListModel>
#include <QMap>
#include <QPair>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace logitune {

class DeviceModel : public QAbstractListModel {
    Q_OBJECT

    // List model metadata
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId NOTIFY selectedChanged)

    // Selected device properties (backward-compatible with existing QML pages)
    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY selectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY selectedChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY selectedBatteryChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY selectedBatteryChanged)
    Q_PROPERTY(QString batteryStatusText READ batteryStatusText NOTIFY selectedBatteryChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY selectedChanged)
    Q_PROPERTY(int currentDPI READ currentDPI NOTIFY settingsReloaded)
    Q_PROPERTY(int minDPI READ minDPI NOTIFY selectedChanged)
    Q_PROPERTY(int maxDPI READ maxDPI NOTIFY selectedChanged)
    Q_PROPERTY(int dpiStep READ dpiStep NOTIFY selectedChanged)
    Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY settingsReloaded)
    Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY settingsReloaded)
    Q_PROPERTY(bool scrollHiRes READ scrollHiRes NOTIFY settingsReloaded)
    Q_PROPERTY(bool scrollInvert READ scrollInvert NOTIFY settingsReloaded)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY activeProfileNameChanged)
    Q_PROPERTY(QString activeWmClass READ activeWmClass NOTIFY activeWmClassChanged)

    Q_PROPERTY(QString frontImage READ frontImage NOTIFY selectedChanged)
    Q_PROPERTY(QString sideImage READ sideImage NOTIFY selectedChanged)
    Q_PROPERTY(QString backImage READ backImage NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList buttonHotspots READ buttonHotspots NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList scrollHotspots READ scrollHotspots NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList controlDescriptors READ controlDescriptors NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList easySwitchSlotPositions READ easySwitchSlotPositions NOTIFY selectedChanged)
    Q_PROPERTY(bool smoothScrollSupported READ smoothScrollSupported NOTIFY selectedChanged)
    Q_PROPERTY(bool thumbWheelSupported READ thumbWheelSupported NOTIFY selectedChanged)
    Q_PROPERTY(QString deviceSerial READ deviceSerial NOTIFY selectedChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY selectedChanged)
    Q_PROPERTY(int activeSlot READ activeSlot NOTIFY selectedChanged)
    Q_PROPERTY(QString deviceStatus READ deviceStatus NOTIFY selectedChanged)
    Q_PROPERTY(QString thumbWheelMode READ thumbWheelMode NOTIFY settingsReloaded)
    Q_PROPERTY(bool thumbWheelInvert READ thumbWheelInvert NOTIFY settingsReloaded)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        FrontImageRole,
        BatteryLevelRole,
        BatteryChargingRole,
        ConnectionTypeRole,
        StatusRole,
        IsSelectedRole,
    };

    explicit DeviceModel(QObject *parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Selection
    int count() const;
    int selectedIndex() const;
    void setSelectedIndex(int index);
    QString selectedDeviceId() const;

    // Physical device management. DeviceModel stores PhysicalDevice pointers
    // (non-owning — DeviceManager owns). Each PhysicalDevice is one row.
    // Transport switches are handled internally by PhysicalDevice, so the
    // carousel row stays stable across Bolt <-> BT.
    void addPhysicalDevice(PhysicalDevice *device);
    void removePhysicalDevice(PhysicalDevice *device);
    bool hasDeviceId(const QString &deviceId) const;
    const QList<PhysicalDevice *> &devices() const;
    Q_INVOKABLE void moveDevice(int from, int to);

    const IDevice *activeDevice() const;

    void setDesktopIntegration(IDesktopIntegration *desktop);
    Q_INVOKABLE void blockGlobalShortcuts(bool block);
    Q_INVOKABLE QVariantList runningApplications() const;

    // Selected device getters
    bool deviceConnected() const;
    QString deviceName() const;
    int batteryLevel() const;
    bool batteryCharging() const;
    QString batteryStatusText() const;
    QString connectionType() const;
    int currentDPI() const;
    int minDPI() const;
    int maxDPI() const;
    int dpiStep() const;
    bool smartShiftEnabled() const;
    int smartShiftThreshold() const;
    QString activeProfileName() const;
    QString activeWmClass() const;

    QString frontImage() const;
    QString sideImage() const;
    QString backImage() const;
    QVariantList buttonHotspots() const;
    QVariantList scrollHotspots() const;
    QVariantList controlDescriptors() const;
    QVariantList easySwitchSlotPositions() const;
    bool smoothScrollSupported() const;
    bool thumbWheelSupported() const;
    QString deviceSerial() const;
    QString firmwareVersion() const;
    int activeSlot() const;
    QString deviceStatus() const;
    Q_INVOKABLE bool isSlotPaired(int slot) const;

    Q_INVOKABLE void setDPI(int value);
    Q_INVOKABLE void setSmartShift(bool enabled, int threshold);
    Q_INVOKABLE void setScrollConfig(bool hiRes, bool invert);
    Q_INVOKABLE void setThumbWheelMode(const QString &mode);
    Q_INVOKABLE void resetAllProfiles();
    Q_INVOKABLE void setGestureAction(const QString &direction, const QString &actionName, const QString &keystroke);
    Q_INVOKABLE QString gestureActionName(const QString &direction) const;
    Q_INVOKABLE QString gestureKeystroke(const QString &direction) const;
    bool scrollHiRes() const;
    bool scrollInvert() const;
    QString thumbWheelMode() const;
    bool thumbWheelInvert() const;
    Q_INVOKABLE void setThumbWheelInvert(bool invert);
    Q_INVOKABLE QString gnomeTrayStatus() const;

    void loadGesturesFromProfile(const QMap<QString, QPair<QString, QString>> &gestures);
    void setActiveProfileName(const QString &name);
    void setActiveWmClass(const QString &wmClass);
    void setDisplayValues(int dpi, bool smartShiftEnabled, int smartShiftThreshold,
                          bool scrollHiRes, bool scrollInvert, const QString &thumbWheelMode,
                          bool thumbWheelInvert = false);

public slots:
    // Re-emit selectedChanged so QML bindings that read from the active
    // IDevice* (controls, hotspots, slots, images) re-fetch. Used by
    // EditorModel after it mutates the underlying JsonDevice in place.
    void refreshFromActiveDevice();

signals:
    void countChanged();
    void selectedChanged();
    void selectedBatteryChanged();
    void selectedSettingsChanged();
    void deviceConnectedChanged();
    void deviceNameChanged();
    void batteryLevelChanged();
    void batteryChargingChanged();
    void connectionTypeChanged();
    void currentDPIChanged();
    void smartShiftEnabledChanged();
    void smartShiftThresholdChanged();
    void scrollConfigChanged();
    void thumbWheelModeChanged();
    void settingsReloaded();
    void activeProfileNameChanged();
    void activeWmClassChanged();
    void gestureChanged();
    void userGestureChanged(const QString &direction, const QString &actionName, const QString &keystroke);
    void dpiChangeRequested(int value);
    void smartShiftChangeRequested(bool enabled, int threshold);
    void scrollConfigChangeRequested(bool hiRes, bool invert);
    void thumbWheelModeChangeRequested(const QString &mode);
    void thumbWheelInvertChangeRequested(bool invert);

private:
    PhysicalDevice *selectedDevice() const;
    void saveDeviceOrder() const;
    QStringList loadDeviceOrder() const;
    int rowForDevice(PhysicalDevice *device) const;
    void insertRow(PhysicalDevice *device);
    void removeRow(PhysicalDevice *device);
    void refreshRow(PhysicalDevice *device);

    // One entry per PhysicalDevice. Transport-switching is handled inside
    // PhysicalDevice (see src/core/PhysicalDevice.h); DeviceModel just
    // observes and reflects whatever primary transport is active.
    QList<PhysicalDevice *> m_devices;

    int m_selectedIndex = -1;

    IDesktopIntegration *m_desktop = nullptr;
    QMap<QString, QPair<QString, QString>> m_gestures;
    QString m_activeProfileName;
    QString m_activeWmClass;

    int m_displayDpi = -1;
    bool m_displaySmartShiftEnabled = false;
    int m_displaySmartShiftThreshold = 0;
    bool m_displayScrollHiRes = false;
    bool m_displayScrollInvert = false;
    QString m_displayThumbWheelMode;
    bool m_displayThumbWheelInvert = false;
    bool m_hasDisplayValues = false;
};

} // namespace logitune
