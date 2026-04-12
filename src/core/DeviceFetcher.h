#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QString>
#include <QPair>

namespace logitune {

class DeviceFetcher : public QObject {
    Q_OBJECT
public:
    explicit DeviceFetcher(QObject *parent = nullptr);

    void fetchManifest();
    void fetchForPid(uint16_t pid);

    // Cache utilities (public for testing)
    void setCacheDir(const QString &dir);
    bool isCacheFresh() const;
    void saveTimestamp();
    void saveEtag(const QString &etag);
    QString loadEtag() const;
    void saveManifest(const QJsonObject &manifest);
    QJsonObject loadManifest() const;
    QPair<QString, QJsonObject> findDeviceForPid(const QJsonObject &manifest, uint16_t pid) const;
    bool deviceNeedsUpdate(const QString &slug, int manifestVersion) const;
    QString deviceCachePath(const QString &slug) const;

signals:
    void descriptorsUpdated();

private:
    void onManifestReceived(const QJsonObject &manifest);
    void downloadDevice(const QString &slug, const QJsonObject &deviceInfo);
    void onFileDownloaded(const QString &slug, const QString &filename, const QByteArray &data);
    void checkDownloadsComplete();

    QNetworkAccessManager m_nam;
    QString m_cacheDir;
    uint16_t m_pendingPid = 0;
    int m_pendingDownloads = 0;
    bool m_hasNewDevices = false;

    static constexpr int kCacheTtlSeconds = 3600;
    static const QString kManifestUrl;
    static const QString kRawBaseUrl;
};

} // namespace logitune
