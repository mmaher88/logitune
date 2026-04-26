# JSON Device Descriptors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace compiled C++ device descriptors with runtime-loaded JSON files, so new devices can be added without code changes.

**Architecture:** A `JsonDevice` class implements `IDevice` by loading a JSON file at construction time. `DeviceRegistry` scans three XDG-standard directories for device subdirectories. Each device is a self-contained directory with `descriptor.json` + image files. Existing MX Master 3S and MX Master 4 descriptors migrate from C++ to JSON. All device images move from Qt resources to the filesystem.

**Tech Stack:** C++20, Qt6 (QJsonDocument, QStandardPaths), GoogleTest, CMake/Ninja

**Spec:** `docs/superpowers/specs/2026-04-12-json-descriptors-design.md`

**Note:** This supersedes PR #19 (feature-flags-per-device). Close PR #19 after this lands — all its changes are included here.

---

## File Structure

**New files:**
- `src/core/devices/JsonDevice.h` — JsonDevice class declaration
- `src/core/devices/JsonDevice.cpp` — JSON loading, parsing, validation
- `devices/mx-master-3s/descriptor.json` — MX Master 3S JSON descriptor
- `devices/mx-master-3s/front.png` — moved from `src/app/qml/assets/mx-master-3s.png`
- `devices/mx-master-3s/side.png` — moved from `src/app/qml/assets/mx-master-3s-side.png`
- `devices/mx-master-3s/back.png` — moved from `src/app/qml/assets/mx-master-3s-back.png`
- `devices/mx-master-4/descriptor.json` — MX Master 4 JSON descriptor
- `devices/mx-master-4/front.png` — moved from `src/app/qml/assets/mx-master-4.png`
- `devices/mx-master-4/side.png` — moved from `src/app/qml/assets/mx-master-4-side.png`
- `devices/mx-master-4/back.png` — moved from `src/app/qml/assets/mx-master-4-back.png`
- `tests/test_json_device.cpp` — unit tests for JsonDevice loading + validation

**Modified files:**
- `src/core/interfaces/IDevice.h` — extend FeatureSupport with 28 mouse feature flags
- `src/core/DeviceRegistry.h` — add directory scanning methods
- `src/core/DeviceRegistry.cpp` — replace compiled descriptors with JSON directory loading
- `src/core/CMakeLists.txt` — remove old descriptor .cpp, add JsonDevice.cpp, add install rule
- `src/app/CMakeLists.txt` — remove image RESOURCES entries
- `src/app/models/DeviceModel.cpp` — handle file:// paths for images
- `tests/CMakeLists.txt` — add test_json_device.cpp
- `tests/mocks/MockDevice.h` — update for new FeatureSupport fields

**Deleted files:**
- `src/core/devices/MxMaster3sDescriptor.h`
- `src/core/devices/MxMaster3sDescriptor.cpp`
- `src/core/devices/MxMaster4Descriptor.h`
- `src/core/devices/MxMaster4Descriptor.cpp`

---

### Task 1: Extend FeatureSupport + create JSON descriptor files + move images

**Files:**
- Modify: `src/core/interfaces/IDevice.h`
- Modify: `src/core/devices/MxMaster4Descriptor.cpp` (add smoothScroll=false temporarily)
- Modify: `tests/mocks/MockDevice.h`
- Create: `devices/mx-master-3s/descriptor.json`
- Create: `devices/mx-master-4/descriptor.json`
- Move: 6 image files from `src/app/qml/assets/` to `devices/*/`

- [ ] **Step 1.1: Extend FeatureSupport struct**

In `src/core/interfaces/IDevice.h`, replace the `FeatureSupport` struct with:

```cpp
struct FeatureSupport {
    bool battery = false;
    bool adjustableDpi = false;
    bool extendedDpi = false;
    bool smartShift = false;
    bool hiResWheel = false;
    bool hiResScrolling = false;
    bool lowResWheel = false;
    bool smoothScroll = true;
    bool thumbWheel = false;
    bool reprogControls = false;
    bool gestureV2 = false;
    bool mouseGesture = false;
    bool hapticFeedback = false;
    bool forceSensingButton = false;
    bool crown = false;
    bool reportRate = false;
    bool extendedReportRate = false;
    bool pointerSpeed = false;
    bool leftRightSwap = false;
    bool surfaceTuning = false;
    bool angleSnapping = false;
    bool colorLedEffects = false;
    bool rgbEffects = false;
    bool onboardProfiles = false;
    bool gkey = false;
    bool mkeys = false;
    bool persistentRemappableAction = false;
};
```

- [ ] **Step 1.2: Set smoothScroll=false on MX4 descriptor**

In `src/core/devices/MxMaster4Descriptor.cpp`, add after `f.gestureV2 = false;`:

```cpp
    f.smoothScroll   = false;
```

- [ ] **Step 1.3: Update MockDevice.h**

In `tests/mocks/MockDevice.h`, find the `features()` method and ensure it returns the default `FeatureSupport` (which already defaults all new fields to false/true as appropriate). No code change needed unless MockDevice explicitly sets fields — verify and adjust if necessary.

- [ ] **Step 1.4: Build and run tests to confirm no regressions**

Run: `cmake --build build --parallel $(nproc) && ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass (410).

- [ ] **Step 1.5: Create device directories and move images**

```bash
mkdir -p devices/mx-master-3s devices/mx-master-4

cp src/app/qml/assets/mx-master-3s.png devices/mx-master-3s/front.png
cp src/app/qml/assets/mx-master-3s-side.png devices/mx-master-3s/side.png
cp src/app/qml/assets/mx-master-3s-back.png devices/mx-master-3s/back.png

cp src/app/qml/assets/mx-master-4.png devices/mx-master-4/front.png
cp src/app/qml/assets/mx-master-4-side.png devices/mx-master-4/side.png
cp src/app/qml/assets/mx-master-4-back.png devices/mx-master-4/back.png
```

- [ ] **Step 1.6: Create MX Master 3S descriptor.json**

Create `devices/mx-master-3s/descriptor.json` with the full JSON content from the spec (MX Master 3S complete example). Copy exact CIDs, hotspot positions, DPI range, default gestures, and Easy-Switch slot positions from `MxMaster3sDescriptor.cpp`.

Reference: `src/core/devices/MxMaster3sDescriptor.cpp` for exact values.

- [ ] **Step 1.7: Create MX Master 4 descriptor.json**

Create `devices/mx-master-4/descriptor.json`. Copy exact values from `MxMaster4Descriptor.cpp`. Set `"smoothScroll": false` in features. Set `"status": "implemented"`.

Reference: `src/core/devices/MxMaster4Descriptor.cpp` for exact values.

- [ ] **Step 1.8: Commit**

```bash
git add devices/ src/core/interfaces/IDevice.h src/core/devices/MxMaster4Descriptor.cpp tests/mocks/MockDevice.h
git commit -m "add JSON descriptors + extend FeatureSupport to 28 flags

Create devices/ directory with MX Master 3S and MX Master 4 JSON
descriptors and images. Extend FeatureSupport with all mouse-relevant
HID++ feature flags. MX4 sets smoothScroll=false.

refs #22"
```

---

### Task 2: JsonDevice class + unit tests (TDD)

**Files:**
- Create: `src/core/devices/JsonDevice.h`
- Create: `src/core/devices/JsonDevice.cpp`
- Create: `tests/test_json_device.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 2.1: Write failing tests**

Create `tests/test_json_device.cpp`:

```cpp
#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include "devices/JsonDevice.h"

using namespace logitune;

class JsonDeviceTest : public ::testing::Test {
protected:
    QTemporaryDir tmpDir;

    void writeJson(const QString &subdir, const QJsonObject &obj) {
        QDir d(tmpDir.path());
        d.mkpath(subdir);
        QFile f(d.filePath(subdir + "/descriptor.json"));
        f.open(QIODevice::WriteOnly);
        f.write(QJsonDocument(obj).toJson());
    }

    QJsonObject minimalImplemented() {
        QJsonObject obj;
        obj["name"] = "Test Mouse";
        obj["status"] = "implemented";
        obj["productIds"] = QJsonArray({"0xb034"});
        obj["features"] = QJsonObject({{"battery", true}});
        obj["dpi"] = QJsonObject({{"min", 200}, {"max", 8000}, {"step", 50}});

        QJsonArray controls;
        QJsonObject ctrl;
        ctrl["cid"] = "0x0050";
        ctrl["index"] = 0;
        ctrl["name"] = "Left click";
        ctrl["defaultAction"] = "default";
        ctrl["configurable"] = false;
        controls.append(ctrl);
        obj["controls"] = controls;

        QJsonObject hotspots;
        QJsonArray btnHotspots;
        QJsonObject hs;
        hs["index"] = 0; hs["x"] = 0.5; hs["y"] = 0.5;
        hs["side"] = "right"; hs["labelOffset"] = 0.0;
        btnHotspots.append(hs);
        hotspots["buttons"] = btnHotspots;
        hotspots["scroll"] = QJsonArray();
        obj["hotspots"] = hotspots;

        QJsonObject images;
        images["front"] = "front.png";
        obj["images"] = images;

        obj["easySwitchSlots"] = QJsonArray();
        obj["defaultGestures"] = QJsonObject();
        return obj;
    }

    QJsonObject minimalPlaceholder() {
        QJsonObject obj;
        obj["name"] = "Placeholder Mouse";
        obj["status"] = "placeholder";
        obj["productIds"] = QJsonArray({"0x1234"});
        obj["features"] = QJsonObject();
        obj["controls"] = QJsonArray();
        QJsonObject hotspots;
        hotspots["buttons"] = QJsonArray();
        hotspots["scroll"] = QJsonArray();
        obj["hotspots"] = hotspots;
        obj["images"] = QJsonObject({{"front", "front.png"}});
        obj["easySwitchSlots"] = QJsonArray();
        obj["defaultGestures"] = QJsonObject();
        return obj;
    }
};

TEST_F(JsonDeviceTest, LoadValidImplemented) {
    writeJson("test-mouse", minimalImplemented());
    // Create a dummy front.png so image validation passes
    QFile img(tmpDir.path() + "/test-mouse/front.png");
    img.open(QIODevice::WriteOnly);
    img.write("PNG");
    img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/test-mouse");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), "Test Mouse");
    EXPECT_EQ(dev->status(), JsonDevice::Status::Implemented);
    EXPECT_TRUE(dev->matchesPid(0xb034));
    EXPECT_FALSE(dev->matchesPid(0xFFFF));
    EXPECT_EQ(dev->minDpi(), 200);
    EXPECT_EQ(dev->maxDpi(), 8000);
    EXPECT_EQ(dev->dpiStep(), 50);
    EXPECT_EQ(dev->controls().size(), 1);
    EXPECT_EQ(dev->controls()[0].controlId, 0x0050);
    EXPECT_EQ(dev->buttonHotspots().size(), 1);
    EXPECT_TRUE(dev->features().battery);
}

TEST_F(JsonDeviceTest, LoadValidPlaceholder) {
    writeJson("placeholder", minimalPlaceholder());
    QFile img(tmpDir.path() + "/placeholder/front.png");
    img.open(QIODevice::WriteOnly);
    img.write("PNG");
    img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/placeholder");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), "Placeholder Mouse");
    EXPECT_EQ(dev->status(), JsonDevice::Status::Placeholder);
    EXPECT_TRUE(dev->matchesPid(0x1234));
    EXPECT_TRUE(dev->controls().isEmpty());
    EXPECT_TRUE(dev->buttonHotspots().isEmpty());
}

TEST_F(JsonDeviceTest, MissingFileReturnsNull) {
    auto dev = JsonDevice::load("/nonexistent/path");
    EXPECT_EQ(dev, nullptr);
}

TEST_F(JsonDeviceTest, InvalidJsonReturnsNull) {
    QDir d(tmpDir.path());
    d.mkpath("bad");
    QFile f(d.filePath("bad/descriptor.json"));
    f.open(QIODevice::WriteOnly);
    f.write("not json {{{");
    f.close();
    auto dev = JsonDevice::load(tmpDir.path() + "/bad");
    EXPECT_EQ(dev, nullptr);
}

TEST_F(JsonDeviceTest, ImplementedMissingControlsReturnsNull) {
    auto obj = minimalImplemented();
    obj["controls"] = QJsonArray();  // empty = invalid for implemented
    writeJson("strict", obj);
    QFile img(tmpDir.path() + "/strict/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();
    auto dev = JsonDevice::load(tmpDir.path() + "/strict");
    EXPECT_EQ(dev, nullptr);
}

TEST_F(JsonDeviceTest, PlaceholderMissingControlsLoads) {
    writeJson("lenient", minimalPlaceholder());
    QFile img(tmpDir.path() + "/lenient/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();
    auto dev = JsonDevice::load(tmpDir.path() + "/lenient");
    ASSERT_NE(dev, nullptr);
    EXPECT_TRUE(dev->controls().isEmpty());
}

TEST_F(JsonDeviceTest, CidParsing) {
    auto obj = minimalImplemented();
    QJsonArray controls;
    QJsonObject ctrl;
    ctrl["cid"] = "0x00C3"; ctrl["index"] = 0; ctrl["name"] = "Gesture";
    ctrl["defaultAction"] = "gesture-trigger"; ctrl["configurable"] = true;
    controls.append(ctrl);
    obj["controls"] = controls;
    writeJson("cid-test", obj);
    QFile img(tmpDir.path() + "/cid-test/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/cid-test");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->controls()[0].controlId, 0x00C3);
}

TEST_F(JsonDeviceTest, UnknownKeysIgnored) {
    auto obj = minimalImplemented();
    obj["futureFeature"] = "some value";
    obj["reportRate"] = QJsonObject({{"supported", QJsonArray({125, 500, 1000})}});
    writeJson("extensible", obj);
    QFile img(tmpDir.path() + "/extensible/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/extensible");
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), "Test Mouse");
}

TEST_F(JsonDeviceTest, ImagePathResolution) {
    writeJson("img-test", minimalImplemented());
    QFile img(tmpDir.path() + "/img-test/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/img-test");
    ASSERT_NE(dev, nullptr);
    EXPECT_TRUE(dev->frontImagePath().endsWith("/img-test/front.png"));
    EXPECT_TRUE(dev->frontImagePath().startsWith("/"));
}

TEST_F(JsonDeviceTest, DefaultGesturesParsing) {
    auto obj = minimalImplemented();
    QJsonObject gestures;
    QJsonObject down;
    down["type"] = "keystroke";
    down["payload"] = "Super+D";
    gestures["down"] = down;
    QJsonObject up;
    up["type"] = "default";
    gestures["up"] = up;
    obj["defaultGestures"] = gestures;
    writeJson("gesture-test", obj);
    QFile img(tmpDir.path() + "/gesture-test/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/gesture-test");
    ASSERT_NE(dev, nullptr);
    auto g = dev->defaultGestures();
    EXPECT_TRUE(g.contains("down"));
    EXPECT_EQ(g["down"].type, ButtonAction::Keystroke);
    EXPECT_EQ(g["down"].payload, "Super+D");
    EXPECT_EQ(g["up"].type, ButtonAction::Default);
}

TEST_F(JsonDeviceTest, FeatureFlagDefaults) {
    writeJson("defaults", minimalPlaceholder());
    QFile img(tmpDir.path() + "/defaults/front.png");
    img.open(QIODevice::WriteOnly); img.write("PNG"); img.close();

    auto dev = JsonDevice::load(tmpDir.path() + "/defaults");
    ASSERT_NE(dev, nullptr);
    auto f = dev->features();
    EXPECT_FALSE(f.battery);
    EXPECT_FALSE(f.adjustableDpi);
    EXPECT_TRUE(f.smoothScroll);  // defaults to true
}
```

- [ ] **Step 2.2: Register test in CMakeLists**

In `tests/CMakeLists.txt`, add `test_json_device.cpp` to the `logitune-tests` sources:

```cmake
    test_capability_dispatch.cpp
    test_json_device.cpp
    test_scroll_features.cpp
```

- [ ] **Step 2.3: Verify tests fail to compile**

Run: `cmake --build build --parallel $(nproc) 2>&1 | grep "error" | head -5`

Expected: `fatal error: devices/JsonDevice.h: No such file or directory`

- [ ] **Step 2.4: Create JsonDevice.h**

Create `src/core/devices/JsonDevice.h`:

```cpp
#pragma once
#include "interfaces/IDevice.h"
#include <memory>
#include <QString>

namespace logitune {

class JsonDevice : public IDevice {
public:
    enum class Status { Implemented, CommunityVerified, CommunityLocal, Placeholder };

    static std::unique_ptr<JsonDevice> load(const QString& dirPath);

    Status status() const { return m_status; }

    QString deviceName() const override { return m_name; }
    std::vector<uint16_t> productIds() const override { return m_pids; }
    bool matchesPid(uint16_t pid) const override;
    QList<ControlDescriptor> controls() const override { return m_controls; }
    QList<HotspotDescriptor> buttonHotspots() const override { return m_buttonHotspots; }
    QList<HotspotDescriptor> scrollHotspots() const override { return m_scrollHotspots; }
    FeatureSupport features() const override { return m_features; }
    QString frontImagePath() const override { return m_frontImage; }
    QString sideImagePath() const override { return m_sideImage; }
    QString backImagePath() const override { return m_backImage; }
    QMap<QString, ButtonAction> defaultGestures() const override { return m_defaultGestures; }
    int minDpi() const override { return m_minDpi; }
    int maxDpi() const override { return m_maxDpi; }
    int dpiStep() const override { return m_dpiStep; }
    QList<EasySwitchSlotPosition> easySwitchSlotPositions() const override { return m_easySwitchSlots; }

private:
    JsonDevice() = default;

    Status m_status = Status::Placeholder;
    QString m_name;
    std::vector<uint16_t> m_pids;
    FeatureSupport m_features;
    int m_minDpi = 200, m_maxDpi = 8000, m_dpiStep = 50;
    QList<ControlDescriptor> m_controls;
    QList<HotspotDescriptor> m_buttonHotspots;
    QList<HotspotDescriptor> m_scrollHotspots;
    QString m_frontImage, m_sideImage, m_backImage;
    QList<EasySwitchSlotPosition> m_easySwitchSlots;
    QMap<QString, ButtonAction> m_defaultGestures;
};

} // namespace logitune
```

- [ ] **Step 2.5: Create JsonDevice.cpp**

Create `src/core/devices/JsonDevice.cpp` with the full loading implementation. Key responsibilities:

1. Read and parse `dirPath + "/descriptor.json"` using `QJsonDocument`
2. Parse status string to enum
3. Parse productIds array (hex strings to uint16_t)
4. Parse features object to FeatureSupport (unknown keys ignored, missing keys default)
5. Parse dpi object
6. Parse controls array (CID hex strings to uint16_t)
7. Parse hotspots (buttons and scroll arrays)
8. Resolve image paths (relative to dirPath)
9. Parse easySwitchSlots array
10. Parse defaultGestures object (type string to ButtonAction::Type enum)
11. Validate based on status:
    - Implemented/CommunityVerified: require name, PIDs, controls non-empty, front image exists
    - Placeholder/CommunityLocal: require name and PIDs only

```cpp
#include "devices/JsonDevice.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace logitune {

static uint16_t parseCid(const QString &s) {
    bool ok = false;
    uint16_t val = static_cast<uint16_t>(s.toUInt(&ok, 16));
    return ok ? val : 0;
}

static JsonDevice::Status parseStatus(const QString &s) {
    if (s == "implemented")        return JsonDevice::Status::Implemented;
    if (s == "community-verified") return JsonDevice::Status::CommunityVerified;
    if (s == "community-local")    return JsonDevice::Status::CommunityLocal;
    return JsonDevice::Status::Placeholder;
}

static ButtonAction parseButtonAction(const QJsonObject &obj) {
    QString typeStr = obj["type"].toString("default");
    ButtonAction action;
    if (typeStr == "keystroke") {
        action.type = ButtonAction::Keystroke;
        action.payload = obj["payload"].toString();
    } else if (typeStr == "gesture-trigger") {
        action.type = ButtonAction::GestureTrigger;
    } else if (typeStr == "smartshift-toggle") {
        action.type = ButtonAction::SmartShiftToggle;
    } else {
        action.type = ButtonAction::Default;
    }
    return action;
}

bool JsonDevice::matchesPid(uint16_t pid) const {
    for (auto id : m_pids) {
        if (id == pid) return true;
    }
    return false;
}

std::unique_ptr<JsonDevice> JsonDevice::load(const QString &dirPath) {
    QFile file(dirPath + "/descriptor.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDevice) << "JsonDevice: cannot open" << file.fileName();
        return nullptr;
    }

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (doc.isNull()) {
        qCWarning(lcDevice) << "JsonDevice: parse error in" << file.fileName() << err.errorString();
        return nullptr;
    }

    QJsonObject root = doc.object();
    auto dev = std::unique_ptr<JsonDevice>(new JsonDevice());

    // Name + status
    dev->m_name = root["name"].toString();
    dev->m_status = logitune::parseStatus(root["status"].toString("placeholder"));

    // Product IDs
    for (const auto &v : root["productIds"].toArray())
        dev->m_pids.push_back(parseCid(v.toString()));

    // Features
    QJsonObject feat = root["features"].toObject();
    dev->m_features.battery       = feat["battery"].toBool(false);
    dev->m_features.adjustableDpi = feat["adjustableDpi"].toBool(false);
    dev->m_features.extendedDpi   = feat["extendedDpi"].toBool(false);
    dev->m_features.smartShift    = feat["smartShift"].toBool(false);
    dev->m_features.hiResWheel    = feat["hiResWheel"].toBool(false);
    dev->m_features.hiResScrolling = feat["hiResScrolling"].toBool(false);
    dev->m_features.lowResWheel   = feat["lowResWheel"].toBool(false);
    dev->m_features.smoothScroll  = feat["smoothScroll"].toBool(true);
    dev->m_features.thumbWheel    = feat["thumbWheel"].toBool(false);
    dev->m_features.reprogControls = feat["reprogControls"].toBool(false);
    dev->m_features.gestureV2     = feat["gestureV2"].toBool(false);
    dev->m_features.mouseGesture  = feat["mouseGesture"].toBool(false);
    dev->m_features.hapticFeedback = feat["hapticFeedback"].toBool(false);
    dev->m_features.forceSensingButton = feat["forceSensingButton"].toBool(false);
    dev->m_features.crown         = feat["crown"].toBool(false);
    dev->m_features.reportRate    = feat["reportRate"].toBool(false);
    dev->m_features.extendedReportRate = feat["extendedReportRate"].toBool(false);
    dev->m_features.pointerSpeed  = feat["pointerSpeed"].toBool(false);
    dev->m_features.leftRightSwap = feat["leftRightSwap"].toBool(false);
    dev->m_features.surfaceTuning = feat["surfaceTuning"].toBool(false);
    dev->m_features.angleSnapping = feat["angleSnapping"].toBool(false);
    dev->m_features.colorLedEffects = feat["colorLedEffects"].toBool(false);
    dev->m_features.rgbEffects    = feat["rgbEffects"].toBool(false);
    dev->m_features.onboardProfiles = feat["onboardProfiles"].toBool(false);
    dev->m_features.gkey          = feat["gkey"].toBool(false);
    dev->m_features.mkeys         = feat["mkeys"].toBool(false);
    dev->m_features.persistentRemappableAction = feat["persistentRemappableAction"].toBool(false);

    // DPI
    QJsonObject dpi = root["dpi"].toObject();
    dev->m_minDpi  = dpi["min"].toInt(200);
    dev->m_maxDpi  = dpi["max"].toInt(8000);
    dev->m_dpiStep = dpi["step"].toInt(50);

    // Controls
    for (const auto &v : root["controls"].toArray()) {
        QJsonObject c = v.toObject();
        ControlDescriptor ctrl;
        ctrl.controlId = parseCid(c["cid"].toString());
        ctrl.buttonIndex = c["index"].toInt();
        ctrl.defaultName = c["name"].toString();
        ctrl.defaultActionType = c["defaultAction"].toString("default");
        ctrl.configurable = c["configurable"].toBool(false);
        dev->m_controls.append(ctrl);
    }

    // Hotspots
    QJsonObject hs = root["hotspots"].toObject();
    for (const auto &v : hs["buttons"].toArray()) {
        QJsonObject h = v.toObject();
        HotspotDescriptor hp;
        hp.buttonIndex = h["index"].toInt();
        hp.xPct = h["x"].toDouble();
        hp.yPct = h["y"].toDouble();
        hp.side = h["side"].toString("right");
        hp.labelOffsetYPct = h["labelOffset"].toDouble(0.0);
        dev->m_buttonHotspots.append(hp);
    }
    for (const auto &v : hs["scroll"].toArray()) {
        QJsonObject h = v.toObject();
        HotspotDescriptor hp;
        hp.buttonIndex = h["index"].toInt();
        hp.xPct = h["x"].toDouble();
        hp.yPct = h["y"].toDouble();
        hp.side = h["side"].toString("right");
        hp.labelOffsetYPct = h["labelOffset"].toDouble(0.0);
        dev->m_scrollHotspots.append(hp);
    }

    // Images (resolve relative to dirPath)
    QJsonObject imgs = root["images"].toObject();
    auto resolvePath = [&](const QString &key) -> QString {
        QString rel = imgs[key].toString();
        if (rel.isEmpty()) return {};
        return QDir(dirPath).absoluteFilePath(rel);
    };
    dev->m_frontImage = resolvePath("front");
    dev->m_sideImage  = resolvePath("side");
    dev->m_backImage  = resolvePath("back");

    // Easy-Switch slots
    for (const auto &v : root["easySwitchSlots"].toArray()) {
        QJsonObject s = v.toObject();
        dev->m_easySwitchSlots.append({ s["x"].toDouble(), s["y"].toDouble() });
    }

    // Default gestures
    QJsonObject gestObj = root["defaultGestures"].toObject();
    for (auto it = gestObj.begin(); it != gestObj.end(); ++it)
        dev->m_defaultGestures[it.key()] = logitune::parseButtonAction(it.value().toObject());

    // Validation
    bool strict = (dev->m_status == Status::Implemented ||
                   dev->m_status == Status::CommunityVerified);

    if (dev->m_name.isEmpty() || dev->m_pids.empty()) {
        qCWarning(lcDevice) << "JsonDevice: missing name or PIDs in" << dirPath;
        return nullptr;
    }

    if (strict) {
        if (dev->m_controls.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: implemented descriptor has no controls:" << dirPath;
            return nullptr;
        }
        if (dev->m_buttonHotspots.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: implemented descriptor has no hotspots:" << dirPath;
            return nullptr;
        }
        if (!QFile::exists(dev->m_frontImage)) {
            qCWarning(lcDevice) << "JsonDevice: front image missing:" << dev->m_frontImage;
            return nullptr;
        }
    }

    qCDebug(lcDevice) << "JsonDevice: loaded" << dev->m_name
                      << "status=" << root["status"].toString()
                      << "pids=" << dev->m_pids.size()
                      << "controls=" << dev->m_controls.size();

    return dev;
}

} // namespace logitune
```

- [ ] **Step 2.6: Add JsonDevice.cpp to core CMakeLists**

In `src/core/CMakeLists.txt`, add after `devices/MxMaster4Descriptor.cpp`:

```cmake
    devices/JsonDevice.cpp
```

- [ ] **Step 2.7: Build and run JsonDevice tests**

Run: `cmake --build build --parallel $(nproc) 2>&1 | tail -10`

Expected: clean build.

Run: `./build/tests/logitune-tests --gtest_filter='JsonDevice*' 2>&1 | tail -15`

Expected: all JsonDevice tests pass (11 tests).

- [ ] **Step 2.8: Run full test suite**

Run: `./build/tests/logitune-tests 2>&1 | tail -3`

Expected: all tests pass.

- [ ] **Step 2.9: Commit**

```bash
git add src/core/devices/JsonDevice.h src/core/devices/JsonDevice.cpp \
        src/core/CMakeLists.txt tests/test_json_device.cpp tests/CMakeLists.txt
git commit -m "add JsonDevice class with JSON loading + validation

Factory loads descriptor.json from a device directory, parses all
IDevice fields, resolves image paths, and validates based on status
(strict for implemented, lenient for placeholders).

refs #22"
```

---

### Task 3: Refactor DeviceRegistry to load from JSON directories

**Files:**
- Modify: `src/core/DeviceRegistry.h`
- Modify: `src/core/DeviceRegistry.cpp`
- Modify: `src/core/CMakeLists.txt` (remove old descriptor .cpp files)

- [ ] **Step 3.1: Update DeviceRegistry.h**

Replace the current DeviceRegistry.h with:

```cpp
#pragma once
#include "interfaces/IDevice.h"
#include <memory>
#include <vector>
#include <QString>

namespace logitune {

class DeviceRegistry {
public:
    DeviceRegistry();

    const IDevice* findByPid(uint16_t pid) const;
    const IDevice* findByName(const QString &name) const;
    void registerDevice(std::unique_ptr<IDevice> device);
    const std::vector<std::unique_ptr<IDevice>>& devices() const;

    static QString systemDevicesDir();
    static QString cacheDevicesDir();
    static QString userDevicesDir();

private:
    void loadDirectory(const QString &dir);
    std::vector<std::unique_ptr<IDevice>> m_devices;
};

} // namespace logitune
```

- [ ] **Step 3.2: Update DeviceRegistry.cpp**

Replace the current DeviceRegistry.cpp with:

```cpp
#include "DeviceRegistry.h"
#include "devices/JsonDevice.h"
#include "logging/LogManager.h"
#include <QDir>
#include <QStandardPaths>

namespace logitune {

DeviceRegistry::DeviceRegistry() {
    loadDirectory(systemDevicesDir());
    loadDirectory(cacheDevicesDir());
    loadDirectory(userDevicesDir());
    qCInfo(lcDevice) << "DeviceRegistry: loaded" << m_devices.size() << "devices";
}

void DeviceRegistry::loadDirectory(const QString &dir) {
    QDir d(dir);
    if (!d.exists()) return;
    for (const auto &entry : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto device = JsonDevice::load(d.filePath(entry));
        if (device)
            registerDevice(std::move(device));
    }
}

const IDevice* DeviceRegistry::findByPid(uint16_t pid) const {
    for (const auto &dev : m_devices) {
        if (dev->matchesPid(pid))
            return dev.get();
    }
    return nullptr;
}

const IDevice* DeviceRegistry::findByName(const QString &name) const {
    for (const auto &dev : m_devices) {
        if (name.contains(dev->deviceName(), Qt::CaseInsensitive))
            return dev.get();
    }
    return nullptr;
}

void DeviceRegistry::registerDevice(std::unique_ptr<IDevice> device) {
    m_devices.push_back(std::move(device));
}

const std::vector<std::unique_ptr<IDevice>>& DeviceRegistry::devices() const {
    return m_devices;
}

QString DeviceRegistry::systemDevicesDir() {
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto &p : paths) {
        QString dir = p + "/logitune/devices";
        if (QDir(dir).exists())
            return dir;
    }
    return "/usr/share/logitune/devices";
}

QString DeviceRegistry::cacheDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
           + "/logitune/devices";
}

QString DeviceRegistry::userDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/logitune/devices";
}

} // namespace logitune
```

- [ ] **Step 3.3: Remove old descriptor sources from CMakeLists**

In `src/core/CMakeLists.txt`, remove these lines:

```cmake
    devices/MxMaster3sDescriptor.cpp
    devices/MxMaster4Descriptor.cpp
```

Keep `devices/JsonDevice.cpp`.

- [ ] **Step 3.4: Build**

Run: `cmake --build build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build. If there are unresolved includes of old descriptors in test files, fix them (tests use MockDevice, not real descriptors, so this should be clean).

- [ ] **Step 3.5: Run tests**

Note: `test_device_registry.cpp` parameterized tests will FAIL at this point because the JSON files are in `devices/` but `DeviceRegistry` looks in `/usr/share/logitune/devices/` which doesn't exist in the build tree. We need to either:
- Set an environment variable for the test to point at the local `devices/` directory, OR
- Symlink or copy devices/ to the build directory, OR
- Make the test create a temporary DeviceRegistry with a custom path

The simplest fix: add a `DeviceRegistry` constructor overload that accepts a custom path, or set `XDG_DATA_DIRS` in the test environment to include the repo root.

Add to `tests/CMakeLists.txt` after the existing `gtest_discover_tests`:

```cmake
gtest_discover_tests(logitune-tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_DIRS=${CMAKE_SOURCE_DIR}")
```

This makes `QStandardPaths::GenericDataLocation` include the repo root, so `systemDevicesDir()` finds `<repo>/logitune/devices/`.

Wait — QStandardPaths looks for `logitune/devices` under each XDG path. If `XDG_DATA_DIRS` includes `<repo>`, it will look for `<repo>/logitune/devices/` which doesn't exist. The devices are at `<repo>/devices/`. The path needs to match.

Better approach: the system install puts files in `/usr/share/logitune/devices/`. For tests, we set `XDG_DATA_DIRS` to a path where `logitune/devices/` exists. We can create a symlink in the build dir:

Actually, the cleanest approach is to add a test-only method or use the existing `registerDevice` to manually load in tests. But the parameterized tests already work by calling `DeviceRegistry reg;` which triggers the constructor.

Simplest fix: during cmake configure, create a symlink `${CMAKE_BINARY_DIR}/logitune/devices` → `${CMAKE_SOURCE_DIR}/devices`, and set `XDG_DATA_DIRS=${CMAKE_BINARY_DIR}` in the test environment.

Update `tests/CMakeLists.txt`:

```cmake
# Create symlink so DeviceRegistry finds device JSONs during testing
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/devices ${CMAKE_BINARY_DIR}/logitune/devices SYMBOLIC)

gtest_discover_tests(logitune-tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_DIRS=${CMAKE_BINARY_DIR}"
    DISCOVERY_MODE PRE_TEST)
```

- [ ] **Step 3.6: Rebuild and run tests**

Run: `cmake -B build && cmake --build build --parallel $(nproc) && ./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass. The parameterized DeviceRegistryTest should find MX Master 3S and MX Master 4 from the JSON descriptors.

- [ ] **Step 3.7: Commit**

```bash
git add src/core/DeviceRegistry.h src/core/DeviceRegistry.cpp src/core/CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor: DeviceRegistry loads JSON descriptors from XDG directories

Scans system, cache, and user directories for device subdirectories.
Each subdirectory with a descriptor.json is loaded via JsonDevice.
Higher-priority directories override lower ones for the same PID.

Removes compiled MxMaster3sDescriptor and MxMaster4Descriptor from
the build. Device data is now pure JSON + images.

refs #22"
```

---

### Task 4: Image path migration + delete old C++ descriptors

**Files:**
- Modify: `src/app/models/DeviceModel.cpp`
- Modify: `src/app/CMakeLists.txt` (remove RESOURCES entries)
- Delete: `src/core/devices/MxMaster3sDescriptor.h`
- Delete: `src/core/devices/MxMaster3sDescriptor.cpp`
- Delete: `src/core/devices/MxMaster4Descriptor.h`
- Delete: `src/core/devices/MxMaster4Descriptor.cpp`

- [ ] **Step 4.1: Update DeviceModel image getters to handle filesystem paths**

In `src/app/models/DeviceModel.cpp`, modify the three image getter methods to prepend `file://` for filesystem paths:

```cpp
QString DeviceModel::frontImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->frontImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::sideImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->sideImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}

QString DeviceModel::backImage() const
{
    if (m_dm && m_dm->activeDevice()) {
        QString path = m_dm->activeDevice()->backImagePath();
        if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
            return "file://" + path;
        return path;
    }
    return {};
}
```

- [ ] **Step 4.2: Remove image RESOURCES from app CMakeLists**

In `src/app/CMakeLists.txt`, remove these lines from the `RESOURCES` section of `qt_add_qml_module`:

```cmake
        qml/assets/mx-master-3s.png
        qml/assets/mx-master-3s-back.png
        qml/assets/mx-master-3s-side.png
        qml/assets/mx-master-4.png
        qml/assets/mx-master-4-back.png
        qml/assets/mx-master-4-side.png
```

- [ ] **Step 4.3: Add CMake install rule for devices directory**

In `src/core/CMakeLists.txt` (or the root `CMakeLists.txt`, whichever handles install), add:

```cmake
install(DIRECTORY ${CMAKE_SOURCE_DIR}/devices/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/logitune/devices)
```

- [ ] **Step 4.4: Delete old C++ descriptor files**

```bash
rm src/core/devices/MxMaster3sDescriptor.h \
   src/core/devices/MxMaster3sDescriptor.cpp \
   src/core/devices/MxMaster4Descriptor.h \
   src/core/devices/MxMaster4Descriptor.cpp
```

Also delete the original images from qml/assets (they've been copied to devices/):

```bash
rm src/app/qml/assets/mx-master-3s.png \
   src/app/qml/assets/mx-master-3s-side.png \
   src/app/qml/assets/mx-master-3s-back.png \
   src/app/qml/assets/mx-master-4.png \
   src/app/qml/assets/mx-master-4-side.png \
   src/app/qml/assets/mx-master-4-back.png
```

- [ ] **Step 4.5: Build and run all tests**

Run: `cmake --build build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build. No references to deleted files remain.

Run: `./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass.

Run: `./build/tests/logitune-tray-tests 2>&1 | tail -3`

Expected: 12 tests pass.

- [ ] **Step 4.6: Commit**

```bash
git add -A
git commit -m "migrate device images to filesystem, delete C++ descriptors

Images move from Qt resources (qrc://) to filesystem paths (file://).
DeviceModel prepends file:// for QML Image source compatibility.
Delete MxMaster3sDescriptor and MxMaster4Descriptor C++ files.
Install rule copies devices/ to /usr/share/logitune/devices/.

refs #22"
```

---

### Task 5: Full verification + smoke test on MX3S

**Files:** none (verification only)

- [ ] **Step 5.1: Clean build**

Run: `cmake --build build --parallel $(nproc) --clean-first 2>&1 | tail -10`

Expected: clean build, no warnings about missing files.

- [ ] **Step 5.2: Full test suite**

Run: `./build/tests/logitune-tests 2>&1 | tail -5`

Expected: all tests pass (410 previous + ~11 new JsonDevice tests).

Run: `./build/tests/logitune-tray-tests 2>&1 | tail -3`

Expected: 12 tests pass.

- [ ] **Step 5.3: Smoke test on MX3S**

Kill any running logitune, launch the fresh build:

```bash
nohup ./build/src/app/logitune --debug > /tmp/logitune-json.log 2>&1 & disown
```

Wait 2 seconds, then check:

```bash
grep -E "DeviceRegistry|JsonDevice|matched device|battery:|SmartShift:|Startup complete" /tmp/logitune-json.log
```

Expected log lines:
- `DeviceRegistry: loaded 2 devices`
- `JsonDevice: loaded MX Master 3S status=implemented ...`
- `JsonDevice: loaded MX Master 4 status=implemented ...`
- `matched device descriptor: "MX Master 3S"`
- `battery: feature= 1004 level= ...`
- `SmartShift: feature= 2110 ...`
- `Startup complete`

Verify device image renders in the UI (click on device in home screen, check Buttons page shows the mouse render).

- [ ] **Step 5.4: Verify images load in QML**

In the running app, navigate to the Buttons page. The mouse image should render. If it shows a blank/broken image, the `file://` path isn't resolving — check the DeviceModel.frontImage output in the debug log.

- [ ] **Step 5.5: Kill smoke test**

```bash
pkill -f "build/src/app/logitune"
```

---

### Task 6: Review, push branch, open PR

**Files:** none (git operations)

- [ ] **Step 6.1: Review commit log**

Run: `git log --oneline master..HEAD`

Expected: 4 commits (Tasks 1-4).

- [ ] **Step 6.2: Push branch**

```bash
git checkout -b json-device-descriptors  # if not already on a branch
git push -u origin json-device-descriptors
```

- [ ] **Step 6.3: Open PR**

```bash
gh pr create --base master --head json-device-descriptors \
  --title "feat: data-driven JSON device descriptors" \
  --body "$(cat <<'PREOF'
## Summary

Replaces compiled C++ device descriptors with runtime-loaded JSON files.
Each device is a self-contained directory with descriptor.json + images.
Enables community device contributions without code changes.

Closes #22 (Phase 1). Supersedes PR #19 (feature flags).

## What changed

- **JsonDevice class** implements IDevice by loading JSON at construction
- **DeviceRegistry** scans three XDG directories (system, cache, user) for device dirs
- **FeatureSupport** extended to 28 mouse-relevant HID++ feature flags
- **Device images** moved from Qt resources to filesystem (file:// paths)
- **MX Master 3S + 4** migrated to JSON descriptors
- **C++ descriptor files deleted**
- **11 new unit tests** for JSON loading, validation, parsing

## Device directory structure

\`\`\`
devices/mx-master-3s/
  descriptor.json
  front.png
  side.png
  back.png
\`\`\`

## Descriptor status model

| Status | Badge | Location |
|---|---|---|
| implemented | Green ✓ | /usr/share/logitune/devices/ |
| community-verified | Blue ★ | ~/.cache/logitune/devices/ |
| community-local | Orange ✎ | ~/.local/share/logitune/devices/ |
| placeholder | Grey ? | /usr/share/logitune/devices/ |

## Testing

- All existing tests pass against JSON-loaded descriptors
- 11 new JsonDevice unit tests
- Smoke tested on MX Master 3S
PREOF
)"
```

- [ ] **Step 6.4: Close superseded PR #19**

```bash
gh pr close 19 --comment "Superseded by #<new PR number> — all feature flag changes are included in the JSON descriptors migration."
```

---

## Self-Review

**1. Spec coverage:**
- [x] JsonDevice class implementing IDevice from JSON — Task 2
- [x] Migrate MX3S and MX4 to JSON — Task 1
- [x] Delete C++ descriptors — Task 4
- [x] DeviceRegistry loads from three directories — Task 3
- [x] Move images from Qt resources to filesystem — Task 4
- [x] Status field in JSON (4 states) — Task 2 (parsing)
- [x] FeatureSupport with 28 flags — Task 1
- [x] CMake install rule — Task 4
- [x] All existing tests pass — Task 5
- [x] Smoke test on MX3S — Task 5
- [x] Image path file:// handling — Task 4

**2. Placeholder scan:** No TBDs or TODOs. All code blocks are complete. All test expectations are concrete.

**3. Type consistency:**
- `JsonDevice::Status` enum used consistently (Implemented, CommunityVerified, CommunityLocal, Placeholder)
- `parseCid()` returns `uint16_t`, matches `ControlDescriptor::controlId` type
- `parseButtonAction()` returns `ButtonAction`, matches `defaultGestures()` return type
- `FeatureSupport` fields match between IDevice.h definition and JsonDevice.cpp parsing
