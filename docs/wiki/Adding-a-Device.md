# Adding a Device

Device support in Logitune is driven entirely by JSON descriptor files. You create a folder under `devices/`, write a `descriptor.json` by hand for the fields the app cannot infer (PIDs, CIDs, features, DPI range), then use [Editor Mode](Editor-Mode) to visually position hotspots and upload images. C++ is only required when a device uses a HID++ feature variant that has no existing capability-table entry (see [When you need C++](#when-you-need-c)).

The MX Master 3S descriptor (`devices/mx-master-3s/`) is used as the worked example throughout this guide. The full annotated file appears at the bottom.

---

## Prerequisites

Gather the following before you start:

- **Product ID (PID):** run `lsusb | grep Logitech` and note the hex ID after `ID 046d:`, or read it from Solaar (`solaar show`). Bolt-receiver connections and Bluetooth connections often report different PIDs; include both. Use the **device WPID** (e.g. `0xb034`), not the Unifying receiver's USB PID (`0xc52b`).
- **Control IDs (CIDs):** logging is enabled by default (toggle it in **Settings → Debug logging** if you disabled it). Launch Logitune, connect the device, then press each physical button one at a time. The log records a line per press:
  ```
  [logitune.device] [DEBUG] button event: CID 0xc3 pressed=true
  ```
  Tail the log while pressing:
  ```bash
  tail -f ~/.local/share/Logitune/Logitune/logs/logitune-$(date +%Y-%m-%d).log \
    | grep 'button event'
  ```
- **Features list:** during device initialization the same log records every HID++ feature the firmware advertises. Scan for them with:
  ```bash
  grep -E 'feature 0x[0-9a-f]+' ~/.local/share/Logitune/Logitune/logs/logitune-*.log | sort -u
  ```
- **DPI range:** search the log for `AdjustableDPI` messages during init — min, max, and step are printed when DeviceSession reads the sensor.
- **Device images:** front, side, and back views as PNG files with a transparent background. Placeholder files are fine for the bootstrap commit; Editor Mode can replace them later.

---

## Step 1: Create the descriptor folder

```bash
mkdir -p devices/<slug>
touch devices/<slug>/front.png
touch devices/<slug>/side.png
touch devices/<slug>/back.png
```

Replace `<slug>` with a lowercase, hyphenated folder name such as `mx-anywhere-3s`. The folder name becomes the canonical identifier used in the registry.

The folder must contain at least `descriptor.json`. Image placeholders let the app start without a verified `front.png` when `status` is `"beta"`.

---

## Step 2: Fill in the bootstrap JSON by hand

The editor cannot set PIDs, CIDs, features flags, or DPI range; those must be correct before Editor Mode can open the device. Create `devices/<slug>/descriptor.json` using the schema below.

A machine-readable [JSON Schema](https://json-schema.org) for this format lives at `devices/schema.json`. Reference it from the top of your `descriptor.json` so editors like VS Code provide inline validation and autocomplete:

```json
{
  "$schema": "../schema.json",
  "name": "MX Master 3S",
  ...
}
```

### Schema

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `name` | string | yes | Display name shown in the UI, e.g. `"MX Master 3S"` |
| `status` | string | yes | `"verified"` or `"beta"`. See [Device Support Status](Getting-Started#device-support-status) |
| `productIds` | array of string | yes | Hex PIDs as strings, e.g. `["0xb034"]`. Include all known PIDs (Bolt, Bluetooth, USB) |
| `features` | object | yes | Map of HID++ feature flags. All keys default to `false`; set to `true` if the device supports the feature |
| `features.battery` | bool | no | Battery level and charging status (HID++ 0x1000 / 0x1004) |
| `features.adjustableDpi` | bool | no | DPI slider (HID++ 0x2201) |
| `features.extendedDpi` | bool | no | Extended DPI range above 8000 (HID++ 0x2202) |
| `features.smartShift` | bool | no | SmartShift ratchet/freespin toggle (HID++ 0x2110 / 0x2111) |
| `features.hiResWheel` | bool | no | High-resolution scroll wheel (HID++ 0x2121) |
| `features.hiResScrolling` | bool | no | Hi-res scrolling mode toggle |
| `features.lowResWheel` | bool | no | Low-resolution wheel fallback |
| `features.smoothScroll` | bool | no | Smooth-scroll animation; defaults to `true` if omitted |
| `features.thumbWheel` | bool | no | Horizontal thumb wheel (HID++ 0x2150) |
| `features.reprogControls` | bool | no | Button remapping via ReprogControlsV4 (HID++ 0x1B04) |
| `features.gestureV2` | bool | no | GestureV2 feature (HID++ 0x6501) |
| `features.mouseGesture` | bool | no | MouseGesture feature |
| `features.hapticFeedback` | bool | no | Haptic feedback |
| `features.forceSensingButton` | bool | no | Force-sensing button |
| `features.crown` | bool | no | Crown / dial control |
| `features.reportRate` | bool | no | Report rate selection |
| `features.extendedReportRate` | bool | no | Extended report rate options |
| `features.pointerSpeed` | bool | no | Pointer speed (separate from DPI) |
| `features.leftRightSwap` | bool | no | Left/right button swap |
| `features.surfaceTuning` | bool | no | Surface calibration |
| `features.angleSnapping` | bool | no | Angle snapping |
| `features.colorLedEffects` | bool | no | Single-color LED effects |
| `features.rgbEffects` | bool | no | RGB LED effects |
| `features.onboardProfiles` | bool | no | On-board profile storage |
| `features.gkey` | bool | no | G-key macro buttons |
| `features.mkeys` | bool | no | M-key mode buttons |
| `features.persistentRemappableAction` | bool | no | Persistent remappable action |
| `dpi` | object | no | DPI configuration. Omit if `adjustableDpi` is false |
| `dpi.min` | int | no | Minimum DPI. Defaults to `200` |
| `dpi.max` | int | no | Maximum DPI. Defaults to `8000` |
| `dpi.step` | int | no | DPI increment. Defaults to `50` |
| `controls` | array of object | yes (if verified) | One entry per button or virtual control |
| `controls[].controlId` | string | yes | HID++ CID as a hex string, e.g. `"0x0050"` |
| `controls[].buttonIndex` | int | yes | Zero-based index into the profile buttons array |
| `controls[].defaultName` | string | yes | Display name for the control, e.g. `"Left click"` |
| `controls[].defaultActionType` | string | yes | Default action: `"default"`, `"gesture-trigger"`, `"smartshift-toggle"`, `"dpi-cycle"`, `"keystroke"`, `"app-launch"`, `"dbus"`, `"media"` |
| `controls[].configurable` | bool | yes | Whether the user can reassign this button |
| `controls[].displayName` | string | no | Optional override for the label shown in the button list |
| `hotspots` | object | yes (if verified) | Interactive overlay positions for the device image |
| `hotspots.buttons` | array of object | yes (if verified) | One entry per configurable button shown on the Buttons page |
| `hotspots.buttons[].buttonIndex` | int | yes | Matches `controls[].buttonIndex` |
| `hotspots.buttons[].xPct` | float | yes | Horizontal position as a fraction of image width (0.0 to 1.0) |
| `hotspots.buttons[].yPct` | float | yes | Vertical position as a fraction of image height (0.0 to 1.0) |
| `hotspots.buttons[].side` | string | yes | Which side of the hotspot the label line extends to: `"left"` or `"right"` |
| `hotspots.buttons[].labelOffsetYPct` | float | no | Vertical label offset to prevent overlap with adjacent hotspots. Defaults to `0.0` |
| `hotspots.scroll` | array of object | no | Scroll-zone hotspots on the Point & Scroll page |
| `hotspots.scroll[].buttonIndex` | int | yes | Negative sentinel: `-1` = main wheel, `-2` = thumb wheel, `-3` = pointer |
| `hotspots.scroll[].xPct` | float | yes | Horizontal position fraction |
| `hotspots.scroll[].yPct` | float | yes | Vertical position fraction |
| `hotspots.scroll[].side` | string | yes | `"left"` or `"right"` |
| `hotspots.scroll[].labelOffsetYPct` | float | no | Vertical label offset. Defaults to `0.0` |
| `hotspots.scroll[].kind` | string | no | Scroll zone type: `"scrollwheel"`, `"thumbwheel"`, or `"pointer"` |
| `images` | object | no | Image file names relative to the descriptor folder |
| `images.front` | string | no | Front-view PNG, e.g. `"front.png"` |
| `images.side` | string | no | Side-view PNG |
| `images.back` | string | no | Back-view PNG |
| `easySwitchSlots` | array of object | no | Easy-Switch slot circle positions on the Easy-Switch page |
| `easySwitchSlots[].xPct` | float | yes | Horizontal position fraction |
| `easySwitchSlots[].yPct` | float | yes | Vertical position fraction |
| `easySwitchSlots[].label` | string | no | Optional slot label override |
| `defaultGestures` | object | no | Default gesture actions keyed by direction: `"up"`, `"down"`, `"left"`, `"right"`, `"click"` |
| `defaultGestures.<dir>.type` | string | yes | Action type string (same values as `defaultActionType`) |
| `defaultGestures.<dir>.payload` | string | no | Action payload, e.g. `"Super+D"` for a keystroke |

> **Tip on feature flags:** you do not need to know which HID++ sub-variant a feature uses. Set `"battery": true` regardless of whether the device uses Battery Unified (0x1004) or Battery Status (0x1000). DeviceManager selects the right variant at runtime via capability dispatch tables in `src/core/hidpp/capabilities/`.

### Minimal bootstrap example

```json
{
  "name": "My Device",
  "status": "beta",
  "productIds": ["0xb037"],
  "features": {
    "battery": true,
    "adjustableDpi": true,
    "reprogControls": true
  },
  "dpi": { "min": 200, "max": 4000, "step": 50 },
  "controls": [
    { "controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",   "defaultActionType": "default", "configurable": false },
    { "controlId": "0x0051", "buttonIndex": 1, "defaultName": "Right click",  "defaultActionType": "default", "configurable": false },
    { "controlId": "0x0052", "buttonIndex": 2, "defaultName": "Middle click", "defaultActionType": "default", "configurable": true  }
  ],
  "hotspots": {
    "buttons": [
      { "buttonIndex": 2, "xPct": 0.65, "yPct": 0.20, "side": "right" }
    ],
    "scroll": []
  },
  "images": {
    "front": "front.png",
    "side":  "side.png",
    "back":  "back.png"
  }
}
```

Placeholder coordinates are fine for the bootstrap commit. Editor Mode is the right tool for positioning hotspots precisely.

---

## Step 3: Register with DeviceRegistry

DeviceRegistry scans three directories in order on startup and appends each successfully loaded descriptor to an internal list. `findByPid` returns the **first** match, so earlier-scanned directories take precedence over later ones for the same PID:

1. **System directory:** `$XDG_DATA_DIRS/logitune/devices` (typically `/usr/share/logitune/devices`). This is where `cmake --install` places the `devices/` folder from the repo.
2. **Cache directory:** `$XDG_CACHE_HOME/logitune/devices` (typically `~/.cache/logitune/devices`). Not normally used directly.
3. **User directory:** `$XDG_DATA_HOME/logitune/devices` (typically `~/.local/share/logitune/devices`). Drop a folder here to test a descriptor without rebuilding.

Because system descriptors are scanned first, a user-local descriptor for a PID that already exists in the system directory will not be picked up. To test a local descriptor for a device that has a system descriptor, either use a PID that is absent from the system directory or remove the system descriptor temporarily.

### Path A: Contribute to the repo (built-in descriptor)

Place your folder under `devices/` in the repository root:

```
devices/
  mx-master-3s/
    descriptor.json
    front.png
    side.png
    back.png
  my-device/          <-- add your folder here
    descriptor.json
    front.png
    side.png
    back.png
```

The root `CMakeLists.txt` installs everything under `devices/` to `${CMAKE_INSTALL_DATADIR}/logitune/devices` automatically. No CMake edits required.

### Path B: Local testing without a rebuild

```bash
mkdir -p ~/.local/share/logitune/devices/my-device
cp -r devices/my-device/* ~/.local/share/logitune/devices/my-device/
```

DeviceRegistry picks this up on next launch. This path is also useful for testing a descriptor against an installed system binary.

---

## Step 4: Polish with Editor Mode

Once the JSON parses without errors (launch Logitune normally and scan the log file at `~/.local/share/Logitune/Logitune/logs/logitune-*.log` for `JsonDevice` warnings), open Editor Mode to refine the visual layout:

```bash
logitune --edit --simulate-all
```

Editor Mode lets you drag hotspot handles to the correct positions on the device image, drag Easy-Switch slot circles, upload real device images, and rename controls. All changes are written back to `descriptor.json` atomically.

For the full Editor Mode walk-through, see [Editor Mode](Editor-Mode).

---

## Step 5: Test

### Eyeball without hardware

```bash
logitune --simulate-all
```

This loads all descriptors in simulate mode, bypassing the real HID++ device. Use it to verify the UI renders correctly: images load, hotspots appear in the right places, the Buttons page lists all controls.

### Smoke-test checklist

- [ ] Device appears in the device carousel
- [ ] Front image renders on the Buttons page
- [ ] Hotspots are positioned over the correct buttons
- [ ] Button list shows the right names and action types
- [ ] Point & Scroll page shows scroll hotspots
- [ ] Easy-Switch page shows three slot circles
- [ ] DPI range and step display correctly
- [ ] No `JsonDevice` warnings in `~/.local/share/Logitune/Logitune/logs/logitune-*.log`

### Descriptor fixture test

The test suite uses the `DeviceSpec` parameterized pattern in `tests/test_device_registry.cpp`. Add an entry to `kDevices[]` for your descriptor:

```cpp
{
    .pid = 0xb037,
    .name = "My Device",
    .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
    .buttonHotspots = 1, .scrollHotspots = 0,
    .minControls = 3,
    .control0Cid = 0x0050, .control5Cid = 0x0052,
    .control5ActionType = "default",
    .control6ActionType = "default",
    .battery = true, .adjustableDpi = true, .smartShift = false,
    .reprogControls = true, .gestureV2 = false,
    .gestureDownType = ButtonAction::Default,
    .gestureDownPayload = "",
    .gestureUpType = ButtonAction::Default,
},
```

Then run:

```bash
logitune-tests
```

All tests must pass before submitting a PR.

---

## Step 6: Submit a PR

Set `"status": "beta"` unless you physically own the device and have verified end-to-end behavior (all configurable buttons divert and un-divert, DPI changes apply, battery reads correctly). Only a maintainer can promote a descriptor to `"verified"`.

See [Contributing](Contributing) for the commit format, branch naming, and PR checklist.

---

## When you need C++

The JSON workflow covers the large majority of devices. You need C++ only when a device uses a **HID++ feature variant** that has no existing capability-table entry.

Examples of variants that already have C++ entries (no action needed):
- Battery: both 0x1000 (Battery Status) and 0x1004 (Battery Unified) are handled.
- SmartShift: both 0x2110 (V1) and 0x2111 (Enhanced) are handled.

If your device advertises a feature at an address the capability tables do not cover, add a new entry in `src/core/hidpp/capabilities/`. The dispatch tables are designed so each new variant is one table entry with no changes to DeviceManager.

See [Architecture: Device Registry](Architecture#device-registry) for details on how the dispatch tables work.

---

> **Note on Easy-Switch buttons.** Some mice (MX Anywhere family, Logi Wave, etc.) have a physical connection-switch button on top of the device. This button is handled by firmware through the multi-host switching mechanism; it is **not** a reprog-controls CID. Do not add an entry like `{ "controlId": "0x00d7" }` to `controls`. The visual 3-dot indicator on the device bottom is represented by the `easySwitchSlots` array instead.

---

## Reference: MX Master 3S descriptor

The complete descriptor for the MX Master 3S. Copy the JSON block as-is; the field guide below explains the notable choices.

```json
{
  "name": "MX Master 3S",
  "status": "verified",
  "productIds": ["0xb034"],

  "features": {
    "battery": true,
    "adjustableDpi": true,
    "extendedDpi": false,
    "smartShift": true,
    "hiResWheel": true,
    "hiResScrolling": false,
    "lowResWheel": false,
    "smoothScroll": true,
    "thumbWheel": true,
    "reprogControls": true,
    "gestureV2": false,
    "mouseGesture": false,
    "hapticFeedback": false,
    "forceSensingButton": false,
    "crown": false,
    "reportRate": false,
    "extendedReportRate": false,
    "pointerSpeed": false,
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

  "dpi": {
    "min": 200,
    "max": 8000,
    "step": 50
  },

  "controls": [
    { "controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",       "defaultActionType": "default",           "configurable": false },
    { "controlId": "0x0051", "buttonIndex": 1, "defaultName": "Right click",      "defaultActionType": "default",           "configurable": false },
    { "controlId": "0x0052", "buttonIndex": 2, "defaultName": "Middle click",     "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x0053", "buttonIndex": 3, "defaultName": "Back",             "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x0056", "buttonIndex": 4, "defaultName": "Forward",          "defaultActionType": "default",           "configurable": true  },
    { "controlId": "0x00C3", "buttonIndex": 5, "defaultName": "Gesture button",   "defaultActionType": "gesture-trigger",   "configurable": true  },
    { "controlId": "0x00C4", "buttonIndex": 6, "defaultName": "Shift wheel mode", "defaultActionType": "smartshift-toggle", "configurable": true  },
    { "controlId": "0x0000", "buttonIndex": 7, "defaultName": "Thumb wheel",      "defaultActionType": "default",           "configurable": true  }
  ],

  "hotspots": {
    "buttons": [
      { "buttonIndex": 2,  "xPct": 0.71, "yPct": 0.15,  "side": "right", "labelOffsetYPct": 0.0  },
      { "buttonIndex": 6,  "xPct": 0.81, "yPct": 0.34,  "side": "right", "labelOffsetYPct": 0.0  },
      { "buttonIndex": 7,  "xPct": 0.55, "yPct": 0.515, "side": "right", "labelOffsetYPct": 0.0  },
      { "buttonIndex": 4,  "xPct": 0.35, "yPct": 0.43,  "side": "left",  "labelOffsetYPct": 0.0  },
      { "buttonIndex": 3,  "xPct": 0.45, "yPct": 0.60,  "side": "left",  "labelOffsetYPct": 0.20 },
      { "buttonIndex": 5,  "xPct": 0.08, "yPct": 0.58,  "side": "left",  "labelOffsetYPct": 0.0  }
    ],
    "scroll": [
      { "buttonIndex": -1, "xPct": 0.73, "yPct": 0.16, "side": "right", "labelOffsetYPct": 0.0, "kind": "scrollwheel" },
      { "buttonIndex": -2, "xPct": 0.55, "yPct": 0.51, "side": "left",  "labelOffsetYPct": 0.0, "kind": "thumbwheel"  },
      { "buttonIndex": -3, "xPct": 0.83, "yPct": 0.54, "side": "right", "labelOffsetYPct": 0.0, "kind": "pointer"     }
    ]
  },

  "images": {
    "front": "front.png",
    "side":  "side.png",
    "back":  "back.png"
  },

  "easySwitchSlots": [
    { "xPct": 0.325, "yPct": 0.658 },
    { "xPct": 0.384, "yPct": 0.642 },
    { "xPct": 0.443, "yPct": 0.643 }
  ],

  "defaultGestures": {
    "up":    { "type": "Default",   "payload": "" },
    "down":  { "type": "Keystroke", "payload": "Super+D" },
    "left":  { "type": "Keystroke", "payload": "Ctrl+Super+Left" },
    "right": { "type": "Keystroke", "payload": "Ctrl+Super+Right" },
    "click": { "type": "Keystroke", "payload": "Super+W" }
  }
}
```

### Field guide

- **`features.battery`** -- battery level and charging status via HID++ 0x1004 (Battery Unified). DeviceManager also handles 0x1000 (Battery Status); set `true` for either variant.
- **`features.adjustableDpi`** -- DPI slider via HID++ 0x2201.
- **`features.smartShift`** -- ratchet/freespin toggle via HID++ 0x2110/0x2111.
- **`features.hiResWheel`** -- hi-res scroll wheel via HID++ 0x2121.
- **`features.thumbWheel`** -- horizontal thumb wheel via HID++ 0x2150.
- **`features.reprogControls`** -- button remapping via HID++ 0x1B04 (ReprogControlsV4).
- **`controls` entries with `configurable: false`** (Left click, Right click) -- non-configurable buttons still need entries so the profile button array is indexed correctly.
- **`controlId: "0x0000"` (Thumb wheel)** -- virtual entry; the thumb wheel is driven by HID++ 0x2150, not ReprogControlsV4, so no real CID exists for it.
- **`hotspots.buttons[].xPct` / `yPct`** -- fractions of image dimensions (0.0 to 1.0). The `side` field controls which direction the callout line extends from the hotspot dot.
- **`hotspots.scroll` negative `buttonIndex` sentinels** -- `-1` = main scroll wheel, `-2` = thumb wheel, `-3` = pointer/sensor area.
