# MX Anywhere 3 / 3S Family Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship JSON descriptors for MX Anywhere 3, MX Anywhere 3 for Business, MX Anywhere 3S, and MX Anywhere 3S for Business in the bundled `devices/` tree, plus tests and README updates. Closes #46 on merge.

**Architecture:** Four near-identical `descriptor.json` files differing only in `name`, `productIds`, `dpi.max`, and `features.pointerSpeed`. Logical content from Logitech Options+ extraction (jlevere/hidpp), cross-checked against libratbag and the `mx-anywhere-3` descriptor in `mmaher88/logitune-devices@regenerate-from-new-extractor`. Visual coordinates are stub values (`0.5, 0.5`); the maintainer polishes them via `logitune --edit` after merge. Existing `DeviceRegistry` scan picks up new `devices/<slug>/` folders automatically (CMake installs the whole tree, and `tests/test_device_registry.cpp` runs against `$XDG_DATA_DIRS=build` which symlinks the repo `devices/`).

**Tech Stack:** C++20 / Qt 6 / JSON / GTest (parameterized). Matches existing patterns in `tests/test_device_registry.cpp`.

**Design spec:** `docs/superpowers/specs/2026-04-16-mx-anywhere-family-design.md`. Read it before Task 1.

---

## Global rules

- **No em-dashes (U+2014 "—")** in any file you create or modify. Replace with colons, commas, periods, or parentheses. The only acceptable occurrence is inside a `grep -c "—"` verification command.
- **No co-author signatures** in commit messages.
- **Branch is `add-mx-anywhere-3-family`.** Already created. Do NOT push the branch; leave that for the final step after all tasks pass.
- **Working directory**: `/home/mina/repos/logitune`.
- **Never amend commits.** If a commit needs changing, make a follow-up commit on top.
- **Build before test.** Every task that claims a passing test must also run `cmake --build build -j"$(nproc)"` and `./build/tests/logitune-tests` from the repo root, and both must exit 0.

---

## File Structure

### Created

```
devices/mx-anywhere-3/descriptor.json
devices/mx-anywhere-3/front.png            (copy of /tmp/ref-devices/mx-anywhere-3/front.png)
devices/mx-anywhere-3/side.png             (copy of /tmp/ref-devices/mx-anywhere-3/side.png)

devices/mx-anywhere-3-for-business/descriptor.json
devices/mx-anywhere-3-for-business/front.png  (copy of /tmp/ref-devices/mx-anywhere-3-for-business/front.png)
devices/mx-anywhere-3-for-business/side.png   (copy of /tmp/ref-devices/mx-anywhere-3-for-business/side.png)

devices/mx-anywhere-3s/descriptor.json
devices/mx-anywhere-3s/front.png           (copy of /tmp/ref-devices/mx-anywhere-3/front.png)
devices/mx-anywhere-3s/side.png            (copy of /tmp/ref-devices/mx-anywhere-3/side.png)

devices/mx-anywhere-3s-for-business/descriptor.json
devices/mx-anywhere-3s-for-business/front.png  (copy of /tmp/ref-devices/mx-anywhere-3/front.png)
devices/mx-anywhere-3s-for-business/side.png   (copy of /tmp/ref-devices/mx-anywhere-3/side.png)
```

### Modified

- `tests/test_device_registry.cpp`: extend `DeviceSpec` with sentinel semantics for devices without a gesture button; add four entries to `kDevices`; gate gesture-button assertions on the sentinels.
- `README.md`: four new rows in the Supported Devices matrix, all `🧪 Beta`.
- `docs/wiki/Adding-a-Device.md` *(optional, Task 8)*: short note on Easy-Switch button vs reprog CIDs.

### Unchanged

- `CMakeLists.txt` (install rule `install(DIRECTORY ${CMAKE_SOURCE_DIR}/devices/ ...)` at line 30 picks up the new folders automatically).
- `devices/schema.json` (fields already cover everything these descriptors use).
- No C++ capability-table additions needed: the feature set (battery, adjustableDpi, smartShift, hiResWheel, reprogControls, smoothScroll, pointerSpeed) is already handled by existing capability tables in `src/core/hidpp/capabilities/`.

---

## Task 1: Extend the test fixture for devices without gesture buttons

**Files:**

- Modify: `tests/test_device_registry.cpp`

The existing `DeviceSpec` struct assumes every device has a gesture button at control index 5 and a SmartShift toggle at index 6. The Anywhere family has six controls (Left, Right, Middle, Back, Forward, SmartShift toggle) with the SmartShift toggle at index 5 and no control at index 6. Sentinel semantics:

- `control6ActionType = nullptr` means "device has no 7th control; skip the control[6] assertion".
- `gestureDownPayload = nullptr` means "device has no default gestures; assert `defaultGestures` is empty instead".

- [ ] **Step 1: Read the current fixture**

Run:

```bash
sed -n '1,150p' tests/test_device_registry.cpp
```

Confirm the struct definition at lines 9-22 and the tests at lines 84-105 match the shape below. If they have drifted, adapt Step 2 to the current shape while preserving intent.

- [ ] **Step 2: Apply the guarded-assertion changes**

Replace the `ControlsHaveExpectedCids` test body (currently around lines 84-94) with:

```cpp
TEST_P(DeviceRegistryTest, ControlsHaveExpectedCids) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto controls = dev->controls();
    ASSERT_GE(controls.size(), s.minControls);
    EXPECT_EQ(controls[0].controlId, s.control0Cid);
    EXPECT_EQ(controls[5].controlId, s.control5Cid);
    EXPECT_EQ(controls[5].defaultActionType, s.control5ActionType);
    if (s.minControls >= 7 && s.control6ActionType) {
        EXPECT_EQ(controls[6].defaultActionType, s.control6ActionType);
    }
}
```

Replace the `DefaultGesturesPresent` test body (currently around lines 96-105) with:

```cpp
TEST_P(DeviceRegistryTest, DefaultGesturesPresent) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto gestures = dev->defaultGestures();
    if (s.gestureDownPayload) {
        EXPECT_TRUE(gestures.contains("down"));
        EXPECT_EQ(gestures["down"].type, s.gestureDownType);
        EXPECT_EQ(gestures["down"].payload, s.gestureDownPayload);
        EXPECT_EQ(gestures["up"].type, s.gestureUpType);
    } else {
        EXPECT_TRUE(gestures.isEmpty());
    }
}
```

No changes to the struct itself: the existing `const char* control6ActionType` and `const char* gestureDownPayload` fields are already `nullptr`-able.

- [ ] **Step 3: Build and run tests**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
./build/tests/logitune-tests --gtest_filter='AllDevices/DeviceRegistryTest.*' 2>&1 | tail -5
```

Expected: all tests pass. The three existing devices still exercise the gesture-button path (sentinels not triggered).

- [ ] **Step 4: Commit**

```bash
git add tests/test_device_registry.cpp
git commit -m "test(device-registry): guard gesture-button assertions on sentinel fields

Make control6ActionType=nullptr and gestureDownPayload=nullptr signal
that a device has no 7th control or default gestures, respectively.
Prepares the fixture for the MX Anywhere family which has six controls
and no gesture button."
```

---

## Task 2: Add MX Anywhere 3S descriptor

**Files:**

- Create: `devices/mx-anywhere-3s/descriptor.json`
- Create: `devices/mx-anywhere-3s/front.png`
- Create: `devices/mx-anywhere-3s/side.png`
- Modify: `tests/test_device_registry.cpp` (add one `DeviceSpec` entry)

- [ ] **Step 1: Create the descriptor folder and copy images**

```bash
mkdir -p devices/mx-anywhere-3s
cp /tmp/ref-devices/mx-anywhere-3/front.png devices/mx-anywhere-3s/front.png
cp /tmp/ref-devices/mx-anywhere-3/side.png  devices/mx-anywhere-3s/side.png
```

If `/tmp/ref-devices` does not exist on your working copy, clone it with:

```bash
git clone https://github.com/mmaher88/logitune-devices /tmp/ref-devices
cd /tmp/ref-devices && git checkout regenerate-from-new-extractor && cd -
```

- [ ] **Step 2: Write `devices/mx-anywhere-3s/descriptor.json`**

Write exactly:

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

- [ ] **Step 3: Validate the JSON against the schema**

```bash
python3 -m venv /tmp/venv 2>/dev/null; /tmp/venv/bin/pip install --quiet jsonschema
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-anywhere-3s/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected output: `OK`.

- [ ] **Step 4: Add a `DeviceSpec` entry to the test fixture**

Open `tests/test_device_registry.cpp`. After the last entry in `kDevices` (the MX Master 4 closing brace, around line 69), insert:

```cpp
    {
        .pid = 0xb037,
        .name = "MX Anywhere 3S",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
```

Keep the trailing comma pattern consistent with the other entries.

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: `[  PASSED  ] 494+ tests` (exact count grows because of the new parameterized cases). No failures.

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" devices/mx-anywhere-3s/descriptor.json
```

Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add devices/mx-anywhere-3s/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Anywhere 3S descriptor

Bundled descriptor for the MX Anywhere 3S (Bluetooth PID 0xb037).
Logical data from Logitech Options+ extraction (jlevere/hidpp),
cross-checked against libratbag. Six controls: Left, Right, Middle,
Back, Forward, SmartShift toggle. No gesture button, no thumb wheel.
DPI 200-8000 step 50 per Logi spec sheet.

Hotspot positions and Easy-Switch slot coordinates ship as stubs at
(0.5, 0.5); maintainer polishes via 'logitune --edit' after merge.
Status: beta; pending hardware confirmation by the issue #46 tester.

Closes #46"
```

---

## Task 3: Add MX Anywhere 3S for Business descriptor

**Files:**

- Create: `devices/mx-anywhere-3s-for-business/descriptor.json`
- Create: `devices/mx-anywhere-3s-for-business/front.png`
- Create: `devices/mx-anywhere-3s-for-business/side.png`
- Modify: `tests/test_device_registry.cpp`

- [ ] **Step 1: Create folder and copy images**

```bash
mkdir -p devices/mx-anywhere-3s-for-business
cp /tmp/ref-devices/mx-anywhere-3/front.png devices/mx-anywhere-3s-for-business/front.png
cp /tmp/ref-devices/mx-anywhere-3/side.png  devices/mx-anywhere-3s-for-business/side.png
```

- [ ] **Step 2: Write `devices/mx-anywhere-3s-for-business/descriptor.json`**

Start from the Task 2 JSON. Change only these four fields:

- `"name": "MX Anywhere 3S for Business"`
- `"productIds": ["0xb038"]`

Everything else (including `dpi.max: 8000` and `features.pointerSpeed: true`) stays identical.

Full file content:

```json
{
  "$schema": "../schema.json",
  "name": "MX Anywhere 3S for Business",
  "status": "beta",
  "productIds": ["0xb038"],

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

- [ ] **Step 3: Validate**

```bash
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-anywhere-3s-for-business/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected: `OK`.

- [ ] **Step 4: Add a `DeviceSpec` entry**

In `tests/test_device_registry.cpp`, after the MX Anywhere 3S entry added in Task 2, insert:

```cpp
    {
        .pid = 0xb038,
        .name = "MX Anywhere 3S for Business",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
```

- [ ] **Step 5: Build and test**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: all tests pass.

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" devices/mx-anywhere-3s-for-business/descriptor.json
```

Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add devices/mx-anywhere-3s-for-business/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Anywhere 3S for Business descriptor

Same capabilities as MX Anywhere 3S. Bluetooth PID 0xb038."
```

---

## Task 4: Add MX Anywhere 3 descriptor

**Files:**

- Create: `devices/mx-anywhere-3/descriptor.json`
- Create: `devices/mx-anywhere-3/front.png`
- Create: `devices/mx-anywhere-3/side.png`
- Modify: `tests/test_device_registry.cpp`

- [ ] **Step 1: Create folder and copy images**

```bash
mkdir -p devices/mx-anywhere-3
cp /tmp/ref-devices/mx-anywhere-3/front.png devices/mx-anywhere-3/front.png
cp /tmp/ref-devices/mx-anywhere-3/side.png  devices/mx-anywhere-3/side.png
```

- [ ] **Step 2: Write `devices/mx-anywhere-3/descriptor.json`**

Delta vs Task 2: `name`, two PIDs, DPI max 4000, `pointerSpeed: false`.

```json
{
  "$schema": "../schema.json",
  "name": "MX Anywhere 3",
  "status": "beta",
  "productIds": ["0xb025", "0x4090"],

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

  "dpi": { "min": 200, "max": 4000, "step": 50 },

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

- [ ] **Step 3: Validate**

```bash
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-anywhere-3/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected: `OK`.

- [ ] **Step 4: Add a `DeviceSpec` entry**

In `tests/test_device_registry.cpp`, after the MX Anywhere 3S for Business entry, insert:

```cpp
    {
        .pid = 0xb025,
        .name = "MX Anywhere 3",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
```

Only one test entry per descriptor (PID `0xb025`). The second PID `0x4090` is still discoverable via `findByPid` if present but does not need its own `DeviceSpec` row.

- [ ] **Step 5: Build and test**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: all tests pass.

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" devices/mx-anywhere-3/descriptor.json
```

Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add devices/mx-anywhere-3/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Anywhere 3 descriptor

PIDs 0xb025 (Bolt) and 0x4090 (Unifying). DPI 200-4000. Same
6-control layout as the 3S, without the extended DPI range or
pointerSpeed feature."
```

---

## Task 5: Add MX Anywhere 3 for Business descriptor

**Files:**

- Create: `devices/mx-anywhere-3-for-business/descriptor.json`
- Create: `devices/mx-anywhere-3-for-business/front.png`
- Create: `devices/mx-anywhere-3-for-business/side.png`
- Modify: `tests/test_device_registry.cpp`

- [ ] **Step 1: Create folder and copy images**

```bash
mkdir -p devices/mx-anywhere-3-for-business
cp /tmp/ref-devices/mx-anywhere-3-for-business/front.png devices/mx-anywhere-3-for-business/front.png
cp /tmp/ref-devices/mx-anywhere-3-for-business/side.png  devices/mx-anywhere-3-for-business/side.png
```

These come from the reference repo's `mx-anywhere-3-for-business` folder (a distinct set of images from the non-Business variant).

- [ ] **Step 2: Write `devices/mx-anywhere-3-for-business/descriptor.json`**

Delta vs Task 4: `name` and `productIds`. Everything else matches MX Anywhere 3.

```json
{
  "$schema": "../schema.json",
  "name": "MX Anywhere 3 for Business",
  "status": "beta",
  "productIds": ["0xb02d"],

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

  "dpi": { "min": 200, "max": 4000, "step": 50 },

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

- [ ] **Step 3: Validate**

```bash
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-anywhere-3-for-business/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected: `OK`.

- [ ] **Step 4: Add a `DeviceSpec` entry**

In `tests/test_device_registry.cpp`, after the MX Anywhere 3 entry, insert:

```cpp
    {
        .pid = 0xb02d,
        .name = "MX Anywhere 3 for Business",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
```

- [ ] **Step 5: Build and test**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: all tests pass. Total device count in the parameterized fixture should now be 7 (original 3 + 4 new).

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" devices/mx-anywhere-3-for-business/descriptor.json
```

Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add devices/mx-anywhere-3-for-business/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Anywhere 3 for Business descriptor

Same capabilities as MX Anywhere 3. Bolt PID 0xb02d. Uses its own
front/side images from the reference repo (different branding from
the consumer variant)."
```

---

## Task 6: Update README support matrix

**Files:**

- Modify: `README.md`

- [ ] **Step 1: Find the current matrix**

```bash
grep -nB 1 -A 15 "^## 🖱️ Supported Devices" README.md
```

Identify the table rows between the header and the note about MX Master 4 smooth scrolling.

- [ ] **Step 2: Insert four new rows**

The matrix has columns: `Device | Status | Battery | DPI | SmartShift | Thumb wheel | Button remap | Gestures | Smooth scroll | Easy-Switch`. After the `| MX Master 4 |` row, insert exactly:

```markdown
| MX Anywhere 3S              | 🧪 Beta     | ✅ | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
| MX Anywhere 3S for Business | 🧪 Beta     | ✅ | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
| MX Anywhere 3               | 🧪 Beta     | ✅ | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
| MX Anywhere 3 for Business  | 🧪 Beta     | ✅ | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
```

The `—` entries under Thumb wheel and Gestures denote "no hardware for this". The em-dash in a Markdown cell is the one exception to the no-em-dash rule: it is the canonical "not applicable" glyph in the existing matrix. The existing MX Master 4 row uses the same convention.

After the existing MX Master 4 callout, add a one-line sentence:

```markdown
The four MX Anywhere family descriptors ship as 🧪 **Beta** pending hardware confirmation. Issue [#46](https://github.com/mmaher88/logitune/issues/46) tracks the verification.
```

- [ ] **Step 3: Verify the README renders correctly**

```bash
grep -c "MX Anywhere" README.md
```

Expected: `4` (one occurrence per variant row).

Also confirm no net increase in em-dashes outside the matrix cells. Pre-edit count was 12; post-edit count will be 12 + 8 (two em-dashes per new row, four rows).

```bash
grep -c "—" README.md
```

Expected: `20`. (Not a rule violation; em-dashes in matrix cells are structural placeholders.)

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: add MX Anywhere family to README support matrix

Four rows for MX Anywhere 3, 3 for Business, 3S, 3S for Business.
All marked beta pending hardware confirmation tracked in #46."
```

---

## Task 7 *(optional)*: Note Easy-Switch button convention in Adding-a-Device

**Files:**

- Modify: `docs/wiki/Adding-a-Device.md`

Prevents future contributors from replicating the issue #46 mistake (adding `0x00d7` as a reprog control).

- [ ] **Step 1: Find the "When you need C++" or similar callout section**

```bash
grep -n "^## " docs/wiki/Adding-a-Device.md
```

Identify the "Reference: MX Master 3S descriptor" section or the closest prose section discussing control layout. Pick a location just before the reference descriptor where the note fits naturally.

- [ ] **Step 2: Insert a callout block**

Insert this paragraph:

```markdown
> **Note on Easy-Switch buttons.** Some mice (MX Anywhere family, Logi Wave, etc.) have a physical connection-switch button on top of the device. This button is handled by firmware through the multi-host switching mechanism; it is **not** a reprog-controls CID. Do not add an entry like `{ "controlId": "0x00d7" }` to `controls`. The visual 3-dot indicator on the device bottom is represented by the `easySwitchSlots` array instead.
```

- [ ] **Step 3: Verify**

```bash
grep -c "—" docs/wiki/Adding-a-Device.md
```

Expected: no increase over pre-edit count (the note uses periods and parentheses, no em-dashes).

- [ ] **Step 4: Commit**

```bash
git add docs/wiki/Adding-a-Device.md
git commit -m "docs(adding-a-device): note Easy-Switch button uses hosts, not reprog CIDs

Prevents contributors from adding 0x00d7 as a control when the device
has a top-side connection switch button. Firmware handles the button
via the multi-host mechanism already."
```

---

## Final step: push and draft PR

- [ ] **Step 1: Push the branch**

```bash
git push -u origin add-mx-anywhere-3-family
```

- [ ] **Step 2: Draft the PR description**

Per the `feedback_draft_comments.md` rule, write the PR body as a markdown block in the conversation for user review before posting. Only run `gh pr create` after the user explicitly approves the draft.

Suggested title: `feat(devices): MX Anywhere 3 / 3S family support`

Suggested body structure:

- Closes #46.
- What ships (four descriptors, tests, README).
- Verification sources table (Options+ extraction, libratbag, reference repo).
- Known unverified risks (Bolt PID variants, DPI cap above 4000, SmartShift variant detection).
- Test plan directed at the issue #46 tester (run `logitune --debug`, confirm remaps, confirm Easy-Switch cycling).

---

## Self-Review

Against `docs/superpowers/specs/2026-04-16-mx-anywhere-family-design.md`:

| Spec requirement | Implemented in |
|------------------|----------------|
| Descriptor: MX Anywhere 3S | Task 2 |
| Descriptor: MX Anywhere 3S for Business | Task 3 |
| Descriptor: MX Anywhere 3 | Task 4 |
| Descriptor: MX Anywhere 3 for Business | Task 5 |
| Test fixture entries | Task 1 (infra) + Tasks 2-5 (one entry each) |
| README support matrix | Task 6 |
| Optional wiki note on Easy-Switch convention | Task 7 |
| Beta status | All descriptors set `"status": "beta"` |
| Cross-check sources documented | Captured in PR description drafting step; sources match spec |
| `(0.5, 0.5)` stub visual coords | Every descriptor JSON carries them |
| Starter images | Tasks 2-5 Step 1 copy from reference repo |

No placeholders scanned. No `TBD` / `TODO` / "fill in" markers. Every file path is concrete. Every command is exact. Every JSON is literal.

Type consistency: `control5ActionType` / `control6ActionType` used identically across Tasks 1, 2, 3, 4, 5. `gestureDownPayload = nullptr` sentinel used identically. PID, DPI max, and `pointerSpeed` values match the per-variant overrides table in the spec.
