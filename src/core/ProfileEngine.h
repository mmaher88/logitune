#pragma once
#include "ButtonAction.h"
#include <QObject>
#include <QString>
#include <QMap>
#include <QFileSystemWatcher>
#include <QSettings>
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
    void setDeviceConfigDir(const QString &dir);
    QStringList profileNames() const;
    void createProfileForApp(const QString &wmClass, const QString &profileName);
    void removeAppProfile(const QString &wmClass);

    // --- Profile cache (Task 1) ---
    Profile& cachedProfile(const QString &name);
    QString displayProfile() const;
    QString hardwareProfile() const;
    void setDisplayProfile(const QString &name);
    void setHardwareProfile(const QString &name);
    void saveProfileToDisk(const QString &name);
    QString profileForApp(const QString &wmClass) const;

signals:
    void displayProfileChanged(const Profile &profile);
    void hardwareProfileChanged(const Profile &profile);

private:
    QString m_configDir;
    QMap<QString, QString> m_appBindings;
    QMap<QString, Profile> m_cache;
    QString m_displayProfile;
    QString m_hardwareProfile;
    QFileSystemWatcher m_fileWatcher;
    bool m_selfWrite = false;  // suppress reload during our own save

    QString profilePath(const QString &name) const;
    QString appBindingsPath() const;
};

} // namespace logitune
