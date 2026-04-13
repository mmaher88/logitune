# Options+ Extractor Rewrite + App-Side Descriptor Cleanup

**Status:** design approved
**Date:** 2026-04-13
**Issue:** #28

## Context

`scripts/generate-from-optionsplus.py` reads an extracted Options+ installer
and produces `descriptor.json` files. On paper this is how every Logitech
mouse in the Options+ catalogue can get a working Logitune descriptor without
hand-curation.

In practice the script has been broken in a cascading way. Each bug fix
uncovered another class of error:

1. Marker coordinates were divided by pixel width instead of by 100, so
   every hotspot clustered near the origin.
2. Control JSON used `cid`/`index`/`name`/`defaultAction`; the parser
   expects `controlId`/`buttonIndex`/`defaultName`/`defaultActionType`
   and silently dropped every mismatched entry.
3. Slot names missing from `SLOT_NAME_MAP` leaked raw constants into
   `defaultName` (e.g., `SLOT_NAME_MODESHIFT_BUTTON`).
4. The thumb-wheel slot has no `_cXX` suffix, so it was silently
   dropped.
5. Scroll hotspots were hardcoded to `[]`; `device_point_scroll_image`
   was never read.
6. Easy-switch slots were hardcoded to `[]`; `device_easyswitch_image`
   was never read.
7. Controls were emitted in Options+ metadata iteration order, but the
   app has several positional assumptions: `ButtonsPage.qml:198`
   hardcodes `buttonId === 7` as the thumb wheel, `PointScrollPage.qml`
   binds `scrollHotspots[0/1/2]` to scroll wheel / thumb wheel / pointer
   speed by position, profile persistence iterates controls by array
   index. Any ordering drift swaps the UI around and scrambles saved
   profiles.

Each fix shipped as the "last one." The root cause was patching without
an end-to-end mapping from Options+ input to descriptor output. This
spec establishes that mapping explicitly and redesigns the script around
it.

## Goals

- **Functional equivalence with hand-written descriptors for the three
  test devices** (MX Master 2S, 3S, 4). Extracted output is interchangeable
  with the shipped versions: 8 controls in canonical order, 3 scroll
  hotspots in fixed order, 3 easy-switch slots, same feature flags, DPI
  range, button names, hotspot positions within ±0.02 of hand-tuned.
- **Prevent silent-skip regressions.** Self-validation layer re-parses
  every generated descriptor against the `JsonDevice.cpp` parser's field
  expectations before writing. Mismatches abort; no broken descriptor
  ever lands on disk.
- **Decouple the app from positional assumptions where feasible.** Scroll
  hotspot lookups go through a `kind` field; thumb-wheel detection goes
  through a CID check. Positional fallbacks stay in place so this ships
  without a coordinated descriptor migration.
- **Golden-file regression coverage.** Committed fixture Options+ data for
  the three test devices drives an automated diff test in CI.

## Non-goals

- **Profile-file migration to CID-keyed persistence.** `AppController`
  still iterates controls by array position when restoring/saving
  profiles. The extractor guarantees deterministic ordering (CID ascending)
  so saved profiles keep working as long as Options+ doesn't insert new
  CIDs between existing ones for a given device. A CID-keyed migration is
  a worthy follow-up but out of scope.
- **Label offset tuning.** `labelOffsetYPct` stays 0.0 for extracted
  output. Visual collision avoidance is a separate runtime-side problem
  (collision-aware layout in `InfoCallout`).
- **Default gesture extraction.** `AppController.cpp:60-65` has hardcoded
  gesture defaults that apply when a descriptor ships `defaultGestures:
  {}`. Extracting these from Options+ would fight those defaults without
  adding value.
- **Handling devices Options+ doesn't know about.** Out of scope —
  issue #22 (device onboarding wizard) is the long-term answer.
- **Replacing shipped mouse images.** The `front.png` / `side.png` /
  `back.png` files that already ship in `devices/mx-master-*/` are
  unchanged by this work. Their provenance is a separate conversation.

## Architecture

### Module layout

```
scripts/optionsplus_extractor/
  __init__.py
  sources.py        # locate + load Options+ device DB and per-device depots
  capabilities.py   # capabilities.* dict  →  features + dpi
  slots.py          # core_metadata.json   →  typed slot records
  canonicalize.py   # sort rules for controls / scroll / easy-switch
  descriptor.py     # build final dict; guarantee parser-compatible field names
  validate.py       # parse-back self-check + golden-file comparisons
  cli.py            # thin argparse wrapper
scripts/generate-from-optionsplus.py   # 5-line shim, unchanged entry point
tests/scripts/
  fixtures/optionsplus/
    main/logioptionsplus/data/devices/
      devices_test.json                # trimmed to 2S/3S/4 entries
    devices/
      mx_master_2s/
        core_metadata.json
        manifest.json
        front_core.png                 # 1×1 placeholder
        side_core.png
        back_core.png
      mx_master_3s/                    # same three JSON files + placeholder PNGs
      mx_master_4/
  test_extractor.py                    # pytest-discoverable unit + golden tests
```

Each module takes typed inputs and returns typed outputs. No hidden
mutation; no shared globals. `descriptor.py` is the only module that
emits JSON-shaped dicts — everything else speaks plain Python dataclasses.

### Why modules

The old script's failure mode was "one big function, one thing
forgotten at a time." Typed boundaries make forgetting structurally
visible:

- `slots.py` returns a `ParsedMetadata` dataclass with explicit
  `buttons: list[ButtonSlot]`, `scroll: list[ScrollSlot]`,
  `easyswitch: list[EasySwitchSlot]`. "We never looked at
  `device_easyswitch_image`" becomes a missing field in the dataclass,
  not a silent `[]` in the output. (`device_gesture_buttons_image` is
  intentionally not parsed — see non-goals.)
- `canonicalize.py` is a set of pure functions from typed records to
  typed records. Unit tests use fixture dataclasses, not Options+ data.
- `validate.py` runs after `descriptor.py` builds a dict, before anything
  hits disk. It catches `cid` vs `controlId` typos and missing
  `buttonIndex` keys at generation time instead of at user-install time.

## Output schema

Target `descriptor.json` for MX Master 2S (illustrative — fields pinned
to `JsonDevice.cpp` parser keys):

```jsonc
{
  "name": "MX Master 2S",
  "status": "community-verified",      // set by script after validation
  "version": 1,
  "productIds": ["0x4069", "0xb019"],  // all PIDs Options+ lists
  "features": {                        // 9-key subset; parser defaults unset to false
    "battery": true, "adjustableDpi": true, "smartShift": true,
    "hiResWheel": true, "thumbWheel": true, "reprogControls": true,
    "smoothScroll": true, "gestureV2": false, "hapticFeedback": false
  },
  "dpi": { "min": 200, "max": 4000, "step": 50 },
  "controls": [                        // canonical CID-ascending; thumbwheel appended last
    { "controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",  "defaultActionType": "default", "configurable": false },
    { "controlId": "0x0051", "buttonIndex": 1, "defaultName": "Right click", "defaultActionType": "default", "configurable": false },
    { "controlId": "0x0052", "buttonIndex": 2, "defaultName": "Middle click", "defaultActionType": "default", "configurable": true  },
    { "controlId": "0x0053", "buttonIndex": 3, "defaultName": "Back",         "defaultActionType": "default", "configurable": true  },
    { "controlId": "0x0056", "buttonIndex": 4, "defaultName": "Forward",      "defaultActionType": "default", "configurable": true  },
    { "controlId": "0x00C3", "buttonIndex": 5, "defaultName": "Gesture button", "defaultActionType": "gesture-trigger",   "configurable": true },
    { "controlId": "0x00C4", "buttonIndex": 6, "defaultName": "Shift wheel mode", "defaultActionType": "smartshift-toggle", "configurable": true },
    { "controlId": "0x0000", "buttonIndex": 7, "defaultName": "Thumb wheel",  "defaultActionType": "default", "configurable": true  }
  ],
  "hotspots": {
    "buttons": [
      { "buttonIndex": 2, "xPct": 0.73, "yPct": 0.18, "side": "right", "labelOffsetYPct": 0.0 },
      { "buttonIndex": 3, "xPct": 0.49, "yPct": 0.70, "side": "left",  "labelOffsetYPct": 0.0 },
      { "buttonIndex": 4, "xPct": 0.47, "yPct": 0.58, "side": "left",  "labelOffsetYPct": 0.0 },
      { "buttonIndex": 5, "xPct": 0.13, "yPct": 0.69, "side": "left",  "labelOffsetYPct": 0.0 },
      { "buttonIndex": 6, "xPct": 0.79, "yPct": 0.36, "side": "right", "labelOffsetYPct": 0.0 },
      { "buttonIndex": 7, "xPct": 0.40, "yPct": 0.46, "side": "left",  "labelOffsetYPct": 0.0 }
    ],
    "scroll": [
      { "kind": "scrollwheel", "buttonIndex": -1, "xPct": 0.73, "yPct": 0.16, "side": "right", "labelOffsetYPct": 0.0 },
      { "kind": "thumbwheel",  "buttonIndex": -2, "xPct": 0.55, "yPct": 0.51, "side": "left",  "labelOffsetYPct": 0.0 },
      { "kind": "pointer",     "buttonIndex": -3, "xPct": 0.83, "yPct": 0.54, "side": "right", "labelOffsetYPct": 0.0 }
    ]
  },
  "images": { "front": "front.png", "side": "side.png", "back": "back.png" },
  "easySwitchSlots": [
    { "xPct": 0.215, "yPct": 0.655 },
    { "xPct": 0.255, "yPct": 0.637 },
    { "xPct": 0.295, "yPct": 0.654 }
  ],
  "defaultGestures": {}                // always empty; AppController supplies defaults
}
```

### New fields

- **`hotspots.scroll[].kind`** — string enum `"scrollwheel" | "thumbwheel"
  | "pointer"`. Optional for backwards compatibility: parser defaults to
  empty string, and `PointScrollPage.qml` falls back to positional
  lookup if no entries have `kind` set. That preserves existing
  descriptors while the script migrates them over.

### Intentional omissions

- `defaultGestures` stays `{}`. AppController's hardcoded defaults kick
  in when it's empty.
- `labelOffsetYPct` stays `0.0`. Visual polish is out of scope for the
  extractor.
- Features beyond the 9 keys the script can determine are omitted; the
  parser defaults them to false, matching the shipped descriptors'
  effective values.

## Transformation rules (field-by-field)

One row per output field, with source and rule. Gaps get a named
fallback and fail-loud rather than silent.

| Output field | Source (Options+) | Rule | Fallback |
|---|---|---|---|
| `name` | `devices_initial.json` → matching `depot` entry → `displayName` | as-is | required; abort device |
| `productIds` | same → `modes[].interfaces[].id` | extract hex suffix from IDs matching `046d_*`; dedupe; sort | required; abort device |
| `version` | script constant | `1` | n/a |
| `status` | set by `descriptor.py` | `"community-verified"` iff ≥1 button hotspot AND front image exists AND self-check passes; else `"placeholder"` | n/a |
| `features.battery` | `capabilities.hasBatteryStatus OR capabilities.unified_battery` | `bool(...)` | `false` |
| `features.adjustableDpi` | `capabilities.hasHighResolutionSensor OR 'highResolutionSensorInfo' in capabilities OR capabilities.pointerSpeed` | `bool(...)` | `false` |
| `features.smartShift` | `capabilities.scroll_wheel_capabilities.smartshift` | `bool(...)` | `false` |
| `features.hiResWheel` | `capabilities.scroll_wheel_capabilities.high_resolution` | `bool(...)` | `false` |
| `features.thumbWheel` | `'mouseThumbWheelOverride' in capabilities` | membership test | `false` |
| `features.reprogControls` | `capabilities.specialKeys.programmable` non-empty | `bool(list)` | `false` |
| `features.smoothScroll` | `capabilities.scroll_wheel_capabilities.smooth_scroll.win OR .mac` | `bool(...)` | `true` (matches parser default) |
| `features.gestureV2` | — | hardcoded | `false` |
| `features.hapticFeedback` | — | hardcoded | `false` |
| `dpi.min` | `capabilities.highResolutionSensorInfo.minDpiValueSensorOn` | int | `200` |
| `dpi.max` | `capabilities.highResolutionSensorInfo.maxDpiValueSensorOn` | int | `4000` |
| `dpi.step` | `capabilities.highResolutionSensorInfo.stepsSensorOn` | int | `50` |
| `controls[]` | `core_metadata.json` → `device_buttons_image.assignments[]` + 2 defaults | See canonicalization | — |
| `controls[].controlId` | `slotId` decimal `_cNNN` suffix OR `THUMBWHEEL_CID=0x0000` for thumb-wheel slots | `f"0x{n:04X}"` | skip if neither |
| `controls[].defaultName` | `slotName` → `SLOT_NAME_MAP` lookup | explicit table | unknown = hard error, device aborted |
| `controls[].defaultActionType` | same table | same | same |
| `controls[].configurable` | same table | same | same |
| `hotspots.buttons[]` | same assignments as controls | one per control, same `buttonIndex` | — |
| `hotspots.buttons[].xPct`/`yPct` | `marker.x`/`marker.y` | divide by 100, clamp [0,1], round to 3 decimals | — |
| `hotspots.buttons[].side` | derived | `"right"` if `xPct > 0.5` else `"left"` | — |
| `hotspots.buttons[].labelOffsetYPct` | — | constant | `0.0` |
| `hotspots.scroll[]` | `core_metadata.json` → `device_point_scroll_image.assignments[]` | See canonicalization | `[]` |
| `hotspots.scroll[].kind` | `slotId` pattern: `*scroll_wheel*` → `"scrollwheel"`, `*thumb_wheel*` → `"thumbwheel"`, `*mouse_settings*` or `*pointer*` → `"pointer"` | explicit pattern table | unknown = hard error |
| `hotspots.scroll[].buttonIndex` | derived | `-1` / `-2` / `-3` by kind | — |
| `easySwitchSlots[]` | `core_metadata.json` → `device_easyswitch_image.assignments[]` | filter `slotId` matching `*easy_switch_N`, sort by N, emit first 3 | `[]` |
| `defaultGestures` | — | constant | `{}` |
| `images.front` | `<depot>/front_core.png` (fallback `front.png`) | copy to output dir | required; skip device if absent |
| `images.side` | `<depot>/side_core.png` (fallback `side.png`) | copy if exists | optional |
| `images.back` | `<depot>/back_core.png` / `bottom_core.png` / `back.png` / `bottom.png` | first that exists | optional |

### Slot name table

```python
SLOT_NAME_MAP = {
    "SLOT_NAME_MIDDLE_BUTTON":       ("Middle click",     "default",           True),
    "SLOT_NAME_BACK_BUTTON":         ("Back",              "default",           True),
    "SLOT_NAME_FORWARD_BUTTON":      ("Forward",           "default",           True),
    "SLOT_NAME_GESTURE_BUTTON":      ("Gesture button",    "gesture-trigger",   True),
    "SLOT_NAME_DPI_BUTTON":          ("DPI button",        "default",           True),
    "SLOT_NAME_LEFT_SCROLL_BUTTON":  ("Shift wheel mode",  "smartshift-toggle", True),
    "SLOT_NAME_MODESHIFT_BUTTON":    ("Shift wheel mode",  "smartshift-toggle", True),
    "SLOT_NAME_SIDE_BUTTON_TOP":     ("Top button",        "default",           True),
    "SLOT_NAME_SIDE_BUTTON_BOTTOM":  ("Bottom button",     "default",           True),
    "SLOT_NAME_THUMBWHEEL":          ("Thumb wheel",       "default",           True),
}
```

Unknown entries are a **hard error for that specific device** — the
device is added to an `unknown_slots.json` report and its descriptor
is not written, but the script continues processing other devices.
At the end of the run, if the report is non-empty, the CLI exits
non-zero. New slot names become maintainer action items, never
silent pass-throughs.

## Canonicalization rules

Three ordering invariants the script guarantees:

1. **Controls** — sorted by `controlId` ascending, with the synthetic
   thumb-wheel entry (CID `0x0000`) pinned to the end. `buttonIndex` is
   assigned positionally starting at 2 (after the 2 defaults). For the
   MX Master line this yields
   `Middle(0x52)/Back(0x53)/Forward(0x56)/Gesture(0xC3)/Shift(0xC4)/Thumb`
   — identical to the shipped order. For devices without some of those
   CIDs (e.g., MX Vertical) the order stays deterministic and stable
   across runs, so position-based profile persistence keeps working.

2. **Scroll hotspots** — sorted by `kind`:
   `scrollwheel` → `thumbwheel` → `pointer`, with `buttonIndex` assigned
   `-1`/`-2`/`-3`. Devices without a thumb wheel skip that slot;
   the other kinds still map to their canonical positions. The app's
   positional fallback in `PointScrollPage.qml` keeps working for
   legacy descriptors that don't set `kind`.

3. **Easy-switch slots** — filter `slotId` for `*_easy_switch_N`
   (1-indexed), ignore the `*_host_change` entry, sort by N, emit first
   3. Devices without the image yield `[]`, which `EasySwitchPage.qml`
   handles by hiding the markers.

All three sorts are pure functions in `canonicalize.py` — typed records
in, typed records out. Unit-testable without touching Options+ data.

### What this does NOT handle

- **CID insertion in future Options+ releases.** If a new CID gets added
  between existing ones for a device (say 0x54 between `Back` and
  `Forward`), the sort shifts subsequent positions and breaks saved
  profiles. The CID-keyed profile migration is the clean fix; until
  then, regeneration risk is documented.
- **Per-device-family ordering overrides.** Every device uses the same
  CID-ascending rule. If a specific device ends up visually ugly, a
  follow-up can add label offset tuning or manual overrides.

## Validation strategy

Three layers, each catching a different class of bug.

### Layer 1 — schema self-check (`validate.py`)

After `descriptor.py` builds a dict and before writing it to disk, run
a Python re-parser that mirrors `JsonDevice.cpp`'s field expectations:

- every control has `controlId` as a valid hex string, `buttonIndex` as
  an int, `defaultName` / `defaultActionType` as strings
- every hotspot has `buttonIndex` / `xPct` / `yPct` / `side`
- PIDs parse as hex
- referenced images exist on disk
- `hotspots.scroll[].kind`, when present, is one of the three allowed
  values

Mismatches abort the device with a loud error. This is the layer that
would have caught `cid` vs `controlId` at generation time instead of
waiting for a user bug report.

### Layer 2 — golden-file tests (`tests/scripts/test_extractor.py`)

Pytest-discoverable. Runs the full extractor against committed fixture
data and asserts functional equivalence against the shipped
`devices/mx-master-{2s,3s,4}/descriptor.json`:

**Structural (must be exactly equal):**
- `len(controls) == 8`, each `buttonIndex` in `[0..7]`, thumb wheel at
  index 7 with CID `0x0000`
- `len(hotspots.buttons) == 6`
- `len(hotspots.scroll) == 3` with kinds
  `["scrollwheel", "thumbwheel", "pointer"]`
- `len(easySwitchSlots) == 3`
- All control `controlId` / `defaultName` / `defaultActionType` values
  exactly match shipped
- Feature flags match (the 9 keys the script emits)
- DPI `min`/`max`/`step` exactly match
- `productIds` is a superset of the shipped list

**Tolerance-based (within ±0.02 of shipped):**
- All button hotspot `xPct`/`yPct`
- All scroll hotspot `xPct`/`yPct`
- All easy-switch slot `xPct`/`yPct`

**Not asserted** (known gaps, documented in test as `# gap:` comments):
- `labelOffsetYPct` (hand-tuned only on shipped)
- `defaultGestures` (shipped has entries, script emits `{}`)

### Layer 3 — full-catalogue dry run (maintainer, pre-commit)

When run against real Options+ data outside the fixtures, the script
produces an `extraction-report.json`:
```json
{
  "processed": 31,
  "skipped_no_images": 1,
  "unknown_slot_names": [
    { "device": "mx_vertical", "slot": "SLOT_NAME_SOME_NEW_THING" }
  ]
}
```
If `unknown_slot_names` is non-empty, the CLI exits non-zero. Runs
locally only — no real Options+ data committed.

### Fixture contents

Options+ source files for three devices, trimmed to just what the script
reads:
- `devices_test.json` — subset of `devices_initial.json` with 2S/3S/4
  capability entries
- Per-device `core_metadata.json` and `manifest.json` (full, unmodified)
- Placeholder 1×1 PNGs for `front_core.png` / `side_core.png` /
  `back_core.png` — the extractor only checks existence and copies,
  never parses images

Total fixture size: ~60 KB.

## App-side cleanup (scope C)

Decouples the app from positional descriptor assumptions where feasible.

1. **`src/core/devices/JsonDevice.h`** — add `QString kind` to
   `HotspotDescriptor`, defaulting to empty.
2. **`JsonDevice.cpp::parseHotspots()`** — one extra line:
   `hd.kind = obj.value(QStringLiteral("kind")).toString();`.
   Backwards compatible: missing `kind` reads as empty string.
3. **`src/app/qml/pages/PointScrollPage.qml`** — add a
   `hotspotByKind(kind, fallbackIdx)` JS helper inside `renderGroup`
   that returns the first entry matching `kind`, falling back to
   `scrollHotspotsData[fallbackIdx]` if none match. Replace the three
   positional callsites with
   `hotspotByKind("scrollwheel", 0)` / `hotspotByKind("thumbwheel", 1)` /
   `hotspotByKind("pointer", 2)`. The positional fallback preserves
   behavior for descriptors that don't yet set `kind`.
4. **`src/app/qml/pages/ButtonsPage.qml:198`** — replace
   `(buttonId === 7)` with `ButtonModel.isThumbWheel(buttonId)`. Add a
   `Q_INVOKABLE bool isThumbWheel(int buttonIndex) const` to
   `ButtonModel` that returns true iff the control at that index has
   `controlId == 0x0000`. Button index 7 no longer has special
   meaning.
5. **Update the three shipped descriptors** (`devices/mx-master-{2s,3s,4}/
   descriptor.json`) — add `"kind": "scrollwheel" | "thumbwheel" |
   "pointer"` to each scroll hotspot. Existing `buttonIndex` values
   (-1/-2/-3) stay for positional fallback; `kind` becomes the primary
   lookup.

### Out of scope (follow-up)

- **CID-keyed profile file migration.** Profile persistence in
  `AppController.cpp:358-439,487-494` and `ProfileEngine.cpp:88` still
  uses positional array iteration. Changing to CID-keyed storage
  (`std::map<uint16_t, ButtonAction>`) would eliminate the last class
  of ordering-drift bug but requires a profile-file format migration.
  Tracked as a follow-up.
- **Label collision avoidance in `InfoCallout`.** Runtime fix for the
  overlapping-labels issue visible on extracted descriptors. Benefits
  hand-written descriptors too. Separate change.

## Error handling (extractor)

- **Unknown `SLOT_NAME_*` constant** → device added to report, its
  descriptor is not written, other devices keep processing. CLI exits
  non-zero at the end of the run if the report is non-empty.
- **Missing front image** → device skipped with warning, script
  continues with other devices, exits zero if all other extractions
  succeed.
- **`controlId` parse failure** → skip that slot, log, continue. A
  device with zero valid controls gets downgraded to `placeholder`
  status by the status rule.
- **Layer 1 self-check failure** → abort that device's run entirely,
  no output written.
- **Layer 2 golden-file diff mismatch** → CI test fails, PR can't
  merge. Maintainer investigates: real regression or intentional
  Options+ update. If intentional, update the shipped descriptor in the
  same PR.
- **Layer 3 unknown-slot report** (local pre-commit only) →
  maintainer sees the report, extends `SLOT_NAME_MAP`, re-runs.

## Testing

### Extractor unit tests (`tests/scripts/test_extractor.py`)

- `capabilities.py` — hand-crafted capability dicts exercise every
  feature-flag branch. Asserts `features_from_capabilities({...})`
  returns the expected subset.
- `slots.py` — synthetic `core_metadata.json`-shaped dicts exercise
  button slot parsing, scroll slot parsing, easy-switch slot parsing,
  and unknown-slot handling. Asserts typed records out.
- `canonicalize.py` — fixture typed records in, sorted typed records
  out. Verifies thumbwheel pinning, scroll ordering, easy-switch
  filtering.
- `descriptor.py` — verifies output dict field names match the
  parser's expected keys. Verifies `status` downgrades correctly on
  empty controls.
- `validate.py` — passes good and bad dicts through the self-check;
  asserts specific failure modes.

### Golden-file integration test

- Runs `cli.main()` against `tests/scripts/fixtures/optionsplus/`,
  loads the three generated descriptors, loads the three shipped
  descriptors, diffs per the Layer 2 rules.

### App-side tests

- `tests/test_json_device.cpp` — extend `LoadValidImplemented` to
  include `kind` on scroll hotspots and assert it round-trips.
- New case: load a descriptor without `kind` and verify
  `PointScrollPage.qml`'s positional fallback still resolves (runtime
  tested via `tests/qml/`).
- `tests/test_button_model.cpp` — new case: `isThumbWheel()` returns
  true for CID 0x0000 and false otherwise; correctly handles the
  no-thumbwheel case (MX Vertical).

## Implementation order

1. Add `kind` field to `HotspotDescriptor` + `JsonDevice.cpp` parser.
2. Add `ButtonModel::isThumbWheel()`.
3. Update `PointScrollPage.qml` and `ButtonsPage.qml`.
4. Update the three shipped descriptors with `kind` fields.
5. Run existing test suite — confirm no regressions, new app tests
   pass.
6. Create `scripts/optionsplus_extractor/` package skeleton with empty
   modules.
7. Port `sources.py` — loading Options+ device DB and depot files.
8. Port `capabilities.py` — features + DPI mapping.
9. Port `slots.py` — typed metadata parsing for all image keys.
10. Port `canonicalize.py` — three sort rules.
11. Port `descriptor.py` — final dict assembly with parser-compatible
    field names.
12. Port `validate.py` — Layer 1 self-check.
13. Port `cli.py` + shim in `scripts/generate-from-optionsplus.py`.
14. Commit `tests/scripts/fixtures/optionsplus/` fixture data.
15. Write `test_extractor.py` unit tests per module.
16. Write golden-file test against shipped descriptors.
17. Run `logitune-tests` and `pytest` — confirm all green.

Each step is independently reviewable. The old monolithic
`generate-from-optionsplus.py` gets replaced with the thin shim in
step 13; there is no separate cleanup step after.

## Known limitations

- `labelOffsetYPct` remains `0.0` on all extracted output; visual
  label overlap on some devices is expected until `InfoCallout` grows
  runtime collision avoidance.
- `defaultGestures` stays `{}`; `AppController` hardcoded defaults
  fill in.
- `easySwitchSlots` and `defaultGestures` can end up empty on devices
  Options+ doesn't provide data for; the UI already handles empty
  arrays gracefully.
- The extractor is not protected against Options+ releasing new CIDs
  in the middle of existing ranges for a device, which would shift
  subsequent `buttonIndex` values and invalidate saved profiles.
  Mitigation: CID-keyed profile persistence (separate follow-up).
- Devices not in Options+ at all (long-tail Logitech mice) are out of
  scope — the device onboarding wizard (issue #22) is the long-term
  answer.
