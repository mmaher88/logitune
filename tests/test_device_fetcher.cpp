#include <gtest/gtest.h>
#include "DeviceFetcher.h"

#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>

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
