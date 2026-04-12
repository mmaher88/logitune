#include "DeviceFetcher.h"
#include "logging/LogManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace logitune {

const QString DeviceFetcher::kManifestUrl =
    QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune-devices/main/manifest.json");
const QString DeviceFetcher::kRawBaseUrl =
    QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune-devices/main/");

DeviceFetcher::DeviceFetcher(QObject *parent)
    : QObject(parent)
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                 + QStringLiteral("/logitune"))
{
}

// ---- Cache utilities --------------------------------------------------------

void DeviceFetcher::setCacheDir(const QString &dir)
{
    m_cacheDir = dir;
}

bool DeviceFetcher::isCacheFresh() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest-timestamp"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const auto stamp = QDateTime::fromString(QString::fromUtf8(f.readAll()).trimmed(), Qt::ISODate);
    if (!stamp.isValid())
        return false;

    return stamp.secsTo(QDateTime::currentDateTimeUtc()) < kCacheTtlSeconds;
}

void DeviceFetcher::saveTimestamp()
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest-timestamp"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
}

void DeviceFetcher::saveEtag(const QString &etag)
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest-etag"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(etag.toUtf8());
}

QString DeviceFetcher::loadEtag() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest-etag"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QString::fromUtf8(f.readAll()).trimmed();
}

void DeviceFetcher::saveManifest(const QJsonObject &manifest)
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}

QJsonObject DeviceFetcher::loadManifest() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QPair<QString, QJsonObject> DeviceFetcher::findDeviceForPid(const QJsonObject &manifest,
                                                             uint16_t pid) const
{
    const QString pidHex = QStringLiteral("0x%1").arg(pid, 4, 16, QLatin1Char('0'));
    const auto devices = manifest[QStringLiteral("devices")].toObject();

    for (auto it = devices.begin(); it != devices.end(); ++it) {
        const auto info = it.value().toObject();
        const auto pids = info[QStringLiteral("pids")].toArray();
        for (const auto &p : pids) {
            if (p.toString().compare(pidHex, Qt::CaseInsensitive) == 0)
                return {it.key(), info};
        }
    }

    return {QString(), QJsonObject()};
}

bool DeviceFetcher::deviceNeedsUpdate(const QString &slug, int manifestVersion) const
{
    const QString path = deviceCachePath(slug) + QStringLiteral("/descriptor.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return true;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return true;

    const int cachedVersion = doc.object()[QStringLiteral("version")].toInt(0);
    return cachedVersion < manifestVersion;
}

QString DeviceFetcher::deviceCachePath(const QString &slug) const
{
    return m_cacheDir + QStringLiteral("/devices/") + slug;
}

// ---- Network stubs (Task 2) -------------------------------------------------

void DeviceFetcher::fetchManifest()
{
    // Stub — implemented in Task 2
}

void DeviceFetcher::fetchForPid(uint16_t pid)
{
    Q_UNUSED(pid)
    // Stub — implemented in Task 2
}

void DeviceFetcher::onManifestReceived(const QJsonObject &manifest)
{
    Q_UNUSED(manifest)
    // Stub — implemented in Task 2
}

void DeviceFetcher::downloadDevice(const QString &slug, const QJsonObject &deviceInfo)
{
    Q_UNUSED(slug)
    Q_UNUSED(deviceInfo)
    // Stub — implemented in Task 2
}

void DeviceFetcher::onFileDownloaded(const QString &slug, const QString &filename,
                                      const QByteArray &data)
{
    Q_UNUSED(slug)
    Q_UNUSED(filename)
    Q_UNUSED(data)
    // Stub — implemented in Task 2
}

void DeviceFetcher::checkDownloadsComplete()
{
    // Stub — implemented in Task 2
}

} // namespace logitune
