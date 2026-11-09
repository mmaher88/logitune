#include "desktop/LinuxDesktopBase.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSet>
#include <QVariantMap>
#include <algorithm>

namespace logitune {

QStringList LinuxDesktopBase::desktopDirs() const
{
    if (!m_desktopDirsOverride.isEmpty())
        return m_desktopDirsOverride;

    static const QStringList dirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/run/host/usr/share/applications"),  // host apps inside Flatpak
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications"),
        QStringLiteral("/var/lib/snapd/desktop/applications")
    };
    return dirs;
}

QString LinuxDesktopBase::findDesktopFile(const QString &key) const
{
    if (key.isEmpty())
        return {};

    // Cache results — scanning .desktop dirs is slow on first call
    auto cached = m_resolveCache.constFind(key);
    if (cached != m_resolveCache.constEnd())
        return cached.value();

    const QStringList dirs = desktopDirs();

    // Match: key already is a .desktop file ID. Checked across every directory
    // before any fuzzy match so that, e.g., native Chrome's google-chrome.desktop
    // wins over flatpak Chrome's com.google.Chrome.desktop (whose StartupWMClass
    // is also "google-chrome").
    if (!key.contains(QLatin1Char('/'))) {
        for (const QString &dir : dirs) {
            if (QFileInfo::exists(dir + QLatin1Char('/') + key + QStringLiteral(".desktop"))) {
                m_resolveCache.insert(key, key);
                return key;
            }
        }
    }

    // Search .desktop files for one whose StartupWMClass or file name component
    // matches the key. Returns the .desktop completeBaseName (canonical ID).
    // This handles cases like resourceClass="zoom" -> "us.zoom.Zoom.desktop"
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;

        const QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            QString baseName = QFileInfo(file).completeBaseName();

            // Match: last component of filename (e.g. "Zoom" from "us.zoom.Zoom")
            QString shortName = baseName.contains('.') ? baseName.section('.', -1) : baseName;
            if (shortName.compare(key, Qt::CaseInsensitive) == 0) {
                m_resolveCache.insert(key, baseName);
                return baseName;
            }

            // Match: StartupWMClass field
            QSettings desktop(d.filePath(file), QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));
            QString wmClass = desktop.value(QStringLiteral("StartupWMClass")).toString();
            if (!wmClass.isEmpty() && wmClass.compare(key, Qt::CaseInsensitive) == 0) {
                m_resolveCache.insert(key, baseName);
                return baseName;
            }
        }
    }

    m_resolveCache.insert(key, QString());
    return {};
}

QString LinuxDesktopBase::resolveDesktopFile(const QString &resourceClass) const
{
    const QString id = findDesktopFile(resourceClass);
    // No .desktop match — use resourceClass as-is
    return id.isEmpty() ? resourceClass : id;
}

QVariantList LinuxDesktopBase::runningApplications() const
{
    // Scan installed .desktop files for GUI applications
    QVariantList result;
    QSet<QString> seen;

    for (const QString &dir : desktopDirs()) {
        QDir d(dir);
        if (!d.exists()) continue;

        const QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            QSettings desktop(d.filePath(file), QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));

            QString type = desktop.value(QStringLiteral("Type")).toString();
            if (type != QStringLiteral("Application")) continue;
            if (desktop.value(QStringLiteral("NoDisplay")).toBool()) continue;

            QString name = desktop.value(QStringLiteral("Name")).toString();
            QString icon = desktop.value(QStringLiteral("Icon")).toString();

            // On KDE 6 / Wayland, KWin's resourceClass matches the .desktop file
            // completeBaseName (e.g. "org.kde.dolphin"), NOT StartupWMClass (e.g. "dolphin").
            // Use completeBaseName so profile bindings match exactly.
            QString wmClass = QFileInfo(file).completeBaseName();

            if (name.isEmpty() || seen.contains(wmClass.toLower()))
                continue;

            seen.insert(wmClass.toLower());
            QVariantMap entry;
            entry[QStringLiteral("wmClass")] = wmClass;
            entry[QStringLiteral("title")] = name;
            entry[QStringLiteral("icon")] = icon;
            result.append(entry);
        }
    }

    // Sort by name
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("title")].toString().toLower()
             < b.toMap()[QStringLiteral("title")].toString().toLower();
    });

    return result;
}

} // namespace logitune
