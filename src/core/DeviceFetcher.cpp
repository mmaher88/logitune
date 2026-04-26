#include "DeviceFetcher.h"
#include "logging/LogManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
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

// ---- Network implementation --------------------------------------------------

void DeviceFetcher::fetchManifest()
{
    if (isCacheFresh()) {
        qCDebug(lcDevice) << "cache fresh, skipping";
        return;
    }

    QNetworkRequest req{QUrl(kManifestUrl)};
    req.setTransferTimeout(10000);

    const QString etag = loadEtag();
    if (!etag.isEmpty())
        req.setRawHeader("If-None-Match", etag.toUtf8());

    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDevice) << "manifest fetch failed:" << reply->errorString();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 304) {
            qCDebug(lcDevice) << "manifest unchanged";
            saveTimestamp();
            return;
        }

        if (status == 200) {
            const QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
            if (!newEtag.isEmpty())
                saveEtag(newEtag);

            const auto doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) {
                qCWarning(lcDevice) << "manifest is not valid JSON object";
                return;
            }

            const QJsonObject manifest = doc.object();
            saveManifest(manifest);
            saveTimestamp();
            onManifestReceived(manifest);
            return;
        }

        qCWarning(lcDevice) << "manifest fetch unexpected status:" << status;
    });
}

void DeviceFetcher::fetchForPid(uint16_t pid)
{
    m_pendingPid = pid;

    // Try cached manifest first
    const QJsonObject cached = loadManifest();
    if (!cached.isEmpty()) {
        const auto [slug, devInfo] = findDeviceForPid(cached, pid);
        if (!slug.isEmpty()) {
            const int version = devInfo[QStringLiteral("version")].toInt(0);
            if (deviceNeedsUpdate(slug, version)) {
                downloadDevice(slug, devInfo);
                return;
            }
            m_pendingPid = 0;
            return;
        }
    }

    // No cached manifest or PID not found — fetch fresh manifest
    QNetworkRequest req{QUrl(kManifestUrl)};
    req.setTransferTimeout(10000);

    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const uint16_t pid = m_pendingPid;
        m_pendingPid = 0;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDevice) << "manifest fetch for PID failed:" << reply->errorString();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            qCWarning(lcDevice) << "manifest fetch for PID unexpected status:" << status;
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            qCWarning(lcDevice) << "manifest is not valid JSON object";
            return;
        }

        const QJsonObject manifest = doc.object();
        saveManifest(manifest);
        saveTimestamp();

        const auto [slug, devInfo] = findDeviceForPid(manifest, pid);
        if (slug.isEmpty()) {
            qCDebug(lcDevice) << "PID" << Qt::hex << pid << "not found in manifest";
            return;
        }

        const int version = devInfo[QStringLiteral("version")].toInt(0);
        if (deviceNeedsUpdate(slug, version))
            downloadDevice(slug, devInfo);
    });
}

void DeviceFetcher::onManifestReceived(const QJsonObject &manifest)
{
    const auto devices = manifest[QStringLiteral("devices")].toObject();
    bool anyNeeded = false;

    for (auto it = devices.begin(); it != devices.end(); ++it) {
        const QString slug = it.key();
        const QJsonObject devInfo = it.value().toObject();
        const int version = devInfo[QStringLiteral("version")].toInt(0);

        if (deviceNeedsUpdate(slug, version)) {
            downloadDevice(slug, devInfo);
            anyNeeded = true;
        }
    }

    if (!anyNeeded)
        qCDebug(lcDevice) << "all devices up to date";
}

void DeviceFetcher::downloadDevice(const QString &slug, const QJsonObject &deviceInfo)
{
    const QJsonArray files = deviceInfo[QStringLiteral("files")].toArray();
    qCDebug(lcDevice) << "downloading" << slug << "(" << files.size() << "files)";

    for (const auto &fileVal : files) {
        const QString filename = fileVal.toString();
        if (filename.isEmpty())
            continue;

        m_pendingDownloads++;

        const QUrl url(kRawBaseUrl + slug + QStringLiteral("/") + filename);
        QNetworkRequest req{url};
        req.setTransferTimeout(10000);

        auto *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, slug, filename]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                qCWarning(lcDevice) << "download failed:" << slug << "/" << filename
                                    << reply->errorString();
            } else {
                onFileDownloaded(slug, filename, reply->readAll());
            }

            m_pendingDownloads--;
            checkDownloadsComplete();
        });
    }
}

void DeviceFetcher::onFileDownloaded(const QString &slug, const QString &filename,
                                      const QByteArray &data)
{
    const QString dir = deviceCachePath(slug);
    QDir().mkpath(dir);

    QFile f(dir + QStringLiteral("/") + filename);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(lcDevice) << "failed to write" << slug << "/" << filename;
        return;
    }
    f.write(data);

    m_hasNewDevices = true;
    qCDebug(lcDevice) << "saved" << slug << "/" << filename;
}

void DeviceFetcher::checkDownloadsComplete()
{
    if (m_pendingDownloads > 0)
        return;

    if (m_hasNewDevices) {
        qCDebug(lcDevice) << "new descriptors downloaded";
        m_hasNewDevices = false;
        emit descriptorsUpdated();
    }
}

} // namespace logitune
