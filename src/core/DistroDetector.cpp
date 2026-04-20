#include "DistroDetector.h"

#include <QFile>
#include <QTextStream>

namespace logitune::util {

namespace {

// Parses a single line of /etc/os-release into (key, value) where the
// value has surrounding double quotes stripped.
bool parseLine(const QString &line, QString &key, QString &value) {
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) return false;
    key = line.left(eq).trimmed();
    QString raw = line.mid(eq + 1).trimmed();
    // /etc/os-release per spec allows both double-quoted and single-quoted
    // values. Historical Alpine and a few embedded distros use single quotes.
    if (raw.size() >= 2
        && ((raw.startsWith(QLatin1Char('"'))  && raw.endsWith(QLatin1Char('"')))
         || (raw.startsWith(QLatin1Char('\'')) && raw.endsWith(QLatin1Char('\'')))))
        raw = raw.mid(1, raw.size() - 2);
    value = raw;
    return true;
}

DistroFamily classify(const QString &id, const QStringList &idLike) {
    // Exact-ID matches first.
    if (id == QLatin1String("arch"))
        return DistroFamily::Arch;
    if (id == QLatin1String("debian") || id == QLatin1String("ubuntu"))
        return DistroFamily::Debian;
    if (id == QLatin1String("fedora") || id == QLatin1String("rhel")
        || id == QLatin1String("rocky") || id == QLatin1String("almalinux")
        || id == QLatin1String("centos"))
        return DistroFamily::Fedora;

    // Fall back to ID_LIKE (space-separated tokens).
    for (const QString &like : idLike) {
        if (like == QLatin1String("arch"))
            return DistroFamily::Arch;
        if (like == QLatin1String("debian") || like == QLatin1String("ubuntu"))
            return DistroFamily::Debian;
        if (like == QLatin1String("fedora") || like == QLatin1String("rhel"))
            return DistroFamily::Fedora;
    }
    return DistroFamily::Unknown;
}

DistroFamily parseOsRelease(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return DistroFamily::Unknown;

    QString id;
    QStringList idLike;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString key, value;
        if (!parseLine(ts.readLine(), key, value)) continue;
        if (key == QLatin1String("ID")) {
            id = value;
        } else if (key == QLatin1String("ID_LIKE")) {
            idLike = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
    }
    return classify(id, idLike);
}

} // namespace

DistroFamily detectDistroFamily() {
    static const DistroFamily cached = parseOsRelease(QStringLiteral("/etc/os-release"));
    return cached;
}

DistroFamily detectDistroFamilyFromFile(const QString &path) {
    return parseOsRelease(path);
}

} // namespace logitune::util
