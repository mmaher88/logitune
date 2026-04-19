# Stale Property NOTIFY Fix — Design

**Status:** approved, ready for implementation plan
**Issue:** #17 (SmartShift button press does not update UI)
**Target release:** whatever beta comes after `v0.3.1-beta.1`, not a dedicated release
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-19

## Summary

Hardware-originated state changes (physical SmartShift button press, DPI
cycle button press, scroll invert change from another app) don't
propagate to QML bindings. Properties read stale values until the next
profile reload.

Fix: give `PhysicalDevice` per-property relay signals, have `DeviceModel`
forward them (gated on the selected device) to the matching
Q_PROPERTY NOTIFY signals that already exist but are never emitted.
Flip the Q_PROPERTY NOTIFYs from the coarse `settingsReloaded` to the
property-specific signals.

## Scope

Fix the live UI for these properties:

- `smartShiftEnabled`
- `smartShiftThreshold`
- `scrollHiRes`
- `scrollInvert`
- `thumbWheelMode`
- `thumbWheelInvert`
- `currentDPI`

All share the same latent bug: Q_PROPERTY NOTIFY points at
`settingsReloaded`, which only fires on a full profile reload, so
hardware-originated changes don't reach QML. `currentDPI` became
particularly visible after v0.3.1-beta.1 shipped the `dpi-cycle` action
that writes DPI outside any profile-reload path.

Out of scope:
- `batteryLevel` / `batteryCharging`: already have a dedicated
  `selectedBatteryChanged` NOTIFY; work correctly.
- `activeSlot` / Easy-Switch host state: same staleness class, but
  mentioned in MX Vertical's beta-tester checklist; wait for confirmation
  before expanding scope.
- `ActionModel` capability filtering: issue #63, separate track.

## Root cause

Signal chain today:

```
DeviceSession                     PhysicalDevice                  DeviceModel
=============                     ==============                  ===========
HID++ notify arrives
  -> m_smartShiftEnabled = ...
  -> emit smartShiftChanged()  ─► connected lambda:
                                  emit stateChanged()           ─► connected lambda:
                                                                    refreshRow(device)
                                                                    (updates QAbstractListModel
                                                                     roles only; no
                                                                     Q_PROPERTY NOTIFY
                                                                     signals fire)
```

`DeviceModel::refreshRow` updates the carousel's `data()` roles for that
row (battery chip, connection indicator) but doesn't emit any
per-property change signal. Q_PROPERTY bindings in QML are attached to
signals like `settingsReloaded` (fired only on profile reload), so
hardware-originated changes don't trigger a re-read.

All the specific signals declared on `DeviceModel`
(`smartShiftEnabledChanged`, `smartShiftThresholdChanged`,
`scrollConfigChanged`, `thumbWheelModeChanged`, `currentDPIChanged`)
exist but are never emitted.

## Design

### Signal flow after the fix

```
DeviceSession                     PhysicalDevice                  DeviceModel
=============                     ==============                  ===========
emit smartShiftChanged(e, t)   ─► lambda emits BOTH:
                                    emit smartShiftChanged(e, t)─► onPhysicalSmartShiftChanged:
                                    emit stateChanged()             if device == selectedDevice():
                                                                      emit smartShiftEnabledChanged()
                                                                      emit smartShiftThresholdChanged()
                                                                    (refreshRow still fires via
                                                                     the existing stateChanged
                                                                     handler so the carousel
                                                                     updates regardless)
```

### Gating rule

DeviceModel only emits a property NOTIFY when the change is on the
currently selected device. A background mouse's HID++ traffic (battery
tick, host change) still updates its carousel row via
`refreshRow(device)`, but does NOT pulse QML bindings that read the
selected device's state. This avoids spurious re-renders and matches
the existing `selectedBatteryChanged` pattern used for battery.

Selection change (a different mouse becomes selected) is already
handled by `selectedChanged` on the properties that care; swapping the
selected device re-evaluates everything on both QML sides.

## Code surface

Six files, no new files.

### `src/core/DeviceSession.{h,cpp}`

Audit the existing emit points and add any missing ones so every property
in scope has a session-level change notification:

- `currentDPIChanged()` must fire from `setDPI` (after the HID++ ack
  lands in `m_currentDPI`) and from any external-origin HID++
  notification that updates `m_currentDPI`. If not already emitted,
  add it.
- `thumbWheelInvertChanged()` must fire when `setThumbWheelInvert` (or
  equivalent) writes the flag. Add the signal if it is not declared;
  add the emit.
- `smartShiftChanged(bool, int)` already exists; no change.
- `scrollConfigChanged()` already exists; no change.
- `thumbWheelModeChanged()` already exists; no change.

Narrow unit tests in `tests/test_device_session.cpp` cover any newly
added emit points. No changes to existing signals' parameter shapes.

### `src/core/PhysicalDevice.h`

Add these signals alongside the existing `stateChanged`:

```cpp
void smartShiftChanged(bool enabled, int threshold);
void scrollConfigChanged();
void thumbWheelModeChanged();
void thumbWheelInvertChanged();
void currentDPIChanged();
```

Parameter shapes mirror DeviceSession's corresponding signals so the
lambda relay can forward arguments directly.

### `src/core/PhysicalDevice.cpp`

Update `connectSessionSignals`: every existing lambda that currently
only emits `stateChanged` also emits the matching per-property signal.
Add one new connection for `DeviceSession::currentDPIChanged` (and
`thumbWheelInvertChanged` if that is a new DeviceSession signal).

Shape:

```cpp
connect(session, &DeviceSession::smartShiftChanged, this,
        [this](bool enabled, int threshold) {
    emit smartShiftChanged(enabled, threshold);
    emit stateChanged();
});
```

The order matters only in the sense that both must fire; `refreshRow`
re-reads all data roles regardless, so there is no risk of the carousel
lagging behind the Q_PROPERTY read.

`disconnectSessionSignals` already uses a blanket
`disconnect(session, nullptr, this, nullptr)`, so no changes there.

### `src/app/models/DeviceModel.h`

Add a declaration for `thumbWheelInvertChanged()` in the `signals:` block
(mirror the existing `thumbWheelModeChanged`). All other signals already
exist.

Flip the Q_PROPERTY NOTIFY declarations:

```cpp
Q_PROPERTY(int currentDPI          READ currentDPI          NOTIFY currentDPIChanged)
Q_PROPERTY(bool smartShiftEnabled  READ smartShiftEnabled   NOTIFY smartShiftEnabledChanged)
Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY smartShiftThresholdChanged)
Q_PROPERTY(bool scrollHiRes        READ scrollHiRes         NOTIFY scrollConfigChanged)
Q_PROPERTY(bool scrollInvert       READ scrollInvert        NOTIFY scrollConfigChanged)
Q_PROPERTY(QString thumbWheelMode  READ thumbWheelMode      NOTIFY thumbWheelModeChanged)
Q_PROPERTY(bool thumbWheelInvert   READ thumbWheelInvert    NOTIFY thumbWheelInvertChanged)
```

`scrollHiRes` and `scrollInvert` share `scrollConfigChanged` because
they change together in `setScrollConfig`. Two NOTIFYs firing on the
same signal is valid Qt.

### `src/app/models/DeviceModel.cpp`

In `addPhysicalDevice` (the same method that already connects
`stateChanged` around line 146), add five new `connect` calls that wire
the new PhysicalDevice signals to small handler lambdas. Each handler:

1. Calls `refreshRow(device)` (safety net; the existing `stateChanged`
   connection already does this, but explicit keeps the handler
   self-contained if future refactoring separates the two).
2. Early-returns if `device != selectedDevice()`.
3. Emits the matching DeviceModel signal(s). For `smartShiftChanged`
   this is both `smartShiftEnabledChanged()` and
   `smartShiftThresholdChanged()`. For `scrollConfigChanged` nothing
   needs to happen inside the handler beyond re-emit because both
   affected Q_PROPERTYs share that NOTIFY.

Keep `settingsReloaded` untouched. It stays as the coarse "reloaded
from profile" signal that still fires on profile reload paths, and
still serves as the NOTIFY for properties that have no other trigger
(nothing in the current in-scope set, but useful to leave in place for
future `activeProfileName` / similar bindings).

### `src/app/AppController.cpp`

No changes. The request signals (`smartShiftChangeRequested` etc.) keep
their existing wiring from DeviceModel to DeviceManager.

## Tests

All new tests go under `tests/test_device_model.cpp` (existing). No new
test files. Framework is the existing GTest + QSignalSpy setup.

### Gate tests

Use a test fixture with two `MockDevice` / `PhysicalDevice` pairs, one
selected. For each property in scope:

- **SelectedDeviceEmits**: spy on the DeviceModel signal, simulate the
  PhysicalDevice's per-property emit on the selected device, assert
  exactly one DeviceModel emission.
- **UnselectedDeviceDoesNotEmit**: same spy, same simulation on the
  *other* PhysicalDevice, assert zero DeviceModel emissions.
- **UnselectedDeviceStillRefreshesRow**: assert that `rowChanged`
  (the QAbstractListModel signal) fires on both selected and
  unselected devices so the carousel stays in sync regardless.

### Scroll-pair coupling

`scrollHiRes` and `scrollInvert` share `scrollConfigChanged`. Test:

- **ScrollConfigChangeEmitsOnce**: one PhysicalDevice
  `scrollConfigChanged` emission produces exactly one DeviceModel
  `scrollConfigChanged` emission (not two), but both `scrollHiRes` and
  `scrollInvert` Q_PROPERTYs return the new value via
  `property("scrollHiRes")` / `property("scrollInvert")`.

### Emit-site tests in `tests/test_device_session.cpp`

For any newly-added DeviceSession emit (likely `currentDPIChanged` and
`thumbWheelInvertChanged`):

- **SetDpiEmitsCurrentDPIChanged**: a connected session, `setDPI(value)`
  calls through the command queue and `currentDPIChanged` fires exactly
  once when the ack arrives. (Follow the existing `setSmartShift`
  pattern from this file.)
- **SetThumbWheelInvertEmitsChanged**: analogous.

These are narrow tests; they verify the signal exists and fires, not
that the HID++ wire is correct.

### Test count

Expected growth: about 10 new test cases. All 539 core + 72 QML must
stay green.

## Rollout

Single PR on branch `fix-stale-property-notifys`. Four commit groups:

1. `feat(device-session): emit currentDPIChanged and thumbWheelInvertChanged` — add missing emit points + tests.
2. `feat(physical-device): relay per-property session signals` — new PhysicalDevice signals + expanded `connectSessionSignals` lambdas.
3. `feat(device-model): emit per-property notifications gated on selection` — handler lambdas in `addPhysicalDevice`, the `thumbWheelInvertChanged` signal declaration, the new tests.
4. `feat(device-model): flip Q_PROPERTY NOTIFY to per-property signals` — the header edits. Must land last so QML bindings only flip to the new NOTIFYs after the emit sites exist.

No separate release tag. Lands incidentally in whatever beta comes next.

## Known risks

- **Double-emit on profile reload.** Today `settingsReloaded` fires on
  profile reload and drives all these properties. After the flip, those
  properties no longer listen to `settingsReloaded`, so profile reload
  must also emit the per-property signals. Audit the existing
  `settingsReloaded` emit sites in `DeviceModel.cpp` (there are four,
  including `setDisplayValues` and `refreshFromActiveDevice`) and add
  the per-property emits alongside them. Tests must cover profile
  reload as a path too.
- **Signal storms on rapid hardware changes.** A chattering physical
  button or a DPI ramp could pulse Q_PROPERTY NOTIFYs in quick
  succession. QML bindings are debounced by the event loop; not an
  actual problem but worth mentioning. The existing
  `refreshRow`/`stateChanged` chain already fires at the same rate
  with no observed issue.

## Out of scope

- `activeSlot` / Easy-Switch host staleness. Same class of bug but
  defer to MX Vertical tester feedback before expanding.
- Any change to the HID++ capability tables.
- `QSortFilterProxyModel` or other ActionModel filtering (issue #63).
