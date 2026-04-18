# MX Vertical Family Support — Design

**Status:** approved, ready for implementation plan
**Issue:** #7 (reporter: @dmaglio)
**Target release:** `v0.3.1-beta.1`
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-17

## Summary

Add beta descriptors for the two MX Vertical variants so the mouse is
recognized, renders correctly in the app, and its configurable controls
round-trip.

| Model | PIDs | DPI max | Status |
|---|---|---|---|
| MX Vertical | `0xb020` (Bolt/BT), `0x407b` (Unifying) | 4000 | beta |
| MX Vertical for Business | `0xb020` (Options+/Solaar to confirm) | 4000 | beta |

Flow mirrors the MX Anywhere family cycle (#46 / PR #54):
- Generate logical content from the hidden Options+ extraction repo.
- Cross-check every field against Solaar and libratbag before merge.
- Ship both variants as `beta`; flip to `verified` once a hardware
  reporter confirms.
- User tunes visual positions (slot circles, hotspots, button callouts)
  via `--edit` mode; implementation only commits logical descriptor
  content and initial/placeholder positions.

## Sources of truth

Order of precedence for logical content:

1. **Hidden Options+ extraction repo** — primary source. Contains the
   canonical HID++ capability map per Logitech product code.
2. **Solaar device database** — second opinion on PIDs, feature flags,
   DPI range.
3. **libratbag / logid** — tie-breaker and sanity check on CID →
   default-action mapping.

Any conflict between sources must be noted in the descriptor's inline
comments or in the PR description (not silently resolved).

## Device data

### Controls (same for both variants)

| CID | Button index | Default name | Default action | Configurable |
|---|---|---|---|---|
| `0x0050` | 0 | Left click | `default` | no |
| `0x0051` | 1 | Right click | `default` | no |
| `0x0052` | 2 | Middle click | `default` | yes |
| `0x0053` | 3 | Back | `default` | yes |
| `0x0056` | 4 | Forward | `default` | yes |
| `0x00C3` | 5 | DPI cycle | `dpi-cycle` | yes |

The thumb-side DPI-change button on MX Vertical is exposed as reprog
CID `0x00C3`, not as a SmartShift toggle like MX Master's `0x00C4`.
This is confirmed by Solaar's device registry.

### Feature flags

| Flag | Value | Notes |
|---|---|---|
| `battery` | true | standard HID++ battery feature |
| `adjustableDpi` | true | 400 to 4000, step 100 |
| `extendedDpi` | false | 4000 is the firmware ceiling |
| `smartShift` | **false** | wheel is mechanically ratcheted, no free-spin |
| `hiResWheel` | true | |
| `hiResScrolling` | false | |
| `smoothScroll` | true | |
| `thumbWheel` | false | MX Vertical has no thumb wheel |
| `reprogControls` | true | |
| `pointerSpeed` | true | Solaar confirms |
| `gestureV2` | false | no gesture button |
| `mouseGesture` | false | |
| `hapticFeedback` | false | |
| `crown` | false | |
| `rgbEffects` | false | |
| `onboardProfiles` | false | |
| `leftRightSwap` | false | right-handed only |

### Easy-Switch

3 slots, driven by the top-side Easy-Switch button (firmware-managed
via the `ChangeHost` feature, NOT a reprog CID).

### DPI

- Min: 400
- Max: 4000
- Step: 100

No extended DPI; firmware caps at 4000 across all firmware revisions
at time of writing.

## File layout

```
devices/
├── mx-vertical/
│   ├── descriptor.json
│   ├── front.png
│   ├── side.png
│   └── back.png
└── mx-vertical-for-business/
    ├── descriptor.json
    ├── front.png   (same hardware, reuse retail renders)
    ├── side.png
    └── back.png
```

Images sourced from Logitech's CDN in the same pattern as the MX
Anywhere work. The for-Business variant is the same mold as the retail
variant, so renders are shared bit-for-bit.

`devices/schema.json` is unchanged — every field used here is already
part of the descriptor schema.

## Tests

Add two entries to the parameterized `DeviceSpec` fixture in
`tests/test_device_registry.cpp`:

```cpp
DeviceSpec{
    .slug = "mx-vertical",
    .expectedName = "MX Vertical",
    .productIds = {0xb020, 0x407b},
    .dpiMin = 400, .dpiMax = 4000, .dpiStep = 100,
    .minControls = 6,
    .control5Cid = 0x00C3,
    .control5ActionType = "dpi-cycle",
    .control6ActionType = nullptr,
    .gestureDownPayload = nullptr,
    .smartShift = false,
    .pointerSpeed = true,
    .thumbWheel = false,
    .hiResWheel = true,
    .smoothScroll = true,
    .reprogControls = true,
    .battery = true,
    .adjustableDpi = true,
},
DeviceSpec{
    .slug = "mx-vertical-for-business",
    .expectedName = "MX Vertical for Business",
    .productIds = {0xb020},
    // rest identical to retail
}
```

Sentinel fields (`control6ActionType = nullptr`,
`gestureDownPayload = nullptr`) are already supported by the fixture
(added during the MX Anywhere cycle). No fixture schema changes.

All 518 tests must stay green. No other test suites should need edits.

## Code changes

None expected in C++:
- `DeviceRegistry` auto-scans the `devices/` tree.
- `DeviceManager` dispatches HID++ capabilities via the capability
  tables in `src/core/hidpp/capabilities/`. Every feature MX Vertical
  uses is already handled.
- `DeviceModel` already exposes the QML properties QML pages read
  (including `smartShiftEnabled`, which simply reads `false` and hides
  the SmartShift UI automatically for this device).

## Docs changes

- `README.md`: add two rows to the support matrix under the beta
  section.
- `docs/wiki/*.md`: no direct edits needed. If any wiki page references
  the support matrix, the auto-sync workflow (fixed in PR #59) will
  push it after the next master push.

## Release flow

1. Merge the implementation PR to master.
2. Tag `v0.3.1-beta.1` on the new master HEAD.
3. Release workflow (now in working order) builds and publishes deb +
   rpm + pacman packages with correct tag-derived versioning.
4. Post a comment on issue #7 tagging @dmaglio with:
   - Release URL
   - Per-distro install command (same template as the MX Anywhere
     comment)
   - Trimmed test checklist covering only MX Vertical features:
     detection, battery, DPI slider (400 - 4000), pointer speed,
     hi-res + smooth scroll, scroll invert, button remap on Middle /
     Back / Forward / DPI-cycle, Easy-Switch host indication, tray
     integration.
   - **Not** on the list (unlike MX Anywhere): SmartShift, thumb
     wheel, gestures.

## Status transition

Per-variant criteria to flip `status` from `beta` to `verified`:

- **MX Vertical (retail):** @dmaglio confirms detection + at least one
  round-trip test (DPI persists, button remap fires).
- **MX Vertical for Business:** stays `beta` until any reporter with
  the SKU confirms. If no tester surfaces within a reasonable window
  and Options+/Solaar data points drift, consider removing the entry
  rather than shipping an unverified descriptor.

## Known risks and mitigations

- **for-Business PID uncertainty:** Options+ and Solaar usually map
  retail + business SKUs to the same HID++ PID, because they are the
  same hardware with different packaging. We will still cross-check
  both sources at implementation time. If the for-Business variant is
  confirmed to share `0xb020`, the two descriptors exist only to give
  users the right product name in the carousel.
- **Images sourced from Logitech CDN:** same pattern as existing MX
  Master and MX Anywhere descriptors. No licensing change.
- **No real-hardware smoke on my side:** my only MX-family mouse is an
  MX Master 3S. The MX Anywhere cycle shipped the same way and the
  one live bug (missing `qml6-module-qtquick-dialogs` dep) was caught
  by the tester rather than local testing. Worth remembering when
  reviewing the PR.

## Out of scope

- MX Ergo (trackball, different form factor).
- Any refactor of the descriptor schema or registry.
- Any change to HID++ capability tables.
- AUR publishing.
