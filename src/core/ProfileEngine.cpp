#include "ProfileEngine.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

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

// ---------------------------------------------------------------------------
// Per-device contexts
// ---------------------------------------------------------------------------

DeviceProfileContext& ProfileEngine::ctx(const QString &serial)
{
    if (!m_byDevice.contains(serial)) {
        qCWarning(lcProfile)
            << "ProfileEngine: lazy-registering unknown device" << serial;
        const QString defaultDir = QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation)
            + "/devices/" + serial + "/profiles";
        registerDevice(serial, defaultDir);
    }
    return m_byDevice[serial];
}

const DeviceProfileContext& ProfileEngine::ctx(const QString &serial) const
{
    static const DeviceProfileContext empty{};
    auto it = m_byDevice.constFind(serial);
    if (it == m_byDevice.constEnd()) {
        qCWarning(lcProfile)
            << "ProfileEngine: const lookup for unknown device" << serial
            << "- returning empty context";
        return empty;
    }
    return *it;
}

void ProfileEngine::registerDevice(const QString &serial, const QString &configDir)
{
    DeviceProfileContext &c = m_byDevice[serial];
    c.configDir = configDir;
    c.cache.clear();
    c.appBindings.clear();

    QDir d(configDir);
    if (!d.exists())
        QDir().mkpath(configDir);

    if (d.exists()) {
        for (const auto &f : d.entryList({"*.conf"}, QDir::Files)) {
            if (f == "app-bindings.conf") continue;
            QString name = QFileInfo(f).baseName();
            c.cache[name] = loadProfile(d.filePath(f));
        }
    }

    const QString bindingsFile = configDir + "/app-bindings.conf";
    if (QFileInfo::exists(bindingsFile))
        c.appBindings = loadAppBindings(bindingsFile);
}

bool ProfileEngine::hasDevice(const QString &serial) const
{
    return m_byDevice.contains(serial);
}

Profile& ProfileEngine::cachedProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (!c.cache.contains(name)) {
        c.cache[name] = Profile{};
        c.cache[name].name = name;
    }
    return c.cache[name];
}

QStringList ProfileEngine::profileNames(const QString &serial) const
{
    const DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty())
        return {};
    QDir dir(c.configDir);
    const QStringList files = dir.entryList({"*.conf"}, QDir::Files);
    QStringList names;
    names.reserve(files.size());
    for (const QString &f : files) {
        const QString base = QFileInfo(f).baseName();
        if (base != QLatin1String("app-bindings"))
            names << base;
    }
    return names;
}

QString ProfileEngine::displayProfile(const QString &serial) const
{
    return ctx(serial).displayProfile;
}

QString ProfileEngine::hardwareProfile(const QString &serial) const
{
    return ctx(serial).hardwareProfile;
}

QString ProfileEngine::profileForApp(const QString &serial, const QString &wmClass) const
{
    const DeviceProfileContext &c = ctx(serial);
    for (auto it = c.appBindings.cbegin(); it != c.appBindings.cend(); ++it) {
        if (it.key().compare(wmClass, Qt::CaseInsensitive) == 0)
            return it.value();
    }
    return QStringLiteral("default");
}

void ProfileEngine::setDisplayProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.displayProfile == name) return;
    c.displayProfile = name;
    emit deviceDisplayProfileChanged(serial, cachedProfile(serial, name));
}

void ProfileEngine::setHardwareProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.hardwareProfile == name) return;
    c.hardwareProfile = name;
    emit deviceHardwareProfileChanged(serial, cachedProfile(serial, name));
}

void ProfileEngine::saveProfileToDisk(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (!c.cache.contains(name) || c.configDir.isEmpty()) return;
    saveProfile(c.configDir + "/" + name + ".conf", c.cache[name]);
}

void ProfileEngine::createProfileForApp(const QString &serial,
                                        const QString &wmClass,
                                        const QString &profileName)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty() || wmClass.isEmpty() || profileName.isEmpty())
        return;
    if (!c.cache.contains(profileName)) {
        c.cache[profileName] = c.cache.value(QStringLiteral("default"));
        c.cache[profileName].name = profileName;
        saveProfile(c.configDir + "/" + profileName + ".conf",
                    c.cache[profileName]);
    }
    c.appBindings[wmClass] = profileName;
    saveAppBindings(c.configDir + "/app-bindings.conf", c.appBindings);
}

void ProfileEngine::removeAppProfile(const QString &serial, const QString &wmClass)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty() || wmClass.isEmpty()) return;

    const QString profileName = c.appBindings.value(wmClass);
    if (profileName.isEmpty()) return;

    QFile::remove(c.configDir + "/" + profileName + ".conf");
    c.cache.remove(profileName);
    c.appBindings.remove(wmClass);
    saveAppBindings(c.configDir + "/app-bindings.conf", c.appBindings);

    if (c.displayProfile == profileName)
        setDisplayProfile(serial, QStringLiteral("default"));
    if (c.hardwareProfile == profileName)
        setHardwareProfile(serial, QStringLiteral("default"));

    qCDebug(lcProfile) << "removed profile" << profileName
                       << "for app" << wmClass << "on device" << serial;
}

} // namespace logitune
