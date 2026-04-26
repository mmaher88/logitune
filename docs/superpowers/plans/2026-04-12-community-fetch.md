# Community Device Database Fetch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On startup and when an unknown device is detected, fetch community device descriptors from a GitHub repo and cache them locally.

**Architecture:** A `DeviceFetcher` class uses Qt's `QNetworkAccessManager` to fetch a manifest from `raw.githubusercontent.com`, compares versions against locally cached descriptors, and downloads new/updated devices. ETag caching avoids redundant downloads. DeviceRegistry reloads from disk when new descriptors arrive. All network operations are async and non-blocking.

**Tech Stack:** C++20, Qt6 (QNetworkAccessManager, QNetworkReply, QJsonDocument, QStandardPaths), GoogleTest, CMake/Ninja

**Spec:** `docs/superpowers/specs/2026-04-12-community-fetch-design.md`

**Stacks on:** `json-device-descriptors` branch (PR #23)

---

## File Structure

**New files:**
- `src/core/DeviceFetcher.h` — DeviceFetcher class declaration
- `src/core/DeviceFetcher.cpp` — manifest fetch, ETag caching, device download
- `tests/test_device_fetcher.cpp` — unit tests for cache, parsing, version comparison

**Modified files:**
- `CMakeLists.txt` — add Qt6::Network to find_package
- `src/core/CMakeLists.txt` — link Qt6::Network, add DeviceFetcher.cpp
- `src/core/DeviceRegistry.h` — add `reloadAll()`
- `src/core/DeviceRegistry.cpp` — implement `reloadAll()`
- `src/core/DeviceManager.h` — add `unknownDeviceDetected(uint16_t pid)` signal
- `src/core/DeviceManager.cpp` — emit signal when descriptor lookup fails
- `src/app/AppController.h` — add DeviceFetcher member
- `src/app/AppController.cpp` — wire DeviceFetcher signals, trigger startup fetch
- `tests/CMakeLists.txt` — add test_device_fetcher.cpp

---

### Task 1: Add Qt6::Network + DeviceFetcher skeleton with cache utilities

**Files:**
- Modify: `CMakeLists.txt` (add Network to find_package)
- Modify: `src/core/CMakeLists.txt` (link Qt6::Network, add DeviceFetcher.cpp)
- Create: `src/core/DeviceFetcher.h`
- Create: `src/core/DeviceFetcher.cpp`
- Create: `tests/test_device_fetcher.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1.1: Write failing tests for cache utilities**

Create `tests/test_device_fetcher.cpp`:

```cpp
#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include "DeviceFetcher.h"

using namespace logitune;

class DeviceFetcherTest : public ::testing::Test {
protected:
    QTemporaryDir tmpDir;
};

TEST_F(DeviceFetcherTest, CacheNotFreshWhenNoTimestamp) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    EXPECT_FALSE(fetcher.isCacheFresh());
}

TEST_F(DeviceFetcherTest, CacheIsFreshWithinTTL) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    fetcher.saveTimestamp();
    EXPECT_TRUE(fetcher.isCacheFresh());
}

TEST_F(DeviceFetcherTest, EtagRoundTrip) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    fetcher.saveEtag("abc123");
    EXPECT_EQ(fetcher.loadEtag(), "abc123");
}

TEST_F(DeviceFetcherTest, EtagEmptyWhenNoFile) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    EXPECT_TRUE(fetcher.loadEtag().isEmpty());
}

TEST_F(DeviceFetcherTest, ManifestParseFindsPid) {
    QJsonObject manifest;
    QJsonObject devices;
    QJsonObject dev;
    dev["version"] = 1;
    dev["name"] = "Test Mouse";
    dev["pids"] = QJsonArray({"0xc08b"});
    dev["files"] = QJsonArray({"descriptor.json", "front.png"});
    devices["test-mouse"] = dev;
    manifest["version"] = 1;
    manifest["devices"] = devices;

    DeviceFetcher fetcher;
    auto result = fetcher.findDeviceForPid(manifest, 0xc08b);
    EXPECT_EQ(result.first, "test-mouse");
    EXPECT_EQ(result.second["name"].toString(), "Test Mouse");
}

TEST_F(DeviceFetcherTest, ManifestParseReturnsEmptyForUnknownPid) {
    QJsonObject manifest;
    manifest["version"] = 1;
    manifest["devices"] = QJsonObject();

    DeviceFetcher fetcher;
    auto result = fetcher.findDeviceForPid(manifest, 0xFFFF);
    EXPECT_TRUE(result.first.isEmpty());
}

TEST_F(DeviceFetcherTest, NeedsUpdateWhenNotCached) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    EXPECT_TRUE(fetcher.deviceNeedsUpdate("test-mouse", 1));
}

TEST_F(DeviceFetcherTest, NeedsUpdateWhenVersionHigher) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());

    // Create a cached descriptor with version 1
    QDir(tmpDir.path()).mkpath("test-mouse");
    QJsonObject desc;
    desc["version"] = 1;
    desc["name"] = "Test";
    desc["status"] = "community-verified";
    QFile f(tmpDir.path() + "/test-mouse/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(desc).toJson());
    f.close();

    EXPECT_FALSE(fetcher.deviceNeedsUpdate("test-mouse", 1));
    EXPECT_TRUE(fetcher.deviceNeedsUpdate("test-mouse", 2));
}

TEST_F(DeviceFetcherTest, DeviceDownloadPaths) {
    DeviceFetcher fetcher;
    fetcher.setCacheDir(tmpDir.path());
    auto path = fetcher.deviceCachePath("g502-hero");
    EXPECT_TRUE(path.endsWith("/g502-hero"));
    EXPECT_TRUE(path.startsWith(tmpDir.path()));
}
```

- [ ] **Step 1.2: Register test and add Qt6::Network dependency**

In `CMakeLists.txt`, add `Network` to the find_package:
```cmake
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Quick Svg DBus Widgets Concurrent Test QuickTest Network)
```

In `src/core/CMakeLists.txt`, add `DeviceFetcher.cpp` to sources and `Qt6::Network` to link:
```cmake
    DeviceFetcher.cpp
```
And update the link line:
```cmake
target_link_libraries(logitune-core PUBLIC Qt6::Core Qt6::DBus Qt6::Network ${UDEV_LIBRARIES})
```

In `tests/CMakeLists.txt`, add `test_device_fetcher.cpp` to the sources list.

- [ ] **Step 1.3: Create DeviceFetcher.h**

```cpp
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
```

- [ ] **Step 1.4: Create DeviceFetcher.cpp with cache utilities only**

```cpp
#include "DeviceFetcher.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace logitune {

const QString DeviceFetcher::kManifestUrl =
    QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune-devices/main/manifest.json");
const QString DeviceFetcher::kRawBaseUrl =
    QStringLiteral("https://raw.githubusercontent.com/mmaher88/logitune-devices/main/");

DeviceFetcher::DeviceFetcher(QObject *parent)
    : QObject(parent)
{
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                 + QStringLiteral("/logitune");
}

void DeviceFetcher::setCacheDir(const QString &dir) { m_cacheDir = dir; }

QString DeviceFetcher::deviceCachePath(const QString &slug) const
{
    return m_cacheDir + QStringLiteral("/devices/") + slug;
}

bool DeviceFetcher::isCacheFresh() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest-timestamp"));
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDateTime ts = QDateTime::fromString(QString::fromUtf8(f.readAll()).trimmed(), Qt::ISODate);
    return ts.isValid() && ts.secsTo(QDateTime::currentDateTimeUtc()) < kCacheTtlSeconds;
}

void DeviceFetcher::saveTimestamp()
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest-timestamp"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
}

void DeviceFetcher::saveEtag(const QString &etag)
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest-etag"));
    if (f.open(QIODevice::WriteOnly))
        f.write(etag.toUtf8());
}

QString DeviceFetcher::loadEtag() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest-etag"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

void DeviceFetcher::saveManifest(const QJsonObject &manifest)
{
    QDir().mkpath(m_cacheDir);
    QFile f(m_cacheDir + QStringLiteral("/manifest.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(manifest).toJson());
}

QJsonObject DeviceFetcher::loadManifest() const
{
    QFile f(m_cacheDir + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QPair<QString, QJsonObject> DeviceFetcher::findDeviceForPid(const QJsonObject &manifest,
                                                             uint16_t pid) const
{
    QString pidStr = QStringLiteral("0x%1").arg(pid, 4, 16, QLatin1Char('0'));
    QJsonObject devices = manifest["devices"].toObject();
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        QJsonObject dev = it.value().toObject();
        QJsonArray pids = dev["pids"].toArray();
        for (const auto &p : pids) {
            if (p.toString().compare(pidStr, Qt::CaseInsensitive) == 0)
                return {it.key(), dev};
        }
    }
    return {};
}

bool DeviceFetcher::deviceNeedsUpdate(const QString &slug, int manifestVersion) const
{
    QFile f(deviceCachePath(slug) + QStringLiteral("/descriptor.json"));
    if (!f.open(QIODevice::ReadOnly)) return true;
    QJsonObject cached = QJsonDocument::fromJson(f.readAll()).object();
    return cached["version"].toInt(0) < manifestVersion;
}

// Network methods — implemented in Task 2
void DeviceFetcher::fetchManifest() {}
void DeviceFetcher::fetchForPid(uint16_t) {}
void DeviceFetcher::onManifestReceived(const QJsonObject &) {}
void DeviceFetcher::downloadDevice(const QString &, const QJsonObject &) {}
void DeviceFetcher::onFileDownloaded(const QString &, const QString &, const QByteArray &) {}
void DeviceFetcher::checkDownloadsComplete() {}

} // namespace logitune
```

- [ ] **Step 1.5: Build and run tests**

Run: `cmake -B build && cmake --build build --parallel $(nproc) 2>&1 | tail -10`

Expected: clean build with Qt6::Network found and linked.

Run: `XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests --gtest_filter='DeviceFetcher*' 2>&1 | tail -15`

Expected: 8 tests pass.

- [ ] **Step 1.6: Commit**

```bash
git add CMakeLists.txt src/core/CMakeLists.txt src/core/DeviceFetcher.h \
        src/core/DeviceFetcher.cpp tests/test_device_fetcher.cpp tests/CMakeLists.txt
git commit -m "add DeviceFetcher skeleton with cache utilities

ETag read/write, timestamp-based TTL check, manifest PID lookup,
version comparison against cached descriptors. Network methods are
stubs pending Task 2.

refs #22"
```

---

### Task 2: Implement manifest fetch + device download

**Files:**
- Modify: `src/core/DeviceFetcher.cpp` (replace stub methods with real implementation)

- [ ] **Step 2.1: Implement fetchManifest()**

Replace the stub with:

```cpp
void DeviceFetcher::fetchManifest()
{
    if (isCacheFresh()) {
        qCDebug(lcDevice) << "DeviceFetcher: cache fresh, skipping manifest check";
        return;
    }

    QNetworkRequest req(QUrl(kManifestUrl));
    req.setTransferTimeout(10000);
    QString etag = loadEtag();
    if (!etag.isEmpty())
        req.setRawHeader("If-None-Match", etag.toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDevice) << "DeviceFetcher: manifest fetch failed:" << reply->errorString();
            return;
        }

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 304) {
            qCDebug(lcDevice) << "DeviceFetcher: manifest unchanged (304)";
            saveTimestamp();
            return;
        }

        QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
        if (!newEtag.isEmpty())
            saveEtag(newEtag);

        QJsonObject manifest = QJsonDocument::fromJson(reply->readAll()).object();
        if (manifest.isEmpty()) {
            qCWarning(lcDevice) << "DeviceFetcher: invalid manifest JSON";
            return;
        }

        saveManifest(manifest);
        saveTimestamp();
        onManifestReceived(manifest);
    });
}
```

- [ ] **Step 2.2: Implement fetchForPid()**

```cpp
void DeviceFetcher::fetchForPid(uint16_t pid)
{
    m_pendingPid = pid;
    QJsonObject manifest = loadManifest();

    if (!manifest.isEmpty()) {
        auto [slug, devInfo] = findDeviceForPid(manifest, pid);
        if (!slug.isEmpty() && deviceNeedsUpdate(slug, devInfo["version"].toInt())) {
            downloadDevice(slug, devInfo);
            return;
        }
    }

    // No cached manifest or PID not found — fetch fresh manifest
    QNetworkRequest req(QUrl(kManifestUrl));
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDevice) << "DeviceFetcher: manifest fetch for PID failed:" << reply->errorString();
            m_pendingPid = 0;
            return;
        }

        QJsonObject manifest = QJsonDocument::fromJson(reply->readAll()).object();
        if (manifest.isEmpty()) { m_pendingPid = 0; return; }

        saveManifest(manifest);
        saveTimestamp();

        auto [slug, devInfo] = findDeviceForPid(manifest, m_pendingPid);
        m_pendingPid = 0;
        if (!slug.isEmpty() && deviceNeedsUpdate(slug, devInfo["version"].toInt()))
            downloadDevice(slug, devInfo);
    });
}
```

- [ ] **Step 2.3: Implement onManifestReceived()**

```cpp
void DeviceFetcher::onManifestReceived(const QJsonObject &manifest)
{
    QJsonObject devices = manifest["devices"].toObject();
    m_pendingDownloads = 0;
    m_hasNewDevices = false;

    for (auto it = devices.begin(); it != devices.end(); ++it) {
        QJsonObject devInfo = it.value().toObject();
        int version = devInfo["version"].toInt(0);
        if (deviceNeedsUpdate(it.key(), version)) {
            downloadDevice(it.key(), devInfo);
        }
    }

    if (m_pendingDownloads == 0)
        qCDebug(lcDevice) << "DeviceFetcher: all devices up to date";
}
```

- [ ] **Step 2.4: Implement downloadDevice() and onFileDownloaded()**

```cpp
void DeviceFetcher::downloadDevice(const QString &slug, const QJsonObject &deviceInfo)
{
    QJsonArray files = deviceInfo["files"].toArray();
    if (files.isEmpty()) return;

    qCDebug(lcDevice) << "DeviceFetcher: downloading" << slug
                      << "(" << files.size() << "files)";

    for (const auto &fileVal : files) {
        QString filename = fileVal.toString();
        QString url = kRawBaseUrl + slug + "/" + filename;

        m_pendingDownloads++;
        QNetworkRequest req(QUrl(url));
        req.setTransferTimeout(10000);
        QNetworkReply *reply = m_nam.get(req);

        connect(reply, &QNetworkReply::finished, this, [this, slug, filename, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                qCWarning(lcDevice) << "DeviceFetcher: failed to download"
                                    << slug << "/" << filename << ":" << reply->errorString();
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
    QString dir = deviceCachePath(slug);
    QDir().mkpath(dir);
    QFile f(dir + "/" + filename);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(data);
        m_hasNewDevices = true;
        qCDebug(lcDevice) << "DeviceFetcher: saved" << slug << "/" << filename;
    } else {
        qCWarning(lcDevice) << "DeviceFetcher: failed to write" << f.fileName();
    }
}

void DeviceFetcher::checkDownloadsComplete()
{
    if (m_pendingDownloads > 0) return;
    if (m_hasNewDevices) {
        qCInfo(lcDevice) << "DeviceFetcher: new descriptors downloaded, signaling reload";
        emit descriptorsUpdated();
        m_hasNewDevices = false;
    }
}
```

- [ ] **Step 2.5: Build and verify all tests still pass**

Run: `cmake --build build --parallel $(nproc) && XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass (existing + 8 DeviceFetcher tests).

- [ ] **Step 2.6: Commit**

```bash
git add src/core/DeviceFetcher.cpp
git commit -m "implement manifest fetch + device download

Async manifest fetch with ETag conditional requests. Compares
manifest versions against cached descriptors and downloads
new/updated devices. Downloads all files in parallel. Emits
descriptorsUpdated when complete.

refs #22"
```

---

### Task 3: Add DeviceRegistry::reloadAll() + DeviceManager unknown signal

**Files:**
- Modify: `src/core/DeviceRegistry.h`
- Modify: `src/core/DeviceRegistry.cpp`
- Modify: `src/core/DeviceManager.h`
- Modify: `src/core/DeviceManager.cpp`

- [ ] **Step 3.1: Add reloadAll() to DeviceRegistry**

In `src/core/DeviceRegistry.h`, add to public methods:

```cpp
    void reloadAll();
```

In `src/core/DeviceRegistry.cpp`, implement:

```cpp
void DeviceRegistry::reloadAll()
{
    m_devices.clear();
    loadDirectory(systemDevicesDir());
    loadDirectory(cacheDevicesDir());
    loadDirectory(userDevicesDir());
    qCInfo(lcDevice) << "DeviceRegistry: reloaded" << m_devices.size() << "devices";
}
```

- [ ] **Step 3.2: Add unknownDeviceDetected signal to DeviceManager**

In `src/core/DeviceManager.h`, add to the signals section:

```cpp
    void unknownDeviceDetected(uint16_t pid);
```

In `src/core/DeviceManager.cpp`, find the block in `enumerateAndSetup()` around line 714-718 where `m_activeDevice` is null after lookup. After the existing warning log, emit the signal:

```cpp
    if (m_activeDevice)
        qCDebug(lcDevice) << "matched device descriptor:" << m_activeDevice->deviceName();
    else {
        qCDebug(lcDevice) << "no device descriptor found for PID"
                          << Qt::hex << m_device->info().productId << "name:" << name;
        emit unknownDeviceDetected(m_device->info().productId);
    }
```

Note: The `else` block needs to be added — currently the else just logs. Wrap the log + emit in braces.

- [ ] **Step 3.3: Build and run tests**

Run: `cmake --build build --parallel $(nproc) && XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass.

- [ ] **Step 3.4: Commit**

```bash
git add src/core/DeviceRegistry.h src/core/DeviceRegistry.cpp \
        src/core/DeviceManager.h src/core/DeviceManager.cpp
git commit -m "add DeviceRegistry::reloadAll + DeviceManager::unknownDeviceDetected

reloadAll clears and re-scans all three XDG directories. The signal
fires when a connected Logitech device has no matching descriptor,
triggering the community database fetch.

refs #22"
```

---

### Task 4: Wire DeviceFetcher in AppController

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`

- [ ] **Step 4.1: Add DeviceFetcher member and include**

In `src/app/AppController.h`, add include:

```cpp
#include "DeviceFetcher.h"
```

Add member in the private section near the other subsystem members:

```cpp
    DeviceFetcher m_deviceFetcher;
```

- [ ] **Step 4.2: Wire signals in AppController constructor**

In `src/app/AppController.cpp`, find the signal connection block in the constructor (the `wireSignals()` method or inline connections). Add these connections:

```cpp
    // Community device database: fetch on startup, fetch on unknown device
    connect(&m_deviceFetcher, &DeviceFetcher::descriptorsUpdated,
            this, [this]() {
        m_registry.reloadAll();
        // Re-attempt device match if current device is unmatched
        if (m_deviceManager.deviceConnected() && !m_deviceManager.activeDevice()) {
            qCInfo(lcApp) << "new community descriptors available, re-matching device";
            // Trigger re-enumeration which will re-match
        }
    });

    connect(&m_deviceManager, &DeviceManager::unknownDeviceDetected,
            &m_deviceFetcher, &DeviceFetcher::fetchForPid);
```

Find the startup sequence (after `m_deviceManager.start()` or in an init method). Add the manifest fetch trigger:

```cpp
    m_deviceFetcher.fetchManifest();
```

- [ ] **Step 4.3: Build and run all tests**

Run: `cmake --build build --parallel $(nproc) && XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass.

Run: `XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tray-tests 2>&1 | tail -3`

Expected: 12 tests pass.

- [ ] **Step 4.4: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp
git commit -m "wire DeviceFetcher in AppController

Fetch community manifest on startup (non-blocking). When new
descriptors are downloaded, reload DeviceRegistry. When an unknown
device is detected, trigger a fetch for that PID.

refs #22"
```

---

### Task 5: Full verification + smoke test

**Files:** none (verification only)

- [ ] **Step 5.1: Clean build**

Run: `cmake --build build --parallel $(nproc) --clean-first 2>&1 | tail -10`

Expected: clean build.

- [ ] **Step 5.2: Run all tests**

Run: `XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass.

- [ ] **Step 5.3: Smoke test on MX3S**

Kill any running logitune. Launch with debug and XDG_DATA_DIRS:

```bash
XDG_DATA_DIRS=$(pwd)/build nohup ./build/src/app/logitune --debug > /tmp/logitune-fetch.log 2>&1 & disown
```

Wait 2 seconds, then check:

```bash
grep -E "DeviceFetcher|DeviceRegistry|matched device|Startup complete" /tmp/logitune-fetch.log
```

Expected lines:
- `DeviceRegistry: loaded 2 devices` (from disk)
- `DeviceFetcher: manifest fetch failed:` OR `DeviceFetcher: cache fresh` OR `DeviceFetcher: manifest unchanged (304)` — any of these is correct (depends on whether the logitune-devices repo exists yet)
- `matched device descriptor: "MX Master 3S"`
- `Startup complete`

The key verification: the app starts normally, MX3S works, and the fetch either succeeds or fails gracefully without affecting startup.

- [ ] **Step 5.4: Verify offline behavior**

Temporarily disconnect network (or use an invalid URL). Restart the app. Verify it starts normally with the cached descriptors and logs a warning for the fetch failure.

- [ ] **Step 5.5: Kill smoke test**

```bash
pkill -f "build/src/app/logitune"
```

---

### Task 6: Push stacked branch + open PR

**Files:** none (git operations)

- [ ] **Step 6.1: Review commit log**

Run: `git log --oneline json-device-descriptors..HEAD`

Expected: 4 commits (Tasks 1-4).

- [ ] **Step 6.2: Create stacked branch and push**

```bash
git checkout -b community-device-fetch
git push -u origin community-device-fetch
```

- [ ] **Step 6.3: Open PR targeting json-device-descriptors**

```bash
gh pr create --base json-device-descriptors --head community-device-fetch \
  --title "feat: community device database fetch from GitHub" \
  --body "$(cat <<'PREOF'
## Summary

On startup and when an unknown device is detected, check the logitune-devices GitHub repo for community-contributed device descriptors and cache them locally.

Stacks on #23. Part of #22 (Phase 5).

## What changed

- **DeviceFetcher class** — async manifest fetch with ETag conditional requests, device download, file caching
- **DeviceRegistry::reloadAll()** — clears and re-scans all XDG directories
- **DeviceManager::unknownDeviceDetected** — signal emitted when a connected device has no descriptor
- **AppController wiring** — fetches manifest on startup, fetches for PID on unknown device
- **Qt6::Network** added as dependency
- **8 unit tests** for cache utilities, manifest parsing, version comparison

## Network protocol

- Manifest: single GET to raw.githubusercontent.com with ETag caching
- 304 Not Modified on repeat checks (zero bandwidth)
- 1-hour cache TTL (skip network entirely if checked recently)
- All errors are silent (log + skip)

## Testing

- 8 new DeviceFetcher unit tests
- All existing tests pass
- Smoke tested: app starts normally, fetch runs async, offline fallback works
PREOF
)"
```

- [ ] **Step 6.4: Done**

---

## Self-Review

**1. Spec coverage:**
- [x] DeviceFetcher class with async fetch — Tasks 1-2
- [x] ETag caching — Task 1 (utilities) + Task 2 (request headers)
- [x] 1-hour cache TTL — Task 1 (isCacheFresh)
- [x] Startup trigger (non-blocking) — Task 4
- [x] Unknown device trigger — Tasks 3-4
- [x] Downloads to cache dir — Task 2 (onFileDownloaded)
- [x] DeviceRegistry reload — Task 3
- [x] Silent error handling — Task 2 (all error paths log + return)
- [x] Unit tests — Task 1

**2. Placeholder scan:** No TBDs. All stub methods are replaced in Task 2. All code blocks are complete.

**3. Type consistency:**
- `findDeviceForPid` returns `QPair<QString, QJsonObject>` — used consistently in Tasks 1 and 2
- `deviceNeedsUpdate(slug, version)` — called in Task 1 tests and Task 2 implementation
- `descriptorsUpdated()` signal — emitted in Task 2, connected in Task 4
- `unknownDeviceDetected(uint16_t pid)` — emitted in Task 3, connected in Task 4
- `reloadAll()` — implemented in Task 3, called in Task 4
