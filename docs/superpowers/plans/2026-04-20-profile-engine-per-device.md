# ProfileEngine Per-Device Contexts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace ProfileEngine's hidden global context with per-device contexts keyed by `deviceSerial`, wire `DeviceModel::selectedChanged` to refresh the UI from the selected device's cached profile, and fix the bug where switching the carousel leaves `ButtonModel` displaying the first-seeded device's profile data (issue #77).

**Architecture:** Introduce `DeviceProfileContext` + `QHash<QString, DeviceProfileContext> m_byDevice` on `ProfileEngine`, migrate every public method and signal to take `deviceSerial` as the first argument, and split the work across three commits: (1) add the new API alongside a legacy shim so nothing breaks, (2) migrate every call site and remove the shim, (3) wire the new selection handler + add tests covering the original bug.

**Tech Stack:** Qt 6 (C++20), CMake, GTest. Existing code at `src/core/ProfileEngine.{h,cpp}`, `src/app/AppController.{h,cpp}`, `src/app/models/ProfileModel.{h,cpp}`, `tests/helpers/AppControllerFixture.h`, `tests/test_app_controller.cpp`, `tests/test_profile_engine.cpp`.

---

## File Structure

### Files modified

- `src/core/ProfileEngine.h` — add `DeviceProfileContext`, `m_byDevice`, serial-aware public API and signals. Remove legacy API in commit 2.
- `src/core/ProfileEngine.cpp` — implement the new API. Legacy wrappers delegate to `"legacy"` serial in commit 1; removed in commit 2.
- `src/app/AppController.h` — add `onSelectedDeviceChanged` slot. Update `onDisplayProfileChanged` signature.
- `src/app/AppController.cpp` — migrate ~40 call sites to pass `selectedDevice()->deviceSerial()`. Add selection handler. Re-wire `ProfileModel::profileAdded` / `profileRemoved` through lambdas.
- `tests/helpers/AppControllerFixture.h` — replace direct `m_currentDevice` assignment + `setDeviceConfigDir` calls with `registerDevice` + `onPhysicalDeviceAdded`. Helpers gain internal serial. Add `addMockDevice` helper for multi-device tests.
- `tests/test_app_controller.cpp` — three new tests exercising multi-device selection.
- `tests/test_profile_engine.cpp` — three new tests exercising serial-aware behavior.

### Conventions

- The word **"serial"** throughout this plan means `PhysicalDevice::deviceSerial()` — stable per-device string identifier that survives transport reconnects.
- `m_profileEngine` is the `ProfileEngine` member on `AppController`.
- The fixture uses `mock-serial` as the serial for its single mock device, and `mock-serial-B` for the second one in multi-device tests.

---

## Commit 1: Introduce per-device contexts alongside legacy API

**Goal:** Add `DeviceProfileContext`, `m_byDevice`, serial-aware API, and serial-bearing signals. Keep the legacy single-context API working by delegating to a `"legacy"` auto-registered serial. All existing tests pass unchanged.

### Task 1.1: Define `DeviceProfileContext` struct and add `m_byDevice` storage

**Files:**
- Modify: `src/core/ProfileEngine.h`

- [ ] **Step 1: Add the struct definition above the class declaration**

Open `src/core/ProfileEngine.h` and add this struct immediately after the `ProfileDelta` struct definition (around line 35), before `class ProfileEngine`:

```cpp
struct DeviceProfileContext {
    QString configDir;                     // per-device profiles dir
    QMap<QString, Profile> cache;          // profileName -> Profile
    QMap<QString, QString> appBindings;    // wmClass -> profileName
    QString displayProfile;                // currently displayed
    QString hardwareProfile;               // currently active on hardware
};
```

- [ ] **Step 2: Add the `m_byDevice` member and `kLegacySerial` constant**

In the `private:` section of `ProfileEngine`, add this block immediately above the existing `m_configDir` member (around line 69):

```cpp
    // Per-device contexts. Key is PhysicalDevice::deviceSerial(). Lazy-
    // registered on first touch; persists for the life of the process.
    QHash<QString, DeviceProfileContext> m_byDevice;

    // Serial used by the legacy single-context API (setDeviceConfigDir,
    // non-serial-taking overloads). Allows the legacy API to keep working
    // during the migration and disappears in commit 2.
    static constexpr const char *kLegacySerial = "legacy";

    // Lazy-registers a context if absent. Returns a reference to the
    // live context in m_byDevice.
    DeviceProfileContext& ctx(const QString &serial);
    const DeviceProfileContext& ctx(const QString &serial) const;
```

Also change the `#include <QMap>` line to additionally include `<QHash>`:

```cpp
#include <QMap>
#include <QHash>
```

- [ ] **Step 3: Build to verify the header compiles**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: build succeeds; no use of `m_byDevice` yet so nothing breaks.

- [ ] **Step 4: Commit**

```bash
git add src/core/ProfileEngine.h
git commit -m "refactor(profile): add DeviceProfileContext struct skeleton

First step in migrating ProfileEngine away from its hidden single-device
context. Follow-up commits populate the new API, migrate call sites,
and wire the selection handler."
```

### Task 1.2: Implement the `ctx()` helper and `registerDevice`

**Files:**
- Modify: `src/core/ProfileEngine.h`, `src/core/ProfileEngine.cpp`

- [ ] **Step 1: Declare `registerDevice` and `hasDevice` in the header**

Open `src/core/ProfileEngine.h`. In the `public:` section, immediately above the `// --- Profile cache (Task 1) ---` comment (around line 55), add:

```cpp
    // Per-device context management
    void registerDevice(const QString &serial, const QString &configDir);
    bool hasDevice(const QString &serial) const;
```

- [ ] **Step 2: Implement `ctx()` and `registerDevice` in the .cpp**

Open `src/core/ProfileEngine.cpp`. Append the following after the existing `appBindingsPath()` method (near end of file, around line 345):

```cpp
// ---------------------------------------------------------------------------
// Per-device contexts
// ---------------------------------------------------------------------------

DeviceProfileContext& ProfileEngine::ctx(const QString &serial)
{
    if (!m_byDevice.contains(serial)) {
        qCWarning(lcProfile)
            << "ProfileEngine: lazy-registering unknown device" << serial;
        const QString defaultDir = QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation)
            + "/devices/" + serial + "/profiles";
        registerDevice(serial, defaultDir);
    }
    return m_byDevice[serial];
}

const DeviceProfileContext& ProfileEngine::ctx(const QString &serial) const
{
    // Const variant: if absent, return a shared empty sentinel. Callers on
    // the const path are read-only and should tolerate empty contexts for
    // unknown serials.
    static const DeviceProfileContext empty{};
    auto it = m_byDevice.constFind(serial);
    if (it == m_byDevice.constEnd())
        return empty;
    return *it;
}

void ProfileEngine::registerDevice(const QString &serial, const QString &configDir)
{
    DeviceProfileContext &c = m_byDevice[serial];
    c.configDir = configDir;
    c.cache.clear();
    c.appBindings.clear();

    QDir d(configDir);
    if (!d.exists())
        QDir().mkpath(configDir);

    if (d.exists()) {
        for (const auto &f : d.entryList({"*.conf"}, QDir::Files)) {
            if (f == "app-bindings.conf") continue;
            QString name = QFileInfo(f).baseName();
            c.cache[name] = loadProfile(d.filePath(f));
        }
    }

    const QString bindingsFile = configDir + "/app-bindings.conf";
    if (QFileInfo::exists(bindingsFile))
        c.appBindings = loadAppBindings(bindingsFile);
}

bool ProfileEngine::hasDevice(const QString &serial) const
{
    return m_byDevice.contains(serial);
}
```

Also add `#include <QStandardPaths>` near the top of the file if it is not already present (check with `grep QStandardPaths src/core/ProfileEngine.cpp` — the existing file does not include it yet; add it alongside the other `#include <Q...>` lines).

- [ ] **Step 3: Build and run existing tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: 565 tests pass, no regressions (the new methods have no callers yet).

- [ ] **Step 4: Commit**

```bash
git add src/core/ProfileEngine.h src/core/ProfileEngine.cpp
git commit -m "refactor(profile): add registerDevice and ctx() helpers

Lazy-register fallback emits qCWarning so accidental orderings during
device add are visible in logs; real call sites in commit 2 will
always register explicitly from onPhysicalDeviceAdded."
```

### Task 1.3: Add serial-aware read methods

**Files:**
- Modify: `src/core/ProfileEngine.h`, `src/core/ProfileEngine.cpp`

- [ ] **Step 1: Declare serial-aware read methods in the header**

In `ProfileEngine.h`, within the `public:` section, after `bool hasDevice(...)` from the previous task, add:

```cpp
    // Read methods (serial-aware)
    Profile& cachedProfile(const QString &serial, const QString &name);
    QStringList profileNames(const QString &serial) const;
    QString displayProfile(const QString &serial) const;
    QString hardwareProfile(const QString &serial) const;
    QString profileForApp(const QString &serial, const QString &wmClass) const;
```

The existing single-arg variants stay for now — they become thin wrappers in Task 1.5.

- [ ] **Step 2: Implement the read methods in the .cpp**

In `ProfileEngine.cpp`, append after `registerDevice` from the previous task:

```cpp
Profile& ProfileEngine::cachedProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (!c.cache.contains(name)) {
        c.cache[name] = Profile{};
        c.cache[name].name = name;
    }
    return c.cache[name];
}

QStringList ProfileEngine::profileNames(const QString &serial) const
{
    const DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty())
        return {};
    QDir dir(c.configDir);
    const QStringList files = dir.entryList({"*.conf"}, QDir::Files);
    QStringList names;
    names.reserve(files.size());
    for (const QString &f : files) {
        const QString base = QFileInfo(f).baseName();
        if (base != QLatin1String("app-bindings"))
            names << base;
    }
    return names;
}

QString ProfileEngine::displayProfile(const QString &serial) const
{
    return ctx(serial).displayProfile;
}

QString ProfileEngine::hardwareProfile(const QString &serial) const
{
    return ctx(serial).hardwareProfile;
}

QString ProfileEngine::profileForApp(const QString &serial, const QString &wmClass) const
{
    const DeviceProfileContext &c = ctx(serial);
    for (auto it = c.appBindings.cbegin(); it != c.appBindings.cend(); ++it) {
        if (it.key().compare(wmClass, Qt::CaseInsensitive) == 0)
            return it.value();
    }
    return QStringLiteral("default");
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
git add src/core/ProfileEngine.h src/core/ProfileEngine.cpp
git commit -m "refactor(profile): add serial-aware read methods

cachedProfile, profileNames, displayProfile, hardwareProfile,
profileForApp now accept a device serial. Existing single-arg
variants still work; they become thin wrappers over the serial
variants in a follow-up commit."
```

### Task 1.4: Add serial-aware write methods and serial-bearing signals

**Files:**
- Modify: `src/core/ProfileEngine.h`, `src/core/ProfileEngine.cpp`

- [ ] **Step 1: Declare serial-aware write methods and new signals**

In `ProfileEngine.h`, after the serial-aware read declarations, add:

```cpp
    // Write methods (serial-aware)
    void setDisplayProfile(const QString &serial, const QString &name);
    void setHardwareProfile(const QString &serial, const QString &name);
    void saveProfileToDisk(const QString &serial, const QString &name);
    void createProfileForApp(const QString &serial,
                             const QString &wmClass,
                             const QString &profileName);
    void removeAppProfile(const QString &serial, const QString &wmClass);
```

In the `signals:` section, immediately after the existing `hardwareProfileChanged(const Profile &)` signal (around line 66), add:

```cpp
    // Per-device variants. Both legacy and new signals fire during commit 1;
    // the legacy ones are removed in commit 2.
    void deviceDisplayProfileChanged(const QString &serial, const Profile &profile);
    void deviceHardwareProfileChanged(const QString &serial, const Profile &profile);
```

- [ ] **Step 2: Implement the write methods in the .cpp**

Append to `ProfileEngine.cpp` after the read implementations:

```cpp
void ProfileEngine::setDisplayProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.displayProfile == name) return;
    c.displayProfile = name;
    emit deviceDisplayProfileChanged(serial, cachedProfile(serial, name));
    if (serial == QLatin1String(kLegacySerial))
        emit displayProfileChanged(cachedProfile(serial, name));
}

void ProfileEngine::setHardwareProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.hardwareProfile == name) return;
    c.hardwareProfile = name;
    emit deviceHardwareProfileChanged(serial, cachedProfile(serial, name));
    if (serial == QLatin1String(kLegacySerial))
        emit hardwareProfileChanged(cachedProfile(serial, name));
}

void ProfileEngine::saveProfileToDisk(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (!c.cache.contains(name) || c.configDir.isEmpty()) return;
    saveProfile(c.configDir + "/" + name + ".conf", c.cache[name]);
}

void ProfileEngine::createProfileForApp(const QString &serial,
                                        const QString &wmClass,
                                        const QString &profileName)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty() || wmClass.isEmpty() || profileName.isEmpty())
        return;
    if (!c.cache.contains(profileName)) {
        c.cache[profileName] = c.cache.value(QStringLiteral("default"));
        c.cache[profileName].name = profileName;
        saveProfile(c.configDir + "/" + profileName + ".conf",
                    c.cache[profileName]);
    }
    c.appBindings[wmClass] = profileName;
    saveAppBindings(c.configDir + "/app-bindings.conf", c.appBindings);
}

void ProfileEngine::removeAppProfile(const QString &serial, const QString &wmClass)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.configDir.isEmpty() || wmClass.isEmpty()) return;

    const QString profileName = c.appBindings.value(wmClass);
    if (profileName.isEmpty()) return;

    QFile::remove(c.configDir + "/" + profileName + ".conf");
    c.cache.remove(profileName);
    c.appBindings.remove(wmClass);
    saveAppBindings(c.configDir + "/app-bindings.conf", c.appBindings);

    if (c.displayProfile == profileName)
        setDisplayProfile(serial, QStringLiteral("default"));
    if (c.hardwareProfile == profileName)
        setHardwareProfile(serial, QStringLiteral("default"));

    qCDebug(lcProfile) << "removed profile" << profileName
                       << "for app" << wmClass << "on device" << serial;
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: builds clean (still no callers of the new API).

- [ ] **Step 4: Commit**

```bash
git add src/core/ProfileEngine.h src/core/ProfileEngine.cpp
git commit -m "refactor(profile): add serial-aware write methods and signals

Emit both the legacy and the new serial-bearing displayProfileChanged /
hardwareProfileChanged during the migration window. Legacy signal only
fires for the 'legacy' serial so the existing single-context flow stays
functional."
```

### Task 1.5: Make legacy single-arg methods delegate to `"legacy"` serial

**Files:**
- Modify: `src/core/ProfileEngine.h`, `src/core/ProfileEngine.cpp`

- [ ] **Step 1: Replace `setDeviceConfigDir` body to call `registerDevice`**

In `ProfileEngine.cpp`, replace the existing `setDeviceConfigDir` implementation (lines ~197-215) with:

```cpp
void ProfileEngine::setDeviceConfigDir(const QString &dir)
{
    // Legacy entry point. Route through the per-device API with the
    // kLegacySerial context so existing callers keep working.
    registerDevice(QLatin1String(kLegacySerial), dir);
    m_configDir = dir;  // keep the legacy field in sync for old readers
}
```

- [ ] **Step 2: Rewrite the legacy single-arg methods as wrappers**

Replace the existing bodies of `cachedProfile(name)`, `profileNames()`, `displayProfile()`, `hardwareProfile()`, `setDisplayProfile(name)`, `setHardwareProfile(name)`, `saveProfileToDisk(name)`, `createProfileForApp(wmClass, name)`, `removeAppProfile(wmClass)`, and `profileForApp(wmClass)` in `ProfileEngine.cpp` with wrappers that delegate to the serial-taking variant using `kLegacySerial`:

```cpp
QStringList ProfileEngine::profileNames() const
{
    return profileNames(QLatin1String(kLegacySerial));
}

Profile& ProfileEngine::cachedProfile(const QString &name)
{
    return cachedProfile(QLatin1String(kLegacySerial), name);
}

QString ProfileEngine::displayProfile() const
{
    return displayProfile(QLatin1String(kLegacySerial));
}

QString ProfileEngine::hardwareProfile() const
{
    return hardwareProfile(QLatin1String(kLegacySerial));
}

void ProfileEngine::setDisplayProfile(const QString &name)
{
    setDisplayProfile(QLatin1String(kLegacySerial), name);
}

void ProfileEngine::setHardwareProfile(const QString &name)
{
    setHardwareProfile(QLatin1String(kLegacySerial), name);
}

void ProfileEngine::saveProfileToDisk(const QString &name)
{
    saveProfileToDisk(QLatin1String(kLegacySerial), name);
}

void ProfileEngine::createProfileForApp(const QString &wmClass,
                                        const QString &profileName)
{
    createProfileForApp(QLatin1String(kLegacySerial), wmClass, profileName);
}

void ProfileEngine::removeAppProfile(const QString &wmClass)
{
    removeAppProfile(QLatin1String(kLegacySerial), wmClass);
}

QString ProfileEngine::profileForApp(const QString &wmClass) const
{
    return profileForApp(QLatin1String(kLegacySerial), wmClass);
}
```

Delete the old helper `profilePath(const QString &name) const` — its callers are now all gone (the new write methods inline the path construction). Same for `appBindingsPath()`.

- [ ] **Step 3: Remove now-unused private members**

In `ProfileEngine.h` private section, the following fields are now dead:
- `QString m_configDir;`
- `QMap<QString, QString> m_appBindings;`
- `QMap<QString, Profile> m_cache;`
- `QString m_displayProfile;`
- `QString m_hardwareProfile;`
- `QFileSystemWatcher m_fileWatcher;` (was already unused)
- `bool m_selfWrite = false;` (was already unused)
- declarations of `profilePath` and `appBindingsPath`

**Keep `m_configDir`** because the legacy wrapper above still touches it for stray readers. Remove the rest.

Final `private:` section should look like:

```cpp
private:
    QString m_configDir;  // legacy mirror of kLegacySerial context.configDir

    QHash<QString, DeviceProfileContext> m_byDevice;

    static constexpr const char *kLegacySerial = "legacy";

    DeviceProfileContext& ctx(const QString &serial);
    const DeviceProfileContext& ctx(const QString &serial) const;
```

Remove the corresponding `#include <QFileSystemWatcher>` and `#include <QSettings>` if nothing else uses them (they were used by removed code).

- [ ] **Step 4: Build and run the full test suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3 && QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3`
Expected: 565/565 core + 72/72 QML pass. The legacy API now routes through per-device contexts, but functional behavior is unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/core/ProfileEngine.h src/core/ProfileEngine.cpp
git commit -m "refactor(profile): legacy API now delegates to 'legacy' serial

Single-context methods become thin wrappers over the serial-aware
variants using a constant 'legacy' serial. Functional behavior is
preserved; a follow-up commit migrates call sites and removes the
wrappers entirely."
```

### Task 1.6: Unit tests for serial-aware ProfileEngine API

**Files:**
- Modify: `tests/test_profile_engine.cpp`

- [ ] **Step 1: Write the three new ProfileEngine tests**

Append the following to the end of `tests/test_profile_engine.cpp`, before the closing namespace (if any) / end of file:

```cpp
// --- Per-device contexts ---

TEST(ProfileEngine, MultipleDevicesKeepSeparateCaches) {
    QTemporaryDir tmpA, tmpB;
    ASSERT_TRUE(tmpA.isValid());
    ASSERT_TRUE(tmpB.isValid());

    logitune::ProfileEngine eng;
    eng.registerDevice(QStringLiteral("A"), tmpA.path());
    eng.registerDevice(QStringLiteral("B"), tmpB.path());

    auto &pa = eng.cachedProfile(QStringLiteral("A"), QStringLiteral("default"));
    auto &pb = eng.cachedProfile(QStringLiteral("B"), QStringLiteral("default"));

    pa.dpi = 1234;
    pb.dpi = 5678;

    EXPECT_EQ(eng.cachedProfile(QStringLiteral("A"),
                                QStringLiteral("default")).dpi, 1234);
    EXPECT_EQ(eng.cachedProfile(QStringLiteral("B"),
                                QStringLiteral("default")).dpi, 5678);
}

TEST(ProfileEngine, UnknownDeviceLazyRegisters) {
    logitune::ProfileEngine eng;
    EXPECT_FALSE(eng.hasDevice(QStringLiteral("ghost")));

    auto &p = eng.cachedProfile(QStringLiteral("ghost"),
                                QStringLiteral("default"));
    EXPECT_EQ(p.dpi, 1000);                  // struct default
    EXPECT_EQ(p.name, QStringLiteral("default"));
    EXPECT_TRUE(eng.hasDevice(QStringLiteral("ghost")));
}

TEST(ProfileEngine, SetDisplayProfileScopedToDevice) {
    QTemporaryDir tmpA, tmpB;
    ASSERT_TRUE(tmpA.isValid());
    ASSERT_TRUE(tmpB.isValid());

    logitune::ProfileEngine eng;
    eng.registerDevice(QStringLiteral("A"), tmpA.path());
    eng.registerDevice(QStringLiteral("B"), tmpB.path());

    eng.setDisplayProfile(QStringLiteral("A"), QStringLiteral("chrome"));

    EXPECT_EQ(eng.displayProfile(QStringLiteral("A")),
              QStringLiteral("chrome"));
    EXPECT_EQ(eng.displayProfile(QStringLiteral("B")), QString());
}
```

If `tests/test_profile_engine.cpp` does not already include the required headers, add near the top:

```cpp
#include "ProfileEngine.h"
#include <QTemporaryDir>
```

- [ ] **Step 2: Run the new tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='ProfileEngine.*' 2>&1 | tail -10`
Expected: all three new tests pass. (Other existing `ButtonAction.*` tests in the same file continue to pass.)

- [ ] **Step 3: Run the full suite**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: 568/568 pass (3 new tests).

- [ ] **Step 4: Commit**

```bash
git add tests/test_profile_engine.cpp
git commit -m "test(profile): unit tests for per-device contexts

Covers: separate caches per serial, lazy-registration of unknown
serials, per-device display-profile scoping."
```

---

## Commit 2: Migrate call sites and remove legacy API

**Goal:** Update every `m_profileEngine.*` call in `AppController` and the `ProfileModel::profileAdded` / `profileRemoved` wiring to pass `selectedDevice()->deviceSerial()`. Rework the test fixture. Delete the legacy wrappers, `setDeviceConfigDir`, and the legacy signals. Existing test suite passes end-to-end with no behavioral change.

### Task 2.1: Add a helper for the selected device's serial

**Files:**
- Modify: `src/app/AppController.h`, `src/app/AppController.cpp`

- [ ] **Step 1: Declare the helper in the header**

In `src/app/AppController.h`, in the `private:` section near the other small helpers (around line 79), add:

```cpp
    QString selectedSerial() const;  // PhysicalDevice::deviceSerial() of the selected device, or empty
```

- [ ] **Step 2: Implement the helper**

In `src/app/AppController.cpp`, append after `selectedSession()` (find its definition with `grep -n "selectedSession" src/app/AppController.cpp`):

```cpp
QString AppController::selectedSerial() const
{
    auto *d = selectedDevice();
    return d ? d->deviceSerial() : QString();
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp
git commit -m "refactor(app): add selectedSerial() helper

Convenience for upcoming migration of ProfileEngine call sites that
need the currently-selected device's deviceSerial."
```

### Task 2.2: Migrate `setupProfileForDevice` to serial-aware API

**Files:**
- Modify: `src/app/AppController.cpp`

- [ ] **Step 1: Rewrite the body to use `registerDevice` and serial arguments**

In `src/app/AppController.cpp`, locate `setupProfileForDevice` (around line 228). The current body uses `m_profileEngine.setDeviceConfigDir(profilesDir)` and then later calls `setHardwareProfile(hwName)` / `setDisplayProfile(hwName)` / `cachedProfile(hwName)` with no serial.

Replace the function body with the serial-aware version. The full function after the edit:

```cpp
void AppController::setupProfileForDevice(PhysicalDevice *device)
{
    m_currentDevice = device->descriptor();

    const QString serial = device->deviceSerial();
    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    const QString profilesDir = configBase
        + QStringLiteral("/devices/") + serial
        + QStringLiteral("/profiles");

    QDir().mkpath(profilesDir);
    m_profileEngine.registerDevice(serial, profilesDir);
    qCDebug(lcApp) << "profile dir:" << profilesDir;

    const QString defaultConf = profilesDir + QStringLiteral("/default.conf");
    if (!QFile::exists(defaultConf)) {
        Profile seed;
        seed.name                = QStringLiteral("Default");
        seed.dpi                 = device->currentDPI();
        seed.smartShiftEnabled   = device->smartShiftEnabled();
        seed.smartShiftThreshold = device->smartShiftThreshold();
        seed.hiResScroll         = device->scrollHiRes();
        seed.scrollDirection     = device->scrollInvert()
            ? QStringLiteral("natural") : QStringLiteral("standard");
        seed.smoothScrolling     = !device->scrollRatchet();
        if (m_currentDevice) {
            const auto controls = m_currentDevice->controls();
            for (int i = 0;
                 i < static_cast<int>(controls.size()) &&
                 i < static_cast<int>(seed.buttons.size()); ++i) {
                const auto &ctrl = controls[i];
                if (ctrl.defaultActionType == "gesture-trigger")
                    seed.buttons[i] = {ButtonAction::GestureTrigger, {}};
                else if (ctrl.defaultActionType == "smartshift-toggle")
                    seed.buttons[i] = {ButtonAction::SmartShiftToggle, {}};
                else if (ctrl.defaultActionType == "dpi-cycle")
                    seed.buttons[i] = {ButtonAction::DpiCycle, {}};
            }
            const auto defaultGestures = m_currentDevice->defaultGestures();
            for (auto it = defaultGestures.begin();
                 it != defaultGestures.end(); ++it) {
                seed.gestures[it.key()] = it.value();
            }
        }
        ProfileEngine::saveProfile(defaultConf, seed);
        m_profileEngine.registerDevice(serial, profilesDir);  // reload cache
        qCDebug(lcApp) << "created default profile at" << defaultConf;
    }

    const QString bindingsFile = profilesDir + QStringLiteral("/app-bindings.conf");
    if (QFile::exists(bindingsFile)) {
        const auto bindings = ProfileEngine::loadAppBindings(bindingsFile);
        QMap<QString, QString> iconLookup;
        const auto apps = m_desktop->runningApplications();
        for (const auto &app : apps) {
            auto map = app.toMap();
            iconLookup[map[QStringLiteral("wmClass")].toString().toLower()]
                = map[QStringLiteral("icon")].toString();
        }
        for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
            QString icon = iconLookup.value(it.key().toLower());
            m_profileModel.restoreProfile(it.key(), it.value(), icon);
        }
    }

    QString hwName = m_profileEngine.hardwareProfile(serial);
    bool isFirstConnect = hwName.isEmpty();
    if (isFirstConnect) {
        hwName = QStringLiteral("default");
        m_profileModel.setHwActiveIndex(0);
        m_profileEngine.setHardwareProfile(serial, hwName);
        m_profileEngine.setDisplayProfile(serial, hwName);
    }

    Profile &p = m_profileEngine.cachedProfile(serial, hwName);
    qCDebug(lcApp) << "setupProfileForDevice: applying profile" << hwName
                   << "thumbWheelMode=" << p.thumbWheelMode;
    applyProfileToHardware(p);
}
```

- [ ] **Step 2: Build and run tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: 568/568 pass. The rewritten function still uses `deviceSerial` on PhysicalDevice, which is stable.

- [ ] **Step 3: Commit**

```bash
git add src/app/AppController.cpp
git commit -m "refactor(app): setupProfileForDevice uses serial-aware ProfileEngine

Calls registerDevice instead of setDeviceConfigDir and passes the
device serial to setHardwareProfile, setDisplayProfile, cachedProfile,
and hardwareProfile. Behavior unchanged."
```

### Task 2.3: Migrate the remaining `AppController` call sites

**Files:**
- Modify: `src/app/AppController.cpp`

- [ ] **Step 1: Update `onPhysicalDeviceAdded`'s transport-setup lambda**

Find the lambda connected to `PhysicalDevice::transportSetupComplete` (around line 196). The current body reads:

```cpp
m_currentDevice = device->descriptor();
Profile &p = m_profileEngine.cachedProfile(m_profileEngine.hardwareProfile());
qCDebug(lcApp) << "device transport ready, applying profile:"
                << m_profileEngine.hardwareProfile();
applyProfileToHardware(p);
```

Replace with:

```cpp
m_currentDevice = device->descriptor();
const QString serial = device->deviceSerial();
Profile &p = m_profileEngine.cachedProfile(serial,
                                           m_profileEngine.hardwareProfile(serial));
qCDebug(lcApp) << "device transport ready, applying profile:"
                << m_profileEngine.hardwareProfile(serial);
applyProfileToHardware(p);
```

- [ ] **Step 2: Update `onUserButtonChanged`**

Find the line `if (m_profileEngine.displayProfile() != m_profileEngine.hardwareProfile())` (around line 325) and replace with:

```cpp
const QString serial = selectedSerial();
if (serial.isEmpty()) return;
if (m_profileEngine.displayProfile(serial) != m_profileEngine.hardwareProfile(serial))
    return;
```

- [ ] **Step 3: Update `onTabSwitched`**

Find the body of `onTabSwitched` (around line 345). Replace:

```cpp
void AppController::onTabSwitched(const QString &profileName)
{
    m_profileEngine.setDisplayProfile(profileName);
}
```

with:

```cpp
void AppController::onTabSwitched(const QString &profileName)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;
    m_profileEngine.setDisplayProfile(serial, profileName);
}
```

- [ ] **Step 4: Update `onWindowFocusChanged`**

Find the block that reads `QString profileName = m_profileEngine.profileForApp(wmClass);` (around line 381). Replace the full block (through `applyProfileToHardware(p);`) with:

```cpp
const QString serial = selectedSerial();
if (serial.isEmpty()) return;

QString profileName = m_profileEngine.profileForApp(serial, wmClass);
if (profileName == m_profileEngine.hardwareProfile(serial))
    return;

Profile &p = m_profileEngine.cachedProfile(serial, profileName);
m_profileEngine.setHardwareProfile(serial, profileName);
applyProfileToHardware(p);
m_profileModel.setHwActiveByProfileName(profileName);
```

- [ ] **Step 5: Update `saveCurrentProfile`**

Find the function body (around line 514). Replace the entire function with:

```cpp
void AppController::saveCurrentProfile()
{
    const QString serial = selectedSerial();
    if (serial.isEmpty()) return;

    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;

    Profile &p = m_profileEngine.cachedProfile(serial, name);
    if (p.name.isEmpty()) p.name = name;

    if (m_currentDevice) {
        const auto controls = m_currentDevice->controls();
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (controls[i].controlId == 0) continue;
            if (static_cast<std::size_t>(i) < p.buttons.size())
                p.buttons[static_cast<std::size_t>(i)] = buttonEntryToAction(
                    m_buttonModel.actionTypeForButton(i),
                    m_buttonModel.actionNameForButton(i));
        }
    }

    for (const auto &dir : {"up", "down", "left", "right", "click"}) {
        QString ks = m_deviceModel.gestureKeystroke(dir);
        if (!ks.isEmpty())
            p.gestures[dir] = {ButtonAction::Keystroke, ks};
    }

    m_profileEngine.saveProfileToDisk(serial, name);
}
```

- [ ] **Step 6: Update the six `onXxxChangeRequested` handlers**

There are six handlers, all structured identically: `onDpiChangeRequested`, `onSmartShiftChangeRequested`, `onScrollConfigChangeRequested`, `onThumbWheelModeChangeRequested`, `onThumbWheelInvertChangeRequested`, and (if present) `applyProfileToHardware` helpers. Each starts with the pattern:

```cpp
QString name = m_profileEngine.displayProfile();
if (name.isEmpty()) return;
Profile &p = m_profileEngine.cachedProfile(name);
// ... modifies p
m_profileEngine.saveProfileToDisk(name);
if (name == m_profileEngine.hardwareProfile()) {
    // ... apply to hardware
}
```

In each handler, change the opening to:

```cpp
const QString serial = selectedSerial();
if (serial.isEmpty()) return;
const QString name = m_profileEngine.displayProfile(serial);
if (name.isEmpty()) return;
Profile &p = m_profileEngine.cachedProfile(serial, name);
```

And the final lines to:

```cpp
m_profileEngine.saveProfileToDisk(serial, name);
if (name == m_profileEngine.hardwareProfile(serial)) {
    // ... apply to hardware (body unchanged)
}
```

Apply this mechanical edit in all six handlers. Find them with:

```bash
grep -nE "^(void AppController::on(Dpi|SmartShift|ScrollConfig|ThumbWheelMode|ThumbWheelInvert))" src/app/AppController.cpp
```

- [ ] **Step 7: Update `onDivertedButtonPressed`**

Find `const Profile &hwProfile = m_profileEngine.cachedProfile(m_profileEngine.hardwareProfile());` (around line 638). Replace with:

```cpp
const QString serial = selectedSerial();
if (serial.isEmpty()) return;
const Profile &hwProfile = m_profileEngine.cachedProfile(
    serial, m_profileEngine.hardwareProfile(serial));
```

- [ ] **Step 8: Re-wire `ProfileModel::profileAdded` and `profileRemoved` through lambdas**

In `wireSignals` (around lines 137-140), replace:

```cpp
connect(&m_profileModel, &ProfileModel::profileAdded,
        &m_profileEngine, &ProfileEngine::createProfileForApp);
connect(&m_profileModel, &ProfileModel::profileRemoved,
        &m_profileEngine, &ProfileEngine::removeAppProfile);
```

with:

```cpp
connect(&m_profileModel, &ProfileModel::profileAdded, this,
        [this](const QString &wmClass, const QString &profileName) {
    const QString serial = selectedSerial();
    if (!serial.isEmpty())
        m_profileEngine.createProfileForApp(serial, wmClass, profileName);
});
connect(&m_profileModel, &ProfileModel::profileRemoved, this,
        [this](const QString &wmClass) {
    const QString serial = selectedSerial();
    if (!serial.isEmpty())
        m_profileEngine.removeAppProfile(serial, wmClass);
});
```

- [ ] **Step 9: Build and run tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`

Expected: the fixture-based tests may now fail because the fixture still uses the legacy API. That's expected and fixed in Task 2.4. If there are compile errors, they point to call sites this task missed — fix them the same way (add `serial` as the first argument) and re-run.

- [ ] **Step 10: Commit**

```bash
git add src/app/AppController.cpp src/app/AppController.h
git commit -m "refactor(app): migrate AppController to serial-aware ProfileEngine

Routes every cachedProfile / displayProfile / hardwareProfile /
setDisplayProfile / setHardwareProfile / saveProfileToDisk /
profileForApp call through selectedSerial(). ProfileModel signals are
now mediated by AppController lambdas so they can inject the serial.

Legacy single-context ProfileEngine wrappers are untouched and may
still be called by fixtures; they are removed after the fixture
rework in the next commit."
```

### Task 2.4: Rework `AppControllerFixture` to use the new API

**Files:**
- Modify: `tests/helpers/AppControllerFixture.h`

- [ ] **Step 1: Replace the fixture SetUp body**

In `tests/helpers/AppControllerFixture.h`, locate the `SetUp()` method (around line 23). Replace the block from line 47 (`m_ctrl->m_profileEngine.setDeviceConfigDir(m_profilesDir);`) through line 70 (`m_ctrl->m_profileEngine.setHardwareProfile(QStringLiteral("default"));`) with:

```cpp
        const QString kSerial = QStringLiteral("mock-serial");

        // Pre-register the mock device with the fixture's temp profile dir
        // BEFORE onPhysicalDeviceAdded runs. setupProfileForDevice would
        // otherwise point the engine at AppConfigLocation, bypassing the
        // temp dir the fixture wrote default.conf into.
        m_ctrl->m_profileEngine.registerDevice(kSerial, m_profilesDir);

        m_device.setupMxControls();

        // Create a mock DeviceSession wrapped in a PhysicalDevice. Mark
        // the session connected so DeviceModel shows its row.
        auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_session = new DeviceSession(std::move(mockHidraw), 0xFF, "Bluetooth",
                                       nullptr, m_ctrl.get());
        m_session->m_connected = true;
        m_session->m_deviceName = QStringLiteral("Mock Device");

        m_physicalDevice = new PhysicalDevice(kSerial, m_ctrl.get());
        m_physicalDevice->attachTransport(m_session);

        // Drive through the normal device-added flow. This sets
        // m_currentDevice, registers the device again (idempotent), and
        // sets displayProfile + hardwareProfile to "default" which
        // triggers onDisplayProfileChanged -> the UI is populated from
        // the already-loaded cached profile.
        m_ctrl->onPhysicalDeviceAdded(m_physicalDevice);
```

Also remove the line `m_ctrl->m_profileEngine.setDeviceConfigDir(m_profilesDir);` from earlier in SetUp if it survives.

- [ ] **Step 2: Update fixture helpers to pass serial internally**

The helpers `createAppProfile`, `setProfileButton`, and `setProfileGesture` all currently operate on the legacy single-context engine. Update each to use `kSerial`:

In `createAppProfile` (the first overload, around line 86), change:

```cpp
Profile p = m_ctrl->m_profileEngine.cachedProfile(QStringLiteral("default"));
// ...
m_ctrl->m_profileEngine.createProfileForApp(wmClass, profileName);
```

to:

```cpp
const QString kSerial = QStringLiteral("mock-serial");
Profile p = m_ctrl->m_profileEngine.cachedProfile(
    kSerial, QStringLiteral("default"));
// ...
m_ctrl->m_profileEngine.createProfileForApp(kSerial, wmClass, profileName);
```

Apply the same pattern to the second `createAppProfile` overload and to `setProfileButton`, `setProfileGesture`. In each helper, the first line of the body should define `const QString kSerial = QStringLiteral("mock-serial");` and every `m_profileEngine.*` call gains `kSerial` as its first argument.

- [ ] **Step 3: Build and run tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`

Expected: 568/568 pass. Any compile error points to a fixture helper still using the legacy API.

- [ ] **Step 4: Commit**

```bash
git add tests/helpers/AppControllerFixture.h
git commit -m "test(fixture): AppControllerFixture uses serial-aware ProfileEngine

Fixture registers the mock device with the engine explicitly, then
drives onPhysicalDeviceAdded to go through the same flow production
uses. Replaces the previous direct m_currentDevice assignment and
setDeviceConfigDir call.

Helper methods now pass 'mock-serial' into the engine internally so
existing test call sites are unchanged."
```

### Task 2.5: Migrate `onDisplayProfileChanged` to the serial-bearing signal

**Files:**
- Modify: `src/app/AppController.h`, `src/app/AppController.cpp`

- [ ] **Step 1: Update the slot signature in the header**

In `src/app/AppController.h`, find `void onDisplayProfileChanged(const Profile &profile);` (around line 60). Replace with:

```cpp
    void onDisplayProfileChanged(const QString &serial, const Profile &profile);
```

- [ ] **Step 2: Update the slot implementation**

In `AppController.cpp`, find `void AppController::onDisplayProfileChanged(const Profile &profile)` (around line 382). Replace with:

```cpp
void AppController::onDisplayProfileChanged(const QString &serial, const Profile &profile)
{
    if (serial != selectedSerial())
        return;

    m_deviceModel.setActiveProfileName(profile.name);

    m_deviceModel.setDisplayValues(
        profile.dpi, profile.smartShiftEnabled, profile.smartShiftThreshold,
        profile.hiResScroll, profile.scrollDirection == "natural",
        profile.thumbWheelMode, profile.thumbWheelInvert);

    restoreButtonModelFromProfile(profile);
}
```

- [ ] **Step 3: Update the wiring to connect the new signal**

In `AppController::wireSignals` (around line 121), find:

```cpp
connect(&m_profileEngine, &ProfileEngine::displayProfileChanged,
        this, &AppController::onDisplayProfileChanged);
```

Replace with:

```cpp
connect(&m_profileEngine, &ProfileEngine::deviceDisplayProfileChanged,
        this, &AppController::onDisplayProfileChanged);
```

- [ ] **Step 4: Build and run tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: 568/568 pass. The filter is now active but there's only one device in the fixture, so `serial != selectedSerial()` never triggers.

- [ ] **Step 5: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp
git commit -m "refactor(app): onDisplayProfileChanged subscribes to per-device signal

Uses deviceDisplayProfileChanged which carries the source device
serial, and filters out changes for non-selected devices. Legacy
displayProfileChanged signal is no longer consumed and is removed
in the next commit."
```

### Task 2.6: Delete legacy single-context API and signals

**Files:**
- Modify: `src/core/ProfileEngine.h`, `src/core/ProfileEngine.cpp`

- [ ] **Step 1: Delete the legacy signal declarations**

In `ProfileEngine.h`, delete the two legacy signals:

```cpp
    void displayProfileChanged(const Profile &profile);       // DELETE
    void hardwareProfileChanged(const Profile &profile);      // DELETE
```

The `deviceDisplayProfileChanged` / `deviceHardwareProfileChanged` signals remain.

- [ ] **Step 2: Delete the legacy single-arg method declarations**

In `ProfileEngine.h`, delete these method declarations (all from the migration-era wrappers):

```cpp
    void setDeviceConfigDir(const QString &dir);
    QStringList profileNames() const;
    void createProfileForApp(const QString &wmClass, const QString &profileName);
    void removeAppProfile(const QString &wmClass);
    Profile& cachedProfile(const QString &name);
    QString displayProfile() const;
    QString hardwareProfile() const;
    void setDisplayProfile(const QString &name);
    void setHardwareProfile(const QString &name);
    void saveProfileToDisk(const QString &name);
    QString profileForApp(const QString &wmClass) const;
```

- [ ] **Step 3: Delete the corresponding implementations in the .cpp**

In `ProfileEngine.cpp`, delete the implementation bodies of all eleven methods listed above. Also delete the `m_configDir` field and any remaining `kLegacySerial`-emitting branch in `setDisplayProfile(serial, name)` / `setHardwareProfile(serial, name)` — the conditional legacy signal emission goes away with the legacy signals.

Final bodies of `setDisplayProfile` and `setHardwareProfile` after this task:

```cpp
void ProfileEngine::setDisplayProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.displayProfile == name) return;
    c.displayProfile = name;
    emit deviceDisplayProfileChanged(serial, cachedProfile(serial, name));
}

void ProfileEngine::setHardwareProfile(const QString &serial, const QString &name)
{
    DeviceProfileContext &c = ctx(serial);
    if (c.hardwareProfile == name) return;
    c.hardwareProfile = name;
    emit deviceHardwareProfileChanged(serial, cachedProfile(serial, name));
}
```

Also remove the `m_configDir` line from the private section and from the constructor / `setDeviceConfigDir` body (both of which are now deleted).

Remove the `kLegacySerial` constant if no code references it anymore.

- [ ] **Step 4: Build and run the full suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3 && QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3`
Expected: 568/568 core + 72/72 QML. Any compile error points to a call site still using the legacy API; fix it to use `selectedSerial()` or the fixture's `kSerial`.

- [ ] **Step 5: Commit**

```bash
git add src/core/ProfileEngine.h src/core/ProfileEngine.cpp
git commit -m "refactor(profile): remove legacy single-context API and signals

ProfileEngine is now exclusively serial-aware. setDeviceConfigDir is
replaced by registerDevice; displayProfileChanged(Profile) is replaced
by deviceDisplayProfileChanged(serial, Profile); same for the
hardware variant.

All call sites migrated in the previous commits; this one drops the
scaffolding."
```

---

## Commit 3: Wire the selection handler and add integration tests

**Goal:** `DeviceModel::selectedChanged` now updates the UI from the newly-selected device's cached profile. Add the six tests from the spec.

### Task 3.1: Add `onSelectedDeviceChanged` slot

**Files:**
- Modify: `src/app/AppController.h`, `src/app/AppController.cpp`

- [ ] **Step 1: Declare the slot in the header**

In `src/app/AppController.h`, add to the `private slots:` section (after the existing slots, around line 70):

```cpp
    void onSelectedDeviceChanged();
```

- [ ] **Step 2: Implement the slot**

In `AppController.cpp`, insert after `onPhysicalDeviceRemoved` (around line 225):

```cpp
// Carousel selection changed. Refresh the UI from the newly-selected
// device's cached profile. No file I/O, no seeding, no hardware apply —
// one-time device provisioning happens in onPhysicalDeviceAdded.
void AppController::onSelectedDeviceChanged()
{
    auto *device = selectedDevice();
    if (!device) return;

    m_currentDevice = device->descriptor();

    const QString serial = device->deviceSerial();
    const QString name = m_profileEngine.displayProfile(serial);
    if (name.isEmpty()) return;  // device not yet fully set up

    const Profile &p = m_profileEngine.cachedProfile(serial, name);
    restoreButtonModelFromProfile(p);
    pushDisplayValues(p);
}
```

- [ ] **Step 3: Wire the slot in `wireSignals`**

In `AppController::wireSignals`, immediately after the `deviceDisplayProfileChanged` connection, add:

```cpp
connect(&m_deviceModel, &DeviceModel::selectedChanged,
        this, &AppController::onSelectedDeviceChanged);
```

- [ ] **Step 4: Build and run the full suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: 568/568 pass. Fixture flow: `onPhysicalDeviceAdded` calls `m_deviceModel.addPhysicalDevice` which auto-selects and fires `selectedChanged`; the new handler runs, reads the just-registered profile, and pushes it to `ButtonModel` + `DeviceModel`. Functional result matches the previous behavior where `onDisplayProfileChanged` did the push via `setDisplayProfile("default")`.

- [ ] **Step 5: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp
git commit -m "feat(app): refresh UI on carousel device selection change

New onSelectedDeviceChanged slot reads the selected device's cached
display profile and pushes it to ButtonModel + DeviceModel display
values. No file I/O or hardware apply — pure read-and-refresh,
idempotent over selection state.

Fixes the case where switching devices in the carousel (either in
--simulate-all or with multiple real devices connected) left the UI
showing the first-seeded device's profile data."
```

### Task 3.2: Add `addMockDevice` helper to the fixture

**Files:**
- Modify: `tests/helpers/AppControllerFixture.h`

- [ ] **Step 1: Declare and implement the helper**

In `tests/helpers/AppControllerFixture.h`, add the following member function in the `protected:` section after the existing helpers (e.g., after `thumbWheel`, around line 184):

```cpp
    // Add a second mock device and register it through the normal flow.
    // Returns the new PhysicalDevice for test-level manipulation.
    PhysicalDevice* addMockDevice(const QString &serialSuffix,
                                  int seedDpi = 1000) {
        const QString serial = QStringLiteral("mock-serial-") + serialSuffix;
        const QString devProfilesDir = m_tmpDir.path()
            + "/" + serial + "/profiles";
        QDir().mkpath(devProfilesDir);

        Profile seed;
        seed.name = QStringLiteral("Default");
        seed.dpi  = seedDpi;
        ProfileEngine::saveProfile(
            devProfilesDir + "/default.conf", seed);

        m_ctrl->m_profileEngine.registerDevice(serial, devProfilesDir);

        auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        auto *session = new DeviceSession(std::move(mockHidraw), 0xFF,
                                          "Bluetooth", nullptr, m_ctrl.get());
        session->m_connected = true;
        session->m_deviceName = QStringLiteral("Mock Device ") + serialSuffix;

        auto *device = new PhysicalDevice(serial, m_ctrl.get());
        device->attachTransport(session);

        m_ctrl->onPhysicalDeviceAdded(device);
        return device;
    }
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: builds clean; no tests use the helper yet.

- [ ] **Step 3: Commit**

```bash
git add tests/helpers/AppControllerFixture.h
git commit -m "test(fixture): addMockDevice helper for multi-device tests

Creates a second PhysicalDevice with an isolated profile dir under
the fixture's temp root, seeds a default.conf with the given DPI,
and drives it through onPhysicalDeviceAdded. Returns the pointer so
tests can manipulate per-device state."
```

### Task 3.3: Integration test — carousel switch swaps ButtonModel

**Files:**
- Modify: `tests/test_app_controller.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_app_controller.cpp`:

```cpp
TEST_F(AppControllerFixture, CarouselSwitchSwapsButtonModel) {
    // The existing fixture sets up device "mock-serial" with a default
    // profile. Set its button 3 action explicitly.
    setProfileButton("default", 3,
                     {ButtonAction::Keystroke, QStringLiteral("Alt+Left")});
    // Force the UI to reflect it.
    m_ctrl->m_deviceModel.setSelectedIndex(0);

    // Add a second device with a DIFFERENT button 3 action on disk.
    auto *secondary = addMockDevice(QStringLiteral("B"));
    {
        const QString serialB = QStringLiteral("mock-serial-B");
        Profile &pB = m_ctrl->m_profileEngine.cachedProfile(
            serialB, QStringLiteral("default"));
        pB.buttons[3] = {ButtonAction::Media, QStringLiteral("Play")};
        m_ctrl->m_profileEngine.saveProfileToDisk(
            serialB, QStringLiteral("default"));
    }

    // Sanity: ButtonModel currently reflects the first device.
    EXPECT_EQ(buttonModel().actionTypeForButton(3),
              QStringLiteral("keystroke"));

    // Switch carousel to the second device.
    const int idxB = m_ctrl->m_deviceModel.devices().indexOf(secondary);
    ASSERT_GE(idxB, 0);
    m_ctrl->m_deviceModel.setSelectedIndex(idxB);

    // ButtonModel now reflects the second device.
    EXPECT_EQ(buttonModel().actionTypeForButton(3),
              QStringLiteral("media-controls"));
}
```

- [ ] **Step 2: Run the test to verify it passes**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='AppControllerFixture.CarouselSwitchSwapsButtonModel' 2>&1 | tail -10`
Expected: PASS. (The selection handler from Task 3.1 is already in place.)

- [ ] **Step 3: Commit**

```bash
git add tests/test_app_controller.cpp
git commit -m "test(app): carousel switch swaps ButtonModel data

Covers issue #77: two mock devices with different button 3 actions;
setSelectedIndex pivots ButtonModel from device A's action to
device B's."
```

### Task 3.4: Integration test — carousel switch swaps display values

**Files:**
- Modify: `tests/test_app_controller.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_app_controller.cpp`:

```cpp
TEST_F(AppControllerFixture, CarouselSwitchSwapsDisplayValues) {
    // Fixture device has DPI 1000 (seeded in SetUp).
    m_ctrl->m_deviceModel.setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);

    auto *secondary = addMockDevice(QStringLiteral("B"), /*seedDpi=*/2500);

    const int idxB = m_ctrl->m_deviceModel.devices().indexOf(secondary);
    ASSERT_GE(idxB, 0);
    m_ctrl->m_deviceModel.setSelectedIndex(idxB);

    EXPECT_EQ(deviceModel().currentDPI(), 2500);

    m_ctrl->m_deviceModel.setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
}
```

- [ ] **Step 2: Run**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='AppControllerFixture.CarouselSwitchSwapsDisplayValues' 2>&1 | tail -10`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/test_app_controller.cpp
git commit -m "test(app): carousel switch swaps DeviceModel display values

Two devices with different seed DPIs. setSelectedIndex pivots the
DeviceModel's currentDPI between the two values in both directions."
```

### Task 3.5: Integration test — display profile change ignored for non-selected device

**Files:**
- Modify: `tests/test_app_controller.cpp`

- [ ] **Step 1: Write the test**

Append to `tests/test_app_controller.cpp`:

```cpp
TEST_F(AppControllerFixture, DisplayProfileChangedIgnoredForNonSelectedDevice) {
    // Fixture selects device "mock-serial" (index 0).
    m_ctrl->m_deviceModel.setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);

    // Add a second device without switching to it.
    addMockDevice(QStringLiteral("B"), /*seedDpi=*/2500);
    EXPECT_EQ(m_ctrl->m_deviceModel.selectedIndex(), 0);

    // Modify device B's profile and fire its display-profile-changed
    // signal. Device A is still selected, so the DeviceModel should
    // not swap to device B's 2500 DPI.
    const QString serialB = QStringLiteral("mock-serial-B");
    Profile &pB = m_ctrl->m_profileEngine.cachedProfile(
        serialB, QStringLiteral("default"));
    pB.dpi = 9999;
    m_ctrl->m_profileEngine.setDisplayProfile(
        serialB, QStringLiteral("default"));
    // setDisplayProfile is a no-op if the name didn't change; force an
    // emission by changing then restoring.
    m_ctrl->m_profileEngine.setDisplayProfile(
        serialB, QStringLiteral("other"));
    m_ctrl->m_profileEngine.setDisplayProfile(
        serialB, QStringLiteral("default"));

    // DeviceModel should still reflect device A.
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
}
```

- [ ] **Step 2: Run**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='AppControllerFixture.DisplayProfileChangedIgnoredForNonSelectedDevice' 2>&1 | tail -10`
Expected: PASS. Filter on `serial != selectedSerial()` in `onDisplayProfileChanged` keeps device A's values.

- [ ] **Step 3: Commit**

```bash
git add tests/test_app_controller.cpp
git commit -m "test(app): onDisplayProfileChanged filters by selected device

Profile changes on a non-selected device do not affect the
DeviceModel's display values. Guards against stale UI updates in
multi-device setups."
```

### Task 3.6: Final verification and cleanup

**Files:**
- None (verification only)

- [ ] **Step 1: Run the complete test suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3 && QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3`
Expected: 571/571 core (568 baseline + 3 integration) + 72/72 QML. All passing.

- [ ] **Step 2: Manual verification in simulate-all mode**

Run on the host:

```bash
pkill -9 -f 'build/src/app/logitune' 2>/dev/null
rm -rf ~/.config/Logitune/Logitune/devices/sim-*
nohup ./build/src/app/logitune --simulate-all > /tmp/logitune-sim.log 2>&1 & disown
sleep 4
```

Navigate to MX Vertical's Buttons page. Verify the DPI cycle card's secondary line reads "DPI cycle" (not "Shift wheel mode"). Switch to MX Master 3S, verify button assignments reflect MX Master 3S. Switch back to MX Vertical, verify it is still correct.

- [ ] **Step 3: Kill the running process**

Run: `pkill -9 -f 'build/src/app/logitune'`

- [ ] **Step 4: Inspect final branch state**

Run: `git log --oneline origin/master..HEAD`
Expected: 3 commits on the branch (or the three commit groups noted in the spec, depending on how the implementer chose to organize them). Each commit has a descriptive message; no test failures at any intermediate commit.

- [ ] **Step 5: Push the branch**

Run: `git push -u origin fix-carousel-reseed-buttonmodel`
Expected: push succeeds; the repo's pre-push hook runs tests and confirms pass.

- [ ] **Step 6: Open the pull request**

```bash
gh pr create --title "ProfileEngine per-device contexts; fix carousel ButtonModel reseed" --body "$(cat <<'EOF'
Closes #77.

## Summary

Replaces ProfileEngine's hidden global context with a map keyed by
deviceSerial. Wires DeviceModel::selectedChanged to refresh the UI
from the newly-selected device's cached profile.

Fixes the case where switching the carousel to a different device left
ButtonModel and DeviceModel display values stuck on the first-seeded
device's profile. On real single-device hardware the bug was
invisible; it surfaced in --simulate-all and on multi-device setups.

## Change breakdown

1. `refactor(profile)` commits: introduce DeviceProfileContext,
   per-device API, serial-bearing signals. Legacy API stays during
   the migration then is deleted.
2. `refactor(app)` commits: migrate all AppController call sites to
   pass selectedSerial(). Rework AppControllerFixture to go through
   onPhysicalDeviceAdded.
3. `feat(app)` + `test(app)` commits: new onSelectedDeviceChanged
   slot + three integration tests covering the bug.

## Test plan

- [x] logitune-tests: 571/571 pass (568 baseline + 3 integration)
- [x] logitune-qml-tests: 72/72 pass
- [x] --simulate-all manual check: MX Vertical's Buttons page shows
      "DPI cycle" as the secondary on the DPI card even when MX
      Anywhere 3 is seeded first; carousel switches swap action
      labels and DPI values correctly.
EOF
)"
```

- [ ] **Step 7: Confirm PR opened**

Expected: `gh pr create` prints a PR URL. Paste it in the session output.

---

## Self-review summary

Every section of the spec has at least one corresponding task:

- **Architecture + registration** → Tasks 1.1, 1.2.
- **Public API (read)** → Task 1.3.
- **Public API (write) + signals** → Task 1.4.
- **Legacy wrappers during migration** → Task 1.5.
- **Engine-level unit tests** → Task 1.6.
- **Call-site migration (AppController)** → Tasks 2.1, 2.2, 2.3.
- **Call-site migration (ProfileModel routing)** → Task 2.3 Step 8.
- **Fixture rework** → Task 2.4.
- **Signal migration** → Task 2.5.
- **Legacy deletion** → Task 2.6.
- **Selection handler** → Task 3.1.
- **addMockDevice helper** → Task 3.2.
- **Three integration tests** → Tasks 3.3, 3.4, 3.5.
- **Final verification + PR** → Task 3.6.

All code snippets are complete; no placeholders or TBDs remain.
