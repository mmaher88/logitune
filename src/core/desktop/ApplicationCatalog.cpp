#include "ApplicationCatalog.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

// Visibility and precedence: https://specifications.freedesktop.org/desktop-entry/latest-single/
namespace logitune {
namespace {
QString unescape(const QString &text)
{
    QString result;
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const auto next = text[++i];
            if (next == 's') result += ' ';
            else if (next == 'n') result += '\n';
            else if (next == 't') result += '\t';
            else if (next == 'r') result += '\r';
            else if (next == '\\') result += '\\';
            else { result += '\\'; result += next; }
        } else result += text[i];
    }
    return result;
}
QMap<QString, QString> desktopEntry(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return {};
    QMap<QString, QString> values;
    bool inEntry = false;
    while (!file.atEnd()) {
        const auto line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith('[')) { inEntry = line == "[Desktop Entry]"; continue; }
        if (!inEntry || line.startsWith('#')) continue;
        const auto equals = line.indexOf('=');
        if (equals > 0) values.insert(line.left(equals).trimmed(), unescape(line.mid(equals + 1)));
    }
    return values;
}
QString localized(const QMap<QString, QString> &entry, const QString &key, QString locale)
{
    locale.replace('-', '_');
    locale.remove(QRegularExpression(QStringLiteral("\\.[^@]+")));
    QStringList candidates{locale};
    const auto modifier = locale.indexOf('@');
    const auto country = locale.indexOf('_');
    if (modifier >= 0) candidates << locale.left(modifier);
    if (country >= 0) {
        if (modifier >= 0) candidates << locale.left(country) + locale.mid(modifier);
        candidates << locale.left(country);
    }
    for (const auto &candidate : candidates) {
        const auto value = entry.value(key + '[' + candidate + ']');
        if (!value.isEmpty()) return value;
    }
    return entry.value(key);
}
bool matches(const QString &list, const QStringList &desktops)
{
    const auto names = list.split(';', Qt::SkipEmptyParts);
    return std::any_of(desktops.begin(), desktops.end(), [&](const QString &name) { return names.contains(name); });
}
// QProcess::splitCommand uses triple quotes for a literal quote. No shell involved.
QString quoteArgument(QString argument)
{
    argument.replace('"', QStringLiteral("\"\"\""));
    return '"' + argument + '"';
}
}
QStringList ApplicationCatalog::directories()
{
    QStringList dirs;
    for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << QDir(path).filePath("applications");
    dirs << QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
         << "/var/lib/flatpak/exports/share/applications"
         << "/var/lib/snapd/desktop/applications" << "/run/host/usr/share/applications";
    dirs.removeDuplicates();
    return dirs;
}
QString ApplicationCatalog::launchCommand(const QString &desktopFile)
{
    if (!QFileInfo(desktopFile).isAbsolute() || !desktopFile.endsWith(".desktop")) return {};
    return QStringLiteral("gio launch ") + quoteArgument(desktopFile);
}
QVariantList ApplicationCatalog::installedApplications()
{
    return applications(directories(), QLocale().name(),
                        qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts));
}
QVariantList ApplicationCatalog::applications(const QStringList &directories, const QString &locale,
                                             const QStringList &desktops)
{
    QVariantList result;
    QSet<QString> seen;
    for (const auto &path : directories) {
        const QDir dir(path);
        QDirIterator files(path, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto file = files.next();
            QString id = dir.relativeFilePath(file);
            id.replace('/', '-');
            // A user override (including Hidden=true) masks the system entry.
            if (seen.contains(id)) continue;
            seen.insert(id);
            const auto entry = desktopEntry(file);
            if (entry.value("Type") != "Application" || entry.value("Hidden") == "true"
                || entry.value("NoDisplay") == "true") continue;
            if (!entry.value("OnlyShowIn").isEmpty() && !matches(entry.value("OnlyShowIn"), desktops)) continue;
            if (matches(entry.value("NotShowIn"), desktops)) continue;
            const auto tryExec = entry.value("TryExec");
            if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty()) continue;
            if (entry.value("Exec").isEmpty() && entry.value("DBusActivatable") != "true") continue;
            const auto name = localized(entry, "Name", locale);
            if (name.isEmpty()) continue;
            const auto absolute = QFileInfo(file).absoluteFilePath();
            result.append(QVariantMap{{"title", name}, {"icon", entry.value("Icon")},
                {"desktopFile", absolute}, {"desktopId", id}, {"command", launchCommand(absolute)}});
        }
    }
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return QString::localeAwareCompare(a.toMap().value("title").toString(), b.toMap().value("title").toString()) < 0;
    });
    return result;
}
}
