#pragma once
#include "DeviceManager.h"
#include "interfaces/IDesktopIntegration.h"
#include <QMap>
#include <QPair>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace logitune {

class DeviceModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY batteryChargingChanged)
    Q_PROPERTY(QString batteryStatusText READ batteryStatusText NOTIFY batteryLevelChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(int currentDPI READ currentDPI NOTIFY settingsReloaded)
    Q_PROPERTY(int minDPI READ minDPI CONSTANT)
    Q_PROPERTY(int maxDPI READ maxDPI CONSTANT)
    Q_PROPERTY(int dpiStep READ dpiStep CONSTANT)
    Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY settingsReloaded)
    Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY settingsReloaded)
    Q_PROPERTY(bool scrollHiRes READ scrollHiRes NOTIFY settingsReloaded)
    Q_PROPERTY(bool scrollInvert READ scrollInvert NOTIFY settingsReloaded)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY activeProfileNameChanged)
    Q_PROPERTY(QString activeWmClass READ activeWmClass NOTIFY activeWmClassChanged)

    // Logging moved to SettingsModel

    // Device descriptor properties (driven by active device)
    Q_PROPERTY(QString frontImage READ frontImage NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString sideImage READ sideImage NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString backImage READ backImage NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QVariantList buttonHotspots READ buttonHotspots NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QVariantList scrollHotspots READ scrollHotspots NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QVariantList controlDescriptors READ controlDescriptors NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QVariantList easySwitchSlotPositions READ easySwitchSlotPositions NOTIFY deviceConnectedChanged)
    Q_PROPERTY(bool smoothScrollSupported READ smoothScrollSupported NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString deviceSerial READ deviceSerial NOTIFY deviceConnectedChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY deviceConnectedChanged)
    Q_PROPERTY(int activeSlot READ activeSlot NOTIFY deviceConnectedChanged)

public:
    explicit DeviceModel(QObject *parent = nullptr);

    void setDeviceManager(DeviceManager *dm);
    void setDesktopIntegration(IDesktopIntegration *desktop);
    Q_INVOKABLE void blockGlobalShortcuts(bool block);
    Q_INVOKABLE QVariantList runningApplications() const;


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

    // Device descriptor getters
    QString frontImage() const;
    QString sideImage() const;
    QString backImage() const;
    QVariantList buttonHotspots() const;
    QVariantList scrollHotspots() const;
    QVariantList controlDescriptors() const;
    QVariantList easySwitchSlotPositions() const;
    bool smoothScrollSupported() const;
    QString deviceSerial() const;
    QString firmwareVersion() const;
    int activeSlot() const;
    Q_INVOKABLE bool isSlotPaired(int slot) const;  // 1-based

    Q_INVOKABLE void setDPI(int value);
    Q_INVOKABLE void setSmartShift(bool enabled, int threshold);
    Q_INVOKABLE void setScrollConfig(bool hiRes, bool invert);
    Q_INVOKABLE void setThumbWheelMode(const QString &mode);
    Q_INVOKABLE void resetAllProfiles();
    Q_INVOKABLE void setGestureAction(const QString &direction, const QString &actionName, const QString &keystroke);
    Q_INVOKABLE QString gestureActionName(const QString &direction) const;
    Q_INVOKABLE QString gestureKeystroke(const QString &direction) const;
    Q_PROPERTY(QString thumbWheelMode READ thumbWheelMode NOTIFY settingsReloaded)
    Q_PROPERTY(bool thumbWheelInvert READ thumbWheelInvert NOTIFY settingsReloaded)
    bool scrollHiRes() const;
    bool scrollInvert() const;
    QString thumbWheelMode() const;
    bool thumbWheelInvert() const;
    Q_INVOKABLE void setThumbWheelInvert(bool invert);
    Q_INVOKABLE QString gnomeTrayStatus() const;

    void loadGesturesFromProfile(const QMap<QString, QPair<QString, QString>> &gestures);

    // Called from AppController to sync displayed profile state into the model
    void setActiveProfileName(const QString &name);
    void setActiveWmClass(const QString &wmClass);
    void setDisplayValues(int dpi, bool smartShiftEnabled, int smartShiftThreshold,
                          bool scrollHiRes, bool scrollInvert, const QString &thumbWheelMode,
                          bool thumbWheelInvert = false);

signals:
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
    void settingsReloaded();  // batch notification for all settings properties
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
    DeviceManager *m_dm = nullptr;
    IDesktopIntegration *m_desktop = nullptr;
    QMap<QString, QPair<QString, QString>> m_gestures; // direction -> (actionName, keystroke)
    QString m_activeProfileName;
    QString m_activeWmClass;

    // Display values — what the UI shows (may differ from hardware when viewing non-active profile)
    int m_displayDpi = -1;              // -1 = use DeviceManager value
    bool m_displaySmartShiftEnabled = false;
    int m_displaySmartShiftThreshold = 0;
    bool m_displayScrollHiRes = false;
    bool m_displayScrollInvert = false;
    QString m_displayThumbWheelMode;
    bool m_displayThumbWheelInvert = false;
    bool m_hasDisplayValues = false;     // false = read from DeviceManager
};

} // namespace logitune
