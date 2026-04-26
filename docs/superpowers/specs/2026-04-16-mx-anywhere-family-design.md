# MX Anywhere 3 / 3S Family Support Design

**Date:** 2026-04-16
**Status:** Approved
**Scope:** Ship descriptors for MX Anywhere 3, MX Anywhere 3 for Business, MX Anywhere 3S, and MX Anywhere 3S for Business. Closes #46.

## Goals

1. Add four device descriptors to the bundled `devices/` tree so the app recognizes and drives these mice out of the box.
2. Base logical data on Logitech Options+ extraction (via `jlevere/hidpp`), cross-checked against libratbag and the `mx-anywhere-3` entry in the `mmaher88/logitune-devices` reference repo regenerate branch.
3. Ship with starter images copied from the reference repo. Visual polish (hotspot positions, Easy-Switch slot coordinates, custom `back.png`) happens in `--edit` mode after merge.
4. Mark all four as `beta` until the issue #46 tester confirms on real hardware.

## Non-Goals

- Hotspot position polish in this PR. All hotspot coordinates ship at `0.5, 0.5` (image center); the descriptor maintainer polishes via `--edit`.
- Easy-Switch slot placement. Coordinates ship at `0.5, 0.5`; polished in `--edit` after uploading a real `back.png`.
- `back.png` capture. Not shipped initially; the Easy-Switch page shows a blank background until a contributor uploads one.
- Promotion to `verified`. Requires a hardware-confirmation round with the issue #46 tester after merge.
- Firmware variants beyond the four in scope (e.g., MX Anywhere 4 if released later).

## Source of Truth

### Logical data: Options+ extraction

`jlevere/hidpp/web/devices/profiles.json` contains profile data extracted from Logitech Options+ for the MX Anywhere 3 / 3-for-Business / 3S / 3S-for-Business family. For each device it gives us:

- `pids`: USB product IDs (Options+ typically records the Bluetooth PID)
- `buttons`: reprog-capable control IDs in decimal
- `features`: canonical feature flags (`battery`, `dpi`, `smartshift`, `hires_scroll`, `thumbwheel`, `pointer_speed`, etc.)
- `dpi`: min / max / step / default
- `hosts`: number of Easy-Switch slots

### Cross-checks

- libratbag `data/devices/logitech-MX-Anywhere-3S.device`: confirms the 3S Bluetooth PID is `0xb037` and the driver is `hidpp20`.
- `mmaher88/logitune-devices@regenerate-from-new-extractor/mx-anywhere-3/descriptor.json`: confirms the 6-control button layout (Left / Right / Middle / Back / Forward / SmartShift toggle) and that the device has no gesture button or thumb wheel.
- MX Master 3S descriptor in our repo: shows that Options+ caps DPI at 4000 for devices whose hardware actually reaches 8000, so DPI max on the 3S/3S-for-Business follows Logi marketing spec (8000) rather than the Options+ cap (4000).

## File Plan

### Created

```
devices/mx-anywhere-3/descriptor.json
devices/mx-anywhere-3/front.png
devices/mx-anywhere-3/side.png

devices/mx-anywhere-3-for-business/descriptor.json
devices/mx-anywhere-3-for-business/front.png
devices/mx-anywhere-3-for-business/side.png

devices/mx-anywhere-3s/descriptor.json
devices/mx-anywhere-3s/front.png
devices/mx-anywhere-3s/side.png

devices/mx-anywhere-3s-for-business/descriptor.json
devices/mx-anywhere-3s-for-business/front.png
devices/mx-anywhere-3s-for-business/side.png
```

All `front.png` / `side.png` are copied from the reference repo. The 3 and both 3S variants use `mx-anywhere-3`'s images. The 3-for-Business variant uses `mx-anywhere-3-for-business`'s own images (separate folder in the reference repo, potentially different logo or branding).

### Modified

- `tests/test_device_registry.cpp`: four new `DeviceSpec` entries so every build confirms the descriptors parse and expose the expected fields.
- `README.md`: support matrix extended with the four new rows, all marked `🧪 Beta`, plus a one-line note that these need community confirmation.

### Optional

- `docs/wiki/Adding-a-Device.md`: a short note explaining that the Easy-Switch button on MX Anywhere 3 / 3S is handled by firmware via the `hosts` mechanism, not as a reprog CID. Prevents future contributors from adding `0x00d7` as a control (the mistake in issue #46).

## Descriptor Content

Shared across all four descriptors (only `name`, `productIds`, `dpi.max`, and `features.pointerSpeed` differ):

```json
{
  "$schema": "../schema.json",
  "name": "MX Anywhere 3S",
  "status": "beta",
  "productIds": ["0xb037"],

  "features": {
    "battery": true,
    "adjustableDpi": true,
    "extendedDpi": false,
    "smartShift": true,
    "hiResWheel": true,
    "hiResScrolling": false,
    "lowResWheel": false,
    "smoothScroll": true,
    "thumbWheel": false,
    "reprogControls": true,
    "gestureV2": false,
    "mouseGesture": false,
    "hapticFeedback": false,
    "forceSensingButton": false,
    "crown": false,
    "reportRate": false,
    "extendedReportRate": false,
    "pointerSpeed": true,
    "leftRightSwap": false,
    "surfaceTuning": false,
    "angleSnapping": false,
    "colorLedEffects": false,
    "rgbEffects": false,
    "onboardProfiles": false,
    "gkey": false,
    "mkeys": false,
    "persistentRemappableAction": false
  },

  "dpi": { "min": 200, "max": 8000, "step": 50 },

  "controls": [
    { "controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",       "defaultActionType": "default",           "configurable": false },
    { "controlId": "0x0051", "buttonIndex": 1, "defaultName": "Right click",      "defaultActionType": "default",           "configurable": false },
    { "controlId": "0x0052", "buttonIndex": 2, "defaultName": "Middle click",     "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x0053", "buttonIndex": 3, "defaultName": "Back",             "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x0056", "buttonIndex": 4, "defaultName": "Forward",          "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x00C4", "buttonIndex": 5, "defaultName": "Shift wheel mode", "defaultActionType": "smartshift-toggle", "configurable": true  }
  ],

  "hotspots": {
    "buttons": [
      { "buttonIndex": 2, "xPct": 0.5, "yPct": 0.5, "side": "right", "labelOffsetYPct": 0.0 },
      { "buttonIndex": 3, "xPct": 0.5, "yPct": 0.5, "side": "left",  "labelOffsetYPct": 0.0 },
      { "buttonIndex": 4, "xPct": 0.5, "yPct": 0.5, "side": "left",  "labelOffsetYPct": 0.0 },
      { "buttonIndex": 5, "xPct": 0.5, "yPct": 0.5, "side": "right", "labelOffsetYPct": 0.0 }
    ],
    "scroll": [
      { "buttonIndex": -1, "xPct": 0.5, "yPct": 0.5, "side": "right", "labelOffsetYPct": 0.0, "kind": "scrollwheel" },
      { "buttonIndex": -3, "xPct": 0.5, "yPct": 0.5, "side": "right", "labelOffsetYPct": 0.0, "kind": "pointer" }
    ]
  },

  "images": { "front": "front.png", "side": "side.png", "back": "back.png" },

  "easySwitchSlots": [
    { "xPct": 0.5, "yPct": 0.5 },
    { "xPct": 0.5, "yPct": 0.5 },
    { "xPct": 0.5, "yPct": 0.5 }
  ],

  "defaultGestures": {}
}
```

### Per-variant overrides

| Variant | `name` | `productIds` | `dpi.max` | `features.pointerSpeed` |
|---------|--------|--------------|-----------|--------------------------|
| MX Anywhere 3 | `"MX Anywhere 3"` | `["0xb025", "0x4090"]` | `4000` | `false` |
| MX Anywhere 3 for Business | `"MX Anywhere 3 for Business"` | `["0xb02d"]` | `4000` | `false` |
| MX Anywhere 3S | `"MX Anywhere 3S"` | `["0xb037"]` | `8000` | `true` |
| MX Anywhere 3S for Business | `"MX Anywhere 3S for Business"` | `["0xb038"]` | `8000` | `true` |

## Verification Posture

| Field | Source of truth | Cross-check | Notes |
|-------|-----------------|-------------|-------|
| PID | jlevere/hidpp Options+ extraction | libratbag `.device` file | libratbag confirms 3S = `0xb037` |
| Protocol | libratbag (hidpp20 for all four) | | |
| Reprog-button CIDs | jlevere `buttons: [82, 83, 86, 196]` (= `0x52, 0x53, 0x56, 0xC4`) | reference repo `mx-anywhere-3` descriptor | Same CIDs across all four |
| Baked-in CIDs | Standard HID++ convention: `0x50 = Left click`, `0x51 = Right click` | MX Master 3S descriptor | Always present on any ReprogControls V4 mouse |
| Feature flags | jlevere `features` object mapped to our schema | reference repo descriptor | `pointerSpeed` enabled on S-variants only (per Options+) |
| DPI range | Logi spec sheet (3S = 200-8000, 3 = 200-4000) | Options+ extraction caps at 4000 for both; MX Master 3S ships at 8000 despite same cap | Hardware likely accepts 8000 on S variants |
| Easy-Switch host count | jlevere `hosts: 3` | All MX mice ship with 3 hosts | |
| Capability-table coverage | `src/core/hidpp/capabilities/` reviewed | Battery (Unified), ReprogControls (V4), SmartShift (standard or Enhanced) all present | No new variant tables needed |

### Known unverified risks

Flagged in the PR description so the issue #46 tester knows where to look:

1. **Bolt receiver PID** for 3S and 3S for Business: jlevere records only the Bluetooth PID. The `mx-anywhere-3` reference has two PIDs (`0x4090` Unifying + `0xb025` Bolt), so the 3S family likely also has a Bolt PID. We ship with just `0xb037` / `0xb038`; tester reports the Bolt PID if their receiver pairing fails to match.
2. **DPI max 8000** on 3S and 3S for Business: per Logi marketing. Options+ caps at 4000. Our MX Master 3S also reports 4000 in Options+ but works at 8000 via HID++. If hardware rejects set-DPI above 4000 we lower the cap.
3. **SmartShift variant**: the app picks `SmartShift` or `SmartShiftEnhanced` automatically at enumeration time. No manual work needed, but logs should show which variant the device advertises.

### Post-merge verification

Tag the issue #46 author on the PR asking them to:

- Run `logitune --debug 2>&1 | grep -E 'PID|feature'` on their 3S and paste the output.
- Confirm the SmartShift toggle works (Middle wheel-mode button cycles ratchet / free-spin).
- Confirm Middle / Back / Forward remap through the app.
- Confirm the Easy-Switch button physically cycles through 3 hosts (firmware-driven; we only need to not interfere).

## Commit Sequence

1. `feat(devices): add MX Anywhere 3S descriptor`: closes #46.
2. `feat(devices): add MX Anywhere 3S for Business descriptor`.
3. `feat(devices): add MX Anywhere 3 descriptor`.
4. `feat(devices): add MX Anywhere 3 for Business descriptor`.
5. `test: smoke-test the four MX Anywhere family descriptors`: four `DeviceSpec` entries in `tests/test_device_registry.cpp`. Run `logitune-tests` locally before committing.
6. `docs: add MX Anywhere family to README support matrix`: four rows with `🧪 Beta` badges and the community-confirmation note.
7. *(optional)* `docs(adding-a-device): note Easy-Switch button uses hosts, not reprog CIDs`: short callout in the wiki guide to prevent the next contributor from replicating the issue #46 `0x00d7` mistake.

Each device commit adds one descriptor plus two image files. The test commit lands after all four so the fixture data exists. README commit lands last to reference the full set.

## Out-of-scope Follow-ups

- Replace starter PNGs with higher-quality captures if the tester produces them.
- Contribute real `back.png` images for all four so the Easy-Switch page has a device base.
- Promote any variant from `beta` to `verified` after hardware confirmation.
- Wiki note about the `0x00d7` Easy-Switch button pitfall (listed as optional Commit 7).
