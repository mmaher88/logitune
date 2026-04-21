# Tray Multi-Device Sections Design

**Status:** approved, ready for implementation plan
**Issue:** #83 (Tray battery indicator only shows the currently-selected device)
**Target release:** next beta
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-21

## Summary

Replace the single-device tray menu with per-device sections. Each
connected device gets its own two-line block (device name + battery
status) separated from other devices by `QMenu` separators. The tray
tooltip surfaces every connected device's battery at a glance.

## Motivation

`TrayManager` today:

- Hardcodes the tooltip to `"Logitune - MX Master 3S"`
  (`src/app/TrayManager.cpp:11`), misleading any user on a different
  device.
- Has a single `m_batteryAction` wired to `DeviceModel::batteryLevelChanged`,
  which reads the *selected* device's battery. Non-selected devices
  are invisible from the tray.
- Never changes shape — same fixed actions on every launch.

With autostart-minimized-to-tray the default since PR #69, the tray is
the primary UX surface. A two-mouse user who stays minimized has no
visibility into the non-selected mouse's charge.

## Behaviour

Menu layout, with `QMenu` separators as decoration:

```
Show Logitune
─────────
MX Master 3S
  Battery: 82%
─────────
MX Anywhere 3S
  Battery: 45%
─────────
Quit
```

- Device ordering matches `DeviceModel::devices()` (user's saved
  carousel order).
- Device name and battery are disabled `QAction`s (display-only).
- Zero devices: section blocks omitted; menu is
  `Show Logitune / separator / Quit`.
- Device connect: new section appended above the final separator +
  Quit. Triggers a tooltip refresh.
- Device disconnect: the three `QAction*`s for that device are
  removed. Triggers a tooltip refresh.
- Battery update: only that device's battery `QAction` text changes.
  Triggers a tooltip refresh.

## Tooltip

Regenerated on every device add/remove and every battery change:

- Zero devices: `"Logitune"`.
- One: `"Logitune — MX Master 3S: 82%"`.
- Multiple: `"Logitune\nMX Master 3S: 82% • MX Anywhere 3S: 45%"`.
  Newline separates app name from the per-device line; bullet
  separates devices in the same line.

Charging indicator: append `⚡` after the percentage on each charging
entry (e.g. `"MX Master 3S: 82% ⚡"`).

## Architecture

`TrayManager` owns the menu and now also owns per-device state. Sketch:

```cpp
struct DeviceEntry {
    QAction *header;       // disabled, device name
    QAction *battery;      // disabled, "Battery: N%"
    QAction *separator;    // separator after the battery row
    QMetaObject::Connection stateConn;
};

class TrayManager : public QObject {
    // ... existing members ...
    DeviceModel *m_deviceModel = nullptr;
    QMap<PhysicalDevice *, DeviceEntry> m_entries;

    void rebuildEntries();   // called on countChanged
    void refreshEntry(PhysicalDevice *device);  // called on stateChanged
    void refreshTooltip();   // called on any device/battery change
};
```

### Constructor

Store `DeviceModel *m_deviceModel`. Build the static "skeleton": Show
action, separator (before device sections), Quit action. Connect
`DeviceModel::countChanged` to `rebuildEntries`. Call `rebuildEntries()`
once to seed whatever is already connected.

### rebuildEntries

1. For each `PhysicalDevice *d` in `m_deviceModel->devices()` not
   already in `m_entries`: build three `QAction*`s, insert them
   into the menu before the final `Quit` action (via
   `m_menu.insertAction`). Connect `d->stateChanged` to
   `[this, d]() { refreshEntry(d); refreshTooltip(); }` and store
   the `QMetaObject::Connection` in the entry so it can be
   disconnected later.
2. For each `PhysicalDevice *` in `m_entries` but not in the model's
   device list: remove its three actions from the menu, disconnect
   `stateConn`, erase from the map.
3. Call `refreshTooltip()`.

### refreshEntry(d)

Read `d->batteryLevel()` and `d->batteryCharging()`. Update the
battery `QAction`'s text to `"Battery: <n>%"` or
`"Battery: <n>% ⚡"` when charging. Header action text stays as
`d->deviceName()` (refreshed too, in case the name arrived after the
transport was added).

### refreshTooltip

Build the tooltip string per the "Tooltip" section and call
`m_trayIcon.setToolTip(...)`.

## Signal flow

```
DeviceModel::countChanged
    → TrayManager::rebuildEntries
        → QAction add/remove + stateChanged reconnect
        → refreshTooltip

PhysicalDevice::stateChanged (per-device lambda)
    → refreshEntry(device)
    → refreshTooltip
```

No polling. All reactive to existing signals.

## Code surface

### `src/app/TrayManager.{h,cpp}` (modified)

- Delete `m_batteryAction` member and accessor. Public API change: the
  existing `QAction *batteryAction()` disappears. Consumers:
  tests only (see below).
- Add `m_deviceModel`, `m_entries`, and the three helper methods.
- Constructor signature unchanged (`TrayManager(DeviceModel *, QObject *)`).

### `tests/test_tray_manager.cpp` (modified)

The file already exists with six tests. Two need adjustment:

- `MenuHasThreeActions` expects 3 non-separator actions. After this
  change, zero-device state has only 2 non-separator actions (Show +
  Quit). Update expected count to 2, and rename to
  `ZeroDevicesOnlyShowsShowAndQuit`.
- `BatteryActionDisabled` asserts `batteryAction()->isEnabled() == false`.
  The accessor goes away; delete this test.

Add five new tests for the multi-device behaviour (listed in the Tests
section below).

### `tests/mocks/MockDevice.h` (modified if necessary)

If `MockDevice` does not already provide a way to stage battery level
and charging state (`m_batteryLevel`, `m_batteryCharging` public
members, or equivalent), add them alongside the existing feature
staging fields. Style matches the existing `m_features` public member.

### No other files

`main.cpp`, `AppController`, models — unchanged. The tray wiring
happens inside `TrayManager` alone.

## Tests

Update existing, add new. Full list after this change:

### Adjusted existing tests

1. **`ZeroDevicesOnlyShowsShowAndQuit`** (renamed from
   `MenuHasThreeActions`) — empty `DeviceModel`, menu has exactly 2
   non-separator actions (Show, Quit).
2. **`ShowActionText`**, **`QuitActionText`**, **`ShowActionEmitsShowWindowRequested`**,
   **`QuitActionEmitsTriggered`** — unchanged.

### Deleted tests

- **`BatteryActionDisabled`** — `batteryAction()` accessor removed;
  per-device battery entries are tested via the new cases.

### New tests

3. **`OneDeviceAddsHeaderAndBatterySection`** — attach one mock
   device with name "Mock Master" and battery 80; assert the menu
   now has 4 non-separator actions (Show, Mock Master, Battery: 80%,
   Quit) and the battery action's text is `"Battery: 80%"`.
4. **`SecondDeviceAppendsSection`** — attach a second mock with
   battery 45; menu has 6 non-separator actions; both battery
   entries match expected text.
5. **`DeviceRemovedStripsSection`** — start with two devices, remove
   one; menu drops back to 4 non-separator actions.
6. **`BatteryChangeUpdatesMatchingEntryOnly`** — two devices, mutate
   device A's battery level and fire `stateChanged`; device A's
   battery text updates, device B's unchanged.
7. **`ChargingSuffixAppearsWhenCharging`** — one device charging at
   60%; battery action text is `"Battery: 60% ⚡"`.
8. **`TooltipReflectsAllDevices`** — two devices, tooltip equals the
   multi-device format from the spec.

Tests construct `PhysicalDevice`/`DeviceSession`/`MockDevice` with
the same `attachMockDevice` helper pattern used in
`tests/test_action_filter_model.cpp`. Expected count delta: six tests
(two removed, eight new → +6 net).

## Rollout

Branch `fix-tray-multi-device-sections`. Single PR, one commit. ~80
lines of C++ in `TrayManager.{h,cpp}` + the test updates. Closes #83.

## Known risks

- **QAction insertion order.** `QMenu::insertAction(before, action)`
  requires the `before` to be the final Quit. Track the Quit
  `QAction*` explicitly as `m_quitAction` (already present) and use it
  as the insert anchor.
- **Device identity stability.** `m_entries` keys are
  `PhysicalDevice*` pointers. Those live at least as long as the
  DeviceModel row (per `DeviceModel::addPhysicalDevice` semantics in
  the current codebase). Tray loses its entry when the row is
  removed, which is the correct behaviour.
- **No signals on `deviceName`.** A device's name is set during
  setup-complete in `PhysicalDevice`. Fold name refresh into the
  same `stateChanged` handler since that fires whenever any
  attribute changes. If the name changes after the tray entry is
  first built, the next `stateChanged` updates it.

## Out of scope

- Low-battery notifications via libnotify. Separate feature.
- Tray icon decoration (colour/badge) reflecting lowest battery. Once
  the per-device list lands, the icon-level signal is easy to add
  but not required here.
- Per-device submenu (e.g. "Show MX Master 3S details"). Keep the
  menu flat for now.
- Manual reordering of tray sections. Order follows
  `DeviceModel::devices()`, which follows the saved carousel order.
