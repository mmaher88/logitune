#include "PanelSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace logitune {

QString panelSettingsPath(const QString &serial)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/devices/" + serial + "/panel.conf";
}

StoredPanelSettings loadPanelSettings(const QString &file)
{
    StoredPanelSettings stored;
    if (!QFileInfo::exists(file))
        return stored;

    QSettings settings(file, QSettings::IniFormat);
    if (settings.contains("Haptics/enabled"))
        stored.hapticsEnabled = settings.value("Haptics/enabled").toBool();
    if (settings.contains("Haptics/level"))
        stored.hapticsLevel = settings.value("Haptics/level").toInt();
    if (settings.contains("Panel/force"))
        stored.panelForce = settings.value("Panel/force").toInt();
    return stored;
}

// QSettings won't create missing parent folders, and a device seen for the first time has none.
static void ensureFolder(const QString &file)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
}

void saveHaptics(const QString &file, bool enabled, int level)
{
    ensureFolder(file);
    QSettings settings(file, QSettings::IniFormat);
    settings.setValue("Haptics/enabled", enabled);
    settings.setValue("Haptics/level", level);
}

void savePanelForce(const QString &file, int force)
{
    ensureFolder(file);
    QSettings settings(file, QSettings::IniFormat);
    settings.setValue("Panel/force", force);
}

} // namespace logitune
