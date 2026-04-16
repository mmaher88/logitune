# Community Device Database Fetch — Design Spec

**Goal:** On startup and when an unknown device is detected, check the `logitune-devices` GitHub repo for community-contributed device descriptors and download them to the local cache.

**Scope:** Phase 5 (partial) of issue #22. Covers the fetch/download logic only. Does NOT cover the wizard, submission flow, repo population, or UI notifications.

**Stacks on:** PR #23 (json-device-descriptors). DeviceRegistry already loads from `~/.cache/logitune/devices/` — this phase populates that directory from GitHub.

---

## Architecture

A new `DeviceFetcher` class handles all network communication with the community database. It's a standalone `QObject` decoupled from `DeviceRegistry` — it downloads files to disk and emits `descriptorsUpdated()`. DeviceRegistry connects to that signal and does a full reload of all three directories.

```
Startup:
  1. DeviceRegistry loads from disk (instant)
  2. App is fully usable
  3. DeviceFetcher::fetchManifest() runs async in background
  4. If manifest changed (ETag mismatch), check for new/updated descriptors
  5. Download new descriptors to ~/.cache/logitune/devices/
  6. Emit descriptorsUpdated() → DeviceRegistry reloads

Unknown device detected:
  1. DeviceManager detects PID not in registry
  2. DeviceFetcher::fetchForPid(pid) runs async
  3. Checks cached manifest for matching PID
  4. If no cached manifest or stale, fetches fresh manifest first
  5. If PID found, downloads that device's descriptor + images
  6. Emit descriptorsUpdated() → DeviceRegistry reloads → device matches
```

---

## DeviceFetcher Class

```cpp
class DeviceFetcher : public QObject {
    Q_OBJECT
public:
    explicit DeviceFetcher(QObject *parent = nullptr);

    // Trigger manifest check (non-blocking, async)
    void fetchManifest();

    // Trigger fetch for a specific PID (non-blocking, async)
    void fetchForPid(uint16_t pid);

signals:
    void descriptorsUpdated();

private:
    void onManifestReply(QNetworkReply *reply);
    void processManifest(const QJsonObject &manifest);
    void downloadDevice(const QString &slug, const QJsonObject &deviceInfo);
    void onFileDownloaded(const QString &slug, const QString &filename, QNetworkReply *reply);

    bool isCacheFresh() const;
    QString cachedEtag() const;
    void saveCachedEtag(const QString &etag);
    void saveCachedManifest(const QJsonObject &manifest);
    QJsonObject loadCachedManifest() const;

    QNetworkAccessManager m_nam;
    QString m_cacheDir;
    uint16_t m_pendingPid = 0;  // non-zero when fetchForPid triggered
    int m_pendingDownloads = 0;  // counter for parallel file downloads

    static constexpr int kCacheTtlSeconds = 3600;
};
```

---

## Network Protocol

### Manifest fetch (startup)

```
GET https://raw.githubusercontent.com/mmaher88/logitune-devices/main/manifest.json
If-None-Match: <cached ETag>

→ 304 Not Modified: nothing to do
→ 200 OK: parse manifest, compare versions, download new/updated devices
→ Network error: log warning, use cached manifest if available
```

### Manifest format

```json
{
  "version": 1,
  "devices": {
    "g502-hero": {
      "version": 1,
      "name": "G502 SE Hero",
      "pids": ["0xc08b"],
      "files": ["descriptor.json", "front.png", "side.png"]
    },
    "mx-vertical": {
      "version": 2,
      "name": "MX Vertical",
      "pids": ["0x407b", "0xb020"],
      "files": ["descriptor.json", "front.png", "side.png", "back.png"]
    }
  }
}
```

### Version comparison

For each device in the manifest:
1. Check if `~/.cache/logitune/devices/<slug>/descriptor.json` exists
2. If not, download all files for that device
3. If yes, read the cached descriptor's `"version"` field and compare to manifest version
4. If manifest version is higher, re-download all files

### File download

```
GET https://raw.githubusercontent.com/mmaher88/logitune-devices/main/<slug>/descriptor.json
GET https://raw.githubusercontent.com/mmaher88/logitune-devices/main/<slug>/front.png
GET https://raw.githubusercontent.com/mmaher88/logitune-devices/main/<slug>/side.png
```

Files are saved directly to `~/.cache/logitune/devices/<slug>/`. Downloads run in parallel via Qt's async networking. When all files for all new devices complete, emit `descriptorsUpdated()`.

---

## Cache Structure

```
~/.cache/logitune/
  devices/
    g502-hero/
      descriptor.json
      front.png
      side.png
    mx-vertical/
      descriptor.json
      front.png
      side.png
      back.png
  manifest.json          (cached copy of remote manifest)
  manifest-etag          (plain text file with the ETag value)
  manifest-timestamp     (ISO 8601 timestamp of last successful fetch)
```

---

## Fetch Timing

### On startup
1. Check `manifest-timestamp` — if less than 1 hour old, skip network entirely
2. If stale or missing, `fetchManifest()` runs async (non-blocking)
3. App is fully usable immediately with cached data

### On unknown device detected
1. DeviceManager detects a Logitech PID not in DeviceRegistry
2. Calls `DeviceFetcher::fetchForPid(pid)`
3. DeviceFetcher checks cached manifest for that PID
4. If found in cached manifest, downloads that device only
5. If not in cached manifest (or manifest is stale), fetches fresh manifest first, then checks
6. If PID not in manifest at all, no-op (device truly unsupported)

---

## ETag Caching

```
First fetch:
  Request:  GET manifest.json
  Response: 200 OK, ETag: "abc123", body: {...}
  Action:   Save ETag to manifest-etag, save body to manifest.json

Subsequent fetch (< 1 hour):
  Action:   Skip entirely (cache TTL not expired)

Subsequent fetch (> 1 hour):
  Request:  GET manifest.json, If-None-Match: "abc123"
  Response: 304 Not Modified
  Action:   Update manifest-timestamp, done

After repo update (maintainer adds new device):
  Request:  GET manifest.json, If-None-Match: "abc123"
  Response: 200 OK, ETag: "def456", body: {...new manifest...}
  Action:   Save new ETag, compare versions, download new devices
```

---

## Error Handling

All errors are silent (log + skip):
- Network timeout (10 second timeout per request)
- DNS failure
- HTTP 403/429 rate limit → log, skip, use cached
- Partial download failure (some files succeed, some fail) → skip that device, don't emit signal for it
- Corrupt JSON in manifest → log, use previously cached manifest
- Disk write failure → log, skip

The app must never crash, hang, or show an error dialog due to fetch failures.

---

## Integration Points

### AppController wiring

```cpp
// In AppController constructor or init:
m_deviceFetcher = new DeviceFetcher(this);

connect(m_deviceFetcher, &DeviceFetcher::descriptorsUpdated,
        this, [this]() {
    m_deviceRegistry.reloadAll();
    // Re-match current device if it was previously unknown
    if (m_deviceManager.deviceConnected() && !m_deviceManager.activeDevice())
        m_deviceManager.retryDeviceMatch();
});

// Trigger startup fetch
m_deviceFetcher->fetchManifest();
```

### DeviceManager → DeviceFetcher for unknown PIDs

```cpp
// In DeviceManager::enumerateAndSetup(), after descriptor lookup fails:
if (!m_activeDevice) {
    emit unknownDeviceDetected(m_devicePid);  // new signal
}

// AppController connects:
connect(&m_deviceManager, &DeviceManager::unknownDeviceDetected,
        m_deviceFetcher, &DeviceFetcher::fetchForPid);
```

### DeviceRegistry reload

Add a `reloadAll()` method to DeviceRegistry:
```cpp
void DeviceRegistry::reloadAll() {
    m_devices.clear();
    loadDirectory(systemDevicesDir());
    loadDirectory(cacheDevicesDir());
    loadDirectory(userDevicesDir());
}
```

---

## Files

### New files
- `src/core/DeviceFetcher.h` — class declaration
- `src/core/DeviceFetcher.cpp` — implementation
- `tests/test_device_fetcher.cpp` — unit tests with mock network

### Modified files
- `src/core/DeviceRegistry.h` — add `reloadAll()`
- `src/core/DeviceRegistry.cpp` — implement `reloadAll()`
- `src/core/DeviceManager.h` — add `unknownDeviceDetected` signal
- `src/core/DeviceManager.cpp` — emit signal when descriptor lookup fails
- `src/app/AppController.cpp` — wire DeviceFetcher, connect signals
- `src/core/CMakeLists.txt` — add DeviceFetcher.cpp
- `tests/CMakeLists.txt` — add test_device_fetcher.cpp

---

## Testing Strategy

### Unit tests (test_device_fetcher.cpp)

Testing network code requires mocking. Qt provides `QNetworkAccessManager` which can be subclassed for testing, but it's complex. Simpler approach: test the non-network parts directly and mock at the file level.

Tests:
- **CacheFreshness** — verify `isCacheFresh()` returns true within TTL, false after
- **EtagReadWrite** — save and load ETag from disk
- **ManifestParsing** — parse a manifest JSON, extract device info for a PID
- **VersionComparison** — given a manifest and a cached descriptor, correctly identify which need updates
- **DownloadPath** — verify correct cache directory paths are constructed
- **ManifestTimestamp** — verify timestamp is saved and read correctly

Integration testing of actual HTTP requests is deferred to manual testing against the real repo.

### Manual testing

- Run app with no cache → verify manifest downloaded and logged
- Run app again within 1 hour → verify no network request (cache TTL)
- Run app after 1 hour → verify conditional request (304 or 200)
- Run app with no network → verify graceful skip, app starts normally
- Connect unknown device with a community descriptor available → verify auto-download

---

## Out of Scope

- Creating the `logitune-devices` GitHub repo (manual)
- Populating the repo with device descriptors (Phase 2 extraction)
- "Submit to community" button in wizard (Phase 4)
- UI notifications for fetch status
- Settings toggle to disable fetching
- Proxy configuration
- Authentication (all requests are unauthenticated)
