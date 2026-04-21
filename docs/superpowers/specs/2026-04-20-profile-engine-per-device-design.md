# ProfileEngine Per-Device Contexts Design

**Status:** approved, ready for implementation plan
**Issue:** #77 (ButtonModel not re-seeded when carousel selection changes)
**Target release:** next beta
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-20

## Summary

`ProfileEngine` currently holds profile state for exactly one device at a
time via a hidden global context (`m_configDir`, `m_cache`,
`m_displayProfile`, `m_hardwareProfile`, `m_appBindings`). Switching the
carousel to a different device does not swap that context, so the UI
continues to display the previously-loaded device's profile data.

Replace the single hidden context with a map keyed by `deviceSerial`, gain
a per-device API, and wire `DeviceModel::selectedChanged` to refresh the
UI from the selected device's cached profile.

## Motivation

In `--simulate-all` mode (and on real multi-device setups), the first
seeded device's profile loads into `ButtonModel` and `DeviceModel`
display values. No code path updates these when the user clicks a
different device in the carousel, because:

- `onPhysicalDeviceAdded` calls `setupProfileForDevice`, which ends in
  `setDisplayProfile("default")`.
- `ProfileEngine::setDisplayProfile(name)` short-circuits when the name
  has not changed.
- All sim devices (and most real-world setups) use `"default"` as the
  profile name, so after the first device's setup, subsequent
  `setDisplayProfile("default")` calls are no-ops.
- No signal fires, so `onDisplayProfileChanged` never runs, so
  `ButtonModel::loadFromProfile` never re-runs.

Net effect: MX Vertical's Buttons page in the full sim displays
"Shift wheel mode" on the DPI cycle card because MX Anywhere 3 happened
to be seeded first. On real hardware with a single Logitech device the
bug is invisible; it surfaces as soon as a user has two supported
devices connected or works in simulate-all mode.

## Approach

Three alternatives considered:

1. **Wire `selectedChanged` to a handler that re-applies the profile**
   (minimal). Required a "last-applied device" tracker and adjustments
   to `AppControllerFixture`. Previous attempt broke ~17 tests because
   the fixture bypasses the production device-add flow.
2. **Split `setupProfileForDevice` into one-time provisioning plus a
   repeatable `applyProfileToUI`**. Cleaner than (1) but still relies
   on the hidden global context; the UI-refresh method must swap
   `setDeviceConfigDir` and clear the cache every selection change.
3. **Per-device contexts in `ProfileEngine`** (chosen). Eliminates the
   hidden global; every profile read/write explicitly names a device.
   Selection-change handler becomes a pure read-and-push. Larger diff
   than (1) or (2) but aligns the data model with reality: profiles
   are per-device.

Going with (3). The refactor is the right long-term direction (enables
future multi-device UI like side-by-side Easy-Switch or profile
import/export). Ships the bug fix and removes an architectural smell
in one PR.

## Architecture

Replace the implicit context in `ProfileEngine` with:

```cpp
struct DeviceProfileContext {
    QString configDir;                    // per-device profiles dir
    QHash<QString, Profile> cache;        // profileName -> Profile
    QHash<QString, QString> appBindings;  // wmClass -> profileName
    QString displayProfile;               // currently displayed
    QString hardwareProfile;              // currently active on hardware
};

class ProfileEngine {
    QHash<QString, DeviceProfileContext> m_byDevice;  // key: deviceSerial
    // ...
};
```

### Registration / lifecycle

- `registerDevice(serial, configDir)` is the explicit entry point called
  from `AppController::onPhysicalDeviceAdded` (and from the test
  fixture). Creates the context, `mkpath`s the dir, loads every
  `*.conf` into the cache, loads `app-bindings.conf`. Idempotent.
- If a call arrives with an unknown serial, the engine lazily creates a
  context using `AppConfigLocation/devices/<serial>/profiles` so the
  API is robust against ordering bugs.
- Contexts persist for the life of the process. On device disconnect
  the context stays in memory; reconnection reuses it.

## Public API

Every method that touches profile state takes `deviceSerial` as its
first parameter:

```cpp
// Registration
void registerDevice(const QString &serial, const QString &configDir);
bool hasDevice(const QString &serial) const;

// Reads
Profile& cachedProfile(const QString &serial, const QString &name);
QStringList profileNames(const QString &serial) const;
QString displayProfile(const QString &serial) const;
QString hardwareProfile(const QString &serial) const;
QString profileForApp(const QString &serial, const QString &wmClass) const;

// Writes
void setDisplayProfile(const QString &serial, const QString &name);
void setHardwareProfile(const QString &serial, const QString &name);
void saveProfileToDisk(const QString &serial, const QString &name);
void createProfileForApp(const QString &serial,
                         const QString &wmClass,
                         const QString &name);
void removeAppProfile(const QString &serial, const QString &wmClass);

// Static helpers (serial-free, stay as-is)
static Profile loadProfile(const QString &path);
static void saveProfile(const QString &path, const Profile &p);
```

`setDeviceConfigDir` is deleted. Its one legitimate use becomes
`registerDevice(serial, dir)`.

## Signal semantics

Signals carry the device serial so consumers can filter:

```cpp
void displayProfileChanged(const QString &serial, const Profile &profile);
void hardwareProfileChanged(const QString &serial, const Profile &profile);
```

`AppController::onDisplayProfileChanged` compares the incoming serial
against `selectedDevice()->deviceSerial()`. Changes for non-selected
devices early-return; UI updates reflect only the currently-selected
device.

No "current device" hidden state in the engine. Selection awareness
lives in the controller where it belongs.

## Selection handler

New slot `AppController::onSelectedDeviceChanged`, wired in
`wireSignals`:

```cpp
connect(&m_deviceModel, &DeviceModel::selectedChanged,
        this, &AppController::onSelectedDeviceChanged);
```

Body:

```cpp
void AppController::onSelectedDeviceChanged()
{
    auto *device = selectedDevice();
    if (!device) return;

    m_currentDevice = device->descriptor();

    const QString serial = device->deviceSerial();
    const QString name = m_profileEngine.displayProfile(serial);
    const Profile &p = m_profileEngine.cachedProfile(serial, name);

    restoreButtonModelFromProfile(p);
    pushDisplayValues(p);
}
```

No `setupProfileForDevice` call here. One-time provisioning happens in
`onPhysicalDeviceAdded`; this handler is pure read-and-push. Idempotent,
no file I/O, no seed.

`onPhysicalDeviceAdded` calls `m_deviceModel.addPhysicalDevice(device)`
which auto-selects the first device, firing `selectedChanged`. The new
handler runs and pushes the freshly-registered context's profile into
the UI. Correct first-attach behavior falls out naturally.

## Code surface

### `src/core/ProfileEngine.{h,cpp}`

- Introduce `DeviceProfileContext` struct.
- Replace `m_configDir`, `m_cache`, `m_displayProfile`,
  `m_hardwareProfile`, `m_appBindings` with `m_byDevice`.
- Rewrite every method to accept `deviceSerial` and look up the
  context. Lazy-register when missing.
- `registerDevice(serial, configDir)` new method.
- Delete `setDeviceConfigDir`.
- Emit new serial-bearing signals.

### `src/app/AppController.{h,cpp}`

- Add `onSelectedDeviceChanged` slot and wire it in `wireSignals`.
- Update every `m_profileEngine.*` call to pass
  `selectedDevice()->deviceSerial()`. Grep identifies ~15 sites.
- `setupProfileForDevice` now calls `registerDevice` instead of
  `setDeviceConfigDir`. The rest of its body (seed default profile,
  set hw/display) uses the serial-aware API.
- `onDisplayProfileChanged` gains a `QString serial` parameter, filters
  to the selected device.

### `src/app/models/ProfileModel.*`

- `profileAdded` / `profileRemoved` signals route through `AppController`
  rather than directly into `ProfileEngine`, so the controller can
  inject the serial. Minor rewiring in `AppController::wireSignals`.

### `src/app/models/ButtonModel.*`

No change. Already per-UI-state, not per-device.

### `tests/helpers/AppControllerFixture.h`

Replace the three shortcuts:

```cpp
// REMOVE: m_ctrl->m_currentDevice = &m_device;
// REMOVE: m_ctrl->m_profileEngine.setDeviceConfigDir(m_profilesDir);
// REMOVE: m_ctrl->m_profileEngine.setDisplayProfile(...) / setHardwareProfile(...)
```

With:

```cpp
const QString kSerial = QStringLiteral("mock-serial");
m_ctrl->m_profileEngine.registerDevice(kSerial, m_profilesDir);

// Session + PhysicalDevice setup (unchanged)
// ...
m_physicalDevice = new PhysicalDevice(kSerial, m_ctrl.get());
m_physicalDevice->attachTransport(m_session);

// Drive through the normal device-added path
m_ctrl->onPhysicalDeviceAdded(m_physicalDevice);
```

Fixture helpers (`createAppProfile`, `setProfileButton`,
`setProfileGesture`) take serial internally; single-device test call
sites unchanged. Multi-device tests (Section 6, tests 1-3) add a
helper `addMockDevice(serialSuffix)` that constructs a second
`PhysicalDevice` / `DeviceSession` pair with its own serial and drives
it through the same `onPhysicalDeviceAdded` flow, returning a pointer
so the test can manipulate per-device profiles.

## Tests

### New

Added to `tests/test_app_controller.cpp`:

1. **`CarouselSwitchSwapsButtonModel`** — two mock devices A and B with
   different button-5 actions. After `setSelectedIndex(1)`,
   `ButtonModel::actionNameForButton(5)` returns B's action.
2. **`CarouselSwitchSwapsDisplayValues`** — same setup with different
   DPI values per device. `deviceModel().currentDPI()` reflects the
   newly-selected device.
3. **`DisplayProfileChangedIgnoredForNonSelectedDevice`** — fire
   `displayProfileChanged(serialB, ...)` while A is selected; assert
   ButtonModel and DeviceModel unchanged.

Added to `tests/test_profile_engine.cpp` (create if absent):

4. **`MultipleDevicesKeepSeparateCaches`** — register A and B, assert
   `cachedProfile("A", "default")` and `cachedProfile("B", "default")`
   are independent.
5. **`UnknownDeviceLazyRegisters`** — call `cachedProfile("unknown",
   "default")` before `registerDevice`; assert a context is created
   and a default-constructed Profile is returned.
6. **`SetDisplayProfileScopedToDevice`** — set display profile to "X"
   on A, verify B's display profile is unchanged.

### Existing

- `AppControllerFixture` tests (~30) recompile cleanly via the helper
  updates. Run the full suite to confirm no behavioral drift.
- `ProfileEngine` existing tests get serial parameters added.

### Manual verification

In `--simulate-all`: MX Vertical's Buttons page now shows "DPI cycle"
as the secondary on the DPI card even when MX Anywhere 3 is seeded
first. Switching between devices in the carousel updates the displayed
actions and DPI values correctly.

## Rollout

Branch `fix-carousel-reseed-buttonmodel`. Three commits:

1. `refactor(profile): introduce per-device contexts in ProfileEngine` —
   add `DeviceProfileContext` + `m_byDevice` + serial-aware API and
   signals. Keep legacy single-context wrappers that auto-register a
   `"legacy"` serial and preserve the old `displayProfileChanged(Profile)`
   /`hardwareProfileChanged(Profile)` signals so callers compile
   unchanged. All tests pass.
2. `refactor(app): route ProfileEngine calls through device serial` —
   update every call site in `AppController` and `ProfileModel` to pass
   a serial. Delete the legacy wrappers, `setDeviceConfigDir`, and the
   legacy single-arg signals. Migrate `onDisplayProfileChanged` to the
   new two-arg signal. Rework `AppControllerFixture` + helpers. Existing
   test suite passes end-to-end with behavior unchanged.
3. `fix(app): re-seed ButtonModel when carousel selection changes` —
   wire `DeviceModel::selectedChanged` to `onSelectedDeviceChanged`.
   Update `onDisplayProfileChanged` to filter by selected device. Add
   the six new tests. Closes #77.

Splitting the mechanical refactor (1, 2) from the behavior change (3)
keeps review tractable. Commits 1 and 2 have zero behavioral delta.

## Known risks

- **Call-site coverage.** Commit 2 touches ~15 call sites across two
  files. A missed site fails to compile (not a runtime bug) and is
  caught immediately.
- **Lazy registration surprises.** If code paths hit `cachedProfile`
  with an unexpected serial before `registerDevice` runs, the engine
  creates a default context pointing at
  `AppConfigLocation/devices/<serial>/profiles` and emits a
  `qCWarning(lcApp) << "ProfileEngine: lazy-registering unknown device"
  << serial` so the condition is visible in logs. Low risk because
  `onPhysicalDeviceAdded` registers before any other code gets the
  serial; the warning is a safety net for regressions in device-add
  ordering.
- **Signal subscribers outside this change.** `displayProfileChanged`
  signal subscribers outside `AppController` would break on the new
  arity. Grep confirms only `AppController::onDisplayProfileChanged`
  consumes it today.

## Out of scope

- QML-level profile-switcher UI (single "default" profile stays the
  norm; per-app profiles already exist).
- Renaming `deviceSerial` to `deviceId` across the codebase.
- Per-device profile history / undo.
- Cross-device profile copy or import.
