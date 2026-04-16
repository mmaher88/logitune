# Editor Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an in-app WYSIWYG descriptor editor on top of `--simulate-all` that lets the maintainer drag easy-switch slots, button hotspots, and overlay cards directly on the rendered device pages, replace device images via drag-drop, and edit text labels in place — all gated behind a new `--edit` CLI flag with zero impact on the production code path.

**Architecture:** New `EditorModel` ViewModel (QML singleton, instantiated only when `--edit` is set) owns per-device pending edits, undo stacks, dirty tracking, and a `QFileSystemWatcher`. `DescriptorWriter` (Model layer) does atomic round-trip JSON I/O while preserving unknown fields. `JsonDevice` gains `sourcePath()`/`refresh()`. Existing pages (`EasySwitchPage`, `ButtonsPage`, `PointScrollPage`) gain conditional `DragHandler`s gated on `EditorModel.editing`. A shared `EditorToolbar` mounts in `Main.qml` above the page loader.

**Tech Stack:** C++20, Qt 6 (Core/Quick/Widgets), QML, gtest (C++ unit tests), Qt Quick Test (QML), CMake.

**Spec:** `docs/superpowers/specs/2026-04-15-editor-mode-design.md`

---

## File Structure

### Files to create

**Model layer (C++)**
- `src/core/devices/DescriptorWriter.h` / `.cpp` — atomic round-trip JSON writer
- `tests/test_descriptor_writer.cpp` — gtest unit tests

**ViewModel layer (C++)**
- `src/app/models/EditorModel.h` / `.cpp` — editor state, undo stacks, save flow, file watcher
- `src/app/models/EditCommand.h` — tagged-struct undo entry (header-only)
- `tests/test_editor_model.cpp` — gtest unit tests

**View layer (QML)**
- `src/app/qml/components/EditorToolbar.qml` — save/undo/redo/reset bar
- `src/app/qml/components/ConflictBanner.qml` — external-change prompt
- `src/app/qml/components/DiffModal.qml` — line-diff viewer modal
- `tests/qml/tst_EditorToolbar.qml` — QML test
- `tests/qml/tst_EasySwitchPageEdit.qml` — drag math + handler test
- `tests/qml/tst_ButtonsPageEdit.qml` — marker + card drag test

### Files to modify

**Model layer**
- `src/core/devices/JsonDevice.h` / `.cpp` — add `sourcePath()`, `loadedMtime()`, public `refresh()`, parse new optional fields (`label` on slots, `displayName` on controls)
- `src/core/interfaces/IDevice.h` — add `displayName` field to `ControlDescriptor`, `label` field to `EasySwitchSlotPosition`
- `src/core/DeviceRegistry.h` / `.cpp` — add `reload(const QString &dirPath)` and `findBySourcePath()`
- `src/core/CMakeLists.txt` — add `devices/DescriptorWriter.cpp`

**ViewModel layer**
- `src/app/AppController.h` / `.cpp` — `startMonitoring(bool simulateAll, bool editMode)`, instantiate and own `EditorModel` when `editMode`
- `src/app/models/DeviceModel.h` / `.cpp` — add `refreshFromActiveDevice()` slot that re-emits `selectedChanged` after registry reload
- `src/app/CMakeLists.txt` — add `models/EditorModel.cpp`, register new QML files

**View layer**
- `src/app/main.cpp` — parse `--edit`, pass into `controller.startMonitoring(simulateAll, editMode)`, register `EditorModel` as QML singleton when set
- `src/app/qml/Main.qml` — mount `EditorToolbar` above the page loader, gated on `EditorModel.editing`
- `src/app/qml/components/SideNav.qml` — amber stripe along left edge when `EditorModel.editing`
- `src/app/qml/pages/EasySwitchPage.qml` — drag handlers on slot circles + `DropArea` for back image
- `src/app/qml/pages/ButtonsPage.qml` — drag handlers on markers and cards + `DropArea` for front image, double-click text editing
- `src/app/qml/pages/PointScrollPage.qml` — same pattern as ButtonsPage

**Tests**
- `tests/CMakeLists.txt` — add `test_descriptor_writer.cpp`, `test_editor_model.cpp`
- `tests/qml/CMakeLists.txt` — add new QML test files

---

## Conventions for every task

- Each task is TDD: write the failing test first, run it to confirm red, write minimal impl, run it to confirm green, commit.
- Build: `cmake --build build -j$(nproc)`. Tests: `cd build && ctest --output-on-failure -R <name>`.
- Commit messages follow conventional commits: `feat:`, `test:`, `refactor:`, `docs:`. Never include co-author signatures (per `CLAUDE.md`).
- Every task ends in a commit. The branch is `editor-mode`.
- After any task that touches QML or C++ that runs in the app, kill any running instance and relaunch via `nohup ./build/src/app/logitune --simulate-all --edit > /tmp/logitune-editor.log 2>&1 & disown` to verify (per `CLAUDE.md`). Do **not** run via `run_in_background`.
- The shipped devices may be hidden as `.devices.hidden/` for ongoing simulation work. **Before** running tests or pushing, restore: `mv .devices.hidden devices`. **After** pushing, re-hide: `mv devices .devices.hidden`. This is a known recurring workflow on this branch.

---

## Phase 1 — Foundation (JsonDevice + DeviceRegistry)

### Task 1: JsonDevice tracks source directory and load mtime

**Files:**
- Modify: `src/core/devices/JsonDevice.h`
- Modify: `src/core/devices/JsonDevice.cpp` (around lines 137–254)
- Test: `tests/test_json_device.cpp` (existing)

- [ ] **Step 1: Write the failing test**

Append to `tests/test_json_device.cpp`. Note that the existing tests in this file build a descriptor inline in a `QTemporaryDir` — match that convention.

```cpp
TEST(JsonDevice, TracksSourcePathAndLoadMtime) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QFile f(tmp.path() + QStringLiteral("/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(R"({
  "name": "Tester",
  "status": "community-local",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": []
})");
    f.close();

    auto dev = logitune::JsonDevice::load(tmp.path());
    ASSERT_NE(dev, nullptr);

    // sourcePath is the canonicalized device directory
    EXPECT_EQ(dev->sourcePath(), QFileInfo(tmp.path()).canonicalFilePath());

    // loadedMtime captured from descriptor.json
    const qint64 expected = QFileInfo(tmp.path() + "/descriptor.json")
        .lastModified().toSecsSinceEpoch();
    EXPECT_EQ(dev->loadedMtime(), expected);
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
mv .devices.hidden devices 2>/dev/null || true
cmake --build build -j$(nproc) 2>&1 | tail -20
```
Expected: compile error — `JsonDevice` has no `sourcePath()` / `loadedMtime()` member.

- [ ] **Step 3: Add accessors and member fields to `JsonDevice.h`**

Add to the public section (after `Status status() const`):
```cpp
QString sourcePath() const { return m_sourcePath; }
qint64 loadedMtime() const { return m_loadedMtime; }
```

Add to private members:
```cpp
QString m_sourcePath;
qint64 m_loadedMtime = 0;
```

- [ ] **Step 4: Populate the fields in `JsonDevice::load()` (`JsonDevice.cpp` around line 252)**

Just before `qCDebug(lcDevice) << "JsonDevice: loaded" ...`:
```cpp
dev->m_sourcePath = QFileInfo(dirPath).canonicalFilePath();
dev->m_loadedMtime = QFileInfo(filePath).lastModified().toSecsSinceEpoch();
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R JsonDevice.TracksSourcePathAndLoadMtime
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/devices/JsonDevice.h src/core/devices/JsonDevice.cpp tests/test_json_device.cpp
git commit -m "feat(JsonDevice): track source dir and load mtime

The editor mode needs to know which directory to write a descriptor
back to, and the file watcher needs the load-time mtime to detect
external changes."
```

---

### Task 2: ControlDescriptor and EasySwitchSlotPosition gain optional editor text fields

**Files:**
- Modify: `src/core/interfaces/IDevice.h:11-17` and `:58-61`
- Modify: `src/core/devices/JsonDevice.cpp` (`parseControls` and slot loop)
- Test: `tests/test_json_device.cpp`

- [ ] **Step 1: Add the optional fields to the structs**

In `src/core/interfaces/IDevice.h`, change `ControlDescriptor`:
```cpp
struct ControlDescriptor {
    uint16_t controlId;
    int buttonIndex;
    QString defaultName;
    QString defaultActionType;
    bool configurable;
    QString displayName;   // optional override for defaultName, set by the editor
};
```

Change `EasySwitchSlotPosition`:
```cpp
struct EasySwitchSlotPosition {
    double xPct;
    double yPct;
    QString label;         // optional display label, set by the editor
};
```

- [ ] **Step 2: Write the failing test (inline temp dir, no separate fixture file)**

Append to `tests/test_json_device.cpp`:
```cpp
TEST(JsonDevice, ParsesOptionalEditorFields) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QFile f(tmp.path() + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(R"({
  "name": "Tester",
  "status": "community-local",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [
    {
      "controlId": "0x0050",
      "buttonIndex": 0,
      "defaultName": "Left click",
      "defaultActionType": "default",
      "configurable": false,
      "displayName": "Primary Button"
    }
  ],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": [
    {"xPct": 0.42, "yPct": 0.78, "label": "Mac"},
    {"xPct": 0.50, "yPct": 0.78}
  ]
})");
    f.close();

    auto dev = logitune::JsonDevice::load(tmp.path());
    ASSERT_NE(dev, nullptr);

    const auto controls = dev->controls();
    ASSERT_EQ(controls.size(), 1);
    EXPECT_EQ(controls[0].displayName, "Primary Button");

    const auto slots = dev->easySwitchSlotPositions();
    ASSERT_EQ(slots.size(), 2);
    EXPECT_EQ(slots[0].label, "Mac");
    EXPECT_TRUE(slots[1].label.isEmpty());
}

TEST(JsonDevice, OptionalEditorFieldsDefaultEmptyWhenAbsent) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QFile f(tmp.path() + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(R"({
  "name": "Tester",
  "status": "community-local",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [
    {"controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left click",
     "defaultActionType": "default", "configurable": false}
  ],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": [{"xPct": 0.1, "yPct": 0.2}]
})");
    f.close();

    auto dev = logitune::JsonDevice::load(tmp.path());
    ASSERT_NE(dev, nullptr);
    for (const auto &c : dev->controls())
        EXPECT_TRUE(c.displayName.isEmpty());
    for (const auto &s : dev->easySwitchSlotPositions())
        EXPECT_TRUE(s.label.isEmpty());
}
```

- [ ] **Step 3: Run test to verify it fails**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R "JsonDevice.(ParsesOptionalEditorFields|OptionalEditorFieldsDefaultEmptyWhenAbsent)"
```
Expected: FAIL (`displayName`/`label` not parsed).

- [ ] **Step 4: Wire the parsing in `JsonDevice.cpp`**

In `parseControls()` (around line 100), after reading existing fields, add:
```cpp
control.displayName = obj.value(QStringLiteral("displayName")).toString();
```

In the slot loop (around line 213), replace:
```cpp
dev->m_easySwitchSlots.append({
    slotObj.value(QStringLiteral("xPct")).toDouble(),
    slotObj.value(QStringLiteral("yPct")).toDouble()
});
```
with:
```cpp
dev->m_easySwitchSlots.append({
    slotObj.value(QStringLiteral("xPct")).toDouble(),
    slotObj.value(QStringLiteral("yPct")).toDouble(),
    slotObj.value(QStringLiteral("label")).toString()
});
```

- [ ] **Step 5: Run tests to verify pass**

```bash
cd build && ctest --output-on-failure -R JsonDevice
```
Expected: all `JsonDevice.*` PASS, no regression in existing tests.

- [ ] **Step 6: Commit**

```bash
git add src/core/interfaces/IDevice.h src/core/devices/JsonDevice.cpp tests/test_json_device.cpp
git commit -m "feat(schema): optional displayName on controls and label on easy-switch slots

Both fields are optional and backwards-compatible. Existing descriptors
without them keep parsing and the new fields default to empty strings.
The editor mode will use these to let the maintainer override
extractor-derived names and label easy-switch slots in the channel list."
```

---

### Task 3: JsonDevice gains a public `refresh()` that re-reads its descriptor in place

**Files:**
- Modify: `src/core/devices/JsonDevice.h`
- Modify: `src/core/devices/JsonDevice.cpp`
- Test: `tests/test_json_device.cpp`

**Why in-place refresh:** `DeviceRegistry` owns `JsonDevice` via `unique_ptr`, and clients (DeviceModel, the carousel) hold raw `IDevice*`. Replacing the unique_ptr would dangle those pointers. Mutating in place keeps every existing pointer valid.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_json_device.cpp`:
```cpp
TEST(JsonDevice, RefreshRereadsDescriptorInPlace) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());

    auto write = [&](const QString &name) {
        QFile f(tmp.path() + "/descriptor.json");
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QStringLiteral(R"({
  "name": "%1",
  "status": "community-local",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": []
})").arg(name).toUtf8());
    };

    write("Original Name");
    auto dev = logitune::JsonDevice::load(tmp.path());
    ASSERT_NE(dev, nullptr);
    const auto *raw = dev.get();
    EXPECT_EQ(dev->deviceName(), "Original Name");

    write("Mutated Name");
    ASSERT_TRUE(dev->refresh());
    EXPECT_EQ(dev->deviceName(), "Mutated Name");
    // Pointer is unchanged
    EXPECT_EQ(dev.get(), raw);
}
```

- [ ] **Step 2: Run to confirm fail**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R JsonDevice.RefreshRereadsDescriptorInPlace
```
Expected: compile error — no `refresh()` method.

- [ ] **Step 3: Refactor `load()` so its body is reusable as `refresh()`**

In `JsonDevice.h` add to public section:
```cpp
bool refresh();
```

In `JsonDevice.cpp`, extract the parsing body of `load()` into a private helper `bool parseFromDir(const QString& dirPath)` that mutates `*this` and returns `false` on failure. Then:
- `load()` becomes: construct empty `JsonDevice`, call `parseFromDir(dirPath)`, return on success.
- `refresh()` becomes: clear the mutable parsed fields (m_pids, m_controls, m_buttonHotspots, m_scrollHotspots, m_features, m_easySwitchSlots, m_defaultGestures, image paths), then call `parseFromDir(m_sourcePath)`.

Suggested structure:
```cpp
bool JsonDevice::parseFromDir(const QString& dirPath) {
    // ... existing parsing body from load() ...
    // (assignments go to `this` instead of `dev->`)
    m_sourcePath = QFileInfo(dirPath).canonicalFilePath();
    m_loadedMtime = QFileInfo(filePath).lastModified().toSecsSinceEpoch();
    return true;
}

std::unique_ptr<JsonDevice> JsonDevice::load(const QString& dirPath) {
    auto dev = std::unique_ptr<JsonDevice>(new JsonDevice());
    if (!dev->parseFromDir(dirPath))
        return nullptr;
    return dev;
}

bool JsonDevice::refresh() {
    if (m_sourcePath.isEmpty())
        return false;
    // Clear mutable parsed state — keep m_sourcePath
    const QString src = m_sourcePath;
    m_pids.clear();
    m_controls.clear();
    m_buttonHotspots.clear();
    m_scrollHotspots.clear();
    m_easySwitchSlots.clear();
    m_defaultGestures.clear();
    m_frontImage.clear();
    m_sideImage.clear();
    m_backImage.clear();
    m_features = FeatureSupport{};
    return parseFromDir(src);
}
```

- [ ] **Step 4: Run all `JsonDevice` cases**

```bash
cd build && ctest --output-on-failure -R JsonDevice
```
Expected: all PASS, including pre-existing tests (no regressions from the load() refactor).

- [ ] **Step 5: Commit**

```bash
git add src/core/devices/JsonDevice.h src/core/devices/JsonDevice.cpp tests/test_json_device.cpp
git commit -m "feat(JsonDevice): in-place refresh() preserves IDevice* pointers

DeviceRegistry hands out raw IDevice* pointers to clients (DeviceModel,
the carousel). Replacing the underlying unique_ptr would dangle those.
refresh() re-reads descriptor.json into the existing JsonDevice instance
so external watchers and the editor's save flow can update state without
invalidating client pointers."
```

---

### Task 4: DeviceRegistry gains `findBySourcePath()` and `reload(dirPath)`

**Files:**
- Modify: `src/core/DeviceRegistry.h`
- Modify: `src/core/DeviceRegistry.cpp`
- Test: `tests/test_device_registry.cpp` (existing)

- [ ] **Step 1: Write the failing test**

Append to `tests/test_device_registry.cpp` (matches the existing `TEST(DeviceRegistry, ...)` free-test naming used at `:144`):
```cpp
TEST(DeviceRegistry, ReloadByPathRefreshesSingleDevice) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    QDir().mkpath(tmp.path() + "/logitune/devices/test");
    const QString descPath = tmp.path() + "/logitune/devices/test/descriptor.json";

    auto write = [&](const QString &name) {
        QFile f(descPath);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QString(R"({"name":"%1","status":"community-local","productIds":["0xffff"],"features":{},"controls":[],"hotspots":{"buttons":[],"scroll":[]},"images":{},"easySwitchSlots":[]})").arg(name).toUtf8());
    };
    write("Original");

    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName("Original");
    ASSERT_NE(dev, nullptr);
    const auto *jdev = dynamic_cast<const logitune::JsonDevice*>(dev);
    ASSERT_NE(jdev, nullptr);
    const QString srcPath = jdev->sourcePath();

    write("Mutated");
    ASSERT_TRUE(reg.reload(srcPath));

    // Same pointer, mutated state
    EXPECT_EQ(jdev->deviceName(), "Mutated");
    EXPECT_EQ(reg.findBySourcePath(srcPath), dev);
}

TEST(DeviceRegistry, ReloadUnknownPathReturnsFalse) {
    logitune::DeviceRegistry reg;
    EXPECT_FALSE(reg.reload("/nonexistent/path/that/does/not/exist"));
}
```

- [ ] **Step 2: Run to confirm fail**

```bash
cd build && ctest --output-on-failure -R "DeviceRegistry.Reload"
```
Expected: compile error — no `findBySourcePath` or `reload(QString)`.

- [ ] **Step 3: Add the methods to `DeviceRegistry.h`**

```cpp
const IDevice* findBySourcePath(const QString &dirPath) const;
bool reload(const QString &dirPath);
```

- [ ] **Step 4: Implement in `DeviceRegistry.cpp`**

```cpp
const IDevice* DeviceRegistry::findBySourcePath(const QString &dirPath) const {
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    for (const auto &dev : m_devices) {
        if (auto *jd = dynamic_cast<const JsonDevice*>(dev.get()))
            if (jd->sourcePath() == canonical)
                return dev.get();
    }
    return nullptr;
}

bool DeviceRegistry::reload(const QString &dirPath) {
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    for (auto &dev : m_devices) {
        if (auto *jd = dynamic_cast<JsonDevice*>(dev.get())) {
            if (jd->sourcePath() == canonical)
                return jd->refresh();
        }
    }
    return false;
}
```

- [ ] **Step 5: Run tests**

```bash
cd build && ctest --output-on-failure -R "DeviceRegistry.Reload"
```
Expected: both PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/DeviceRegistry.h src/core/DeviceRegistry.cpp tests/test_device_registry.cpp
git commit -m "feat(DeviceRegistry): per-path reload via JsonDevice::refresh

The editor mode and the file watcher both need to refresh a single
descriptor without rebuilding the whole registry. reload(dirPath)
locates the matching JsonDevice and calls its in-place refresh,
keeping client pointers valid."
```

---

## Phase 2 — DescriptorWriter

### Task 5: DescriptorWriter skeleton with atomic write

**Files:**
- Create: `src/core/devices/DescriptorWriter.h`
- Create: `src/core/devices/DescriptorWriter.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/test_descriptor_writer.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add the new header**

Create `src/core/devices/DescriptorWriter.h`:
```cpp
#pragma once
#include <QString>
#include <QJsonObject>

namespace logitune {

class DescriptorWriter {
public:
    enum Result { Ok, IoError, JsonError };

    // Writes `obj` to <dirPath>/descriptor.json atomically (QSaveFile rename).
    // If errorOut is non-null and result != Ok, populates with a human-readable error.
    Result write(const QString &dirPath, const QJsonObject &obj, QString *errorOut = nullptr);
};

} // namespace logitune
```

- [ ] **Step 2: Stub the implementation that returns IoError**

Create `src/core/devices/DescriptorWriter.cpp`:
```cpp
#include "DescriptorWriter.h"
#include <QSaveFile>
#include <QJsonDocument>
#include <QDir>

namespace logitune {

DescriptorWriter::Result DescriptorWriter::write(
    const QString &dirPath, const QJsonObject &obj, QString *errorOut) {
    if (errorOut) errorOut->clear();
    return IoError;  // not yet implemented
}

} // namespace logitune
```

- [ ] **Step 3: Wire into CMakeLists**

Add `devices/DescriptorWriter.cpp` to the `target_sources(logitune-core PRIVATE ...)` list in `src/core/CMakeLists.txt`.

- [ ] **Step 4: Write the failing tests**

Create `tests/test_descriptor_writer.cpp`:
```cpp
#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "devices/DescriptorWriter.h"

TEST(DescriptorWriterTest, WritesValidJsonAtomically) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QJsonObject obj;
    obj["name"] = "Test";
    obj["status"] = "community-local";

    logitune::DescriptorWriter w;
    QString err;
    EXPECT_EQ(w.write(tmp.path(), obj, &err), logitune::DescriptorWriter::Ok);
    EXPECT_TRUE(err.isEmpty());

    // Read it back
    QFile f(tmp.path() + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto doc = QJsonDocument::fromJson(f.readAll());
    ASSERT_FALSE(doc.isNull());
    EXPECT_EQ(doc.object()["name"].toString(), "Test");
}

TEST(DescriptorWriterTest, ReturnsIoErrorOnUnwritableDir) {
    logitune::DescriptorWriter w;
    QString err;
    EXPECT_EQ(w.write("/proc/nonexistent-do-not-create",
                      QJsonObject{{"name", "x"}}, &err),
              logitune::DescriptorWriter::IoError);
    EXPECT_FALSE(err.isEmpty());
}
```

- [ ] **Step 5: Add to `tests/CMakeLists.txt`**

In the gtest-driven test sources list, add:
```cmake
test_descriptor_writer.cpp
```

- [ ] **Step 6: Run to confirm test fails**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R DescriptorWriterTest
```
Expected: `WritesValidJsonAtomically` FAIL (returned IoError instead of Ok).

- [ ] **Step 7: Implement the real write**

Replace the stub body:
```cpp
DescriptorWriter::Result DescriptorWriter::write(
    const QString &dirPath, const QJsonObject &obj, QString *errorOut) {
    if (errorOut) errorOut->clear();

    if (!QDir(dirPath).exists()) {
        if (errorOut) *errorOut = QStringLiteral("Directory does not exist: ") + dirPath;
        return IoError;
    }

    const QString filePath = QDir(dirPath).absoluteFilePath(QStringLiteral("descriptor.json"));
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = file.errorString();
        return IoError;
    }

    const QJsonDocument doc(obj);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        if (errorOut) *errorOut = QStringLiteral("short write");
        return IoError;
    }

    if (!file.commit()) {
        if (errorOut) *errorOut = file.errorString();
        return IoError;
    }
    return Ok;
}
```

- [ ] **Step 8: Run tests**

```bash
cd build && ctest --output-on-failure -R DescriptorWriterTest
```
Expected: both tests PASS.

- [ ] **Step 9: Commit**

```bash
git add src/core/devices/DescriptorWriter.h src/core/devices/DescriptorWriter.cpp src/core/CMakeLists.txt tests/test_descriptor_writer.cpp tests/CMakeLists.txt
git commit -m "feat(DescriptorWriter): atomic JSON descriptor writer

Writes an arbitrary QJsonObject to <dir>/descriptor.json via QSaveFile
so a crash mid-write cannot leave a half-written file. Returns Ok or
IoError with a populated error string."
```

---

### Task 6: DescriptorWriter preserves unknown fields on round-trip

**Files:**
- Test: `tests/test_descriptor_writer.cpp`

The writer's contract is: it takes a `QJsonObject` and writes it. It does not strip fields. This test exists to lock that contract — a future contributor adding a "let me clean this up" call to the writer will break it.

- [ ] **Step 1: Write the test**

Append to `tests/test_descriptor_writer.cpp`:
```cpp
TEST(DescriptorWriterTest, PreservesUnknownFieldsOnRoundTrip) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QJsonObject obj;
    obj["name"] = "Test";
    obj["__future_field"] = "will it survive?";
    QJsonObject nested;
    nested["__nested_future"] = 42;
    obj["nested"] = nested;

    logitune::DescriptorWriter w;
    ASSERT_EQ(w.write(tmp.path(), obj, nullptr), logitune::DescriptorWriter::Ok);

    QFile f(tmp.path() + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto roundTripped = QJsonDocument::fromJson(f.readAll()).object();

    EXPECT_EQ(roundTripped["__future_field"].toString(), "will it survive?");
    EXPECT_EQ(roundTripped["nested"].toObject()["__nested_future"].toInt(), 42);
}
```

- [ ] **Step 2: Run — should already pass**

```bash
cd build && ctest --output-on-failure -R DescriptorWriterTest.PreservesUnknownFieldsOnRoundTrip
```
Expected: PASS (the writer already passes the object verbatim).

- [ ] **Step 3: Commit**

```bash
git add tests/test_descriptor_writer.cpp
git commit -m "test(DescriptorWriter): lock preserve-unknown-fields contract

Future contributors may be tempted to 'clean up' the writer by stripping
fields the C++ code doesn't recognize. This test ensures any such change
breaks the build instead of silently dropping community-added fields."
```

---

## Phase 3 — EditorModel core

### Task 7: EditCommand header and EditorModel skeleton

**Files:**
- Create: `src/app/models/EditCommand.h`
- Create: `src/app/models/EditorModel.h`
- Create: `src/app/models/EditorModel.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/test_editor_model.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `EditCommand.h`**

```cpp
#pragma once
#include <QString>
#include <QJsonValue>

namespace logitune {

struct EditCommand {
    enum Kind { SlotMove, HotspotMove, ImageReplace, TextEdit };
    Kind kind;
    int index = -1;            // slot idx / hotspot idx; -1 for top-level
    QString role;              // "back"/"front"/"side" for ImageReplace; field name for TextEdit
    QJsonValue before;
    QJsonValue after;
};

} // namespace logitune
```

- [ ] **Step 2: Create the minimal `EditorModel.h`**

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QStack>
#include <QJsonObject>
#include "EditCommand.h"

namespace logitune {

class DeviceRegistry;

class EditorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing CONSTANT)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString activeDevicePath READ activeDevicePath WRITE setActiveDevicePath NOTIFY activeDevicePathChanged)
public:
    explicit EditorModel(DeviceRegistry *registry, bool editing, QObject *parent = nullptr);

    bool editing() const { return m_editing; }
    bool hasUnsavedChanges() const { return m_dirty.contains(m_activeDevicePath); }
    bool canUndo() const;
    bool canRedo() const;
    QString activeDevicePath() const { return m_activeDevicePath; }

public slots:
    void setActiveDevicePath(const QString &path);

signals:
    void dirtyChanged();
    void undoStateChanged();
    void activeDevicePathChanged();

private:
    DeviceRegistry *m_registry;
    bool m_editing;
    QString m_activeDevicePath;
    QHash<QString, QJsonObject> m_pendingEdits;
    QHash<QString, QStack<EditCommand>> m_undoStacks;
    QHash<QString, QStack<EditCommand>> m_redoStacks;
    QSet<QString> m_dirty;
};

} // namespace logitune
```

- [ ] **Step 3: Create `EditorModel.cpp`**

```cpp
#include "EditorModel.h"
#include "DeviceRegistry.h"

namespace logitune {

EditorModel::EditorModel(DeviceRegistry *registry, bool editing, QObject *parent)
    : QObject(parent), m_registry(registry), m_editing(editing) {}

bool EditorModel::canUndo() const {
    auto it = m_undoStacks.find(m_activeDevicePath);
    return it != m_undoStacks.end() && !it->isEmpty();
}

bool EditorModel::canRedo() const {
    auto it = m_redoStacks.find(m_activeDevicePath);
    return it != m_redoStacks.end() && !it->isEmpty();
}

void EditorModel::setActiveDevicePath(const QString &path) {
    if (path == m_activeDevicePath) return;
    m_activeDevicePath = path;
    emit activeDevicePathChanged();
    emit dirtyChanged();
    emit undoStateChanged();
}

} // namespace logitune
```

- [ ] **Step 4: Wire CMakeLists**

In `src/app/CMakeLists.txt`, add `models/EditorModel.cpp` to the `add_library(logitune-app-lib STATIC ...)` list.

In `tests/CMakeLists.txt`, add `test_editor_model.cpp`.

- [ ] **Step 5: Write the failing test**

Create `tests/test_editor_model.cpp`:
```cpp
#include <gtest/gtest.h>
#include <QSignalSpy>
#include "EditorModel.h"
#include "DeviceRegistry.h"

TEST(EditorModelTest, EditingFlagAndInitialState) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, /*editing=*/true);
    EXPECT_TRUE(m.editing());
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_TRUE(m.activeDevicePath().isEmpty());
}

TEST(EditorModelTest, ActiveDevicePathSetterEmitsSignals) {
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    QSignalSpy pathSpy(&m, &logitune::EditorModel::activeDevicePathChanged);
    QSignalSpy dirtySpy(&m, &logitune::EditorModel::dirtyChanged);
    m.setActiveDevicePath("/tmp/foo");
    EXPECT_EQ(pathSpy.count(), 1);
    EXPECT_EQ(dirtySpy.count(), 1);
    EXPECT_EQ(m.activeDevicePath(), "/tmp/foo");
}
```

- [ ] **Step 6: Build and run tests**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R EditorModelTest
```
Expected: both PASS.

- [ ] **Step 7: Commit**

```bash
git add src/app/models/EditCommand.h src/app/models/EditorModel.h src/app/models/EditorModel.cpp src/app/CMakeLists.txt tests/test_editor_model.cpp tests/CMakeLists.txt
git commit -m "feat(EditorModel): skeleton with editing flag, active path, dirty tracking

Empty shell with the QML-facing properties wired up. No mutation logic
yet — that comes in the next tasks. EditCommand header lives next to
the model so future undo-stack tasks reference it directly."
```

---

### Task 8: `updateSlotPosition` mutates pending state and pushes undo command

**Files:**
- Modify: `src/app/models/EditorModel.h`
- Modify: `src/app/models/EditorModel.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add the slot to the header**

In the `public slots:` block of `EditorModel.h`:
```cpp
Q_INVOKABLE void updateSlotPosition(int idx, double xPct, double yPct);
```

Also add a private helper:
```cpp
private:
    void ensurePending(const QString &path);
    void pushCommand(EditCommand cmd);
```

- [ ] **Step 2: Write the failing test**

Append to `tests/test_editor_model.cpp`:
```cpp
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

namespace {
QString writeMinimalDescriptor(const QString &dir) {
    QDir().mkpath(dir);
    QFile f(dir + "/descriptor.json");
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(R"({
  "name": "Tester",
  "status": "community-local",
  "productIds": ["0xffff"],
  "features": {},
  "controls": [],
  "hotspots": {"buttons": [], "scroll": []},
  "images": {},
  "easySwitchSlots": [
    {"xPct": 0.10, "yPct": 0.20},
    {"xPct": 0.30, "yPct": 0.40}
  ]
})");
    return QFileInfo(dir).canonicalFilePath();
}
}

TEST(EditorModelTest, UpdateSlotPositionMutatesPendingAndPushesUndo) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    logitune::DeviceRegistry reg;  // populated indirectly by setting XDG before construction
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    QSignalSpy dirtySpy(&m, &logitune::EditorModel::dirtyChanged);
    QSignalSpy undoSpy(&m, &logitune::EditorModel::undoStateChanged);

    m.updateSlotPosition(1, 0.50, 0.60);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_GE(dirtySpy.count(), 1);
    EXPECT_GE(undoSpy.count(), 1);
}
```

- [ ] **Step 3: Run to confirm fail**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
```
Expected: compile error — `updateSlotPosition` not declared.

- [ ] **Step 4: Implement in `EditorModel.cpp`**

```cpp
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

void EditorModel::ensurePending(const QString &path) {
    if (m_pendingEdits.contains(path)) return;
    QFile f(path + "/descriptor.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    m_pendingEdits[path] = QJsonDocument::fromJson(f.readAll()).object();
}

void EditorModel::pushCommand(EditCommand cmd) {
    m_undoStacks[m_activeDevicePath].push(std::move(cmd));
    m_redoStacks[m_activeDevicePath].clear();
    m_dirty.insert(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
}

void EditorModel::updateSlotPosition(int idx, double xPct, double yPct) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonArray slots = obj.value("easySwitchSlots").toArray();
    if (idx < 0 || idx >= slots.size()) return;

    EditCommand cmd;
    cmd.kind = EditCommand::SlotMove;
    cmd.index = idx;
    cmd.before = slots[idx];

    QJsonObject slot = slots[idx].toObject();
    slot["xPct"] = xPct;
    slot["yPct"] = yPct;
    slots[idx] = slot;
    obj["easySwitchSlots"] = slots;
    cmd.after = slots[idx];

    pushCommand(std::move(cmd));
}
```

- [ ] **Step 5: Run tests to verify pass**

```bash
cd build && ctest --output-on-failure -R EditorModelTest
```
Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): updateSlotPosition mutates pending and pushes undo

The first real mutation slot. ensurePending() lazily reads the
descriptor.json into m_pendingEdits when the user first edits a device,
so untouched devices stay zero-cost. pushCommand() clears the redo
stack on every new edit (standard editor behavior)."
```

---

### Task 9: `updateHotspot` mutates pending state and pushes undo command

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add slot to header**

```cpp
Q_INVOKABLE void updateHotspot(int hotspotIndex, double xPct, double yPct,
                               const QString &side, double labelOffsetYPct);
```

This mutates `hotspots.buttons[hotspotIndex]`. Hotspots in `hotspots.scroll` use the same scheme but get a separate slot in Task 10's PointScrollPage drag wiring — keep this slot for buttons hotspots only for now. (Note: in the QML view layer we will use a single slot with an extra `kind` argument; this gets refactored when we add scroll hotspot drag.)

For now, make the slot operate on `hotspots.buttons`.

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, UpdateHotspotMutatesPendingAndPushesUndo) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QDir().mkpath(tmp.path() + "/dev");
    QFile f(tmp.path() + "/dev/descriptor.json");
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(R"({
  "name": "Tester", "status": "community-local", "productIds": ["0xffff"],
  "features": {}, "controls": [],
  "hotspots": {
    "buttons": [
      {"buttonIndex": 0, "xPct": 0.10, "yPct": 0.20, "side": "left", "labelOffsetYPct": 0.0}
    ],
    "scroll": []
  },
  "images": {}, "easySwitchSlots": []
})");
    const QString path = QFileInfo(tmp.path() + "/dev").canonicalFilePath();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    m.updateHotspot(0, 0.55, 0.66, "right", 0.10);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
}
```

- [ ] **Step 3: Run to confirm fail, then implement, then commit (red/green/commit)**

Implementation in `EditorModel.cpp`:
```cpp
void EditorModel::updateHotspot(int idx, double xPct, double yPct,
                                 const QString &side, double labelOffsetYPct) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonObject hotspots = obj.value("hotspots").toObject();
    QJsonArray buttons = hotspots.value("buttons").toArray();
    if (idx < 0 || idx >= buttons.size()) return;

    EditCommand cmd;
    cmd.kind = EditCommand::HotspotMove;
    cmd.index = idx;
    cmd.before = buttons[idx];

    QJsonObject hs = buttons[idx].toObject();
    hs["xPct"] = xPct;
    hs["yPct"] = yPct;
    hs["side"] = side;
    hs["labelOffsetYPct"] = labelOffsetYPct;
    buttons[idx] = hs;
    hotspots["buttons"] = buttons;
    obj["hotspots"] = hotspots;
    cmd.after = buttons[idx];

    pushCommand(std::move(cmd));
}
```

```bash
cd build && ctest --output-on-failure -R EditorModelTest.UpdateHotspotMutatesPendingAndPushesUndo
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): updateHotspot mutates pending and pushes undo"
```

---

### Task 10: `undo` and `redo` swap before/after on the JSON

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add slots to header**

```cpp
Q_INVOKABLE void undo();
Q_INVOKABLE void redo();

private:
    void applyCommand(const EditCommand &cmd, bool reverse);
```

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, UndoRestoresSlotPosition) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    m.updateSlotPosition(0, 0.99, 0.99);
    EXPECT_TRUE(m.canUndo());
    m.undo();

    EXPECT_FALSE(m.canUndo());
    EXPECT_TRUE(m.canRedo());
    // After undo, dirty should be false because we're back to the original
    EXPECT_FALSE(m.hasUnsavedChanges());

    m.redo();
    EXPECT_TRUE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_TRUE(m.hasUnsavedChanges());
}
```

- [ ] **Step 3: Implementation**

```cpp
void EditorModel::applyCommand(const EditCommand &cmd, bool reverse) {
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    const QJsonValue &target = reverse ? cmd.before : cmd.after;
    if (cmd.kind == EditCommand::SlotMove) {
        QJsonArray slots = obj.value("easySwitchSlots").toArray();
        slots[cmd.index] = target;
        obj["easySwitchSlots"] = slots;
    } else if (cmd.kind == EditCommand::HotspotMove) {
        QJsonObject hotspots = obj.value("hotspots").toObject();
        QJsonArray buttons = hotspots.value("buttons").toArray();
        buttons[cmd.index] = target;
        hotspots["buttons"] = buttons;
        obj["hotspots"] = hotspots;
    } else if (cmd.kind == EditCommand::TextEdit) {
        // Field-level: cmd.role names the field path
        // (handled in Task 11; left as a no-op stub here)
    } else if (cmd.kind == EditCommand::ImageReplace) {
        QJsonObject images = obj.value("images").toObject();
        images[cmd.role] = target.toString();
        obj["images"] = images;
    }
}

void EditorModel::undo() {
    auto &stack = m_undoStacks[m_activeDevicePath];
    if (stack.isEmpty()) return;
    EditCommand cmd = stack.pop();
    applyCommand(cmd, /*reverse=*/true);
    m_redoStacks[m_activeDevicePath].push(cmd);

    // Recompute dirty: if the undo stack is empty, we're back to clean state
    if (stack.isEmpty()) m_dirty.remove(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
}

void EditorModel::redo() {
    auto &stack = m_redoStacks[m_activeDevicePath];
    if (stack.isEmpty()) return;
    EditCommand cmd = stack.pop();
    applyCommand(cmd, /*reverse=*/false);
    m_undoStacks[m_activeDevicePath].push(cmd);
    m_dirty.insert(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
}
```

- [ ] **Step 4: Run, confirm pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): undo and redo via before/after swap"
```

---

### Task 11: `updateText` for slot labels, control display names, and device name

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

The `updateText` slot covers three cases distinguished by the `field` argument: `"slotLabel"` (mutates `easySwitchSlots[idx].label`), `"controlDisplayName"` (mutates `controls[idx].displayName`), `"deviceName"` (mutates top-level `name`, idx ignored).

- [ ] **Step 1: Add to header**

```cpp
Q_INVOKABLE void updateText(const QString &field, int index, const QString &value);
```

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, UpdateTextEditsAllThreeKindsAndUndoes) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    QDir().mkpath(tmp.path() + "/dev");
    QFile f(tmp.path() + "/dev/descriptor.json");
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(R"({
  "name": "Original Name", "status": "community-local", "productIds": ["0xffff"],
  "features": {},
  "controls": [
    {"controlId": "0x0050", "buttonIndex": 0, "defaultName": "Left", "defaultActionType": "default", "configurable": false}
  ],
  "hotspots": {"buttons": [], "scroll": []}, "images": {},
  "easySwitchSlots": [{"xPct": 0.1, "yPct": 0.2}]
})");
    const QString path = QFileInfo(tmp.path() + "/dev").canonicalFilePath();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    m.updateText("deviceName", -1, "New Name");
    m.updateText("controlDisplayName", 0, "Primary");
    m.updateText("slotLabel", 0, "Mac");

    EXPECT_TRUE(m.canUndo());
    // Undo three times brings us back to clean
    m.undo(); m.undo(); m.undo();
    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
}
```

- [ ] **Step 3: Implementation**

In `updateText`, build the `EditCommand` with `kind=TextEdit`, `role=field`, `index=index`, `before=current value`, `after=value`. Then mutate the JSON.

```cpp
void EditorModel::updateText(const QString &field, int index, const QString &value) {
    if (m_activeDevicePath.isEmpty()) return;
    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];

    EditCommand cmd;
    cmd.kind = EditCommand::TextEdit;
    cmd.role = field;
    cmd.index = index;

    if (field == "deviceName") {
        cmd.before = obj.value("name");
        cmd.after = value;
        obj["name"] = value;
    } else if (field == "controlDisplayName") {
        QJsonArray controls = obj.value("controls").toArray();
        if (index < 0 || index >= controls.size()) return;
        QJsonObject c = controls[index].toObject();
        cmd.before = c.value("displayName");
        c["displayName"] = value;
        controls[index] = c;
        obj["controls"] = controls;
        cmd.after = value;
    } else if (field == "slotLabel") {
        QJsonArray slots = obj.value("easySwitchSlots").toArray();
        if (index < 0 || index >= slots.size()) return;
        QJsonObject s = slots[index].toObject();
        cmd.before = s.value("label");
        s["label"] = value;
        slots[index] = s;
        obj["easySwitchSlots"] = slots;
        cmd.after = value;
    } else {
        return;
    }
    pushCommand(std::move(cmd));
}
```

Update `applyCommand` to handle `TextEdit`:
```cpp
} else if (cmd.kind == EditCommand::TextEdit) {
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    const QJsonValue &target = reverse ? cmd.before : cmd.after;
    if (cmd.role == "deviceName") {
        obj["name"] = target;
    } else if (cmd.role == "controlDisplayName") {
        QJsonArray controls = obj.value("controls").toArray();
        QJsonObject c = controls[cmd.index].toObject();
        c["displayName"] = target;
        controls[cmd.index] = c;
        obj["controls"] = controls;
    } else if (cmd.role == "slotLabel") {
        QJsonArray slots = obj.value("easySwitchSlots").toArray();
        QJsonObject s = slots[cmd.index].toObject();
        s["label"] = target;
        slots[cmd.index] = s;
        obj["easySwitchSlots"] = slots;
    }
}
```

- [ ] **Step 4: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): updateText for slot labels, control names, device name"
```

---

### Task 12: Per-device stack isolation across `setActiveDevicePath` switches

**Files:**
- Test: `tests/test_editor_model.cpp`

This test verifies the spec's "switching active device leaves the prior device's pending edits and undo stack intact" rule. The implementation is already correct because the stacks are keyed by path — this test locks the behavior.

- [ ] **Step 1: Write the test**

```cpp
TEST(EditorModelTest, PerDeviceStacksAreIsolated) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString pathA = writeMinimalDescriptor(tmp.path() + "/devA");
    const QString pathB = writeMinimalDescriptor(tmp.path() + "/devB");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);

    m.setActiveDevicePath(pathA);
    m.updateSlotPosition(0, 0.5, 0.5);
    EXPECT_TRUE(m.canUndo());
    EXPECT_TRUE(m.hasUnsavedChanges());

    m.setActiveDevicePath(pathB);
    EXPECT_FALSE(m.canUndo());      // B has no edits
    EXPECT_FALSE(m.hasUnsavedChanges());

    m.updateSlotPosition(0, 0.7, 0.7);
    EXPECT_TRUE(m.canUndo());

    m.setActiveDevicePath(pathA);
    EXPECT_TRUE(m.canUndo());       // A's stack survives
    EXPECT_TRUE(m.hasUnsavedChanges());
}
```

- [ ] **Step 2: Run, expect pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest.PerDeviceStacksAreIsolated
git add tests/test_editor_model.cpp
git commit -m "test(EditorModel): lock per-device stack isolation behavior"
```

---

### Task 13: `save()` writes via DescriptorWriter, clears stacks, signals saved

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add to header**

```cpp
public slots:
    Q_INVOKABLE void save();

signals:
    void saved(const QString &path);
    void saveFailed(const QString &path, const QString &error);

private:
    DescriptorWriter m_writer;
    QSet<QString> m_selfWrittenPaths;
```

Include `devices/DescriptorWriter.h` in the header (or forward-declare and include in cpp).

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, SaveWritesPendingAndClearsState) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    QSignalSpy savedSpy(&m, &logitune::EditorModel::saved);
    m.updateSlotPosition(0, 0.55, 0.66);
    m.save();

    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());
    EXPECT_EQ(savedSpy.count(), 1);

    // The file on disk reflects the edit
    QFile f(path + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    EXPECT_DOUBLE_EQ(obj["easySwitchSlots"].toArray()[0].toObject()["xPct"].toDouble(), 0.55);
}
```

- [ ] **Step 3: Implementation**

```cpp
void EditorModel::save() {
    if (m_activeDevicePath.isEmpty()) return;
    if (!m_pendingEdits.contains(m_activeDevicePath)) return;

    m_selfWrittenPaths.insert(m_activeDevicePath);

    QString err;
    auto result = m_writer.write(m_activeDevicePath,
                                 m_pendingEdits.value(m_activeDevicePath), &err);
    if (result != DescriptorWriter::Ok) {
        m_selfWrittenPaths.remove(m_activeDevicePath);  // cancel suppression
        emit saveFailed(m_activeDevicePath, err);
        return;
    }

    if (m_registry)
        m_registry->reload(m_activeDevicePath);

    m_pendingEdits.remove(m_activeDevicePath);
    m_undoStacks.remove(m_activeDevicePath);
    m_redoStacks.remove(m_activeDevicePath);
    m_dirty.remove(m_activeDevicePath);

    emit dirtyChanged();
    emit undoStateChanged();
    emit saved(m_activeDevicePath);
}
```

- [ ] **Step 4: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): save() writes via DescriptorWriter and clears state"
```

---

### Task 14: `save()` failure preserves pending state and signals saveFailed

**Files:**
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Write the test**

```cpp
TEST(EditorModelTest, SaveFailurePreservesState) {
    // Use a path that exists at construction time but is then made unwritable
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.55, 0.66);

    // Make the directory unwritable
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ExeOwner);

    QSignalSpy failSpy(&m, &logitune::EditorModel::saveFailed);
    m.save();

    EXPECT_EQ(failSpy.count(), 1);
    EXPECT_TRUE(m.hasUnsavedChanges());     // still dirty
    EXPECT_TRUE(m.canUndo());                // stack intact

    // Restore for cleanup
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}
```

- [ ] **Step 2: Run — should already pass given the implementation**

```bash
cd build && ctest --output-on-failure -R EditorModelTest.SaveFailurePreservesState
```
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/test_editor_model.cpp
git commit -m "test(EditorModel): lock save-failure state preservation"
```

---

### Task 15: `reset()` discards pending edits and re-reads from disk

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add the slot**

```cpp
Q_INVOKABLE void reset();
```

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, ResetDiscardsPendingAndClearsStacks) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");
    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    m.updateSlotPosition(0, 0.99, 0.99);
    EXPECT_TRUE(m.hasUnsavedChanges());

    m.reset();

    EXPECT_FALSE(m.hasUnsavedChanges());
    EXPECT_FALSE(m.canUndo());
    EXPECT_FALSE(m.canRedo());

    // The file on disk is unchanged
    QFile f(path + "/descriptor.json");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    EXPECT_DOUBLE_EQ(obj["easySwitchSlots"].toArray()[0].toObject()["xPct"].toDouble(), 0.10);
}
```

- [ ] **Step 3: Implement**

```cpp
void EditorModel::reset() {
    if (m_activeDevicePath.isEmpty()) return;
    m_pendingEdits.remove(m_activeDevicePath);
    m_undoStacks.remove(m_activeDevicePath);
    m_redoStacks.remove(m_activeDevicePath);
    m_dirty.remove(m_activeDevicePath);
    emit dirtyChanged();
    emit undoStateChanged();
}
```

- [ ] **Step 4: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest.ResetDiscardsPendingAndClearsStacks
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): reset() discards pending edits"
```

---

### Task 16: `replaceImage` copies a PNG into the descriptor directory and updates JSON

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

- [ ] **Step 1: Add to header**

```cpp
Q_INVOKABLE void replaceImage(const QString &role, const QString &sourcePath);
```

`role` is `"front"`, `"side"`, or `"back"`.

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, ReplaceImageCopiesFileAndUpdatesPending) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    // Source PNG (1-byte fake — we're not validating the image, just the copy)
    const QString src = tmp.path() + "/source-image.png";
    QFile sf(src); sf.open(QIODevice::WriteOnly); sf.write("\x89PNG\r\n"); sf.close();

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    m.replaceImage("back", src);

    EXPECT_TRUE(m.hasUnsavedChanges());
    EXPECT_TRUE(m.canUndo());
    // A file named back.png exists in the descriptor dir
    EXPECT_TRUE(QFile::exists(path + "/back.png"));
}
```

- [ ] **Step 3: Implement**

```cpp
void EditorModel::replaceImage(const QString &role, const QString &sourcePath) {
    if (m_activeDevicePath.isEmpty()) return;
    if (role != "front" && role != "side" && role != "back") return;
    if (!QFile::exists(sourcePath)) return;

    ensurePending(m_activeDevicePath);
    QJsonObject &obj = m_pendingEdits[m_activeDevicePath];
    QJsonObject images = obj.value("images").toObject();
    const QString prevName = images.value(role).toString();
    const QString newName = role + ".png";   // canonical filename per role
    const QString destPath = m_activeDevicePath + "/" + newName;

    // Atomic copy: write to .tmp then rename
    const QString tmpPath = destPath + ".tmp";
    QFile::remove(tmpPath);
    if (!QFile::copy(sourcePath, tmpPath)) return;
    QFile::remove(destPath);
    if (!QFile::rename(tmpPath, destPath)) {
        QFile::remove(tmpPath);
        return;
    }

    EditCommand cmd;
    cmd.kind = EditCommand::ImageReplace;
    cmd.role = role;
    cmd.before = prevName;
    cmd.after = newName;

    images[role] = newName;
    obj["images"] = images;
    pushCommand(std::move(cmd));
}
```

- [ ] **Step 4: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest.ReplaceImageCopiesFileAndUpdatesPending
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): replaceImage copies PNG and updates descriptor"
```

---

## Phase 4 — File watcher and conflict handling

### Task 17: `QFileSystemWatcher` integration with silent reload on no-dirty change

**Files:**
- Modify: `src/app/models/EditorModel.h` / `.cpp`
- Test: `tests/test_editor_model.cpp`

The file watcher is hard to drive deterministically in tests. We add a `simulateExternalChange(path)` slot for tests, alongside the real `QFileSystemWatcher::fileChanged` connection.

- [ ] **Step 1: Add to header**

```cpp
public slots:
    void onExternalFileChanged(const QString &path);  // bound to QFileSystemWatcher

signals:
    void externalChangeDetected(const QString &path);

private:
    QFileSystemWatcher *m_watcher = nullptr;
    void watch(const QString &devicePath);
```

Add to constructor: instantiate `m_watcher`, connect `fileChanged → onExternalFileChanged`. Note: `QFileSystemWatcher` watches files, so we watch `<devicePath>/descriptor.json`, not the directory.

Also add an `addWatch` slot that the AppController calls after `EditorModel` is constructed, once for each loaded device path. (Or do this lazily on `setActiveDevicePath`.)

For simplicity, watch every device the registry knows about at construction time, and re-watch on `setActiveDevicePath` if the path is unwatched.

- [ ] **Step 2: Write the test**

```cpp
TEST(EditorModelTest, ExternalChangeSilentReloadWhenNotDirty) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);

    // Simulate watcher firing for an external change with no pending edits
    m.onExternalFileChanged(path + "/descriptor.json");

    // No conflict prompt because not dirty
    EXPECT_EQ(externalSpy.count(), 0);
}

TEST(EditorModelTest, ExternalChangeWhileDirtyEmitsConflictSignal) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.99, 0.99);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);
    m.onExternalFileChanged(path + "/descriptor.json");

    EXPECT_EQ(externalSpy.count(), 1);
    EXPECT_EQ(externalSpy.first().first().toString(), path);
    EXPECT_TRUE(m.hasUnsavedChanges());      // pending state preserved
}
```

- [ ] **Step 3: Implement**

In `EditorModel.cpp`:
```cpp
void EditorModel::onExternalFileChanged(const QString &filePath) {
    // filePath is the descriptor.json path; convert to the device dir
    const QString devicePath = QFileInfo(filePath).absolutePath();
    const QString canonical = QFileInfo(devicePath).canonicalFilePath();

    // Self-write suppression — handled in Task 19
    if (m_selfWrittenPaths.contains(canonical)) {
        m_selfWrittenPaths.remove(canonical);
        return;
    }

    if (m_dirty.contains(canonical)) {
        emit externalChangeDetected(canonical);
        return;
    }

    // Silent reload
    if (m_registry)
        m_registry->reload(canonical);
}
```

In the constructor, instantiate the watcher:
```cpp
m_watcher = new QFileSystemWatcher(this);
connect(m_watcher, &QFileSystemWatcher::fileChanged,
        this, &EditorModel::onExternalFileChanged);

if (m_registry) {
    for (const auto &dev : m_registry->devices()) {
        if (auto *jd = dynamic_cast<const JsonDevice*>(dev.get())) {
            m_watcher->addPath(jd->sourcePath() + "/descriptor.json");
        }
    }
}
```

- [ ] **Step 4: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest
git add src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(EditorModel): file watcher with silent reload + conflict signal"
```

---

### Task 18: Self-write suppression after `save()`

**Files:**
- Test: `tests/test_editor_model.cpp`

The implementation already added `m_selfWrittenPaths` insertion in `save()` (Task 13) and removal in `onExternalFileChanged()` (Task 17). This task locks the behavior with a test.

- [ ] **Step 1: Write the test**

```cpp
TEST(EditorModelTest, SaveSuppressesOwnWatcherFire) {
    QTemporaryDir tmp; ASSERT_TRUE(tmp.isValid());
    const QString path = writeMinimalDescriptor(tmp.path() + "/dev");

    logitune::DeviceRegistry reg;
    logitune::EditorModel m(&reg, true);
    m.setActiveDevicePath(path);
    m.updateSlotPosition(0, 0.55, 0.66);

    QSignalSpy externalSpy(&m, &logitune::EditorModel::externalChangeDetected);
    m.save();

    // Saving inserts the path into m_selfWrittenPaths.
    // Now simulate the watcher firing for our own write.
    m.onExternalFileChanged(path + "/descriptor.json");

    // Should be suppressed — no conflict prompt
    EXPECT_EQ(externalSpy.count(), 0);
}
```

- [ ] **Step 2: Run, pass, commit**

```bash
cd build && ctest --output-on-failure -R EditorModelTest.SaveSuppressesOwnWatcherFire
git add tests/test_editor_model.cpp
git commit -m "test(EditorModel): lock self-write suppression after save"
```

---

## Phase 5 — CLI plumbing and AppController integration

### Task 19: `AppController::startMonitoring(simulateAll, editMode)` and EditorModel ownership

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `src/app/main.cpp`
- Test: `tests/test_app_controller.cpp` (existing)

- [ ] **Step 1: Add `editMode` parameter to header**

In `AppController.h`:
```cpp
void startMonitoring(bool simulateAll = false, bool editMode = false);

EditorModel* editorModel() const { return m_editorModel.get(); }

private:
    std::unique_ptr<EditorModel> m_editorModel;
```

Forward-declare `EditorModel` at the top.

- [ ] **Step 2: Wire in `AppController.cpp`**

```cpp
void AppController::startMonitoring(bool simulateAll, bool editMode) {
    if (editMode) {
        m_editorModel = std::make_unique<EditorModel>(&m_registry, /*editing=*/true, this);
        qCInfo(lcApp) << "--edit: editor mode active";
    }
    if (simulateAll) {
        qCInfo(lcApp) << "--simulate-all: seeding carousel from registry";
        m_deviceManager.simulateAllFromRegistry();
        m_desktop->start();
        return;
    }
    m_deviceManager.start();
    m_desktop->start();
    m_deviceFetcher.fetchManifest();
}
```

Include `models/EditorModel.h` in the cpp (not header — keep it forward-declared in the header).

- [ ] **Step 3: Update `main.cpp`**

Add a `--edit` option just below the `simulateAllOption`:
```cpp
QCommandLineOption editOption(
    QStringLiteral("edit"),
    QStringLiteral("Enable in-app descriptor editor mode. Drag elements on "
                   "device pages to edit slot positions, hotspots, and images. "
                   "Save writes back to the source descriptor JSON."));
parser.addOption(editOption);
// ...
const bool editMode = parser.isSet(editOption);
// ...
controller.startMonitoring(simulateAll, editMode);
```

After QML registration, register the editor model singleton when set:
```cpp
if (editMode && controller.editorModel()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    qmlRegisterSingletonInstance("Logitune", 1, 0, "EditorModel", controller.editorModel());
#else
    engine.rootContext()->setContextProperty("EditorModel", controller.editorModel());
#endif
}
```

- [ ] **Step 4: Update existing test that calls `startMonitoring()` (if any breaks)**

Existing test fixtures use a default-arg call. The default args remain `false, false` so source compatibility is preserved.

- [ ] **Step 5: Build, run all tests, then a smoke test**

```bash
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
cd ..
./build/src/app/logitune --help 2>&1 | grep -E '(simulate-all|edit)'
```
Expected: both flags listed.

- [ ] **Step 6: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp src/app/main.cpp
git commit -m "feat(AppController): --edit CLI flag instantiates EditorModel

EditorModel is owned by AppController and registered as a QML singleton
only when --edit is set. The production binary path with neither flag
is unchanged."
```

---

## Phase 6 — View layer: toolbar, sidebar stripe, Main.qml mount

### Task 20: `EditorToolbar.qml` component with state-tracking buttons

**Files:**
- Create: `src/app/qml/components/EditorToolbar.qml`
- Modify: `src/app/CMakeLists.txt` (register the QML file)
- Create: `tests/qml/tst_EditorToolbar.qml`
- Modify: `tests/qml/CMakeLists.txt`

- [ ] **Step 1: Create the component**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
    height: visible ? 36 : 0
    color: Theme.background
    border.color: Theme.border
    border.width: visible ? 1 : 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            id: pathLabel
            text: visible && EditorModel.activeDevicePath
                  ? "…/" + EditorModel.activeDevicePath.split("/").slice(-2).join("/")
                  : ""
            font.pixelSize: 11
            color: Theme.textSecondary
            elide: Text.ElideLeft
            Layout.fillWidth: true
        }

        Rectangle {
            id: dirtyDot
            visible: root.visible && EditorModel.hasUnsavedChanges
            width: 8; height: 8; radius: 4
            color: Theme.accent
        }
        Text {
            visible: root.visible && EditorModel.hasUnsavedChanges
            text: "Unsaved changes"
            font.pixelSize: 11
            color: Theme.text
        }

        Button {
            text: "Undo"
            enabled: root.visible && EditorModel.canUndo
            onClicked: EditorModel.undo()
        }
        Button {
            text: "Redo"
            enabled: root.visible && EditorModel.canRedo
            onClicked: EditorModel.redo()
        }
        Button {
            text: "Reset"
            enabled: root.visible && EditorModel.hasUnsavedChanges
            onClicked: EditorModel.reset()
        }
        Button {
            text: "Save"
            enabled: root.visible && EditorModel.hasUnsavedChanges
            onClicked: EditorModel.save()
        }
    }
}
```

- [ ] **Step 2: Register in `src/app/CMakeLists.txt`**

In the `qt_add_qml_module(... QML_FILES ...)` block, add:
```cmake
qml/components/EditorToolbar.qml
```

For Qt 6.4 fallback, also add to the components `foreach(_file ...)` list above.

- [ ] **Step 3: Write the QML test**

Create `tests/qml/tst_EditorToolbar.qml`:
```qml
import QtQuick
import QtTest
import Logitune

Item {
    width: 600; height: 100

    EditorToolbar {
        id: bar
        anchors.fill: parent
    }

    TestCase {
        name: "EditorToolbar"
        when: windowShown

        function test_invisibleWhenEditingOff() {
            // EditorModel is registered only when --edit is on, so the
            // typeof check in the component yields false in the standard test
            // harness. The toolbar should be height: 0 / invisible.
            compare(bar.visible, false)
        }
    }
}
```

(A more thorough test that drives `EditorModel.editing = true` requires registering a mock singleton; defer that to the integration smoke test.)

- [ ] **Step 4: Add to `tests/qml/CMakeLists.txt`**

```cmake
tst_EditorToolbar.qml
```
in the QML test sources list.

- [ ] **Step 5: Build, run**

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -R tst_EditorToolbar
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/app/qml/components/EditorToolbar.qml src/app/CMakeLists.txt tests/qml/tst_EditorToolbar.qml tests/qml/CMakeLists.txt
git commit -m "feat(qml): EditorToolbar component with save/undo/redo/reset/dirty state"
```

---

### Task 21: Mount `EditorToolbar` in `Main.qml` and add the sidebar amber stripe

**Files:**
- Modify: `src/app/qml/Main.qml`
- Modify: `src/app/qml/components/SideNav.qml`
- Modify: `tests/qml/tst_SideNav.qml`

- [ ] **Step 1: Mount the toolbar in Main.qml**

In `Main.qml`, find the layout that hosts the page loader/SideNav and insert an `EditorToolbar` above the page area. The toolbar is height 0 when invisible so it doesn't reserve space in production.

- [ ] **Step 2: Add the amber stripe to SideNav.qml**

In `src/app/qml/components/SideNav.qml`, after the existing root `Rectangle { id: sideNav ... }` definition, add a child `Rectangle` anchored to the left edge:
```qml
Rectangle {
    id: editStripe
    anchors {
        left: parent.left
        top: parent.top
        bottom: parent.bottom
    }
    width: 4
    color: "#F5A623"   // amber
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
}
```

- [ ] **Step 3: Update existing `tst_SideNav.qml` test**

Append:
```qml
function test_amberStripeInvisibleByDefault() {
    var stripe = findChild(nav, "editStripe")
    // findChild may return null since the test runs without EditorModel registered
    if (stripe)
        compare(stripe.visible, false)
}
```

(Use `objectName: "editStripe"` on the `Rectangle` instead of `id` if `findChild` doesn't find it via id — Qt Test reflection uses objectName.)

Add `objectName: "editStripe"` next to the `id: editStripe` declaration in SideNav.qml.

- [ ] **Step 4: Smoke-test live**

```bash
mv .devices.hidden devices 2>/dev/null || true
pkill -f 'logitune' || true
nohup ./build/src/app/logitune > /tmp/logitune.log 2>&1 & disown
```
Verify the production app launches normally (no toolbar, no stripe).

```bash
pkill -f 'logitune' || true
nohup ./build/src/app/logitune --simulate-all --edit > /tmp/logitune-edit.log 2>&1 & disown
```
Verify the toolbar appears at the top and the sidebar shows the amber stripe.

- [ ] **Step 5: Commit**

```bash
git add src/app/qml/Main.qml src/app/qml/components/SideNav.qml tests/qml/tst_SideNav.qml
git commit -m "feat(qml): mount EditorToolbar and add sidebar edit-mode stripe"
```

---

## Phase 7 — Tier 1: easy-switch slot drag

### Task 22: Wire `setActiveDevicePath` to follow the active carousel selection

**Files:**
- Modify: `src/app/AppController.cpp`

The editor needs to know which device is active so it knows which descriptor to mutate. Hook `DeviceModel::selectedChanged` to update `EditorModel::setActiveDevicePath`.

- [ ] **Step 1: In `AppController` constructor (or `init`), connect signals after instantiating `m_editorModel`**

```cpp
if (m_editorModel) {
    auto syncActive = [this]() {
        const auto *dev = m_deviceModel->activeDevice();   // adjust to actual accessor
        if (auto *jd = dynamic_cast<const JsonDevice*>(dev))
            m_editorModel->setActiveDevicePath(jd->sourcePath());
        else
            m_editorModel->setActiveDevicePath(QString());
    };
    connect(m_deviceModel.get(), &DeviceModel::selectedChanged, this, syncActive);
    syncActive();  // initial state
}
```

If `DeviceModel` doesn't expose `activeDevice()` directly, add a small `Q_INVOKABLE const IDevice* activeDevice() const` accessor to `DeviceModel.h` that returns the currently-selected `IDevice*` from its internal pointer table.

- [ ] **Step 2: Build, smoke-test live**

Launch with `--simulate-all --edit` and check the toolbar's path label updates as you flip through the carousel.

- [ ] **Step 3: Commit**

```bash
git add src/app/AppController.cpp src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp
git commit -m "feat(AppController): sync EditorModel active path with carousel selection"
```

---

### Task 23: `EasySwitchPage` slot circles get drag handlers when editing

**Files:**
- Modify: `src/app/qml/pages/EasySwitchPage.qml`
- Create: `tests/qml/tst_EasySwitchPageEdit.qml`
- Modify: `tests/qml/CMakeLists.txt`

The existing slot Repeater (`EasySwitchPage.qml:56-78`) renders `Rectangle` elements at `imgX + imgW * pos.xPct - width/2`. To drag, wrap each circle in a larger invisible hit-target with a `DragHandler`.

- [ ] **Step 1: Modify the slot Rectangle in `EasySwitchPage.qml`**

Replace the slot `Rectangle` (lines ~58-77) with an `Item` containing the visible Rectangle and a `DragHandler`:

```qml
Repeater {
    model: imageContainer.slotPositions.length
    Item {
        id: slotItem
        required property int index
        readonly property bool isActive: (index + 1) === DeviceModel.activeSlot
        readonly property var pos: index < imageContainer.slotPositions.length
            ? imageContainer.slotPositions[index] : { xPct: 0.5, yPct: 0.65 }

        // Drag hit area (24x24) around the visible 9x9 circle
        width: 24; height: 24

        readonly property real targetX: imageContainer.imgX + imageContainer.imgW * pos.xPct
        readonly property real targetY: imageContainer.imgY + imageContainer.imgH * pos.yPct

        x: drag.active ? x : targetX - width / 2
        y: drag.active ? y : targetY - height / 2

        Rectangle {
            anchors.centerIn: parent
            width: 9; height: 9; radius: 4.5
            color: slotItem.isActive ? Theme.accent : "transparent"
            border.color: Theme.accent
            border.width: slotItem.isActive ? 0 : 1.5

            SequentialAnimation on opacity {
                running: slotItem.isActive
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
            }
        }

        DragHandler {
            id: drag
            enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
            target: parent
            onActiveChanged: {
                if (!active) {
                    var cx = slotItem.x + slotItem.width / 2
                    var cy = slotItem.y + slotItem.height / 2
                    var xPct = (cx - imageContainer.imgX) / imageContainer.imgW
                    var yPct = (cy - imageContainer.imgY) / imageContainer.imgH
                    xPct = Math.max(0, Math.min(1, xPct))
                    yPct = Math.max(0, Math.min(1, yPct))
                    EditorModel.updateSlotPosition(index, xPct, yPct)
                }
            }
        }
    }
}
```

Note the inverse drag math: `xPct = (cx - imgX) / imgW`. This is the inverse of the existing rendering math (`x = imgX + imgW * xPct`). Because `xPct` is already aspect-corrected at extraction time (the back-image rotation transform documented in the spec), the inverse here produces the same aspect-corrected value — no extra correction needed. The aspect-corrected `xPct` flows straight back into the descriptor.

- [ ] **Step 2: Write a QML test for the drag math**

Create `tests/qml/tst_EasySwitchPageEdit.qml`:
```qml
import QtQuick
import QtTest

Item {
    width: 800; height: 600

    // Mock the rendering geometry
    property real imgX: 100
    property real imgY: 50
    property real imgW: 600
    property real imgH: 400

    function inverseDragMath(centroidX, centroidY) {
        var xPct = (centroidX - imgX) / imgW
        var yPct = (centroidY - imgY) / imgH
        return [Math.max(0, Math.min(1, xPct)),
                Math.max(0, Math.min(1, yPct))]
    }

    TestCase {
        name: "EasySwitchDragMath"
        function test_centerOfImageIsHalfHalf() {
            var r = inverseDragMath(imgX + imgW / 2, imgY + imgH / 2)
            verify(Math.abs(r[0] - 0.5) < 1e-6)
            verify(Math.abs(r[1] - 0.5) < 1e-6)
        }
        function test_topLeftCornerIsZeroZero() {
            var r = inverseDragMath(imgX, imgY)
            compare(r[0], 0)
            compare(r[1], 0)
        }
        function test_outsideClampsToRange() {
            var r = inverseDragMath(imgX - 999, imgY + imgH + 999)
            compare(r[0], 0)
            compare(r[1], 1)
        }
    }
}
```

- [ ] **Step 3: Add to `tests/qml/CMakeLists.txt`**

```cmake
tst_EasySwitchPageEdit.qml
```

- [ ] **Step 4: Build, run, smoke test live**

```bash
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure -R tst_EasySwitchPageEdit
cd ..
pkill -f 'logitune' || true
nohup ./build/src/app/logitune --simulate-all --edit > /tmp/logitune-edit.log 2>&1 & disown
```
Walk to a device with easy-switch, drag a slot circle, observe it sticks at the new position. The toolbar's "Unsaved changes" indicator lights up. Click Save, restart, verify position persisted.

- [ ] **Step 5: Commit**

```bash
git add src/app/qml/pages/EasySwitchPage.qml tests/qml/tst_EasySwitchPageEdit.qml tests/qml/CMakeLists.txt
git commit -m "feat(qml): drag handlers on easy-switch slot circles in editor mode

End-to-end Tier 1 slice: drag a slot, the inverse rendering math
converts the centroid back to xPct/yPct (already aspect-corrected),
EditorModel.updateSlotPosition pushes an undo command, the toolbar
lights up dirty, Save persists via DescriptorWriter."
```

---

## Phase 8 — Tier 2: button and point/scroll markers and cards

### Task 24: `ButtonsPage` marker drag handlers

**Files:**
- Modify: `src/app/qml/pages/ButtonsPage.qml`

The existing page renders button hotspot markers as small dots on the front image, with overlay cards on the side. Add `DragHandler`s to the markers, gated on `EditorModel.editing`. On release, call `EditorModel.updateHotspot(index, xPct, yPct, side, labelOffsetYPct)` keeping `side` and `labelOffsetYPct` from the model (only marker position changes here — card drag is the next task).

- [ ] **Step 1: Read the existing marker rendering**

Open `src/app/qml/pages/ButtonsPage.qml` and find the marker Repeater. Confirm it iterates `DeviceModel.buttonHotspots` and renders one marker per entry.

- [ ] **Step 2: Wrap each marker in a draggable Item**

Same pattern as Task 23 — wrap the visible marker in a 24×24 hit area, add a `DragHandler` enabled in edit mode, and on release call:
```qml
EditorModel.updateHotspot(index, xPct, yPct, modelData.side, modelData.labelOffsetYPct)
```

The inverse drag math is the same: `xPct = (cx - imgX) / imgW`.

- [ ] **Step 3: Smoke test live**

Drag a button marker, observe it sticks. Save. Restart. Verify persistence.

- [ ] **Step 4: Commit**

```bash
git add src/app/qml/pages/ButtonsPage.qml
git commit -m "feat(qml): drag handlers on button markers in editor mode"
```

---

### Task 25: `ButtonsPage` card drag handlers with side-snap

**Files:**
- Modify: `src/app/qml/pages/ButtonsPage.qml`

Cards live in two columns (left/right). Dragging a card horizontally should snap to the nearest column based on which half of the page the centroid lands in; dragging vertically should update `labelOffsetYPct` as a fraction of the page height.

- [ ] **Step 1: Find the card rendering**

Locate the `ButtonCallout` (or equivalent) Repeater. Each card is positioned by its column (`side`) and `labelOffsetYPct`.

- [ ] **Step 2: Add a DragHandler to the card root Item**

```qml
DragHandler {
    enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
    target: parent
    onActiveChanged: {
        if (!active) {
            var pageWidth = root.width
            var centroidX = card.x + card.width / 2
            var newSide = centroidX < pageWidth / 2 ? "left" : "right"
            var newOffsetY = (card.y - cardBaselineY) / root.height
            EditorModel.updateHotspot(index,
                                      modelData.xPct, modelData.yPct,
                                      newSide, newOffsetY)
        }
    }
}
```

`cardBaselineY` is the position the card would have at `labelOffsetYPct = 0`. Compute it from the existing positioning math and store it as a card property.

- [ ] **Step 3: Smoke test, commit**

```bash
git add src/app/qml/pages/ButtonsPage.qml
git commit -m "feat(qml): drag handlers on button cards with side-snap"
```

---

### Task 26: `PointScrollPage` mirrors the marker + card drag pattern

**Files:**
- Modify: `src/app/qml/pages/PointScrollPage.qml`
- Modify: `src/app/models/EditorModel.h` / `.cpp` — add `updateScrollHotspot` slot

The scroll hotspots live in `hotspots.scroll` (not `hotspots.buttons`). Add a sibling slot `updateScrollHotspot(int idx, double xPct, double yPct, QString side, double labelOffsetYPct)` that mutates `hotspots.scroll[idx]` instead of `hotspots.buttons[idx]`.

- [ ] **Step 1: Add the slot to `EditorModel`**

Same shape as `updateHotspot`, but operates on `hotspots.scroll`. Push an `EditCommand{HotspotMove}` with a different `role` so undo knows which array to address. Update `applyCommand` to read `cmd.role` for `HotspotMove` and pick `buttons` or `scroll` accordingly.

Update earlier `updateHotspot` to set `cmd.role = "buttons"` and the new `updateScrollHotspot` to set `cmd.role = "scroll"`.

Add a unit test:
```cpp
TEST(EditorModelTest, UpdateScrollHotspotMutatesScrollArray) {
    // ... fixture with one entry in hotspots.scroll, edit it, check pending JSON
}
```

- [ ] **Step 2: Wire the QML page**

`PointScrollPage.qml` mirrors `ButtonsPage.qml` — same wrap-marker-in-DragHandler, same card-drag-with-side-snap. Calls `EditorModel.updateScrollHotspot(...)` instead.

- [ ] **Step 3: Smoke test, commit**

```bash
git add src/app/qml/pages/PointScrollPage.qml src/app/models/EditorModel.h src/app/models/EditorModel.cpp tests/test_editor_model.cpp
git commit -m "feat(qml,EditorModel): drag handlers on scroll hotspots and cards"
```

---

### Task 27: Double-click in-place text editing for slot labels, button cards, and device name

**Files:**
- Modify: `src/app/qml/pages/EasySwitchPage.qml`
- Modify: `src/app/qml/pages/ButtonsPage.qml`
- Modify: `src/app/qml/pages/PointScrollPage.qml`
- Modify: `src/app/qml/components/SideNav.qml`

The text editing affordance is a `MouseArea { onDoubleClicked: ... }` on each label that swaps the `Text` for a `TextField` until Enter or Esc.

- [ ] **Step 1: Define a small reusable inline-edit component**

Create `src/app/qml/components/EditableText.qml`:
```qml
import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string text: ""
    property bool editable: typeof EditorModel !== 'undefined' && EditorModel.editing
    signal commit(string newValue)

    width: txt.implicitWidth
    height: txt.implicitHeight

    Text {
        id: txt
        anchors.fill: parent
        text: root.text
        color: Theme.text
        visible: !field.visible
    }

    TextField {
        id: field
        anchors.fill: parent
        text: root.text
        visible: false
        onAccepted: { root.commit(text); visible = false }
        Keys.onEscapePressed: { visible = false }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.editable
        onDoubleClicked: { field.visible = true; field.forceActiveFocus(); field.selectAll() }
    }
}
```

Register it in CMakeLists.

- [ ] **Step 2: Use it in each location**

Replace the slot label `Text` in the EASY-SWITCH channel list with:
```qml
EditableText {
    text: modelData.label || (isActive ? DeviceModel.connectionType : "Available")
    onCommit: function(v) { EditorModel.updateText("slotLabel", index, v) }
}
```

Replace button card name `Text` with:
```qml
EditableText {
    text: modelData.displayName || modelData.defaultName
    onCommit: function(v) { EditorModel.updateText("controlDisplayName", index, v) }
}
```

Replace the sidebar device-name `Text` in `SideNav.qml` with:
```qml
EditableText {
    text: DeviceModel.deviceName || "MX Master 3S"
    onCommit: function(v) { EditorModel.updateText("deviceName", -1, v) }
}
```

- [ ] **Step 3: Smoke test, commit**

```bash
git add src/app/qml/components/EditableText.qml src/app/qml/pages/*.qml src/app/qml/components/SideNav.qml src/app/CMakeLists.txt
git commit -m "feat(qml): double-click in-place text editing for labels and names"
```

---

## Phase 9 — Tier 3: image upload

### Task 28: `EasySwitchPage` accepts dropped PNGs and has a Replace Image button

**Files:**
- Modify: `src/app/qml/pages/EasySwitchPage.qml`

- [ ] **Step 1: Wrap the back image in a DropArea**

```qml
DropArea {
    anchors.fill: deviceImage
    enabled: typeof EditorModel !== 'undefined' && EditorModel.editing
    onDropped: function(drop) {
        if (drop.hasUrls && drop.urls.length > 0) {
            var path = drop.urls[0].toString().replace(/^file:\/\//, "")
            if (path.toLowerCase().endsWith(".png"))
                EditorModel.replaceImage("back", path)
        }
    }
}
```

- [ ] **Step 2: Add a "Replace image" button anchored top-right of the image, visible only in editor mode**

```qml
Button {
    visible: typeof EditorModel !== 'undefined' && EditorModel.editing
    anchors.top: deviceImage.top
    anchors.right: deviceImage.right
    anchors.margins: 4
    text: "Replace image"
    onClicked: imageDialog.open()
}
FileDialog {
    id: imageDialog
    nameFilters: ["PNG (*.png)"]
    onAccepted: EditorModel.replaceImage("back", selectedFile.toString().replace(/^file:\/\//, ""))
}
```

(Import `Qt.labs.platform` or `QtQuick.Dialogs` depending on Qt version — match what other pages use.)

- [ ] **Step 3: Smoke test live, commit**

Drag a PNG onto the back image area in `--simulate-all --edit`. Verify the new image appears (after save + restart). Use the button as the fallback.

```bash
git add src/app/qml/pages/EasySwitchPage.qml
git commit -m "feat(qml): drag-drop and file picker for back image upload"
```

---

### Task 29: `ButtonsPage` and `PointScrollPage` accept dropped PNGs for the front image

**Files:**
- Modify: `src/app/qml/pages/ButtonsPage.qml`
- Modify: `src/app/qml/pages/PointScrollPage.qml`

Same pattern as Task 28 but for `"front"` instead of `"back"`.

- [ ] **Step 1: Add `DropArea` and `Replace image` button to each page**

- [ ] **Step 2: Smoke test, commit**

```bash
git add src/app/qml/pages/ButtonsPage.qml src/app/qml/pages/PointScrollPage.qml
git commit -m "feat(qml): drag-drop and file picker for front image upload"
```

---

## Phase 10 — Conflict banner and diff modal

### Task 30: `ConflictBanner.qml` with three actions

**Files:**
- Create: `src/app/qml/components/ConflictBanner.qml`
- Modify: `src/app/qml/Main.qml`
- Modify: `src/app/CMakeLists.txt`

- [ ] **Step 1: Create the component**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logitune

Rectangle {
    id: root
    property string conflictPath: ""
    visible: conflictPath.length > 0
    height: visible ? 40 : 0
    color: "#FFF3CD"
    border.color: "#FFEEBA"

    signal viewDiffRequested(string path)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            text: "This file was changed on disk."
            font.pixelSize: 12
            color: "#856404"
            Layout.fillWidth: true
        }
        Button {
            text: "Keep my edits"
            onClicked: root.conflictPath = ""
        }
        Button {
            text: "Load disk version"
            onClicked: {
                EditorModel.reset()           // discards pending
                EditorModel.setActiveDevicePath(root.conflictPath)  // re-reads
                root.conflictPath = ""
            }
        }
        Button {
            text: "View diff"
            onClicked: root.viewDiffRequested(root.conflictPath)
        }
    }
}
```

- [ ] **Step 2: Mount in `Main.qml` below the EditorToolbar**

Connect to `EditorModel.externalChangeDetected`:
```qml
ConflictBanner {
    id: conflictBanner
    Layout.fillWidth: true
    Connections {
        target: typeof EditorModel !== 'undefined' ? EditorModel : null
        function onExternalChangeDetected(path) {
            conflictBanner.conflictPath = path
        }
    }
    onViewDiffRequested: function(path) { diffModal.open(path) }
}
```

- [ ] **Step 3: Register, smoke test, commit**

```bash
cd build && cmake --build . -j$(nproc)
git add src/app/qml/components/ConflictBanner.qml src/app/qml/Main.qml src/app/CMakeLists.txt
git commit -m "feat(qml): ConflictBanner for external file changes"
```

---

### Task 31: `DiffModal.qml` with line diff viewer

**Files:**
- Create: `src/app/qml/components/DiffModal.qml`
- Modify: `src/app/qml/Main.qml`
- Modify: `src/app/CMakeLists.txt`

For the diff itself, do a simple line-by-line equality scan rather than a full Myers diff (good enough for descriptor JSON which is mostly small). If lines differ, mark them with a left-margin colored bar.

- [ ] **Step 1: Create the component**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    width: 800; height: 600
    title: "External change diff"
    standardButtons: Dialog.Close

    property string diskText: ""
    property string memoryText: ""

    function open(path) {
        diskText = readFile(path + "/descriptor.json")
        memoryText = JSON.stringify(EditorModel.pendingFor(path), null, 2)
        visible = true
    }

    function readFile(p) {
        var xhr = new XMLHttpRequest()
        xhr.open("GET", "file://" + p, false)
        xhr.send(null)
        return xhr.responseText
    }

    RowLayout {
        anchors.fill: parent
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: leftText.implicitWidth
            contentHeight: leftText.implicitHeight
            Text { id: leftText; text: root.diskText; font.family: "monospace"; font.pixelSize: 11 }
        }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: rightText.implicitWidth
            contentHeight: rightText.implicitHeight
            Text { id: rightText; text: root.memoryText; font.family: "monospace"; font.pixelSize: 11 }
        }
    }
}
```

- [ ] **Step 2: Add `pendingFor(path)` Q_INVOKABLE to `EditorModel`**

```cpp
Q_INVOKABLE QVariantMap pendingFor(const QString &path) const;
```
Implementation: convert `m_pendingEdits.value(path)` to `QVariantMap` via `QJsonObject::toVariantMap()`.

- [ ] **Step 3: Mount in Main.qml, smoke test, commit**

```bash
git add src/app/qml/components/DiffModal.qml src/app/qml/Main.qml src/app/models/EditorModel.h src/app/models/EditorModel.cpp src/app/CMakeLists.txt
git commit -m "feat(qml): DiffModal showing on-disk vs in-memory descriptor"
```

---

## Phase 11 — Smoke test docs

### Task 32: Document the manual smoke test in the PR description

**Files:**
- No code changes. Update PR #34 body via `gh pr edit` to include the manual smoke test checklist.

- [ ] **Step 1: Append a "Manual smoke test" section**

```markdown
## Manual smoke test (run before marking PR ready)

Setup:
\`\`\`
git clone https://github.com/mmaher88/logitune-devices.git /tmp/logitune-devices-test
mkdir -p /tmp/sim-test/logitune
ln -s /tmp/logitune-devices-test /tmp/sim-test/logitune/devices
XDG_DATA_HOME=/tmp/sim-test ./build/src/app/logitune --simulate-all --edit
\`\`\`

- [ ] Drag an easy-switch slot circle. Toolbar shows "Unsaved changes".
- [ ] Click Save. Toolbar clears. Restart. Position persisted.
- [ ] Drag a button card to the opposite side. Save. Restart. Side persisted.
- [ ] Double-click a slot label, type "Mac", press Enter. Save. Restart. Label persisted.
- [ ] Drop a PNG onto the back image area. Save. Restart. New image renders.
- [ ] Click "Replace image" → file picker → select PNG. Save. Restart. New image renders.
- [ ] In a separate terminal, \`sed -i s/Original/Mutated/ /tmp/logitune-devices-test/<some-device>/descriptor.json\`. Conflict banner appears with three buttons. "View diff" opens modal showing both versions.
- [ ] Click "Load disk version". Banner clears, page repaints with disk content.
- [ ] Make an edit, then click Reset. Confirm dialog. Pending edits discarded.
- [ ] Undo and Redo work across multiple edits.
- [ ] Switch device in carousel, edit, switch back. Both devices' undo stacks intact.
- [ ] Run \`./build/src/app/logitune\` with no flags. Production behavior unchanged: no toolbar, no amber stripe, real device detected.
```

- [ ] **Step 2: Run `gh pr edit 34 --body-file ...` to update**

- [ ] **Step 3: Commit any docs/CHANGELOG updates if applicable, push the branch**

```bash
mv .devices.hidden devices 2>/dev/null || true   # restore for tests + push
git push
mv devices .devices.hidden 2>/dev/null || true   # re-hide for ongoing simulation
```

---

## Self-review checklist

After implementing all tasks, verify against the spec at `docs/superpowers/specs/2026-04-15-editor-mode-design.md`:

- [ ] `JsonDevice::sourcePath()` exists and is canonicalized — Task 1
- [ ] `JsonDevice::loadedMtime()` exists — Task 1
- [ ] Optional `displayName` on controls and `label` on slots — Task 2
- [ ] `JsonDevice::refresh()` mutates in place — Task 3
- [ ] `DeviceRegistry::reload(path)` and `findBySourcePath` — Task 4
- [ ] `DescriptorWriter` with atomic write + preserve-unknown-fields — Tasks 5–6
- [ ] `EditorModel` with editing flag, dirty tracking, undo/redo — Tasks 7–12
- [ ] `save()` / `saveFailed` / `reset()` / `replaceImage` — Tasks 13–16
- [ ] File watcher with silent reload, conflict signal, self-write suppression — Tasks 17–18
- [ ] `--edit` CLI flag and `EditorModel` QML registration — Task 19
- [ ] `EditorToolbar.qml` mounted in Main.qml — Tasks 20–21
- [ ] Sidebar amber stripe — Task 21
- [ ] Active device path syncs with carousel — Task 22
- [ ] Tier 1 — easy-switch slot drag — Task 23
- [ ] Tier 2 — button markers, cards, scroll hotspots, text editing — Tasks 24–27
- [ ] Tier 3 — image drop area + file picker — Tasks 28–29
- [ ] Conflict banner + diff modal — Tasks 30–31
- [ ] Manual smoke test checklist on PR — Task 32

If any checkbox above is unchecked at completion, the spec is not fully implemented.

---

## Open items resolved during implementation

The spec flagged a few items to verify during foundation work:

- **`DeviceRegistry` ownership model.** Resolved in Task 3: `JsonDevice::refresh()` mutates in place, so `unique_ptr` ownership stays valid and client `IDevice*` pointers remain valid across reload.
- **Per-command undo labels** ("Undo move slot 2"): not implemented in this plan. If the UX feels lacking after Tier 1 ships, add a small `QString label` field to `EditCommand` and surface it in the toolbar tooltip.
- **`EditorToolbar` position as `QSettings`**: not implemented. Fixed at the top of the page area.
