#include "ProfileEngine.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QFileInfo>

namespace logitune {

// ---------------------------------------------------------------------------
// ButtonAction
// ---------------------------------------------------------------------------

ButtonAction ButtonAction::parse(const QString &str)
{
    if (str.isEmpty() || str == "default")
        return {Default, {}};

    if (str == "gesture-trigger")
        return {GestureTrigger, {}};

    if (str == "smartshift-toggle")
        return {SmartShiftToggle, {}};

    if (str == "dpi-cycle")
        return {DpiCycle, {}};

    // Prefixed forms: "type:payload"
    const int colon = str.indexOf(':');
    if (colon == -1)
        return {Default, {}};

    const QString prefix  = str.left(colon);
    const QString payload = str.mid(colon + 1);

    if (prefix == "keystroke") {
        if (payload == "smartshift-toggle") return {SmartShiftToggle, {}};
        return {Keystroke, payload};
    }
    if (prefix == "media")      return {Media,       payload};
    if (prefix == "dbus")       return {DBus,        payload};
    if (prefix == "app-launch") return {AppLaunch,   payload};

    // Unknown prefix — treat as default
    return {Default, {}};
}

QString ButtonAction::serialize() const
{
    switch (type) {
    case Default:       return "default";
    case GestureTrigger:  return "gesture-trigger";
    case SmartShiftToggle: return "smartshift-toggle";
    case DpiCycle:      return "dpi-cycle";
    case Keystroke:     return "keystroke:" + payload;
    case Media:         return "media:" + payload;
    case DBus:          return "dbus:" + payload;
    case AppLaunch:     return "app-launch:" + payload;
    }
    return "default";
}

// ---------------------------------------------------------------------------
// ProfileEngine — static helpers
// ---------------------------------------------------------------------------

Profile ProfileEngine::loadProfile(const QString &path)
{
    QSettings s(path, QSettings::IniFormat);
    Profile p;

    p.version             = s.value("General/version", 1).toInt();
    p.name                = s.value("General/name").toString();
    p.icon                = s.value("General/icon").toString();

    p.dpi                 = s.value("DPI/value", 1000).toInt();

    p.smartShiftEnabled   = s.value("SmartShift/enabled", true).toBool();
    p.smartShiftThreshold = s.value("SmartShift/threshold", 128).toInt();

    const QString scrollMode = s.value("Scroll/mode", "ratchet").toString();
    p.smoothScrolling     = (scrollMode == "smooth");
    p.scrollDirection     = s.value("Scroll/direction", "standard").toString();
    p.hiResScroll         = s.value("Scroll/hires", true).toBool();

    p.thumbWheelMode      = s.value("ThumbWheel/mode", "scroll").toString();
    p.thumbWheelInvert    = s.value("ThumbWheel/invert", false).toBool();

    s.beginGroup("Buttons");
    const QStringList buttonKeys = s.childKeys();
    for (const QString &key : buttonKeys) {
        bool ok = false;
        int idx = key.toInt(&ok);
        if (ok && idx >= 0 && idx < static_cast<int>(p.buttons.size()))
            p.buttons[static_cast<std::size_t>(idx)] = ButtonAction::parse(s.value(key).toString());
    }
    s.endGroup();

    s.beginGroup("Gestures");
    const QStringList gestureKeys = s.childKeys();
    for (const QString &key : gestureKeys)
        p.gestures[key] = ButtonAction::parse(s.value(key).toString());
    s.endGroup();

    return p;
}

void ProfileEngine::saveProfile(const QString &path, const Profile &profile)
{
    QSettings s(path, QSettings::IniFormat);
    s.clear();  // wipe all keys before writing — prevents duplicate sections

    s.setValue("General/version", profile.version);
    s.setValue("General/name",    profile.name);
    s.setValue("General/icon",    profile.icon);

    s.setValue("DPI/value", profile.dpi);

    s.setValue("SmartShift/enabled",   profile.smartShiftEnabled);
    s.setValue("SmartShift/threshold", profile.smartShiftThreshold);

    s.setValue("Scroll/mode",      profile.smoothScrolling ? "smooth" : "ratchet");
    s.setValue("Scroll/direction", profile.scrollDirection);
    s.setValue("Scroll/hires",     profile.hiResScroll);

    s.setValue("ThumbWheel/mode",  profile.thumbWheelMode);
    s.setValue("ThumbWheel/invert", profile.thumbWheelInvert);

    s.beginGroup("Buttons");
    for (std::size_t i = 0; i < profile.buttons.size(); ++i)
        s.setValue(QString::number(static_cast<int>(i)), profile.buttons[i].serialize());
    s.endGroup();

    s.beginGroup("Gestures");
    for (const auto &[key, action] : profile.gestures)
        s.setValue(key, action.serialize());
    s.endGroup();

    s.sync();
}

QMap<QString, QString> ProfileEngine::loadAppBindings(const QString &path)
{
    QSettings s(path, QSettings::IniFormat);
    QMap<QString, QString> bindings;

    s.beginGroup("Bindings");
    const QStringList keys = s.childKeys();
    for (const QString &key : keys)
        bindings[key] = s.value(key).toString();
    s.endGroup();

    return bindings;
}

void ProfileEngine::saveAppBindings(const QString &path, const QMap<QString, QString> &bindings)
{
    QSettings s(path, QSettings::IniFormat);

    s.remove("Bindings");
    s.beginGroup("Bindings");
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it)
        s.setValue(it.key(), it.value());
    s.endGroup();

    s.sync();
}

ProfileDelta ProfileEngine::diff(const Profile &a, const Profile &b)
{
    ProfileDelta delta;

    delta.dpiChanged = (a.dpi != b.dpi);

    delta.smartShiftChanged = (a.smartShiftEnabled   != b.smartShiftEnabled ||
                               a.smartShiftThreshold != b.smartShiftThreshold);

    delta.scrollChanged = (a.smoothScrolling  != b.smoothScrolling  ||
                           a.scrollDirection  != b.scrollDirection  ||
                           a.hiResScroll      != b.hiResScroll      ||
                           a.thumbWheelMode   != b.thumbWheelMode ||
                           a.thumbWheelInvert != b.thumbWheelInvert);

    delta.buttonsChanged  = (a.buttons  != b.buttons);
    delta.gesturesChanged = (a.gestures != b.gestures);

    return delta;
}

// ---------------------------------------------------------------------------
// ProfileEngine — instance methods
// ---------------------------------------------------------------------------

ProfileEngine::ProfileEngine(QObject *parent)
    : QObject(parent)
{
}

void ProfileEngine::setDeviceConfigDir(const QString &dir)
{
    m_configDir = dir;
    m_cache.clear();

    QDir d(dir);
    if (d.exists()) {
        for (const auto &f : d.entryList({"*.conf"}, QDir::Files)) {
            if (f == "app-bindings.conf") continue;
            QString name = QFileInfo(f).baseName();
            m_cache[name] = loadProfile(d.filePath(f));
        }
    }

    // Load app bindings if the file exists
    const QString bindingsFile = appBindingsPath();
    if (QFileInfo::exists(bindingsFile))
        m_appBindings = loadAppBindings(bindingsFile);
}

QStringList ProfileEngine::profileNames() const
{
    if (m_configDir.isEmpty())
        return {};

    QDir dir(m_configDir);
    const QStringList files = dir.entryList({"*.conf"}, QDir::Files);

    QStringList names;
    names.reserve(files.size());
    for (const QString &f : files)
        names << QFileInfo(f).baseName();
    return names;
}

void ProfileEngine::createProfileForApp(const QString &wmClass, const QString &profileName)
{
    if (m_configDir.isEmpty() || wmClass.isEmpty() || profileName.isEmpty())
        return;

    // Only create if the profile doesn't already exist (loaded from disk at startup).
    // Without this guard, every app restart would overwrite saved customizations.
    if (!m_cache.contains(profileName)) {
        m_cache[profileName] = m_cache.value(QStringLiteral("default"));
        m_cache[profileName].name = profileName;
        saveProfile(profilePath(profileName), m_cache[profileName]);
    }

    m_appBindings[wmClass] = profileName;
    saveAppBindings(appBindingsPath(), m_appBindings);
}

void ProfileEngine::removeAppProfile(const QString &wmClass)
{
    if (m_configDir.isEmpty() || wmClass.isEmpty())
        return;

    const QString profileName = m_appBindings.value(wmClass);
    if (profileName.isEmpty())
        return;

    // Remove the profile file
    QFile::remove(profilePath(profileName));

    // Remove from cache
    m_cache.remove(profileName);

    // Remove from app bindings and save
    m_appBindings.remove(wmClass);
    saveAppBindings(appBindingsPath(), m_appBindings);

    // If the removed profile was displayed or on hardware, switch back to default
    if (m_displayProfile == profileName)
        setDisplayProfile(QStringLiteral("default"));
    if (m_hardwareProfile == profileName)
        setHardwareProfile(QStringLiteral("default"));

    qCDebug(lcProfile) << "removed profile" << profileName << "for app" << wmClass;
}

// ---------------------------------------------------------------------------
// Profile cache (Task 1)
// ---------------------------------------------------------------------------

Profile& ProfileEngine::cachedProfile(const QString &name)
{
    if (!m_cache.contains(name)) {
        m_cache[name] = Profile{};
        m_cache[name].name = name;
    }
    return m_cache[name];
}

QString ProfileEngine::displayProfile() const
{
    return m_displayProfile;
}

QString ProfileEngine::hardwareProfile() const
{
    return m_hardwareProfile;
}

void ProfileEngine::setDisplayProfile(const QString &name)
{
    if (m_displayProfile == name) return;
    m_displayProfile = name;
    emit displayProfileChanged(cachedProfile(name));
}

void ProfileEngine::setHardwareProfile(const QString &name)
{
    if (m_hardwareProfile == name) return;
    m_hardwareProfile = name;
    emit hardwareProfileChanged(cachedProfile(name));
}

void ProfileEngine::saveProfileToDisk(const QString &name)
{
    if (!m_cache.contains(name) || m_configDir.isEmpty()) return;
    saveProfile(profilePath(name), m_cache[name]);
}

QString ProfileEngine::profileForApp(const QString &appId) const
{
    // Simple case-insensitive lookup. The caller (KDeDesktop) is responsible
    // for resolving window identity to a canonical app ID that matches
    // .desktop file names used as binding keys.
    for (auto it = m_appBindings.cbegin(); it != m_appBindings.cend(); ++it) {
        if (it.key().compare(appId, Qt::CaseInsensitive) == 0)
            return it.value();
    }

    return QStringLiteral("default");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QString ProfileEngine::profilePath(const QString &name) const
{
    return m_configDir + "/" + name + ".conf";
}

QString ProfileEngine::appBindingsPath() const
{
    return m_configDir + "/app-bindings.conf";
}

} // namespace logitune
