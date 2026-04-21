#pragma once
#include "ButtonAction.h"
#include <QObject>
#include <QString>
#include <QMap>
#include <QHash>
#include <array>
#include <map>

namespace logitune {

struct Profile {
    int version = 1;
    QString name;
    QString icon;
    int dpi = 1000;
    bool smartShiftEnabled = true;
    int smartShiftThreshold = 128;
    bool smoothScrolling = false;
    QString scrollDirection = "standard";  // "standard" or "natural"
    bool hiResScroll = true;
    std::array<ButtonAction, 16> buttons;  // indexed by ControlDescriptor::buttonIndex
    std::map<QString, ButtonAction> gestures;  // "up","down","left","right","click"
    QString thumbWheelMode = "scroll";  // "scroll", "zoom", "volume", "none"
    bool thumbWheelInvert = false;
};

struct ProfileDelta {
    bool dpiChanged = false;
    bool smartShiftChanged = false;
    bool scrollChanged = false;
    bool buttonsChanged = false;
    bool gesturesChanged = false;
};

struct DeviceProfileContext {
    QString configDir;
    QMap<QString, Profile> cache;
    QMap<QString, QString> appBindings;
    QString displayProfile;
    QString hardwareProfile;
};

class ProfileEngine : public QObject {
    Q_OBJECT
public:
    explicit ProfileEngine(QObject *parent = nullptr);

    // Static helpers (testable)
    static Profile loadProfile(const QString &path);
    static void saveProfile(const QString &path, const Profile &profile);
    static QMap<QString, QString> loadAppBindings(const QString &path);
    static void saveAppBindings(const QString &path, const QMap<QString, QString> &bindings);
    static ProfileDelta diff(const Profile &a, const Profile &b);

    // Instance methods
    void registerDevice(const QString &serial, const QString &configDir);
    bool hasDevice(const QString &serial) const;

    Profile& cachedProfile(const QString &serial, const QString &name);
    QStringList profileNames(const QString &serial) const;
    QString displayProfile(const QString &serial) const;
    QString hardwareProfile(const QString &serial) const;
    QString profileForApp(const QString &serial, const QString &wmClass) const;

    void setDisplayProfile(const QString &serial, const QString &name);
    void setHardwareProfile(const QString &serial, const QString &name);
    void saveProfileToDisk(const QString &serial, const QString &name);
    void createProfileForApp(const QString &serial,
                             const QString &wmClass,
                             const QString &profileName);
    void removeAppProfile(const QString &serial, const QString &wmClass);

signals:
    void deviceDisplayProfileChanged(const QString &serial, const Profile &profile);
    void deviceHardwareProfileChanged(const QString &serial, const Profile &profile);

private:
    // Per-device contexts. Key is PhysicalDevice::deviceSerial(). Lazy-
    // registered on first touch; persists for the life of the process.
    QHash<QString, DeviceProfileContext> m_byDevice;

    DeviceProfileContext& ctx(const QString &serial);
    const DeviceProfileContext& ctx(const QString &serial) const;
};

} // namespace logitune
