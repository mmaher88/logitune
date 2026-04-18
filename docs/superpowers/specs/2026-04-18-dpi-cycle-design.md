# DPI Cycle Action — Design

**Status:** approved, ready for implementation plan
**Branch:** `add-mx-vertical-family` (lands together with MX Vertical support)
**Target release:** `v0.3.1-beta.1`
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-18

## Summary

Add a first-class `DpiCycle` action that any configurable button on any
`adjustableDpi: true` device can be bound to. The action steps the
device's DPI through a preset ring with wrap-around.

MX Vertical's thumb-side DPI button (CID `0x00C3`) ships with
`defaultActionType: "dpi-cycle"` and a curated ring that matches
Logitech's factory defaults. Every other existing device gains the
option of `"DPI cycle"` in the action picker, using a computed
fallback ring derived from their own `dpi: {min, max, step}`.

## Motivation

MX Vertical has a dedicated DPI-cycle button that Options+ (and the
firmware itself) uses to step through a preset DPI list. Logitune
currently has no such action type. The beta-first workaround was
`defaultActionType: "default"` so firmware owns the button (#7), but
that trades two things away:

1. Users of any mouse cannot bind "DPI cycle" to a different button.
2. The MX Vertical descriptor lies about what the button does (says
   "default" but the label is "DPI cycle").

Implementing the action properly lets the descriptor be semantically
honest and makes DPI cycle a generic, user-selectable action across the
entire supported lineup.

## Sources of truth

No external HID++ feature needs to be reverse-engineered — this is a
host-side action built on top of the existing `AdjustableDpi` feature
that `DeviceSession::setDPI` already uses.

Preset values for MX Vertical (`[400, 1000, 1750, 4000]`) match
Logitech's factory defaults as documented in Solaar and confirmed in
Options+ for Windows.

## Data model

Extend the existing `dpi` block in the descriptor with an optional
`cycleRing` array.

```json
"dpi": {
  "min": 400,
  "max": 4000,
  "step": 100,
  "cycleRing": [400, 1000, 1750, 4000]
}
```

Rules enforced by `devices/schema.json`:

- `cycleRing` is optional. Absence is valid.
- When present, it is an array of integers with `minItems: 2`.
- Each entry must be within `[min, max]` and an exact multiple of
  `step`. (Schema validation; runtime also ignores out-of-range entries
  defensively.)

`devices/schema.json` also restores `"dpi-cycle"` to the
`defaultActionType` enum.

### MX Vertical descriptors

Both retail and for-Business gain:

1. `"cycleRing": [400, 1000, 1750, 4000]` inside the `dpi` object.
2. Control `0x00C3` `defaultActionType` changes from `"default"` back
   to `"dpi-cycle"`.

No other bundled descriptor is modified in this PR. MX Master and MX
Anywhere descriptors can add curated rings as a follow-up; until then
they fall back to the computed ring.

## Fallback ring

When a device's descriptor has no `cycleRing` and `adjustableDpi: true`:

```
ring = [
  minDpi,
  round_to_step((minDpi + maxDpi) / 2, step),
  maxDpi
]
```

Example values:
- MX Master 3S (`min=200`, `max=8000`, `step=50`) → `[200, 4100, 8000]`
- MX Anywhere 3 (`min=200`, `max=4000`, `step=50`) → `[200, 2100, 4000]`

When `adjustableDpi: false` the cycle is a silent no-op with a debug
log line (same defense-in-depth pattern `SmartShiftToggle` uses today
on mice without SmartShift).

## Code surface

Small, isolated changes. One new header addition, six touched files.

### `src/core/ButtonAction.h`

Add `DpiCycle` to the `Type` enum, placed between `SmartShiftToggle`
and `AppLaunch` so the enum order mirrors the ActionModel row order.

### `src/core/ButtonAction.cpp`

Add `"dpi-cycle"` ↔ `DpiCycle` arms in both `parse()` and
`serialize()`.

### `src/core/interfaces/IDevice.h`

Add a pure virtual getter:

```cpp
virtual std::vector<int> dpiCycleRing() const = 0;
```

### `src/core/devices/JsonDevice.{h,cpp}`

Parse `dpi.cycleRing` alongside `min`, `max`, `step`. Store as
`std::vector<int>`. Expose via `dpiCycleRing()` returning the stored
vector (empty when absent). Reject out-of-range / non-step values with
a `qCWarning` and drop them from the ring — do NOT reject the whole
descriptor (consistent with existing loose parsing for hotspot
coordinates).

### `src/core/DeviceSession.{h,cpp}`

New public slot:

```cpp
Q_INVOKABLE void cycleDpi();
```

Implementation outline:

1. If not connected or no active device: log debug, return.
2. Build `effectiveRing` from `activeDevice()->dpiCycleRing()`; if
   empty and the device supports `adjustableDpi`, compute the fallback
   ring as described above.
3. If `effectiveRing` is still empty (device has no adjustable DPI at
   all): log debug, return.
4. Find current slot: `auto idx = nearest_index(effectiveRing,
   m_currentDPI)`. Closest-by-abs-diff so an off-preset value (e.g.
   user had set DPI via slider) still advances predictably.
5. Advance with wrap: `next = effectiveRing[(idx + 1) %
   effectiveRing.size()]`.
6. Call `setDPI(next)` — the existing setter handles the HID++ write
   and emits `currentDPIChanged`.

`nearest_index` is a small helper; if it needs a home beyond this
translation unit, put it in `src/core/DeviceSession.cpp` file-scope
with an internal linkage anonymous namespace.

### `src/core/ActionExecutor.cpp`

New case in `execute()`:

```cpp
case ButtonAction::DpiCycle:
    if (auto *session = m_sessionProvider->activeSession())
        session->cycleDpi();
    break;
```

Match whatever "active session" access pattern the file already uses
for HID++ calls; do not introduce a new one.

### `src/app/models/ActionModel.cpp`

One new row in the static `m_actions` vector, alphabetically slotted:

```cpp
{ "DPI cycle", "Step through the device's DPI preset list", "dpi-cycle", "" },
```

No capability filtering is added in this PR (tracked separately in
issue #63).

### `devices/schema.json`

Two edits:

1. Restore `"dpi-cycle"` to the `defaultActionType` enum.
2. Add `cycleRing` to the `dpi` object schema:

```json
"cycleRing": {
  "type": "array",
  "items": { "type": "integer" },
  "minItems": 2
}
```

Range/step validation is left to the parser and runtime since the
schema has no cross-field references.

## Tests

All new tests live under `tests/` and run against the existing GTest
suite (no new frameworks, no fixture refactors).

### `tests/test_button_action.cpp`

Add:
- `parse("dpi-cycle")` returns `{type: DpiCycle, payload: ""}`.
- `serialize({type: DpiCycle, payload: ""})` returns `"dpi-cycle"`.
- Round-trip preserves the value.

(If this file does not exist yet, create it mirroring the shape of the
existing parser tests in the codebase.)

### `tests/test_device_registry.cpp`

Extend the MX Vertical `MxVerticalForBusinessRegistered` test and the
parameterized MX Vertical entry:

- `dev->dpiCycleRing() == std::vector<int>{400, 1000, 1750, 4000}` for
  both variants.
- New standalone `TEST(DeviceRegistry, MxMaster3sHasNoDpiCycleRing)`:
  `dev->dpiCycleRing().empty()` is true.

### `tests/test_device_session_dpi_cycle.cpp` (new)

Unit-test the stepping logic without touching HID++ hardware. Use a
test double for the HID++ transport so `setDPI` is intercepted and its
argument recorded.

Cases:

1. `ring=[400, 1000, 1750, 4000]`, `currentDPI=400` → after `cycleDpi`,
   recorded call is `setDPI(1000)`.
2. Same ring, `currentDPI=4000` → `setDPI(400)` (wrap-around).
3. Same ring, `currentDPI=1500` (off-preset) → nearest is 1750 so next
   slot is `setDPI(4000)`.
4. `ring=[]`, `adjustableDpi=true`, `min=200, max=8000, step=50`,
   `currentDPI=200` → `setDPI(4100)` (computed fallback).
5. `ring=[]`, `adjustableDpi=false`, any `currentDPI` → no `setDPI`
   call issued; one debug log line emitted.

Verify no `setDPI` call is made when the device is not connected or
no active session is set.

### Schema validation runner

Add an invalid case: `cycleRing: [400, 450]` on a device whose `step`
is `100` should be flagged at parse time (warning logged, 450 dropped).
Verify via log-inspecting test helper already used elsewhere, or add a
narrow test if none exists.

## Documentation

- README support matrix unchanged (status stays beta across all
  affected devices).
- `docs/wiki/*.md` — no manual edits needed in this PR; wiki-sync
  workflow picks up any README changes automatically.
- `docs/adding-a-device.md` — add one sentence under the `dpi` section:
  "Optional `cycleRing: [int, …]` lists the DPI values a `dpi-cycle`
  button steps through. Omit to use a computed `[min, mid, max]`
  fallback."

## Rollout

Single PR, single release. Lands on the existing
`add-mx-vertical-family` branch before pushing. Same
`v0.3.1-beta.1` tag after master merge.

Commit groups (on top of the existing MX Vertical commits):

1. `feat(schema): add dpi-cycle action and cycleRing field` — schema
   enum restore + `cycleRing` schema addition.
2. `feat(core): DeviceSession::cycleDpi() with cycleRing + fallback` —
   ButtonAction enum, parse/serialize, IDevice getter, JsonDevice
   parser, DeviceSession method, unit tests.
3. `feat(core): dispatch dpi-cycle through ActionExecutor` —
   ActionExecutor case, ActionModel row.
4. `feat(devices): MX Vertical reactivates dpi-cycle default` —
   `cycleRing` in both variants, `defaultActionType` flipped back to
   `"dpi-cycle"`, MX Vertical fixture test updated to expect the ring
   and the new action type.

## Known risks

- **Action visible on mice that hide it poorly.** Users with e.g. MX
  Master can now bind "DPI cycle" to any configurable button. On those
  mice the fallback ring is computed, not curated — it may not match
  what Options+ does. Acceptable for beta; follow-up work (per-device
  curated rings, and capability filtering per issue #63) makes it
  cleaner.
- **Off-preset starting DPI.** If the user left DPI at e.g. 1500 via
  the slider, the first press of a DPI-cycle button snaps to the
  nearest preset and advances. Documented in tests (case 3). If this
  feels jarring in practice we revisit.
- **Cycle-ring race with DPI slider.** `cycleDpi()` reads
  `m_currentDPI` and writes a new value; the slider writes via the
  same `setDPI` path. Worst case: a press during a slider drag sets
  an out-of-order value. The existing command queue serializes HID++
  writes so the device always reaches the final state; no descriptor
  change needed here.

## Out of scope

- Curating `cycleRing` for MX Master / MX Anywhere descriptors to
  match their factory rings. Follow-up, low priority.
- Capability-based filtering of `ActionModel` (issue #63).
- Advanced ring edit UI (e.g. letting users configure their own ring
  in the app). Ships only the descriptor-side mechanism for now.
- Changes to the HID++ capability tables in
  `src/core/hidpp/capabilities/`. This action lives on top of the
  existing `AdjustableDpi` feature handling; no new capability
  variants needed.
