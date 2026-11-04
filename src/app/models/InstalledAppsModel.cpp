#include "InstalledAppsModel.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

namespace logitune {

InstalledAppsModel::InstalledAppsModel(QObject *parent) : QAbstractListModel(parent)
{
    QStringList dirs;
    for (const auto &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << dir + "/applications";
    dirs << QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
         << "/var/lib/flatpak/exports/share/applications"
         << "/var/lib/snapd/desktop/applications" << "/run/host/usr/share/applications";
    reload(dirs);
}

int InstalledAppsModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_apps.size(); }
QHash<int, QByteArray> InstalledAppsModel::roleNames() const
{
    return {{Qt::UserRole + 1, "name"}, {Qt::UserRole + 2, "icon"},
            {Qt::UserRole + 3, "command"}, {Qt::UserRole + 4, "desktopId"}};
}
QVariant InstalledAppsModel::data(const QModelIndex &index, int role) const
{
    return get(index.row()).value(QString::fromUtf8(roleNames().value(role)));
}
QVariantMap InstalledAppsModel::get(int row) const
{
    return row >= 0 && row < m_apps.size() ? m_apps[row] : QVariantMap{};
}

void InstalledAppsModel::reload(const QStringList &directories, const QString &locale)
{
    QList<QVariantMap> apps;
    QSet<QString> seen;
    const QString language = locale.isEmpty() ? QLocale().name() : locale;
    for (const auto &dir : directories) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString id = QDir(dir).relativeFilePath(path).replace('/', '-');
            if (seen.contains(id)) continue;
            seen.insert(id);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) continue;
            QHash<QString, QString> fields;
            bool entry = false;
            while (!file.atEnd()) {
                QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith('[')) { entry = line == "[Desktop Entry]"; continue; }
                if (!entry || line.startsWith('#')) continue;
                int equals = line.indexOf('=');
                if (equals > 0) fields.insert(line.left(equals), line.mid(equals + 1));
            }
            if (fields.value("Hidden") == "true" || fields.value("NoDisplay") == "true"
                || fields.value("Type") != "Application" || fields.value("Exec").isEmpty()) continue;
            QString tryExec = fields.value("TryExec");
            if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty()) continue;
            QString name = fields.value("Name[" + language + "]",
                fields.value("Name[" + language.section('_', 0, 0) + "]", fields.value("Name")));
            name.replace("\\s", " ").replace("\\n", " ").replace("\\\\", "\\");
            if (name.isEmpty()) continue;
            // Delegate desktop field codes, quoting, Terminal and DBus activation to GIO.
            QString quoted = path;
            quoted.replace("\"", "\"\"\"");
            apps.append({{"name", name}, {"icon", fields.value("Icon")}, {"desktopId", id},
                         {"command", "gio launch \"" + quoted + "\""}});
        }
    }
    std::sort(apps.begin(), apps.end(), [](const auto &a, const auto &b) {
        return QString::localeAwareCompare(a.value("name").toString(), b.value("name").toString()) < 0;
    });
    beginResetModel(); m_apps = apps; endResetModel();
}

} // namespace logitune
