# MX Vertical Family Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship JSON descriptors for MX Vertical and MX Vertical for Business in the bundled `devices/` tree, plus test coverage and a README entry. Closes #7 on merge, tracks to `v0.3.1-beta.1`.

**Architecture:** Two near-identical `descriptor.json` files differing only in `name` and `productIds`. Logical content from Logitech Options+ extraction (jlevere/hidpp), cross-checked against Solaar and libratbag. Visual coordinates (hotspots, Easy-Switch slots) ship as stub values at `(0.5, 0.5)`; the maintainer polishes them via `logitune --edit` after merge. Existing `DeviceRegistry` scan picks up new `devices/<slug>/` folders automatically because CMake installs the whole tree and `tests/test_device_registry.cpp` runs against `$XDG_DATA_DIRS=build` which symlinks the repo `devices/`.

**Tech Stack:** C++20 / Qt 6 / JSON / GTest (parameterized). Matches existing patterns in `tests/test_device_registry.cpp`.

**Design spec:** `docs/superpowers/specs/2026-04-17-mx-vertical-design.md`. Read it before Task 1.

---

## Global rules

- **No em-dashes (U+2014 "—")** in any file you create or modify. Replace with colons, commas, periods, or parentheses. The only acceptable occurrence is inside a `grep -c "—"` verification command.
- **No co-author signatures** in commit messages.
- **Branch is `add-mx-vertical-family`.** Already created from master with the spec committed. Do NOT push the branch; leave that for the final task after all tests pass.
- **Working directory:** `/home/mina/repos/logitune`.
- **Never amend commits.** If a commit needs changing, make a follow-up commit on top.
- **Build before test.** Every task that claims a passing test must also run `cmake --build build -j"$(nproc)"` and `XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests` from the repo root, and both must exit 0.

---

## File Structure

### Created

```
devices/mx-vertical/descriptor.json
devices/mx-vertical/front.png              (from Logitech CDN, see Task 1)
devices/mx-vertical/side.png               (from Logitech CDN, see Task 1)
devices/mx-vertical/back.png               (from Logitech CDN, see Task 1)

devices/mx-vertical-for-business/descriptor.json
devices/mx-vertical-for-business/front.png (copy of MX Vertical front.png)
devices/mx-vertical-for-business/side.png  (copy of MX Vertical side.png)
devices/mx-vertical-for-business/back.png  (copy of MX Vertical back.png)
```

### Modified

- `tests/test_device_registry.cpp`: add two entries to `kDevices`. No struct changes; the sentinel fields (`control6ActionType = nullptr`, `gestureDownPayload = nullptr`) were already added during the MX Anywhere cycle.
- `README.md`: two new rows in the Supported Devices matrix, both `🧪 Beta`.

### Unchanged

- `CMakeLists.txt` (install rule `install(DIRECTORY ${CMAKE_SOURCE_DIR}/devices/ ...)` picks up the new folders automatically).
- `devices/schema.json` (every field these descriptors use is already in the schema).
- No C++ capability-table additions needed: every feature MX Vertical uses (`battery`, `adjustableDpi`, `hiResWheel`, `reprogControls`, `smoothScroll`, `pointerSpeed`) is already handled in `src/core/hidpp/capabilities/`. SmartShift is `false` so the capability table is simply never consulted for these devices.

---

## Task 1: Prepare MX Vertical images

**Files:**

- Create: `devices/mx-vertical/front.png`
- Create: `devices/mx-vertical/side.png`
- Create: `devices/mx-vertical/back.png`

Logitech publishes product-gallery renders on their public CDN. MX Vertical lives under `mx-vertical-advanced-ergonomic-mouse`. We download three views and trim each to a square transparent PNG so it composes with `Image.PreserveAspectFit` in QML.

- [ ] **Step 1: Create the folder**

```bash
mkdir -p devices/mx-vertical
```

- [ ] **Step 2: Download the three CDN views**

```bash
CDN=https://resource.logitech.com/content/dam/logitech/en/products/mice/mx-vertical/gallery
curl -fL -o devices/mx-vertical/front.png "$CDN/mx-vertical-topview-gallery.png"
curl -fL -o devices/mx-vertical/side.png  "$CDN/mx-vertical-mainview-gallery.png"
curl -fL -o devices/mx-vertical/back.png  "$CDN/mx-vertical-bottomview-gallery.png"
```

Expected: three files, each > 100 kB. If any URL 404s, try the fallback pattern `https://resource.logitech.com/content/dam/logitech/en/products/mice/mx-vertical/product-gallery/mx-vertical-<view>view.png` before giving up, and surface the failure so the spec can pick an alternate image source.

- [ ] **Step 3: Verify dimensions and format**

```bash
identify devices/mx-vertical/*.png 2>&1
```

Expected: three lines, each `PNG <WxH>`. Widths should be ≥ 600px so the UI renders sharp at our 380px image box.

- [ ] **Step 4: Confirm transparency**

```bash
identify -format '%f %A\n' devices/mx-vertical/*.png 2>&1
```

Expected: each filename followed by `Blend` or `True` (the alpha channel is present). If a view came back opaque, flag the image for later replacement but do not block the PR on it; the render path tolerates opaque backgrounds.

- [ ] **Step 5: Do not commit yet**

Images are committed together with the descriptor in Task 2 so the two always land in the same commit.

---

## Task 2: Add MX Vertical descriptor

**Files:**

- Create: `devices/mx-vertical/descriptor.json`
- Modify: `tests/test_device_registry.cpp`

- [ ] **Step 1: Write `devices/mx-vertical/descriptor.json`**

```json
{
  "$schema": "../schema.json",
  "name": "MX Vertical",
  "status": "beta",
  "productIds": ["0xb020", "0x407b"],

  "features": {
    "battery": true,
    "adjustableDpi": true,
    "extendedDpi": false,
    "smartShift": false,
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

  "dpi": { "min": 400, "max": 4000, "step": 100 },

  "controls": [
    { "controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",   "defaultActionType": "default",    "configurable": false },
    { "controlId": "0x0051", "buttonIndex": 1, "defaultName": "Right click",  "defaultActionType": "default",    "configurable": false },
    { "controlId": "0x0052", "buttonIndex": 2, "defaultName": "Middle click", "defaultActionType": "default",    "configurable": true  },
    { "controlId": "0x0053", "buttonIndex": 3, "defaultName": "Back",         "defaultActionType": "default",    "configurable": true  },
    { "controlId": "0x0056", "buttonIndex": 4, "defaultName": "Forward",      "defaultActionType": "default",    "configurable": true  },
    { "controlId": "0x00C3", "buttonIndex": 5, "defaultName": "DPI cycle",    "defaultActionType": "dpi-cycle",  "configurable": true  }
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

- [ ] **Step 2: Validate the JSON against the schema**

```bash
python3 -m venv /tmp/venv 2>/dev/null; /tmp/venv/bin/pip install --quiet jsonschema
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-vertical/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected output: `OK`.

- [ ] **Step 3: Add a `DeviceSpec` entry to the test fixture**

Open `tests/test_device_registry.cpp`. Find the `kDevices` array (the list of parameterized entries) and append after the last existing entry, keeping the trailing comma pattern consistent:

```cpp
    {
        .pid = 0xb020,
        .name = "MX Vertical",
        .minDpi = 400, .maxDpi = 4000, .dpiStep = 100,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "dpi-cycle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
```

If your copy of `DeviceSpec` diverges from the list above (e.g. new fields were added since this plan was written), fill the new fields to match the "no gesture, no SmartShift, 6-control" profile of the existing MX Anywhere entries in the same file; do NOT invent new semantics.

- [ ] **Step 4: Build**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: ends with `Linking CXX executable src/app/logitune` and exits 0.

- [ ] **Step 5: Run tests**

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: final line `[  PASSED  ] <N> tests.` with no failures. The count is larger than 518 because each parameterized test fans out across every device.

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" devices/mx-vertical/descriptor.json
```

Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add devices/mx-vertical/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Vertical descriptor

Bundled descriptor for the MX Vertical (Bolt/BT PID 0xb020, Unifying
WPID 0x407b). Logical data from Logitech Options+ extraction
(jlevere/hidpp), cross-checked against Solaar and libratbag. Six
controls: Left, Right, Middle, Back, Forward, DPI cycle. No gesture
button, no thumb wheel, no SmartShift (ratcheted wheel only). DPI
400-4000 step 100 per Logi spec sheet.

Hotspot positions and Easy-Switch slot coordinates ship as stubs at
(0.5, 0.5); maintainer polishes via 'logitune --edit' after merge.
Status: beta; pending hardware confirmation by the issue #7 tester.

Refs #7"
```

---

## Task 3: Add MX Vertical for Business descriptor

**Files:**

- Create: `devices/mx-vertical-for-business/descriptor.json`
- Create: `devices/mx-vertical-for-business/front.png`
- Create: `devices/mx-vertical-for-business/side.png`
- Create: `devices/mx-vertical-for-business/back.png`
- Modify: `tests/test_device_registry.cpp`

Same hardware as retail, just the Business SKU. Images are copied from the retail variant. Options+ and Solaar both map it to the same PID `0xb020`; we keep it listed separately so the carousel shows the correct product name for users who bought the Business SKU.

- [ ] **Step 1: Create folder and copy images**

```bash
mkdir -p devices/mx-vertical-for-business
cp devices/mx-vertical/front.png devices/mx-vertical-for-business/front.png
cp devices/mx-vertical/side.png  devices/mx-vertical-for-business/side.png
cp devices/mx-vertical/back.png  devices/mx-vertical-for-business/back.png
```

- [ ] **Step 2: Write `devices/mx-vertical-for-business/descriptor.json`**

Start from the Task 2 JSON (copy the entire file). Change only these two fields:

- `"name": "MX Vertical for Business"`
- `"productIds": ["0xb020"]`

Everything else (features, DPI range, controls, hotspots, Easy-Switch slots, default gestures) stays identical. If at implementation time Options+ or Solaar publishes a distinct PID for the Business SKU, add it to the `productIds` array without removing `0xb020`, and note the second PID source in the commit message.

- [ ] **Step 3: Validate the JSON against the schema**

```bash
/tmp/venv/bin/python3 - <<'EOF'
import json, jsonschema
schema = json.load(open('devices/schema.json'))
doc = json.load(open('devices/mx-vertical-for-business/descriptor.json'))
jsonschema.validate(instance=doc, schema=schema)
print("OK")
EOF
```

Expected: `OK`.

- [ ] **Step 4: Add a non-parameterized test for the for-Business variant**

Both descriptors share PID `0xb020`, so the parameterized `DeviceSpec` fixture (which keys on `.pid` and uses `registry.findByPid`) cannot host a second entry without the two tests fighting for the same PID. Add a separate test that looks up the Business variant by name instead. `DeviceRegistry::findByName(const QString&)` is already public (`src/core/DeviceRegistry.h:14`).

Near the bottom of `tests/test_device_registry.cpp`, after the `INSTANTIATE_TEST_SUITE_P` line, add:

```cpp
TEST(DeviceRegistry, MxVerticalForBusinessRegistered) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("MX Vertical for Business"));
    ASSERT_NE(dev, nullptr);
    const auto ids = dev->productIds();
    EXPECT_NE(std::find(ids.begin(), ids.end(), 0xb020), ids.end());
    EXPECT_EQ(dev->maxDpi(), 4000);
    EXPECT_EQ(dev->minDpi(), 400);
    EXPECT_EQ(dev->controls().size(), 6);
    EXPECT_EQ(dev->controls()[5].controlId, QStringLiteral("0x00C3"));
    EXPECT_EQ(dev->controls()[5].defaultActionType, QStringLiteral("dpi-cycle"));
    EXPECT_FALSE(dev->features().smartShift);
    EXPECT_TRUE(dev->features().pointerSpeed);
}
```

Notes for the implementer:

- `logitune::DeviceRegistry reg;` auto-scans on construction. No `loadAll` is needed; the existing `TEST(DeviceRegistry, ReloadByPathRefreshesSingleDevice)` at line 220 of the same file is a working reference.
- `findByName` takes a `QString`; use `QStringLiteral(...)`.
- `IDevice` exposes `productIds()` (`std::vector<uint16_t>`), `minDpi()` / `maxDpi()` / `dpiStep()` (int), `controls()` (`QList<ControlDescriptor>`), and `features()` (`FeatureSupport`). Declared in `src/core/interfaces/IDevice.h`.
- `ControlDescriptor::controlId` and `defaultActionType` are `QString` (stored as hex literal like `"0x00C3"` in the descriptor JSON and preserved through parsing).
- Project is C++20 (`CMAKE_CXX_STANDARD 20` in `CMakeLists.txt:5`). `std::ranges::contains` is C++23; use `std::find` as shown above.
- Do NOT add a parameterized `DeviceSpec` entry for for-Business; that would collide with the retail entry on the shared PID `0xb020`.

- [ ] **Step 5: Build**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

- [ ] **Step 6: Run tests**

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: `[  PASSED  ] <N> tests.` with no failures.

- [ ] **Step 7: Verify no em-dashes**

```bash
grep -c "—" devices/mx-vertical-for-business/descriptor.json
```

Expected: `0`.

- [ ] **Step 8: Commit**

```bash
git add devices/mx-vertical-for-business/ tests/test_device_registry.cpp
git commit -m "feat(devices): add MX Vertical for Business descriptor

Same hardware as the retail MX Vertical. Options+ and Solaar both
report PID 0xb020 for both SKUs. The separate entry exists so the
carousel shows the correct product name for Business-SKU users.

Images reuse the retail MX Vertical renders bit-for-bit.

Status: beta; no dedicated hardware tester for this SKU yet.
If Options+ or Solaar later publishes a distinct Business PID, add
it to productIds without removing 0xb020.

Refs #7"
```

---

## Task 4: Update README support matrix

**Files:**

- Modify: `README.md`

- [ ] **Step 1: Find the device matrix**

```bash
grep -n "MX Anywhere\|Supported Devices\|Supported mice" README.md | head -10
```

Locate the table section (look for a Markdown table with columns like `Device`, `PIDs`, `Status`). If the table uses a different column layout on master, adapt Step 2 to match.

- [ ] **Step 2: Add two rows**

Insert two new rows just after the last MX Anywhere row, matching the existing column layout. Example (adapt cell count to the actual table):

```markdown
| MX Vertical              | `0xb020`, `0x407b` | 🧪 Beta |
| MX Vertical for Business | `0xb020`           | 🧪 Beta |
```

If the README has a separate "Beta" and "Verified" section rather than a single table with a Status column, add the rows under the Beta section only.

- [ ] **Step 3: Verify no em-dashes introduced**

```bash
grep -c "—" README.md
```

Record the count before and after the edit (running the command against `HEAD~1:README.md` and `README.md`). They must match; do not increase the count.

Verification one-liner:

```bash
before=$(git show HEAD:README.md | grep -c "—"); after=$(grep -c "—" README.md); echo "before=$before after=$after"; [ "$before" = "$after" ]
```

Expected: the final `[` test exits 0 (silently). Any non-zero exit means an em-dash leaked in; remove it before continuing.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: add MX Vertical family to README support matrix

Two new beta rows under the support matrix, matching the same layout
used for the MX Anywhere family."
```

---

## Task 5: Final verification and PR handoff

**Files:** none modified; this task only verifies and hands off.

- [ ] **Step 1: Clean build from scratch**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: both commands exit 0. Tail of the second command ends at `Linking CXX executable src/app/logitune`.

- [ ] **Step 2: Full test run**

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -5
```

Expected: `[  PASSED  ] <N> tests.` where `<N>` is strictly larger than `518` (the pre-MX-Vertical count) because the parameterized tests fan out over two new entries.

- [ ] **Step 3: Simulation sanity check**

```bash
pkill -f logitune 2>/dev/null; sleep 1
nohup ./build/src/app/logitune --simulate-all > /tmp/logitune-sim-verify.log 2>&1 & disown
sleep 3
grep -iE "error|failed|exception|missing" /tmp/logitune-sim-verify.log | head -5
pkill -f logitune 2>/dev/null
```

Expected: the grep returns nothing, or only acknowledged warnings (e.g. unrelated tray-extension hints). Any line containing `error` or `failed` that touches MX Vertical blocks the PR until fixed.

- [ ] **Step 4: Edit-mode sanity check**

```bash
nohup ./build/src/app/logitune --simulate-all --edit > /tmp/logitune-edit-verify.log 2>&1 & disown
sleep 3
grep -iE "error|failed" /tmp/logitune-edit-verify.log | head -5
pkill -f logitune 2>/dev/null
```

Same expectation as Step 3.

- [ ] **Step 5: Summarize for handoff**

Write a short handoff summary (keep it as a scratch note, not in the repo) listing:

- What was added: two MX Vertical descriptors, two fixture entries, README updates.
- What still needs human action before publishing:
  - Tune hotspots + Easy-Switch slot coordinates via `logitune --edit`.
  - Verify the CDN image URLs on a blank system.
  - Confirm Options+/Solaar agreement on the for-Business PID (`0xb020`).
- Expected release artifact: `v0.3.1-beta.1` to be tagged from master after PR merges.

Hand the summary back to the maintainer for the PR-body draft and the tester-facing comment on issue #7.

- [ ] **Step 6: Do not push or open the PR**

The branch stays local. The maintainer reviews the diff, tunes visuals with `logitune --edit`, then runs their usual push + PR cycle.

---

## Done criteria

- Clean Debug build from scratch: 0 errors, 0 warnings introduced by this work.
- Full test run: all tests pass, test count strictly increased over the pre-plan baseline.
- `--simulate-all` and `--simulate-all --edit` both launch without MX-Vertical-touching errors in the log.
- Four commits on `add-mx-vertical-family`, one per task (Tasks 1 + 2 share the first commit; Tasks 3, 4, and a potential fix-up are the remaining ones).
- `grep -c "—" devices/mx-vertical*/descriptor.json README.md` prints `0` for every file.
