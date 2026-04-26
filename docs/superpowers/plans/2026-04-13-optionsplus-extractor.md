# Options+ Extractor Rewrite + App-Side Descriptor Cleanup — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the brittle `scripts/generate-from-optionsplus.py` with a modular extractor package, add a `kind` field to scroll hotspots so the app can look them up by name instead of by position, and update the three shipped descriptors so they carry the new field.

**Architecture:** Seven-module Python package (`scripts/optionsplus_extractor/`) with typed dataclasses at every boundary. Layer-1 schema self-check inside the script. Layer-2 golden-file pytest suite against committed trimmed Options+ fixture data. App-side changes replace `buttonId === 7` and positional `scrollHotspots[0/1/2]` with lookups by CID (`0x0000`) and by `kind` string.

**Tech Stack:** Python 3.10+ (dataclasses, pathlib, argparse), pytest, Qt6/C++20, QML, gtest.

**Spec:** [`docs/superpowers/specs/2026-04-13-optionsplus-extractor-design.md`](../specs/2026-04-13-optionsplus-extractor-design.md)

**Branch:** `fix-optionsplus-extraction` already exists with two prior reactive fix commits plus the spec commit. Implementation continues on that branch; the old script content gets replaced in Task 13. PR #29 will be updated in its final state to describe the rewrite rather than the reactive patches.

---

## File Structure

### Created

| Path | Purpose |
|---|---|
| `scripts/optionsplus_extractor/__init__.py` | package marker |
| `scripts/optionsplus_extractor/sources.py` | locate & load Options+ device DB + per-device depots |
| `scripts/optionsplus_extractor/capabilities.py` | capabilities dict → features + dpi |
| `scripts/optionsplus_extractor/slots.py` | core_metadata.json → typed slot records |
| `scripts/optionsplus_extractor/canonicalize.py` | sort & index-assignment rules |
| `scripts/optionsplus_extractor/descriptor.py` | assemble JSON-shaped dict with parser-compatible keys |
| `scripts/optionsplus_extractor/validate.py` | Layer-1 self-check |
| `scripts/optionsplus_extractor/cli.py` | argparse wrapper |
| `tests/scripts/fixtures/optionsplus/main/logioptionsplus/data/devices/devices_test.json` | trimmed 3-device fixture |
| `tests/scripts/fixtures/optionsplus/devices/mx_master_2s/{core_metadata,manifest}.json` | 2S depot |
| `tests/scripts/fixtures/optionsplus/devices/mx_master_3s/{core_metadata,manifest}.json` | 3S depot |
| `tests/scripts/fixtures/optionsplus/devices/mx_master_4/{core_metadata,manifest}.json` | 4 depot |
| `tests/scripts/fixtures/optionsplus/devices/*/{front,side,back}_core.png` | 1×1 placeholder PNGs |
| `tests/scripts/test_extractor.py` | unit + golden-file tests |
| `tests/scripts/__init__.py` | package marker |

### Modified

| Path | What changes |
|---|---|
| `src/core/interfaces/IDevice.h` | add `QString kind` to `HotspotDescriptor` |
| `src/core/devices/JsonDevice.cpp` | `parseHotspots()` reads `kind` |
| `src/app/models/ButtonModel.h` | add `controlId` to `ButtonEntry`, new `ButtonAssignment` struct, `isThumbWheel()` |
| `src/app/models/ButtonModel.cpp` | populate CIDs in defaults, track CID in `loadFromProfile`, implement `isThumbWheel` |
| `src/app/AppController.cpp` | `restoreButtonModelFromProfile` passes CIDs through to ButtonModel |
| `src/app/qml/pages/ButtonsPage.qml` | line 198 uses `ButtonModel.isThumbWheel()` |
| `src/app/qml/pages/PointScrollPage.qml` | introduces `hotspotByKind()` helper, three callsites updated |
| `devices/mx-master-2s/descriptor.json` | add `kind` to each scroll hotspot |
| `devices/mx-master-3s/descriptor.json` | add `kind` to each scroll hotspot |
| `devices/mx-master-4/descriptor.json` | add `kind` to each scroll hotspot |
| `tests/test_json_device.cpp` | cover `kind` round-trip + missing-`kind` fallback |
| `tests/test_button_model.cpp` | cover `isThumbWheel` |
| `scripts/generate-from-optionsplus.py` | Task 13 replaces content with 5-line shim |

### Removed

Nothing. The old top-level script is replaced in Task 13, not deleted.

---

## Phase 1 — App-side foundation (scope C)

Phase 1 lays the descriptor schema change, app-side lookup plumbing, and updated shipped descriptors. Nothing in Phase 1 depends on the Python package; Phase 2 can assume `kind` already round-trips through the parser.

### Task 1: Add `kind` field to `HotspotDescriptor`

**Files:**
- Modify: `src/core/interfaces/IDevice.h` (HotspotDescriptor struct, lines 19-25)
- Modify: `tests/test_json_device.cpp` (extend existing `LoadValidImplemented` test)

- [ ] **Step 1.1: Add field to struct**

Replace the existing `HotspotDescriptor` struct with:

```cpp
struct HotspotDescriptor {
    int buttonIndex;
    double xPct;
    double yPct;
    QString side;
    double labelOffsetYPct;
    QString kind;   // "scrollwheel" | "thumbwheel" | "pointer"; empty for button hotspots
};
```

- [ ] **Step 1.2: Add test assertion for default value**

In `tests/test_json_device.cpp`, inside `TEST(JsonDevice, LoadValidImplemented)`, right after the existing hotspot size assertions (around line 166), add:

```cpp
EXPECT_TRUE(dev->buttonHotspots()[0].kind.isEmpty())
    << "button hotspots should default to empty kind";
```

- [ ] **Step 1.3: Run tests to verify nothing breaks from the added field**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='JsonDevice.*'`
Expected: existing JsonDevice tests pass; new assertion passes (empty default).

- [ ] **Step 1.4: Commit**

```bash
git add src/core/interfaces/IDevice.h tests/test_json_device.cpp
git commit -m "add kind field to HotspotDescriptor"
```

---

### Task 2: Parse `kind` in `JsonDevice::parseHotspots`

**Files:**
- Modify: `src/core/devices/JsonDevice.cpp` (parseHotspots, lines 107-121)
- Modify: `tests/test_json_device.cpp` (new test case)

- [ ] **Step 2.1: Write failing test**

Add a new test case in `tests/test_json_device.cpp` after `LoadValidImplemented`:

```cpp
TEST(JsonDevice, HotspotKindRoundTrip)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    QJsonObject root = makeMinimalImplemented();
    QJsonObject hotspots = root.value("hotspots").toObject();
    QJsonArray scroll;

    QJsonObject h1;
    h1["buttonIndex"] = -1;
    h1["xPct"] = 0.7;
    h1["yPct"] = 0.2;
    h1["side"] = "right";
    h1["kind"] = "scrollwheel";
    scroll.append(h1);

    QJsonObject h2;
    h2["buttonIndex"] = -2;
    h2["xPct"] = 0.4;
    h2["yPct"] = 0.5;
    h2["side"] = "left";
    h2["kind"] = "thumbwheel";
    scroll.append(h2);

    hotspots["scroll"] = scroll;
    root["hotspots"] = hotspots;

    writeJson(dir, root);
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);
    ASSERT_EQ(dev->scrollHotspots().size(), 2);
    EXPECT_EQ(dev->scrollHotspots()[0].kind, QStringLiteral("scrollwheel"));
    EXPECT_EQ(dev->scrollHotspots()[1].kind, QStringLiteral("thumbwheel"));
}
```

- [ ] **Step 2.2: Run test, confirm failure**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='JsonDevice.HotspotKindRoundTrip'`
Expected: test builds, runs, fails with `kind` == `""` (empty) because parseHotspots doesn't read the field yet.

- [ ] **Step 2.3: Add the parser line**

In `src/core/devices/JsonDevice.cpp`, inside `parseHotspots` (around line 117, after the `labelOffsetYPct` line), add:

```cpp
        hd.kind = obj.value(QStringLiteral("kind")).toString();
```

The full function becomes:

```cpp
static QList<HotspotDescriptor> parseHotspots(const QJsonArray& arr)
{
    QList<HotspotDescriptor> result;
    for (const auto& val : arr) {
        const QJsonObject obj = val.toObject();
        HotspotDescriptor hd;
        hd.buttonIndex = obj.value(QStringLiteral("buttonIndex")).toInt();
        hd.xPct = obj.value(QStringLiteral("xPct")).toDouble();
        hd.yPct = obj.value(QStringLiteral("yPct")).toDouble();
        hd.side = obj.value(QStringLiteral("side")).toString();
        hd.labelOffsetYPct = obj.value(QStringLiteral("labelOffsetYPct")).toDouble(0.0);
        hd.kind = obj.value(QStringLiteral("kind")).toString();
        result.append(hd);
    }
    return result;
}
```

- [ ] **Step 2.4: Rebuild and rerun test**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='JsonDevice.HotspotKindRoundTrip'`
Expected: PASS.

- [ ] **Step 2.5: Run the full JsonDevice test suite**

Run: `XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='JsonDevice.*'`
Expected: all JsonDevice tests pass (no regression from missing `kind` — empty default handles it).

- [ ] **Step 2.6: Commit**

```bash
git add src/core/devices/JsonDevice.cpp tests/test_json_device.cpp
git commit -m "parse kind field on scroll hotspots"
```

---

### Task 3: Add `controlId` to `ButtonEntry` and introduce `ButtonAssignment`

**Files:**
- Modify: `src/app/models/ButtonModel.h`
- Modify: `src/app/models/ButtonModel.cpp`

- [ ] **Step 3.1: Update ButtonModel.h — add struct + field + method**

Replace the top of `src/app/models/ButtonModel.h` (above the class declaration) with:

```cpp
#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <cstdint>
#include <qqmlintegration.h>

namespace logitune {

struct ButtonEntry {
    int buttonId;
    QString buttonName;
    QString actionName;
    QString actionType;
    uint16_t controlId;  // HID++ CID; 0x0000 = virtual thumb wheel
};

struct ButtonAssignment {
    QString actionName;
    QString actionType;
    uint16_t controlId;
};

class ButtonModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        ButtonIdRole = Qt::UserRole + 1,
        ButtonNameRole,
        ActionNameRole,
        ActionTypeRole,
    };

    explicit ButtonModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setAction(int buttonId, const QString &actionName, const QString &actionType);
    Q_INVOKABLE QString actionNameForButton(int buttonId) const;
    Q_INVOKABLE QString actionTypeForButton(int buttonId) const;
    Q_INVOKABLE bool isThumbWheel(int buttonId) const;

    /// Programmatic bulk update -- does NOT emit per-row dataChanged.
    /// Emits a single modelReset so QML rebinds all at once.
    void loadFromProfile(const QList<ButtonAssignment> &assignments);

signals:
    void userActionChanged(int buttonId, const QString &actionName, const QString &actionType);

private:
    QList<ButtonEntry> m_buttons;
};

} // namespace logitune
```

- [ ] **Step 3.2: Update ButtonModel.cpp — populate CIDs in defaults**

Replace the constructor and add `isThumbWheel` and the updated `loadFromProfile`. Open `src/app/models/ButtonModel.cpp` and make these targeted changes:

**Constructor (lines 5-19):**

```cpp
ButtonModel::ButtonModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Default button assignments — hardcoded until ProfileEngine integration.
    // controlIds line up with Jelco's canonical MX-family descriptor ordering
    // so ButtonModel::isThumbWheel returns the right answer before a real
    // device descriptor has loaded.
    m_buttons = {
        { 0, "Left click",   "Left click",        "default",           0x0050 },
        { 1, "Right click",  "Right click",       "default",           0x0051 },
        { 2, "Middle click", "Middle click",      "default",           0x0052 },
        { 3, "Back",         "Back",              "default",           0x0053 },
        { 4, "Forward",      "Forward",           "default",           0x0056 },
        { 5, "Gesture",      "Gestures",          "gesture-trigger",   0x00C3 },
        { 6, "Shift wheel",  "Shift wheel mode",  "smartshift-toggle", 0x00C4 },
        { 7, "Thumb wheel",  "Horizontal scroll", "default",           0x0000 },
    };
}
```

**Replace `loadFromProfile` (lines 67-79):**

```cpp
void ButtonModel::loadFromProfile(const QList<ButtonAssignment> &assignments)
{
    beginResetModel();
    for (int i = 0; i < assignments.size() && i < m_buttons.size(); ++i) {
        m_buttons[i].actionName = assignments[i].actionName;
        m_buttons[i].actionType = assignments[i].actionType;
        m_buttons[i].controlId  = assignments[i].controlId;
    }
    endResetModel();

    if (!m_buttons.isEmpty())
        emit dataChanged(index(0), index(m_buttons.size() - 1));
}
```

**Add `isThumbWheel` method at the bottom of the namespace (before the closing `}`):**

```cpp
bool ButtonModel::isThumbWheel(int buttonId) const
{
    for (const auto &entry : m_buttons) {
        if (entry.buttonId == buttonId)
            return entry.controlId == 0x0000;
    }
    return false;
}
```

- [ ] **Step 3.3: Build — expect AppController to fail compilation**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: FAIL with a compile error in `AppController.cpp` around line 421 because `loadFromProfile`'s signature changed from `QList<QPair<QString, QString>>` to `QList<ButtonAssignment>`. This is expected — Task 4 fixes the caller.

- [ ] **Step 3.4: Commit the ButtonModel half of the change**

```bash
git add src/app/models/ButtonModel.h src/app/models/ButtonModel.cpp
git commit -m "extend ButtonModel with controlId and isThumbWheel"
```

---

### Task 4: Update `AppController::restoreButtonModelFromProfile` to pass CIDs

**Files:**
- Modify: `src/app/AppController.cpp` (restoreButtonModelFromProfile, lines 358-421)

- [ ] **Step 4.1: Rewrite the loop**

In `src/app/AppController.cpp`, replace the body of `restoreButtonModelFromProfile` from the `QList<QPair<QString, QString>> buttons;` line through the `m_buttonModel.loadFromProfile(buttons);` line. The new loop tracks CIDs alongside name/type:

```cpp
    QList<ButtonAssignment> assignments;
    for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
        const auto &ctrl = controls[i];
        if (ctrl.controlId == 0) {
            static const QMap<QString, QString> kWheelNames = {
                {"scroll", "Horizontal scroll"}, {"zoom", "Zoom in/out"},
                {"volume", "Volume control"}, {"none", "No action"}
            };
            QString wheelName = kWheelNames.value(p.thumbWheelMode, p.thumbWheelMode);
            QString wheelType = (p.thumbWheelMode == "scroll") ? "default" : "wheel-mode";
            assignments.append({wheelName, wheelType, ctrl.controlId});
            continue;
        }
        const auto &ba = (static_cast<std::size_t>(i) < p.buttons.size())
            ? p.buttons[static_cast<std::size_t>(i)]
            : ButtonAction{ButtonAction::Default, {}};

        QString aType, aName;
        switch (ba.type) {
        case ButtonAction::Default:
            aType = QStringLiteral("default");
            aName = ctrl.defaultName;
            break;
        case ButtonAction::GestureTrigger:
            aType = QStringLiteral("gesture-trigger");
            aName = QStringLiteral("Gestures");
            break;
        case ButtonAction::SmartShiftToggle:
            aType = QStringLiteral("smartshift-toggle");
            aName = QStringLiteral("Shift wheel mode");
            break;
        case ButtonAction::Keystroke:
            aType = QStringLiteral("keystroke");
            aName = buttonActionToName(ba);
            break;
        case ButtonAction::AppLaunch:
            aType = QStringLiteral("app-launch");
            aName = buttonActionToName(ba);
            break;
        case ButtonAction::Media: {
            aType = QStringLiteral("media-controls");
            static const QHash<QString, QString> kMediaNames = {
                {"Play", "Play/Pause"}, {"Next", "Next track"},
                {"Previous", "Previous track"}, {"Stop", "Stop"},
                {"Mute", "Mute"}, {"VolumeUp", "Volume up"},
                {"VolumeDown", "Volume down"},
            };
            aName = kMediaNames.value(ba.payload, ba.payload);
            break;
        }
        default:
            aType = QStringLiteral("default");
            aName = ctrl.defaultName;
            break;
        }
        assignments.append({aName, aType, ctrl.controlId});
    }

    m_buttonModel.loadFromProfile(assignments);
```

- [ ] **Step 4.2: Rebuild — expect success**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: build succeeds, no errors.

- [ ] **Step 4.3: Run existing tests to confirm no behavior regressions**

Run: `XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -5`
Expected: all tests green (466+ passing).

- [ ] **Step 4.4: Commit**

```bash
git add src/app/AppController.cpp
git commit -m "thread controlId through ButtonModel::loadFromProfile"
```

---

### Task 5: Add `ButtonModel::isThumbWheel` unit test

**Files:**
- Modify: `tests/test_button_model.cpp` (new test case)

- [ ] **Step 5.1: Write the test**

Add at the end of `tests/test_button_model.cpp` (before the closing namespace brace if present, otherwise at end of file):

```cpp
TEST(ButtonModelTest, IsThumbWheelDefault)
{
    logitune::ButtonModel m;
    // Default constructor uses canonical CID layout — button 7 is thumb wheel.
    EXPECT_TRUE(m.isThumbWheel(7));
    EXPECT_FALSE(m.isThumbWheel(0));  // left click
    EXPECT_FALSE(m.isThumbWheel(2));  // middle click
    EXPECT_FALSE(m.isThumbWheel(99)); // nonexistent id
}

TEST(ButtonModelTest, IsThumbWheelAfterLoadFromProfile)
{
    logitune::ButtonModel m;
    // Simulate a device with no thumb wheel (e.g., MX Vertical):
    // 8-slot layout but slot 7 carries a real CID instead of 0x0000.
    QList<logitune::ButtonAssignment> assignments = {
        { "Left click",  "default", 0x0050 },
        { "Right click", "default", 0x0051 },
        { "Middle click","default", 0x0052 },
        { "Back",        "default", 0x0053 },
        { "Forward",     "default", 0x0056 },
        { "DPI",         "default", 0x00FD },
        { "Unused",      "default", 0xFFFF },
        { "Unused",      "default", 0xFFFF },
    };
    m.loadFromProfile(assignments);
    EXPECT_FALSE(m.isThumbWheel(7));
    EXPECT_FALSE(m.isThumbWheel(5));
}
```

- [ ] **Step 5.2: Build and run the new tests**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='ButtonModelTest.IsThumbWheel*'`
Expected: both new tests PASS.

- [ ] **Step 5.3: Commit**

```bash
git add tests/test_button_model.cpp
git commit -m "test ButtonModel::isThumbWheel"
```

---

### Task 6: Update `ButtonsPage.qml` to use `isThumbWheel`

**Files:**
- Modify: `src/app/qml/pages/ButtonsPage.qml:198`

- [ ] **Step 6.1: Replace the positional check**

In `src/app/qml/pages/ButtonsPage.qml` line 198 (inside the function that sets `actionsPanel` properties), change:

```qml
        actionsPanel.isWheel           = (buttonId === 7) // thumb wheel
```

to:

```qml
        actionsPanel.isWheel           = ButtonModel.isThumbWheel(buttonId)
```

- [ ] **Step 6.2: Rebuild**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: builds cleanly.

- [ ] **Step 6.3: Smoke-test by launching the app**

Run: `pkill -x logitune 2>/dev/null; nohup ./build/src/app/logitune > /tmp/logitune-task6.log 2>&1 & disown`
Then manually: open the Buttons page, click the Thumb wheel hotspot, confirm the action picker shows wheel-mode options (scroll/zoom/volume/none), not button actions. Click the Gesture button hotspot, confirm the action picker shows the full button action list (not wheel modes).

Expected: behavior is unchanged from the shipped descriptor (thumb wheel → wheel modes, gesture → full actions). Since CID 0x0000 still lands at `buttonId==7` in the shipped 3S, both old and new logic produce the same result — this task is about removing the *dependency* on that accident, not about changing observable behavior.

Kill the app: `pkill -x logitune`

- [ ] **Step 6.4: Commit**

```bash
git add src/app/qml/pages/ButtonsPage.qml
git commit -m "detect thumb wheel by CID instead of buttonIndex"
```

---

### Task 7: Add `hotspotByKind` helper to `PointScrollPage.qml`

**Files:**
- Modify: `src/app/qml/pages/PointScrollPage.qml`

- [ ] **Step 7.1: Add the helper function**

In `src/app/qml/pages/PointScrollPage.qml`, inside `renderGroup` (the `Item` with `id: renderGroup` around line 74), add a JS function property just below `scrollHotspotsData`:

```qml
            readonly property var scrollHotspotsData: DeviceModel.scrollHotspots

            function hotspotByKind(kind, fallbackIdx) {
                for (var i = 0; i < scrollHotspotsData.length; i++) {
                    if (scrollHotspotsData[i].kind === kind)
                        return scrollHotspotsData[i];
                }
                return scrollHotspotsData.length > fallbackIdx
                    ? scrollHotspotsData[fallbackIdx]
                    : null;
            }
```

- [ ] **Step 7.2: Replace the three positional lookups**

Change the `hs:` bindings in the three InfoCallout blocks:

- Scroll wheel callout (around line 94):
  ```qml
                  readonly property var hs: renderGroup.hotspotByKind("scrollwheel", 0)
  ```
- Thumb wheel callout (around line 120):
  ```qml
                  readonly property var hs: renderGroup.hotspotByKind("thumbwheel", 1)
  ```
- Pointer speed callout (around line 141):
  ```qml
                  readonly property var hs: renderGroup.hotspotByKind("pointer", 2)
  ```

- [ ] **Step 7.3: Rebuild**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: builds cleanly.

- [ ] **Step 7.4: Smoke-test with the shipped descriptor (which lacks `kind`)**

The positional fallback should keep rendering the callouts at the same positions as before.

Run: `pkill -x logitune 2>/dev/null; nohup ./build/src/app/logitune > /tmp/logitune-task7.log 2>&1 & disown`
Manually: open Point & Scroll page. Confirm Scroll wheel / Thumb wheel / Pointer speed callouts appear in the correct spots on the mouse image (matching the pre-task behavior). Kill: `pkill -x logitune`.

- [ ] **Step 7.5: Commit**

```bash
git add src/app/qml/pages/PointScrollPage.qml
git commit -m "look up scroll hotspots by kind with positional fallback"
```

---

### Task 8: Add `kind` to the three shipped descriptors

**Files:**
- Modify: `devices/mx-master-2s/descriptor.json`
- Modify: `devices/mx-master-3s/descriptor.json`
- Modify: `devices/mx-master-4/descriptor.json`

- [ ] **Step 8.1: Update MX Master 2S**

In `devices/mx-master-2s/descriptor.json`, find the `hotspots.scroll` array. Each entry currently looks like:

```json
{ "buttonIndex": -1, "xPct": 0.73, "yPct": 0.20, "side": "right", "labelOffsetYPct": 0.0 }
```

Add a `"kind"` field to each. Use the `buttonIndex` mapping: `-1 → "scrollwheel"`, `-2 → "thumbwheel"`, `-3 → "pointer"`. For example the first scroll entry becomes:

```json
{ "buttonIndex": -1, "xPct": 0.73, "yPct": 0.20, "side": "right", "labelOffsetYPct": 0.0, "kind": "scrollwheel" }
```

Apply the same transformation to all three scroll entries in the file.

- [ ] **Step 8.2: Update MX Master 3S**

Same rule in `devices/mx-master-3s/descriptor.json`. Three scroll entries, three `kind` additions.

- [ ] **Step 8.3: Update MX Master 4**

Same rule in `devices/mx-master-4/descriptor.json`.

- [ ] **Step 8.4: Verify JSON is still valid**

Run: `for f in devices/mx-master-{2s,3s,4}/descriptor.json; do python3 -c "import json; json.load(open('$f'))" && echo "$f: ok"; done`
Expected: all three print `ok`.

- [ ] **Step 8.5: Run the full test suite**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -5`
Expected: all existing tests pass. The `kind` field is a pure addition; nothing asserts its absence.

- [ ] **Step 8.6: Smoke-test with updated descriptors**

Run: `pkill -x logitune 2>/dev/null; nohup ./build/src/app/logitune > /tmp/logitune-task8.log 2>&1 & disown`
Manually: open the Point & Scroll page — now the callouts resolve via `kind` lookup instead of positional fallback. Confirm they still render in the correct positions. Kill: `pkill -x logitune`.

- [ ] **Step 8.7: Commit**

```bash
git add devices/mx-master-2s/descriptor.json \
        devices/mx-master-3s/descriptor.json \
        devices/mx-master-4/descriptor.json
git commit -m "add kind field to shipped scroll hotspots"
```

---

### Task 9: Phase 1 checkpoint — full test run

- [ ] **Step 9.1: Full suite**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -5`
Expected: 466+ tests green (two new ones from Tasks 2 and 5).

- [ ] **Step 9.2: Tray tests**

Run: `XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tray-tests 2>&1 | tail -5`
Expected: all tray tests green.

- [ ] **Step 9.3: Smoke-test the real app with the real hardware**

Run: `pkill -x logitune 2>/dev/null; nohup ./build/src/app/logitune > /tmp/logitune-phase1.log 2>&1 & disown`
Manually verify:
- Device shows up in carousel with correct name
- Buttons page: click Thumb wheel → wheel-mode picker. Click Gesture button → button actions picker.
- Point & Scroll page: all three callouts in correct positions
- Easy-Switch page: slot markers visible
Then kill: `pkill -x logitune`.

No commit — nothing to commit, this is a checkpoint.

---

## Phase 2 — Extractor rewrite

Phase 2 builds the Python package module-by-module, TDD-style, against committed fixture data.

### Task 10: Package skeleton + pytest discovery

**Files:**
- Create: `scripts/optionsplus_extractor/__init__.py`
- Create: `scripts/optionsplus_extractor/{sources,capabilities,slots,canonicalize,descriptor,validate,cli}.py` (empty stubs)
- Create: `tests/scripts/__init__.py`
- Create: `tests/scripts/test_extractor.py` (single smoke test)

- [ ] **Step 10.1: Create the package directory structure**

```bash
mkdir -p scripts/optionsplus_extractor tests/scripts/fixtures/optionsplus/main/logioptionsplus/data/devices
mkdir -p tests/scripts/fixtures/optionsplus/devices/mx_master_2s
mkdir -p tests/scripts/fixtures/optionsplus/devices/mx_master_3s
mkdir -p tests/scripts/fixtures/optionsplus/devices/mx_master_4
```

- [ ] **Step 10.2: Create `scripts/optionsplus_extractor/__init__.py`**

```python
"""Modular Options+ → Logitune descriptor extractor.

Spec: docs/superpowers/specs/2026-04-13-optionsplus-extractor-design.md
"""
```

- [ ] **Step 10.3: Create empty module stubs**

Each of `sources.py`, `capabilities.py`, `slots.py`, `canonicalize.py`, `descriptor.py`, `validate.py`, `cli.py` starts with:

```python
"""<module purpose — see spec §"Architecture">."""
```

Specific one-line docstrings per module:

- `sources.py` — `"""Locate and load Options+ device DB and per-device depot files."""`
- `capabilities.py` — `"""Map Options+ capabilities dict to our features + dpi."""`
- `slots.py` — `"""Parse core_metadata.json image assignments into typed slot records."""`
- `canonicalize.py` — `"""Sort / index-assignment rules for controls, scroll hotspots, easy-switch slots."""`
- `descriptor.py` — `"""Assemble the final descriptor dict with parser-compatible field names."""`
- `validate.py` — `"""Layer 1 schema self-check: verify generated dict matches JsonDevice parser expectations."""`
- `cli.py` — `"""argparse wrapper — glues sources → capabilities → slots → canonicalize → descriptor → validate."""`

- [ ] **Step 10.4: Create `tests/scripts/__init__.py`**

Empty file (marker only): `touch tests/scripts/__init__.py`

- [ ] **Step 10.5: Create a smoke test that imports the package**

`tests/scripts/test_extractor.py`:

```python
"""Tests for scripts/optionsplus_extractor/. See spec
docs/superpowers/specs/2026-04-13-optionsplus-extractor-design.md."""

import importlib


def test_package_imports():
    mod = importlib.import_module("optionsplus_extractor")
    assert mod.__doc__ is not None


def test_all_submodules_import():
    for name in ("sources", "capabilities", "slots", "canonicalize",
                 "descriptor", "validate", "cli"):
        importlib.import_module(f"optionsplus_extractor.{name}")
```

- [ ] **Step 10.6: Run the smoke test**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -15`
Expected: both tests PASS. (The `cd scripts` puts `optionsplus_extractor` on `sys.path`. The real CLI invocation will also set the path; this is the simplest test rig.)

- [ ] **Step 10.7: Commit**

```bash
git add scripts/optionsplus_extractor/ tests/scripts/
git commit -m "scaffold optionsplus_extractor package"
```

---

### Task 11: Commit trimmed fixture data

**Files:**
- Create: `tests/scripts/fixtures/optionsplus/main/logioptionsplus/data/devices/devices_test.json`
- Create: `tests/scripts/fixtures/optionsplus/devices/mx_master_{2s,3s,4}/core_metadata.json`
- Create: `tests/scripts/fixtures/optionsplus/devices/mx_master_{2s,3s,4}/manifest.json`
- Create: `tests/scripts/fixtures/optionsplus/devices/mx_master_{2s,3s,4}/{front,side,back}_core.png`

- [ ] **Step 11.1: Copy trimmed device DB**

From the maintainer's local extract at `/tmp/optionsplus/main/logioptionsplus/data/devices/`, write a new file containing just the three devices we care about (2S/3S/4). Use Python to extract the subset rather than hand-editing:

```bash
python3 <<'PY'
import json, os
src_dir = "/tmp/optionsplus/main/logioptionsplus/data/devices"
dst = "tests/scripts/fixtures/optionsplus/main/logioptionsplus/data/devices/devices_test.json"
keep = {"mx_master_2s", "mx_master_3s", "mx_master_4"}
out = {"devices": []}
for fname in sorted(os.listdir(src_dir)):
    try:
        data = json.load(open(os.path.join(src_dir, fname), encoding="utf-8-sig"))
    except Exception:
        continue
    for d in data.get("devices", []):
        if d.get("depot") in keep and d.get("type") == "MOUSE":
            out["devices"].append(d)
json.dump(out, open(dst, "w"), indent=2)
print(f"wrote {len(out['devices'])} devices")
PY
```

Expected: `wrote 3 devices`.

- [ ] **Step 11.2: Copy per-device `core_metadata.json` and `manifest.json`**

```bash
for dev in mx_master_2s mx_master_3s mx_master_4; do
  src="/tmp/optionsplus/devices/$dev"
  dst="tests/scripts/fixtures/optionsplus/devices/$dev"
  mkdir -p "$dst"
  [ -f "$src/core_metadata.json" ] && cp "$src/core_metadata.json" "$dst/"
  [ -f "$src/metadata.json" ]      && cp "$src/metadata.json"      "$dst/"
  cp "$src/manifest.json" "$dst/"
done
```

- [ ] **Step 11.3: Create 1×1 placeholder PNGs**

```bash
python3 <<'PY'
import struct, zlib, os
# Minimal 1x1 grayscale PNG, pre-built byte sequence.
def make_png():
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0)
    ihdr_chunk = struct.pack(">I", len(ihdr)) + b"IHDR" + ihdr + struct.pack(">I", zlib.crc32(b"IHDR" + ihdr) & 0xffffffff)
    raw = b"\x00\x00"
    comp = zlib.compress(raw)
    idat_chunk = struct.pack(">I", len(comp)) + b"IDAT" + comp + struct.pack(">I", zlib.crc32(b"IDAT" + comp) & 0xffffffff)
    iend_chunk = struct.pack(">I", 0) + b"IEND" + struct.pack(">I", zlib.crc32(b"IEND") & 0xffffffff)
    return sig + ihdr_chunk + idat_chunk + iend_chunk
data = make_png()
base = "tests/scripts/fixtures/optionsplus/devices"
for dev in ("mx_master_2s", "mx_master_3s", "mx_master_4"):
    for name in ("front_core.png", "side_core.png", "back_core.png"):
        path = os.path.join(base, dev, name)
        open(path, "wb").write(data)
        print(path)
PY
```

Expected: 9 PNG paths printed.

- [ ] **Step 11.4: Verify fixture validity**

Run: `python3 -c "
import json, os
base = 'tests/scripts/fixtures/optionsplus'
d = json.load(open(f'{base}/main/logioptionsplus/data/devices/devices_test.json'))
assert len(d['devices']) == 3, f'expected 3 devices, got {len(d[\"devices\"])}'
for dev in ('mx_master_2s', 'mx_master_3s', 'mx_master_4'):
    meta = f'{base}/devices/{dev}/core_metadata.json'
    if not os.path.exists(meta):
        meta = f'{base}/devices/{dev}/metadata.json'
    assert os.path.exists(meta), f'missing metadata for {dev}'
    json.load(open(meta))
print('OK')
"`
Expected: `OK`.

- [ ] **Step 11.5: Commit**

```bash
git add tests/scripts/fixtures/
git commit -m "add trimmed options+ fixture data for extractor tests"
```

---

### Task 12: `sources.py` — load device DB and depot files

**Files:**
- Modify: `scripts/optionsplus_extractor/sources.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 12.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from pathlib import Path
from optionsplus_extractor import sources

FIXTURE_ROOT = Path(__file__).parent / "fixtures" / "optionsplus"


def test_load_device_db_returns_three_mice():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    assert set(mice.keys()) == {"mx_master_2s", "mx_master_3s", "mx_master_4"}


def test_device_db_entry_has_expected_fields():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    m3s = mice["mx_master_3s"]
    assert m3s.name == "MX Master 3S"
    assert "0xb034" in m3s.product_ids or "0x2b034" in m3s.product_ids \
        or any(pid.lower().endswith("b034") for pid in m3s.product_ids)
    assert isinstance(m3s.capabilities, dict)
    assert m3s.capabilities.get("hasHighResolutionSensor") is True


def test_load_depot_finds_metadata_and_images():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    assert depot.metadata is not None
    assert "images" in depot.metadata
    assert depot.front_image is not None
    assert depot.front_image.exists()
```

- [ ] **Step 12.2: Run tests, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: FAIL — `sources` module has nothing in it yet.

- [ ] **Step 12.3: Implement `sources.py`**

Replace `scripts/optionsplus_extractor/sources.py` with:

```python
"""Locate and load Options+ device DB and per-device depot files."""

from __future__ import annotations

import glob
import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class DeviceDbEntry:
    depot: str                  # e.g. "mx_master_3s"
    name: str                   # displayName from Options+
    product_ids: list[str] = field(default_factory=list)  # hex strings like "0xb019"
    capabilities: dict = field(default_factory=dict)


@dataclass
class Depot:
    path: Path
    metadata: Optional[dict]     # core_metadata.json or metadata.json contents
    front_image: Optional[Path]  # front_core.png | front.png
    side_image: Optional[Path]
    back_image: Optional[Path]


def load_device_db(main_dir: Path) -> dict[str, DeviceDbEntry]:
    """Load all mice from Options+ device database.

    `main_dir` is the extracted logioptionsplus depot root. The devices
    JSON files live at `<main_dir>/data/devices/devices*.json`.
    """
    result: dict[str, DeviceDbEntry] = {}
    pattern = str(main_dir / "data" / "devices" / "devices*.json")
    for path in sorted(glob.glob(pattern)):
        try:
            data = json.load(open(path, encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            continue
        for d in data.get("devices", []):
            if d.get("type") != "MOUSE":
                continue
            depot = d.get("depot", "")
            if not depot:
                continue
            pids = _extract_pids(d)
            entry = result.get(depot)
            if entry is None:
                entry = DeviceDbEntry(
                    depot=depot,
                    name=d.get("displayName", depot),
                    product_ids=sorted(pids),
                    capabilities=d.get("capabilities", {}) or {},
                )
                result[depot] = entry
            else:
                merged = sorted(set(entry.product_ids) | pids)
                entry.product_ids = merged
                if not entry.capabilities and d.get("capabilities"):
                    entry.capabilities = d["capabilities"]
    return result


def _extract_pids(device_entry: dict) -> set[str]:
    pids: set[str] = set()
    for mode in device_entry.get("modes", []) or []:
        for iface in mode.get("interfaces", []) or []:
            iid = iface.get("id", "") or ""
            if "046d" not in iid.lower():
                continue
            pid_hex = iid.lower().split("_")[-1] if "_" in iid else ""
            if pid_hex:
                pids.add(f"0x{pid_hex}")
    return pids


def load_depot(depot_dir: Path) -> Depot:
    """Load a per-device depot directory.

    Handles both modern (`core_metadata.json` + `*_core.png`) and legacy
    (`metadata.json` + unprefixed PNGs) naming conventions.
    """
    metadata = _load_first(depot_dir, ["core_metadata.json", "metadata.json"])
    front = _find_first(depot_dir, ["front_core.png", "front.png"])
    side  = _find_first(depot_dir, ["side_core.png",  "side.png"])
    back  = _find_first(
        depot_dir,
        ["back_core.png", "bottom_core.png", "back.png", "bottom.png"],
    )
    return Depot(
        path=depot_dir,
        metadata=metadata,
        front_image=front,
        side_image=side,
        back_image=back,
    )


def _load_first(base: Path, names: list[str]) -> Optional[dict]:
    for n in names:
        p = base / n
        if p.exists():
            try:
                return json.load(open(p))
            except (OSError, json.JSONDecodeError):
                return None
    return None


def _find_first(base: Path, names: list[str]) -> Optional[Path]:
    for n in names:
        p = base / n
        if p.exists():
            return p
    return None
```

- [ ] **Step 12.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: all four sources-related tests PASS.

- [ ] **Step 12.5: Commit**

```bash
git add scripts/optionsplus_extractor/sources.py tests/scripts/test_extractor.py
git commit -m "sources module: load options+ device db and depot files"
```

---

### Task 13: `capabilities.py` — features + DPI mapping

**Files:**
- Modify: `scripts/optionsplus_extractor/capabilities.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 13.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from optionsplus_extractor import capabilities


def test_features_for_mx_master_3s():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    caps = mice["mx_master_3s"].capabilities
    f = capabilities.features_from_capabilities(caps)
    assert f["battery"] is True
    assert f["adjustableDpi"] is True
    assert f["smartShift"] is True
    assert f["hiResWheel"] is True
    assert f["thumbWheel"] is True
    assert f["reprogControls"] is True
    assert f["smoothScroll"] is True
    assert f["gestureV2"] is False
    assert f["hapticFeedback"] is False


def test_dpi_from_high_res_sensor_info():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    caps = mice["mx_master_3s"].capabilities
    dpi = capabilities.dpi_from_capabilities(caps)
    assert dpi["min"] == 200
    assert dpi["max"] == 8000
    assert dpi["step"] == 50


def test_dpi_defaults_when_no_sensor_info():
    dpi = capabilities.dpi_from_capabilities({})
    assert dpi == {"min": 200, "max": 4000, "step": 50}


def test_features_default_false_on_empty_caps():
    f = capabilities.features_from_capabilities({})
    assert f["battery"] is False
    assert f["smartShift"] is False
    # smoothScroll DEFAULTS TO TRUE per parser convention
    assert f["smoothScroll"] is True
```

- [ ] **Step 13.2: Run tests, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k capabilities -v 2>&1 | tail -15`
Expected: FAIL with AttributeError (functions don't exist yet).

- [ ] **Step 13.3: Implement `capabilities.py`**

Replace `scripts/optionsplus_extractor/capabilities.py` with:

```python
"""Map Options+ capabilities dict to our features + dpi."""

from __future__ import annotations


def features_from_capabilities(caps: dict) -> dict:
    """Map an Options+ `capabilities` object to our 9-key feature subset.

    The parser defaults unset feature keys to false, except smoothScroll
    which defaults to true. We emit explicitly for the 9 keys we can
    determine from Options+ data.
    """
    swc = caps.get("scroll_wheel_capabilities", {}) or {}
    smooth = swc.get("smooth_scroll", {})
    if isinstance(smooth, bool):
        smooth_on = smooth
    else:
        smooth_on = bool((smooth or {}).get("win") or (smooth or {}).get("mac"))

    has_adjustable_dpi = (
        bool(caps.get("hasHighResolutionSensor"))
        or "highResolutionSensorInfo" in caps
        or bool(caps.get("pointerSpeed"))
    )
    has_programmable = bool((caps.get("specialKeys") or {}).get("programmable"))

    return {
        "battery": bool(
            caps.get("hasBatteryStatus") or caps.get("unified_battery")
        ),
        "adjustableDpi": has_adjustable_dpi,
        "smartShift": bool(swc.get("smartshift")),
        "hiResWheel": bool(swc.get("high_resolution")),
        "thumbWheel": "mouseThumbWheelOverride" in caps,
        "reprogControls": has_programmable,
        "smoothScroll": smooth_on if caps else True,
        "gestureV2": False,       # not represented in Options+ DB
        "hapticFeedback": False,  # not represented in Options+ DB
    }


def dpi_from_capabilities(caps: dict) -> dict:
    """Read DPI range from highResolutionSensorInfo; fall back to safe defaults."""
    info = caps.get("highResolutionSensorInfo")
    if info:
        return {
            "min": info.get("minDpiValueSensorOn", 200),
            "max": info.get("maxDpiValueSensorOn", 4000),
            "step": info.get("stepsSensorOn", 50),
        }
    return {"min": 200, "max": 4000, "step": 50}
```

- [ ] **Step 13.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: all tests up to this point PASS.

- [ ] **Step 13.5: Commit**

```bash
git add scripts/optionsplus_extractor/capabilities.py tests/scripts/test_extractor.py
git commit -m "capabilities module: features + dpi mapping"
```

---

### Task 14: `slots.py` — parse metadata into typed records

**Files:**
- Modify: `scripts/optionsplus_extractor/slots.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 14.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from optionsplus_extractor import slots


def test_parse_buttons_for_3s():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    parsed = slots.parse(depot.metadata)
    assert len(parsed.buttons) == 6  # 6 configurable buttons on MX Master 3S
    cids = sorted(b.cid for b in parsed.buttons)
    # 3S has Middle(0x52), Back(0x53), Forward(0x56), Gesture(0xC3),
    # Shift(0xC4), Thumb(0x0000 synthetic)
    assert 0x0000 in cids  # thumbwheel synthetic
    assert 0x0052 in cids
    assert 0x00C3 in cids
    assert 0x00C4 in cids


def test_parse_scroll_for_3s():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    parsed = slots.parse(depot.metadata)
    kinds = {s.kind for s in parsed.scroll}
    assert kinds == {"scrollwheel", "thumbwheel", "pointer"}


def test_parse_easyswitch_for_3s():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    parsed = slots.parse(depot.metadata)
    assert len(parsed.easyswitch) == 3
    # slot numbers must be 1, 2, 3 in order
    assert [s.index for s in parsed.easyswitch] == [1, 2, 3]


def test_marker_coords_are_percentages_not_pixels():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    parsed = slots.parse(depot.metadata)
    for b in parsed.buttons:
        assert 0.0 <= b.x_pct <= 1.0
        assert 0.0 <= b.y_pct <= 1.0
    # Middle click sits near the top of the scroll wheel — x and y
    # should be well above the origin, not a fraction-of-a-pixel away.
    middle = next(b for b in parsed.buttons if b.cid == 0x0052)
    assert middle.x_pct > 0.5
    assert middle.y_pct < 0.3
    assert middle.y_pct > 0.05  # not clustered at origin


def test_unknown_slot_name_raises():
    bogus = {
        "images": [{
            "key": "device_buttons_image",
            "origin": {"width": 100, "height": 100},
            "assignments": [{
                "slotId": "x_c82",
                "slotName": "SLOT_NAME_NONEXISTENT_BUTTON",
                "marker": {"x": 50, "y": 50},
            }],
        }],
    }
    import pytest
    with pytest.raises(slots.UnknownSlotName) as exc:
        slots.parse(bogus)
    assert "SLOT_NAME_NONEXISTENT_BUTTON" in str(exc.value)
```

- [ ] **Step 14.2: Run tests, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k "parse or unknown" -v 2>&1 | tail -20`
Expected: FAIL — `slots.parse` doesn't exist yet.

- [ ] **Step 14.3: Implement `slots.py`**

Replace `scripts/optionsplus_extractor/slots.py` with:

```python
"""Parse core_metadata.json image assignments into typed slot records."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Optional


# Virtual CID used for the thumb wheel. The HID++ device does not expose
# a real CID for the thumb wheel button, so we emit a synthetic 0x0000
# entry that the app recognizes as a sentinel (AppController.cpp:366).
THUMBWHEEL_CID = 0x0000


# Slot name → (defaultName, defaultActionType, configurable)
SLOT_NAME_MAP: dict[str, tuple[str, str, bool]] = {
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


class UnknownSlotName(Exception):
    """Raised when a slot name isn't in SLOT_NAME_MAP."""


@dataclass
class ButtonSlot:
    cid: int
    name: str
    action_type: str
    configurable: bool
    x_pct: float
    y_pct: float


@dataclass
class ScrollSlot:
    kind: str        # "scrollwheel" | "thumbwheel" | "pointer"
    x_pct: float
    y_pct: float


@dataclass
class EasySwitchSlot:
    index: int       # 1-based
    x_pct: float
    y_pct: float


@dataclass
class ParsedMetadata:
    buttons: list[ButtonSlot] = field(default_factory=list)
    scroll: list[ScrollSlot] = field(default_factory=list)
    easyswitch: list[EasySwitchSlot] = field(default_factory=list)


def parse(metadata: Optional[dict]) -> ParsedMetadata:
    """Parse a core_metadata.json dict into typed slot records.

    Unknown slot names raise UnknownSlotName. Assignments with no
    recognizable CID or slot kind are silently skipped (not all
    slots in every image are ones we care about).
    """
    out = ParsedMetadata()
    if not metadata:
        return out

    images = metadata.get("images", []) or []
    for img in images:
        key = img.get("key")
        assignments = img.get("assignments", []) or []
        if key == "device_buttons_image":
            out.buttons.extend(_parse_buttons(assignments))
        elif key == "device_point_scroll_image":
            out.scroll.extend(_parse_scroll(assignments))
        elif key == "device_easyswitch_image":
            out.easyswitch.extend(_parse_easyswitch(assignments))
        # device_gesture_buttons_image is intentionally skipped
        # (default gestures come from AppController hardcoded defaults)

    return out


def _marker_to_pct(marker: dict) -> tuple[float, float]:
    """Options+ markers encode position as percentages in [0, 100]."""
    x = round(float(marker.get("x", 0)) / 100.0, 3)
    y = round(float(marker.get("y", 0)) / 100.0, 3)
    return max(0.0, min(1.0, x)), max(0.0, min(1.0, y))


_CID_SUFFIX_RE = re.compile(r"_c(\d+)$")


def _cid_from_slot_id(slot_id: str) -> Optional[int]:
    m = _CID_SUFFIX_RE.search(slot_id or "")
    return int(m.group(1)) if m else None


def _is_thumbwheel_slot(slot_id: str, slot_name: str) -> bool:
    return (
        slot_name == "SLOT_NAME_THUMBWHEEL"
        or "thumb_wheel" in (slot_id or "").lower()
    )


def _parse_buttons(assignments: list[dict]) -> list[ButtonSlot]:
    out: list[ButtonSlot] = []
    for a in assignments:
        slot_id = a.get("slotId", "") or ""
        slot_name = a.get("slotName", "") or ""

        cid = _cid_from_slot_id(slot_id)
        is_thumb = _is_thumbwheel_slot(slot_id, slot_name)
        if cid is None and not is_thumb:
            continue
        if cid is None and is_thumb:
            cid = THUMBWHEEL_CID

        if slot_name not in SLOT_NAME_MAP:
            raise UnknownSlotName(
                f"unknown slot name {slot_name!r} (slotId={slot_id!r})"
            )
        name, action_type, configurable = SLOT_NAME_MAP[slot_name]

        x, y = _marker_to_pct(a.get("marker", {}) or {})
        out.append(ButtonSlot(
            cid=cid,
            name=name,
            action_type=action_type,
            configurable=configurable,
            x_pct=x,
            y_pct=y,
        ))
    return out


def _scroll_kind_from_slot_id(slot_id: str) -> Optional[str]:
    sid = slot_id.lower()
    if "scroll_wheel" in sid:
        return "scrollwheel"
    if "thumb_wheel" in sid:
        return "thumbwheel"
    if "mouse_settings" in sid or "pointer" in sid:
        return "pointer"
    return None


def _parse_scroll(assignments: list[dict]) -> list[ScrollSlot]:
    out: list[ScrollSlot] = []
    for a in assignments:
        slot_id = a.get("slotId", "") or ""
        kind = _scroll_kind_from_slot_id(slot_id)
        if kind is None:
            # No recognized kind — skip rather than raise.  Scroll-image
            # assignments are a known superset of what PointScrollPage
            # renders.
            continue
        x, y = _marker_to_pct(a.get("marker", {}) or {})
        out.append(ScrollSlot(kind=kind, x_pct=x, y_pct=y))
    return out


_EASYSWITCH_RE = re.compile(r"_easy_switch_(\d+)$")


def _parse_easyswitch(assignments: list[dict]) -> list[EasySwitchSlot]:
    out: list[EasySwitchSlot] = []
    for a in assignments:
        slot_id = a.get("slotId", "") or ""
        m = _EASYSWITCH_RE.search(slot_id)
        if not m:
            continue
        idx = int(m.group(1))
        x, y = _marker_to_pct(a.get("marker", {}) or {})
        out.append(EasySwitchSlot(index=idx, x_pct=x, y_pct=y))
    out.sort(key=lambda s: s.index)
    return out
```

- [ ] **Step 14.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: all slot tests PASS.

- [ ] **Step 14.5: Commit**

```bash
git add scripts/optionsplus_extractor/slots.py tests/scripts/test_extractor.py
git commit -m "slots module: typed parsing of core_metadata image assignments"
```

---

### Task 15: `canonicalize.py` — sort rules

**Files:**
- Modify: `scripts/optionsplus_extractor/canonicalize.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 15.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from optionsplus_extractor import canonicalize
from optionsplus_extractor.slots import (
    ButtonSlot, ScrollSlot, EasySwitchSlot, THUMBWHEEL_CID,
)


def test_canonicalize_buttons_sorts_by_cid_thumb_last():
    raw = [
        ButtonSlot(cid=0x00C4, name="Shift",   action_type="smartshift-toggle", configurable=True, x_pct=0.8, y_pct=0.3),
        ButtonSlot(cid=THUMBWHEEL_CID, name="Thumb wheel", action_type="default", configurable=True, x_pct=0.5, y_pct=0.5),
        ButtonSlot(cid=0x0052, name="Middle",  action_type="default", configurable=True, x_pct=0.7, y_pct=0.2),
        ButtonSlot(cid=0x0056, name="Forward", action_type="default", configurable=True, x_pct=0.5, y_pct=0.6),
        ButtonSlot(cid=0x0053, name="Back",    action_type="default", configurable=True, x_pct=0.5, y_pct=0.7),
        ButtonSlot(cid=0x00C3, name="Gesture", action_type="gesture-trigger", configurable=True, x_pct=0.1, y_pct=0.6),
    ]
    sorted_ = canonicalize.sort_buttons(raw)
    cids = [b.cid for b in sorted_]
    assert cids == [0x0052, 0x0053, 0x0056, 0x00C3, 0x00C4, THUMBWHEEL_CID]


def test_canonicalize_scroll_sorts_by_kind():
    raw = [
        ScrollSlot(kind="pointer",     x_pct=0.83, y_pct=0.54),
        ScrollSlot(kind="scrollwheel", x_pct=0.73, y_pct=0.16),
        ScrollSlot(kind="thumbwheel",  x_pct=0.55, y_pct=0.51),
    ]
    sorted_ = canonicalize.sort_scroll(raw)
    assert [s.kind for s in sorted_] == ["scrollwheel", "thumbwheel", "pointer"]


def test_canonicalize_scroll_handles_missing_kind():
    raw = [
        ScrollSlot(kind="pointer",     x_pct=0.83, y_pct=0.54),
        ScrollSlot(kind="scrollwheel", x_pct=0.73, y_pct=0.16),
    ]
    sorted_ = canonicalize.sort_scroll(raw)
    assert [s.kind for s in sorted_] == ["scrollwheel", "pointer"]


def test_canonicalize_easyswitch_keeps_first_three():
    raw = [
        EasySwitchSlot(index=3, x_pct=0.3, y_pct=0.3),
        EasySwitchSlot(index=1, x_pct=0.1, y_pct=0.1),
        EasySwitchSlot(index=2, x_pct=0.2, y_pct=0.2),
    ]
    sorted_ = canonicalize.sort_easyswitch(raw)
    assert [s.index for s in sorted_] == [1, 2, 3]
```

- [ ] **Step 15.2: Run, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k canonicalize -v 2>&1 | tail -15`
Expected: FAIL — functions don't exist.

- [ ] **Step 15.3: Implement `canonicalize.py`**

Replace `scripts/optionsplus_extractor/canonicalize.py` with:

```python
"""Sort / index-assignment rules for controls, scroll hotspots, easy-switch slots."""

from __future__ import annotations

from .slots import ButtonSlot, ScrollSlot, EasySwitchSlot, THUMBWHEEL_CID


_SCROLL_KIND_ORDER = {"scrollwheel": 0, "thumbwheel": 1, "pointer": 2}


def sort_buttons(buttons: list[ButtonSlot]) -> list[ButtonSlot]:
    """Sort buttons by CID ascending; synthetic thumb wheel (0x0000) appended last.

    For the MX Master line this yields Middle / Back / Forward / Gesture /
    Shift / Thumb — identical to the shipped descriptor ordering, so
    position-based profile persistence keeps working.
    """
    def key(b: ButtonSlot) -> tuple[int, int]:
        if b.cid == THUMBWHEEL_CID:
            return (1, 0)
        return (0, b.cid)
    return sorted(buttons, key=key)


def sort_scroll(scrolls: list[ScrollSlot]) -> list[ScrollSlot]:
    """Sort scroll slots by canonical kind order; drops unknown kinds.

    The app's PointScrollPage looks hotspots up by `kind`, so ordering
    here is cosmetic — but it matches Jelco's shipped order
    (scrollwheel / thumbwheel / pointer) so golden-file diffs stay
    stable.
    """
    return sorted(
        (s for s in scrolls if s.kind in _SCROLL_KIND_ORDER),
        key=lambda s: _SCROLL_KIND_ORDER[s.kind],
    )


def sort_easyswitch(slots: list[EasySwitchSlot]) -> list[EasySwitchSlot]:
    """Sort easy-switch slots by 1-based index; keeps only the first three."""
    ordered = sorted(slots, key=lambda s: s.index)
    return ordered[:3]
```

- [ ] **Step 15.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -15`
Expected: all canonicalize tests PASS.

- [ ] **Step 15.5: Commit**

```bash
git add scripts/optionsplus_extractor/canonicalize.py tests/scripts/test_extractor.py
git commit -m "canonicalize module: sort rules for controls/scroll/easyswitch"
```

---

### Task 16: `descriptor.py` — assemble final dict

**Files:**
- Modify: `scripts/optionsplus_extractor/descriptor.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 16.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from optionsplus_extractor import descriptor as descbuilder


def test_build_descriptor_uses_parser_compatible_field_names():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    entry = mice["mx_master_3s"]
    d = descbuilder.build(entry, depot)

    # Top-level fields the parser reads
    assert d["name"] == "MX Master 3S"
    assert d["version"] == 1
    assert d["status"] in ("community-verified", "placeholder")
    assert isinstance(d["productIds"], list)
    assert all(pid.startswith("0x") for pid in d["productIds"])

    # Controls use controlId / buttonIndex / defaultName / defaultActionType
    assert len(d["controls"]) == 8
    for c in d["controls"]:
        assert set(c.keys()) == {
            "controlId", "buttonIndex", "defaultName",
            "defaultActionType", "configurable",
        }
        assert c["controlId"].startswith("0x")
    # Canonical ordering: thumb wheel at index 7
    assert d["controls"][7]["controlId"] == "0x0000"
    assert d["controls"][7]["defaultName"] == "Thumb wheel"
    # buttonIndex sequence is 0..7
    assert [c["buttonIndex"] for c in d["controls"]] == list(range(8))


def test_build_descriptor_hotspot_fields():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    d = descbuilder.build(mice["mx_master_3s"], depot)

    assert len(d["hotspots"]["buttons"]) == 6
    for h in d["hotspots"]["buttons"]:
        assert set(h.keys()) >= {"buttonIndex", "xPct", "yPct", "side", "labelOffsetYPct"}
        assert 0.0 <= h["xPct"] <= 1.0
        assert 0.0 <= h["yPct"] <= 1.0

    assert len(d["hotspots"]["scroll"]) == 3
    assert [s["kind"] for s in d["hotspots"]["scroll"]] == \
        ["scrollwheel", "thumbwheel", "pointer"]
    assert [s["buttonIndex"] for s in d["hotspots"]["scroll"]] == [-1, -2, -3]


def test_build_descriptor_easyswitch():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    d = descbuilder.build(mice["mx_master_3s"], depot)
    assert len(d["easySwitchSlots"]) == 3
    for s in d["easySwitchSlots"]:
        assert set(s.keys()) == {"xPct", "yPct"}


def test_build_descriptor_status_downgrades_on_empty_controls():
    empty_depot = sources.Depot(
        path=FIXTURE_ROOT,   # anything
        metadata={"images": []},  # no assignments → no buttons → no hotspots
        front_image=None,
        side_image=None,
        back_image=None,
    )
    entry = sources.DeviceDbEntry(
        depot="bogus",
        name="Bogus",
        product_ids=["0x1234"],
        capabilities={},
    )
    d = descbuilder.build(entry, empty_depot)
    assert d["status"] == "placeholder"
```

- [ ] **Step 16.2: Run, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k descriptor -v 2>&1 | tail -20`
Expected: FAIL.

- [ ] **Step 16.3: Implement `descriptor.py`**

Replace `scripts/optionsplus_extractor/descriptor.py` with:

```python
"""Assemble the final descriptor dict with parser-compatible field names."""

from __future__ import annotations

from .sources import DeviceDbEntry, Depot
from .capabilities import features_from_capabilities, dpi_from_capabilities
from . import slots as slots_mod
from . import canonicalize


_DEFAULT_CONTROLS = [
    {
        "controlId": "0x0050",
        "buttonIndex": 0,
        "defaultName": "Left click",
        "defaultActionType": "default",
        "configurable": False,
    },
    {
        "controlId": "0x0051",
        "buttonIndex": 1,
        "defaultName": "Right click",
        "defaultActionType": "default",
        "configurable": False,
    },
]


def build(entry: DeviceDbEntry, depot: Depot) -> dict:
    """Assemble a descriptor dict for one device.

    Raises `slots.UnknownSlotName` if the metadata contains a slot name
    not in the mapping table.
    """
    parsed = slots_mod.parse(depot.metadata)

    ordered_buttons = canonicalize.sort_buttons(parsed.buttons)
    ordered_scroll = canonicalize.sort_scroll(parsed.scroll)
    ordered_easyswitch = canonicalize.sort_easyswitch(parsed.easyswitch)

    controls: list[dict] = list(_DEFAULT_CONTROLS)
    button_hotspots: list[dict] = []
    for idx_offset, b in enumerate(ordered_buttons):
        button_index = len(_DEFAULT_CONTROLS) + idx_offset
        controls.append({
            "controlId": f"0x{b.cid:04X}",
            "buttonIndex": button_index,
            "defaultName": b.name,
            "defaultActionType": b.action_type,
            "configurable": b.configurable,
        })
        button_hotspots.append({
            "buttonIndex": button_index,
            "xPct": b.x_pct,
            "yPct": b.y_pct,
            "side": "right" if b.x_pct > 0.5 else "left",
            "labelOffsetYPct": 0.0,
        })

    scroll_hotspots: list[dict] = []
    for slot_index, s in enumerate(ordered_scroll, start=1):
        scroll_hotspots.append({
            "kind": s.kind,
            "buttonIndex": -slot_index,
            "xPct": s.x_pct,
            "yPct": s.y_pct,
            "side": "right" if s.x_pct > 0.5 else "left",
            "labelOffsetYPct": 0.0,
        })

    easy_switch = [
        {"xPct": s.x_pct, "yPct": s.y_pct} for s in ordered_easyswitch
    ]

    images: dict[str, str] = {}
    if depot.front_image is not None:
        images["front"] = "front.png"
    if depot.side_image is not None:
        images["side"] = "side.png"
    if depot.back_image is not None:
        images["back"] = "back.png"

    has_buttons = len(button_hotspots) > 0
    status = "community-verified" if (has_buttons and "front" in images) else "placeholder"

    return {
        "name": entry.name,
        "status": status,
        "version": 1,
        "productIds": sorted(entry.product_ids),
        "features": features_from_capabilities(entry.capabilities),
        "dpi": dpi_from_capabilities(entry.capabilities),
        "controls": controls,
        "hotspots": {
            "buttons": button_hotspots,
            "scroll": scroll_hotspots,
        },
        "images": images,
        "easySwitchSlots": easy_switch,
        "defaultGestures": {},
    }
```

- [ ] **Step 16.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: all descriptor tests PASS.

- [ ] **Step 16.5: Commit**

```bash
git add scripts/optionsplus_extractor/descriptor.py tests/scripts/test_extractor.py
git commit -m "descriptor module: assemble parser-compatible descriptor dict"
```

---

### Task 17: `validate.py` — Layer 1 schema self-check

**Files:**
- Modify: `scripts/optionsplus_extractor/validate.py`
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 17.1: Write failing tests**

Append to `tests/scripts/test_extractor.py`:

```python
from optionsplus_extractor import validate


def test_validate_accepts_good_descriptor():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    d = descbuilder.build(mice["mx_master_3s"], depot)
    # Does not raise
    validate.check(d, depot)


def test_validate_rejects_wrong_control_field_name():
    d = {
        "name": "Bogus",
        "status": "community-verified",
        "productIds": ["0x1234"],
        "features": {},
        "controls": [
            # wrong field name — the exact bug class we're guarding against
            {"cid": "0x0050", "buttonIndex": 0, "defaultName": "L", "defaultActionType": "default", "configurable": False},
        ],
        "hotspots": {"buttons": [], "scroll": []},
        "images": {"front": "front.png"},
        "easySwitchSlots": [],
    }
    import pytest
    with pytest.raises(validate.SchemaError) as exc:
        validate.check(d, None)
    assert "controlId" in str(exc.value)


def test_validate_rejects_invalid_scroll_kind():
    d = {
        "name": "Bogus",
        "status": "community-verified",
        "productIds": ["0x1234"],
        "features": {},
        "controls": [
            {"controlId": "0x0050", "buttonIndex": 0, "defaultName": "L", "defaultActionType": "default", "configurable": False},
        ],
        "hotspots": {
            "buttons": [],
            "scroll": [
                {"kind": "bogus", "buttonIndex": -1, "xPct": 0.5, "yPct": 0.5, "side": "right", "labelOffsetYPct": 0.0},
            ],
        },
        "images": {"front": "front.png"},
        "easySwitchSlots": [],
    }
    import pytest
    with pytest.raises(validate.SchemaError):
        validate.check(d, None)


def test_validate_rejects_missing_productIds():
    d = {
        "name": "Bogus",
        "status": "community-verified",
        "productIds": [],
        "features": {},
        "controls": [],
        "hotspots": {"buttons": [], "scroll": []},
        "images": {},
        "easySwitchSlots": [],
    }
    import pytest
    with pytest.raises(validate.SchemaError):
        validate.check(d, None)
```

- [ ] **Step 17.2: Run, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k validate -v 2>&1 | tail -20`
Expected: FAIL.

- [ ] **Step 17.3: Implement `validate.py`**

Replace `scripts/optionsplus_extractor/validate.py` with:

```python
"""Layer 1 schema self-check: verify generated dict matches JsonDevice parser expectations."""

from __future__ import annotations

from typing import Optional

from .sources import Depot


class SchemaError(Exception):
    """Raised when a generated descriptor doesn't match the parser's field expectations."""


_ALLOWED_SCROLL_KINDS = {"", "scrollwheel", "thumbwheel", "pointer"}

_REQUIRED_CONTROL_KEYS = {"controlId", "buttonIndex", "defaultName", "defaultActionType"}
_REQUIRED_HOTSPOT_KEYS = {"buttonIndex", "xPct", "yPct", "side"}


def check(descriptor: dict, depot: Optional[Depot]) -> None:
    """Validate a generated descriptor dict against JsonDevice.cpp expectations.

    Raises SchemaError with a specific message on the first mismatch.
    `depot` is only used to verify referenced images exist; pass None
    to skip that check (used by tests that build descriptors without
    real image files).
    """
    _check_top_level(descriptor)
    _check_controls(descriptor.get("controls", []))
    _check_hotspots(descriptor.get("hotspots", {}))
    _check_images(descriptor.get("images", {}), depot)


def _check_top_level(d: dict) -> None:
    if not d.get("name"):
        raise SchemaError("missing or empty 'name'")
    pids = d.get("productIds") or []
    if not isinstance(pids, list) or not pids:
        raise SchemaError("productIds must be a non-empty list")
    for pid in pids:
        if not isinstance(pid, str) or not pid.startswith("0x"):
            raise SchemaError(f"productIds entry {pid!r} is not a hex string")
        try:
            int(pid, 16)
        except ValueError:
            raise SchemaError(f"productIds entry {pid!r} is not a valid hex int")


def _check_controls(controls: list) -> None:
    for i, c in enumerate(controls):
        if not isinstance(c, dict):
            raise SchemaError(f"controls[{i}] is not an object")
        missing = _REQUIRED_CONTROL_KEYS - set(c.keys())
        if missing:
            raise SchemaError(
                f"controls[{i}] missing required keys {sorted(missing)} "
                f"(got keys: {sorted(c.keys())})"
            )
        cid = c["controlId"]
        if not isinstance(cid, str) or not cid.startswith("0x"):
            raise SchemaError(f"controls[{i}].controlId {cid!r} is not a hex string")
        try:
            int(cid, 16)
        except ValueError:
            raise SchemaError(f"controls[{i}].controlId {cid!r} is not valid hex")
        if not isinstance(c["buttonIndex"], int):
            raise SchemaError(f"controls[{i}].buttonIndex must be int")


def _check_hotspots(hotspots: dict) -> None:
    for group in ("buttons", "scroll"):
        entries = hotspots.get(group, [])
        for i, h in enumerate(entries):
            if not isinstance(h, dict):
                raise SchemaError(f"hotspots.{group}[{i}] is not an object")
            missing = _REQUIRED_HOTSPOT_KEYS - set(h.keys())
            if missing:
                raise SchemaError(
                    f"hotspots.{group}[{i}] missing required keys {sorted(missing)}"
                )
            if not isinstance(h["xPct"], (int, float)):
                raise SchemaError(f"hotspots.{group}[{i}].xPct must be numeric")
            if not (0.0 <= h["xPct"] <= 1.0 and 0.0 <= h["yPct"] <= 1.0):
                raise SchemaError(
                    f"hotspots.{group}[{i}] coords out of range [0,1]: "
                    f"({h['xPct']}, {h['yPct']})"
                )
            if group == "scroll":
                kind = h.get("kind", "")
                if kind not in _ALLOWED_SCROLL_KINDS:
                    raise SchemaError(
                        f"hotspots.scroll[{i}].kind {kind!r} not in "
                        f"{sorted(_ALLOWED_SCROLL_KINDS - {''})}"
                    )


def _check_images(images: dict, depot: Optional[Depot]) -> None:
    if depot is None:
        return
    front = images.get("front")
    if front and depot.front_image is None:
        raise SchemaError("images.front set but depot has no front image")
```

- [ ] **Step 17.4: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -20`
Expected: all validate tests PASS.

- [ ] **Step 17.5: Commit**

```bash
git add scripts/optionsplus_extractor/validate.py tests/scripts/test_extractor.py
git commit -m "validate module: layer-1 schema self-check"
```

---

### Task 18: `cli.py` + shim + end-to-end smoke test

**Files:**
- Modify: `scripts/optionsplus_extractor/cli.py`
- Modify: `scripts/generate-from-optionsplus.py` (replace with shim)
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 18.1: Write a failing end-to-end test**

Append to `tests/scripts/test_extractor.py`:

```python
import shutil
from optionsplus_extractor import cli


def test_cli_end_to_end(tmp_path):
    out_dir = tmp_path / "out"
    rc = cli.run(
        devices_dir=FIXTURE_ROOT / "devices",
        main_dir=FIXTURE_ROOT / "main" / "logioptionsplus",
        output_dir=out_dir,
    )
    assert rc == 0
    # Three devices extracted
    for dev_slug in ("mx-master-2s", "mx-master-3s", "mx-master-4"):
        desc_path = out_dir / dev_slug / "descriptor.json"
        assert desc_path.exists(), f"missing {desc_path}"
        img_path = out_dir / dev_slug / "front.png"
        assert img_path.exists(), f"missing {img_path}"
```

- [ ] **Step 18.2: Run, confirm failure**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k end_to_end -v 2>&1 | tail -15`
Expected: FAIL — `cli.run` doesn't exist.

- [ ] **Step 18.3: Implement `cli.py`**

Replace `scripts/optionsplus_extractor/cli.py` with:

```python
"""argparse wrapper — glues sources → capabilities → slots → canonicalize → descriptor → validate."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Optional

from . import sources, descriptor, validate
from .slots import UnknownSlotName


def _slugify(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def run(
    devices_dir: Path,
    main_dir: Path,
    output_dir: Path,
    dry_run: bool = False,
    skip_existing: bool = False,
) -> int:
    """Run the full extraction pipeline. Returns a process exit code."""
    devices_dir = Path(devices_dir)
    main_dir = Path(main_dir)
    output_dir = Path(output_dir)

    mice = sources.load_device_db(main_dir)
    print(f"Found {len(mice)} mice in Options+ database", file=sys.stderr)

    processed = 0
    skipped_no_images = 0
    unknown_slot_reports: list[dict] = []

    for depot_name, entry in sorted(mice.items(), key=lambda kv: kv[1].name):
        slug = _slugify(entry.name)
        depot_dir = devices_dir / depot_name
        out_dir = output_dir / slug

        if skip_existing and (out_dir / "descriptor.json").exists():
            continue

        depot = sources.load_depot(depot_dir)
        if depot.front_image is None:
            print(f"skip {slug}: no front image", file=sys.stderr)
            skipped_no_images += 1
            continue

        try:
            desc = descriptor.build(entry, depot)
        except UnknownSlotName as e:
            unknown_slot_reports.append({"device": slug, "error": str(e)})
            print(f"skip {slug}: unknown slot — {e}", file=sys.stderr)
            continue

        try:
            validate.check(desc, depot)
        except validate.SchemaError as e:
            print(f"skip {slug}: schema self-check failed — {e}", file=sys.stderr)
            continue

        if dry_run:
            print(f"  {entry.name:40s} -> {slug}/ ({len(desc['controls'])} controls)")
            processed += 1
            continue

        out_dir.mkdir(parents=True, exist_ok=True)
        with open(out_dir / "descriptor.json", "w") as f:
            json.dump(desc, f, indent=2)
            f.write("\n")
        shutil.copy2(depot.front_image, out_dir / "front.png")
        if depot.side_image is not None:
            shutil.copy2(depot.side_image, out_dir / "side.png")
        if depot.back_image is not None:
            shutil.copy2(depot.back_image, out_dir / "back.png")

        print(
            f"  {slug}: {len(desc['controls'])} controls, "
            f"{len(desc['hotspots']['buttons'])} hotspots, "
            f"{len(desc['hotspots']['scroll'])} scroll, "
            f"{len(desc['easySwitchSlots'])} easyswitch"
        )
        processed += 1

    report_path = output_dir / "extraction-report.json"
    output_dir.mkdir(parents=True, exist_ok=True)
    with open(report_path, "w") as f:
        json.dump({
            "processed": processed,
            "skipped_no_images": skipped_no_images,
            "unknown_slot_names": unknown_slot_reports,
        }, f, indent=2)

    print(
        f"Done: {processed} generated, {skipped_no_images} missing images, "
        f"{len(unknown_slot_reports)} unknown slots",
        file=sys.stderr,
    )
    return 1 if unknown_slot_reports else 0


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate logitune descriptors from extracted Options+ data",
    )
    parser.add_argument("--devices-dir", required=True,
                        help="extracted per-device depot directory")
    parser.add_argument("--main-dir", required=True,
                        help="extracted logioptionsplus main depot directory")
    parser.add_argument("--output-dir", required=True,
                        help="output directory for descriptors")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-existing", action="store_true", default=False)
    args = parser.parse_args(argv)
    return run(
        devices_dir=Path(args.devices_dir),
        main_dir=Path(args.main_dir),
        output_dir=Path(args.output_dir),
        dry_run=args.dry_run,
        skip_existing=args.skip_existing,
    )


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 18.4: Replace the top-level shim**

Overwrite `scripts/generate-from-optionsplus.py` with:

```python
#!/usr/bin/env python3
"""Entry-point shim for optionsplus_extractor.cli.

The real implementation lives in scripts/optionsplus_extractor/. This
shim stays in place so existing docs and CI paths keep working.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from optionsplus_extractor.cli import main

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 18.5: Rerun tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -15`
Expected: all tests PASS, including `test_cli_end_to_end`.

- [ ] **Step 18.6: Commit**

```bash
git add scripts/optionsplus_extractor/cli.py scripts/generate-from-optionsplus.py tests/scripts/test_extractor.py
git commit -m "cli wrapper + top-level shim; end-to-end pipeline test"
```

---

### Task 19: Golden-file integration test

**Files:**
- Modify: `tests/scripts/test_extractor.py`

- [ ] **Step 19.1: Write the golden-file test**

Append to `tests/scripts/test_extractor.py`:

```python
SHIPPED_DEVICE_DIR = Path(__file__).parent.parent.parent / "devices"


def _load_shipped(slug: str) -> dict:
    return json.load(open(SHIPPED_DEVICE_DIR / slug / "descriptor.json"))


def _load_extracted(tmp_path: Path, slug: str) -> dict:
    return json.load(open(tmp_path / "out" / slug / "descriptor.json"))


@pytest.mark.parametrize("slug", ["mx-master-2s", "mx-master-3s", "mx-master-4"])
def test_golden_file_equivalence(tmp_path, slug):
    out_dir = tmp_path / "out"
    cli.run(
        devices_dir=FIXTURE_ROOT / "devices",
        main_dir=FIXTURE_ROOT / "main" / "logioptionsplus",
        output_dir=out_dir,
    )
    extracted = _load_extracted(tmp_path, slug)
    shipped = _load_shipped(slug)

    # --- structural (exact match) ---
    assert extracted["name"] == shipped["name"]
    assert len(extracted["controls"]) == 8
    assert len(extracted["controls"]) == len(shipped["controls"])
    assert len(extracted["hotspots"]["buttons"]) == len(shipped["hotspots"]["buttons"])
    assert len(extracted["hotspots"]["scroll"]) == 3
    assert [s["kind"] for s in extracted["hotspots"]["scroll"]] == \
        ["scrollwheel", "thumbwheel", "pointer"]
    assert len(extracted["easySwitchSlots"]) == len(shipped["easySwitchSlots"])

    # productIds: extracted should be a superset
    assert set(shipped["productIds"]).issubset(set(extracted["productIds"]))

    # Control identities
    for ec, sc in zip(extracted["controls"], shipped["controls"]):
        assert ec["controlId"] == sc["controlId"]
        assert ec["buttonIndex"] == sc["buttonIndex"]
        assert ec["defaultName"] == sc["defaultName"]
        assert ec["defaultActionType"] == sc["defaultActionType"]

    # Feature flags — script emits a 9-key subset; compare those 9 only.
    for key in ("battery", "adjustableDpi", "smartShift", "hiResWheel",
                "thumbWheel", "reprogControls", "smoothScroll",
                "gestureV2", "hapticFeedback"):
        assert extracted["features"].get(key, False) == shipped["features"].get(key, False), \
            f"{slug}: feature {key} differs"

    # DPI exact
    assert extracted["dpi"] == shipped["dpi"]

    # --- tolerance (±0.02) on coordinates ---
    for eh, sh in zip(extracted["hotspots"]["buttons"], shipped["hotspots"]["buttons"]):
        assert abs(eh["xPct"] - sh["xPct"]) < 0.02, f"{slug} button xPct drift"
        assert abs(eh["yPct"] - sh["yPct"]) < 0.02, f"{slug} button yPct drift"
    for eh, sh in zip(extracted["hotspots"]["scroll"], shipped["hotspots"]["scroll"]):
        assert abs(eh["xPct"] - sh["xPct"]) < 0.02, f"{slug} scroll xPct drift"
        assert abs(eh["yPct"] - sh["yPct"]) < 0.02, f"{slug} scroll yPct drift"
    for es, ss in zip(extracted["easySwitchSlots"], shipped["easySwitchSlots"]):
        assert abs(es["xPct"] - ss["xPct"]) < 0.02, f"{slug} easyswitch xPct drift"
        assert abs(es["yPct"] - ss["yPct"]) < 0.02, f"{slug} easyswitch yPct drift"

    # --- gaps not asserted (documented) ---
    # labelOffsetYPct — shipped is hand-tuned, extracted is 0.0
    # defaultGestures — shipped has values, extracted is {}
```

Add `import pytest` to the top of `test_extractor.py` if it's not already there.

- [ ] **Step 19.2: Run the golden-file tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -k golden -v 2>&1 | tail -30`
Expected: three PASSes — one per device.

If any fail, investigate: the failure is either a real drift (fix the extractor) or a shipped descriptor that needs updating (update the descriptor). Do not lower the tolerance.

- [ ] **Step 19.3: Commit**

```bash
git add tests/scripts/test_extractor.py
git commit -m "golden-file test: extracted output matches shipped 2s/3s/4"
```

---

### Task 20: Wire extractor tests into existing test runners

**Files:**
- Modify: `scripts/pre-push` (add pytest invocation)

- [ ] **Step 20.1: Inspect the existing pre-push hook**

Run: `cat scripts/pre-push`

Identify where the existing `logitune-tests` invocation lives. We want to add a pytest run for `tests/scripts/` alongside it.

- [ ] **Step 20.2: Add pytest step to pre-push**

Append to `scripts/pre-push` (before the final success message, after the existing test-runner invocations):

```bash
echo "🐍 Running extractor pytest suite..."
(cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -q) || {
    echo "❌ extractor tests failed"
    exit 1
}
```

- [ ] **Step 20.3: Verify the hook succeeds**

Run: `bash scripts/pre-push 2>&1 | tail -20`
Expected: logitune-tests passes, then pytest passes, then "pushing" message.

- [ ] **Step 20.4: Commit**

```bash
git add scripts/pre-push
git commit -m "wire extractor pytest into pre-push hook"
```

---

### Task 21: Final verification + push

- [ ] **Step 21.1: Full C++ test suite**

Run: `cmake --build build -j$(nproc) && XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -5`
Expected: 469+ tests green (three added in Phase 1: `JsonDevice.HotspotKindRoundTrip`, `ButtonModelTest.IsThumbWheelDefault`, `ButtonModelTest.IsThumbWheelAfterLoadFromProfile`).

- [ ] **Step 21.2: Tray tests**

Run: `XDG_DATA_DIRS=$(pwd)/build QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tray-tests 2>&1 | tail -5`
Expected: green.

- [ ] **Step 21.3: Extractor tests**

Run: `cd scripts && python3 -m pytest ../tests/scripts/test_extractor.py -v 2>&1 | tail -30`
Expected: all green, including 3 golden-file tests.

- [ ] **Step 21.4: End-to-end smoke test on real hardware**

Run: `pkill -x logitune 2>/dev/null; nohup ./build/src/app/logitune > /tmp/logitune-final.log 2>&1 & disown`
Manually verify with the real MX Master 3S:
- Device appears in carousel with correct name and image
- Buttons page: click Thumb wheel → wheel-mode picker (scroll/zoom/volume/none)
- Buttons page: click Gesture button → button actions picker
- Point & Scroll page: all three callouts in correct positions
- Easy-Switch page: slot markers visible in correct spots
Then: `pkill -x logitune`.

- [ ] **Step 21.5: Regenerate descriptors from real Options+ data locally (maintainer step)**

Run: `python3 scripts/generate-from-optionsplus.py --devices-dir /tmp/optionsplus/devices --main-dir /tmp/optionsplus/main/logioptionsplus --output-dir /tmp/logitune-devices-out 2>&1 | tail -20`

Inspect `/tmp/logitune-devices-out/extraction-report.json` — confirm `unknown_slot_names` is empty (if not, Task 14's `SLOT_NAME_MAP` needs extending — see follow-up below).

Diff the three shipped descriptors against the regenerated ones:

```bash
for slug in mx-master-2s mx-master-3s mx-master-4; do
    diff -u "devices/$slug/descriptor.json" "/tmp/logitune-devices-out/$slug/descriptor.json" | head -30 || true
    echo "--- $slug diff above ---"
done
```

Any differences should be within the tolerance window from the golden tests. This step is a final manual sanity check that the local extract still works end-to-end against real Options+ data, not just the committed fixtures.

- [ ] **Step 21.6: Push and update PR #29**

```bash
git push
```

Then via gh: update PR #29's title and body to describe the rewrite instead of the reactive patches. Reference issue #28 and the spec path.

---

## Known follow-ups (out of scope for this plan)

- **CID-keyed profile persistence.** `AppController.cpp:358-439,487-494` still iterates controls by array position. A future migration to a CID-keyed profile file format would eliminate the last ordering-drift class of bug. Requires a profile-file migration path for existing users.
- **Label collision avoidance in `InfoCallout`.** Runtime fix for overlapping hotspot labels. Benefits hand-written and extracted descriptors alike.
- **Extending `SLOT_NAME_MAP`** when new Options+ releases introduce unknown slot names. The Task 21.5 dry-run will surface these; the fix is a one-line addition to `scripts/optionsplus_extractor/slots.py` plus a re-run.
- **Device onboarding wizard** (issue #22). With the extractor producing valid descriptors for every Options+-known device, the wizard's job shrinks to just the long-tail devices Options+ doesn't know about.
