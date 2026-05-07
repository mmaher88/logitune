#include <gtest/gtest.h>
#include "DeviceFetcher.h"

#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QByteArray>

using namespace logitune;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

class DeviceFetcherTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_tmp.isValid());
        m_fetcher.setCacheDir(m_tmp.path());
    }

    QTemporaryDir m_tmp;
    DeviceFetcher m_fetcher;
};

static QJsonObject makeTestManifest()
{
    QJsonObject mx3s;
    mx3s[QStringLiteral("pids")] = QJsonArray{QStringLiteral("0xc08b")};
    mx3s[QStringLiteral("version")] = 2;
    mx3s[QStringLiteral("files")] = QJsonArray{
        QStringLiteral("descriptor.json"),
        QStringLiteral("front.png"),
    };

    QJsonObject devices;
    devices[QStringLiteral("mx-master-3s")] = mx3s;

    QJsonObject manifest;
    manifest[QStringLiteral("devices")] = devices;
    return manifest;
}

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const char *name, const QByteArray &value)
        : m_name(name)
        , m_hadValue(qEnvironmentVariableIsSet(name))
        , m_previous(qgetenv(name))
    {
        qputenv(name, value);
    }

    ~ScopedEnvironmentVariable()
    {
        if (m_hadValue)
            qputenv(m_name, m_previous);
        else
            qunsetenv(m_name);
    }

private:
    const char *m_name;
    bool m_hadValue;
    QByteArray m_previous;
};

class ScopedUnsetEnvironmentVariable {
public:
    explicit ScopedUnsetEnvironmentVariable(const char *name)
        : m_name(name)
        , m_hadValue(qEnvironmentVariableIsSet(name))
        , m_previous(qgetenv(name))
    {
        qunsetenv(name);
    }

    ~ScopedUnsetEnvironmentVariable()
    {
        if (m_hadValue)
            qputenv(m_name, m_previous);
    }

private:
    const char *m_name;
    bool m_hadValue;
    QByteArray m_previous;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(DeviceFetcherTest, CacheNotFreshWhenNoTimestamp)
{
    EXPECT_FALSE(m_fetcher.isCacheFresh());
}

TEST_F(DeviceFetcherTest, CacheIsFreshWithinTTL)
{
    m_fetcher.saveTimestamp();
    EXPECT_TRUE(m_fetcher.isCacheFresh());
}

TEST_F(DeviceFetcherTest, EtagRoundTrip)
{
    m_fetcher.saveEtag(QStringLiteral("abc123"));
    EXPECT_EQ(m_fetcher.loadEtag(), QStringLiteral("abc123"));
}

TEST_F(DeviceFetcherTest, EtagEmptyWhenNoFile)
{
    EXPECT_TRUE(m_fetcher.loadEtag().isEmpty());
}

TEST_F(DeviceFetcherTest, ManifestParseFindsPid)
{
    const auto manifest = makeTestManifest();
    const auto [slug, info] = m_fetcher.findDeviceForPid(manifest, 0xc08b);

    EXPECT_EQ(slug, QStringLiteral("mx-master-3s"));
    EXPECT_EQ(info[QStringLiteral("version")].toInt(), 2);
}

TEST_F(DeviceFetcherTest, BundledManifestLoads)
{
    const auto manifest = m_fetcher.loadBundledManifest();
    ASSERT_FALSE(manifest.isEmpty());
    EXPECT_TRUE(manifest[QStringLiteral("devices")].toObject().contains(QStringLiteral("mx-mechanical")));
}

TEST_F(DeviceFetcherTest, BundledManifestFindsMxMechanicalPid)
{
    const auto manifest = m_fetcher.loadBundledManifest();
    const auto [slug, info] = m_fetcher.findDeviceForPid(manifest, 0xb366);

    EXPECT_EQ(slug, QStringLiteral("mx-mechanical"));
    EXPECT_EQ(info[QStringLiteral("version")].toInt(), 1);
}

TEST_F(DeviceFetcherTest, ManifestFilenameSafety)
{
    EXPECT_TRUE(m_fetcher.isSafeManifestFilename(QStringLiteral("descriptor.json")));
    EXPECT_TRUE(m_fetcher.isSafeManifestFilename(QStringLiteral("front.png")));
    EXPECT_TRUE(m_fetcher.isSafeManifestFilename(QStringLiteral("side.svg")));

    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QString()));
    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QStringLiteral("../descriptor.json")));
    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QStringLiteral("nested/front.png")));
    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QStringLiteral("nested\\front.png")));
    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QStringLiteral("/tmp/front.png")));
    EXPECT_FALSE(m_fetcher.isSafeManifestFilename(QStringLiteral("descriptor.json.bak")));
}

TEST_F(DeviceFetcherTest, ManifestParseReturnsEmptyForUnknownPid)
{
    QJsonObject manifest;
    manifest[QStringLiteral("devices")] = QJsonObject{};

    const auto [slug, info] = m_fetcher.findDeviceForPid(manifest, 0xFFFF);
    EXPECT_TRUE(slug.isEmpty());
    EXPECT_TRUE(info.isEmpty());
}

TEST_F(DeviceFetcherTest, NeedsUpdateWhenNotCached)
{
    EXPECT_TRUE(m_fetcher.deviceNeedsUpdate(QStringLiteral("mx-master-3s"), 1));
}

TEST_F(DeviceFetcherTest, NeedsUpdateWhenVersionHigher)
{
    const QString devDir = m_fetcher.deviceCachePath(QStringLiteral("mx-master-3s"));
    QDir().mkpath(devDir);

    QJsonObject descriptor;
    descriptor[QStringLiteral("version")] = 1;

    QFile f(devDir + QStringLiteral("/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(descriptor).toJson(QJsonDocument::Compact));
    f.close();

    // manifest version 2 > cached version 1 => needs update
    EXPECT_TRUE(m_fetcher.deviceNeedsUpdate(QStringLiteral("mx-master-3s"), 2));

    // same version => no update needed
    EXPECT_FALSE(m_fetcher.deviceNeedsUpdate(QStringLiteral("mx-master-3s"), 1));
}

TEST_F(DeviceFetcherTest, DeviceCachePaths)
{
    const QString expected = m_tmp.path() + QStringLiteral("/devices/mx-master-3s");
    EXPECT_EQ(m_fetcher.deviceCachePath(QStringLiteral("mx-master-3s")), expected);
}

TEST_F(DeviceFetcherTest, DescriptorFeedUrlsUseDefaultsWhenEnvironmentUnset)
{
    ScopedUnsetEnvironmentVariable manifest("LOGITUNE_DEVICE_MANIFEST_URL");
    ScopedUnsetEnvironmentVariable rawBase("LOGITUNE_DEVICE_RAW_BASE_URL");

    EXPECT_EQ(DeviceFetcher::manifestUrl(),
              QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune/master/devices/manifest.json"));
    EXPECT_EQ(DeviceFetcher::rawBaseUrl(),
              QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune/master/devices/"));
}

TEST_F(DeviceFetcherTest, DescriptorFeedUrlsUseDefaultsWhenEnvironmentEmpty)
{
    ScopedEnvironmentVariable manifest("LOGITUNE_DEVICE_MANIFEST_URL", QByteArray());
    ScopedEnvironmentVariable rawBase("LOGITUNE_DEVICE_RAW_BASE_URL", QByteArray());

    EXPECT_EQ(DeviceFetcher::manifestUrl(),
              QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune/master/devices/manifest.json"));
    EXPECT_EQ(DeviceFetcher::rawBaseUrl(),
              QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune/master/devices/"));
}

TEST_F(DeviceFetcherTest, DescriptorFeedUrlsUseEnvironmentOverrides)
{
    ScopedEnvironmentVariable manifest(
        "LOGITUNE_DEVICE_MANIFEST_URL",
        QByteArray("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices/manifest.json"));
    ScopedEnvironmentVariable rawBase(
        "LOGITUNE_DEVICE_RAW_BASE_URL",
        QByteArray("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices/"));

    EXPECT_EQ(DeviceFetcher::manifestUrl(),
              QStringLiteral("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices/manifest.json"));
    EXPECT_EQ(DeviceFetcher::rawBaseUrl(),
              QStringLiteral("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices/"));
}

TEST_F(DeviceFetcherTest, DescriptorFeedRawBaseOverrideAddsTrailingSlash)
{
    ScopedEnvironmentVariable rawBase(
        "LOGITUNE_DEVICE_RAW_BASE_URL",
        QByteArray("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices///"));

    EXPECT_EQ(DeviceFetcher::rawBaseUrl(),
              QStringLiteral("https://raw.githubusercontent.com/example/logitune/descriptor-work/devices/"));
}
