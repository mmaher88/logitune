# Data-Driven JSON Device Descriptors — Design Spec

**Goal:** Replace compiled C++ device descriptors with runtime-loaded JSON files, enabling community device contributions without code changes.

**Scope:** Phase 1 of the device onboarding wizard (issue #22). Covers JSON format, loading infrastructure, and migration of existing descriptors. Does NOT cover the wizard UI, community database fetching, Options+ extraction, or carousel rendering.

**Dependencies:** None. This is the prerequisite for multi-device (#21), wizard (Phase 4), and community database (Phase 5).

---

## Architecture

`DeviceRegistry` loads device descriptors from JSON files on disk instead of compiled C++ classes. Each device is a self-contained directory with a `descriptor.json` and image files. A `JsonDevice` class implements the existing `IDevice` interface by reading JSON into member variables at construction time.

Three source directories are scanned in priority order (higher overrides lower):

```
1. ~/.local/share/logitune/devices/          (user — wizard output, community-local)
2. ~/.cache/logitune/devices/                (cache — community-verified, fetched from GitHub)
3. /usr/share/logitune/devices/              (system — implemented + placeholders, shipped)
```

If the same PID appears in multiple directories, the highest-priority source wins.

---

## Device Directory Structure

Each device is a directory containing a descriptor and images:

```
/usr/share/logitune/devices/
  mx-master-3s/
    descriptor.json
    front.png
    side.png
    back.png
  mx-master-4/
    descriptor.json
    front.png
    side.png
    back.png
```

Image paths in the JSON are relative to the descriptor directory. The `JsonDevice::load()` factory resolves them to absolute paths at load time.

---

## Descriptor Status Model

Each descriptor has a `status` field indicating its lifecycle stage:

| Status | Where it lives | How it gets there | Visual badge |
|---|---|---|---|
| `implemented` | `/usr/share/logitune/devices/` | Shipped with binary, fully tested | Green ✓ |
| `community-verified` | `~/.cache/logitune/devices/` | Merged into logitune-devices GitHub repo | Blue ★ |
| `community-local` | `~/.local/share/logitune/devices/` | User ran the wizard | Orange ✎ |
| `placeholder` | `/usr/share/logitune/devices/` | Extracted from Options+/Solaar, unverified | Grey ? |

Badge rendering is deferred to Phase 3 (carousel). Phase 1 only stores and exposes the status value.

---

## JSON Schema

### Complete example (MX Master 3S)

```json
{
  "name": "MX Master 3S",
  "status": "implemented",
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
    { "cid": "0x0050", "index": 0, "name": "Left click", "defaultAction": "default", "configurable": false },
    { "cid": "0x0051", "index": 1, "name": "Right click", "defaultAction": "default", "configurable": false },
    { "cid": "0x0052", "index": 2, "name": "Middle click", "defaultAction": "default", "configurable": true },
    { "cid": "0x0053", "index": 3, "name": "Back", "defaultAction": "default", "configurable": true },
    { "cid": "0x0056", "index": 4, "name": "Forward", "defaultAction": "default", "configurable": true },
    { "cid": "0x00C3", "index": 5, "name": "Gesture button", "defaultAction": "gesture-trigger", "configurable": true },
    { "cid": "0x00C4", "index": 6, "name": "Shift wheel mode", "defaultAction": "smartshift-toggle", "configurable": true },
    { "cid": "0x0000", "index": 7, "name": "Thumb wheel", "defaultAction": "default", "configurable": true }
  ],

  "hotspots": {
    "buttons": [
      { "index": 2, "x": 0.71, "y": 0.15, "side": "right", "labelOffset": 0.0 },
      { "index": 6, "x": 0.81, "y": 0.34, "side": "right", "labelOffset": 0.0 },
      { "index": 7, "x": 0.55, "y": 0.515, "side": "right", "labelOffset": 0.0 },
      { "index": 4, "x": 0.35, "y": 0.43, "side": "left", "labelOffset": 0.0 },
      { "index": 3, "x": 0.45, "y": 0.60, "side": "left", "labelOffset": 0.20 },
      { "index": 5, "x": 0.08, "y": 0.58, "side": "left", "labelOffset": 0.0 }
    ],
    "scroll": [
      { "index": -1, "x": 0.73, "y": 0.16, "side": "right", "labelOffset": 0.0 },
      { "index": -2, "x": 0.50, "y": 0.51, "side": "left", "labelOffset": 0.0 },
      { "index": -3, "x": 0.82, "y": 0.34, "side": "right", "labelOffset": 0.0 }
    ]
  },

  "images": {
    "front": "front.png",
    "side": "side.png",
    "back": "back.png"
  },

  "easySwitchSlots": [
    { "x": 0.325, "y": 0.658 },
    { "x": 0.384, "y": 0.642 },
    { "x": 0.443, "y": 0.643 }
  ],

  "defaultGestures": {
    "up": { "type": "default" },
    "down": { "type": "keystroke", "payload": "Super+D" },
    "left": { "type": "keystroke", "payload": "Meta+Ctrl+Right" },
    "right": { "type": "keystroke", "payload": "Meta+Ctrl+Left" },
    "click": { "type": "keystroke", "payload": "Super+Tab" }
  }
}
```

### Feature-specific config sections

These top-level keys are only read when the corresponding feature flag is `true`:

```json
{
  "dpiStages": [400, 800, 1600, 3200, 6400],

  "reportRate": {
    "supported": [125, 250, 500, 1000],
    "default": 1000
  },

  "rgbZones": [
    { "id": "logo", "name": "Logo LED" },
    { "id": "wheel", "name": "Scroll wheel LED" },
    { "id": "side", "name": "Side strip" }
  ],

  "crown": {
    "hasRatchet": true,
    "defaultMode": "scroll"
  }
}
```

### Placeholder example (minimal)

```json
{
  "name": "M720 Triathlon",
  "status": "placeholder",
  "productIds": ["0x405e"],
  "features": {
    "battery": true,
    "adjustableDpi": true,
    "reprogControls": true
  },
  "dpi": { "min": 200, "max": 4000, "step": 50 },
  "controls": [
    { "cid": "0x0050", "index": 0, "name": "Left click", "defaultAction": "default", "configurable": false },
    { "cid": "0x0051", "index": 1, "name": "Right click", "defaultAction": "default", "configurable": false }
  ],
  "hotspots": { "buttons": [], "scroll": [] },
  "images": { "front": "front.png", "side": "side.png" },
  "easySwitchSlots": [],
  "defaultGestures": {}
}
```

### Extensibility rule

Unknown top-level keys are silently ignored. When the app adds support for a new feature, it starts reading the corresponding config section. Old app versions skip it. No schema migration needed.

The `features` flags tell the app what to show in the UI. If a flag exists but the app doesn't have UI for it yet, the flag is ignored and no control is rendered.

---

## JsonDevice Class

```cpp
class JsonDevice : public IDevice {
public:
    enum class Status { Implemented, CommunityVerified, CommunityLocal, Placeholder };

    // Factory — returns nullptr on fatal error (missing file, invalid JSON).
    // Validation: strict for Implemented/CommunityVerified, lenient for Placeholder/CommunityLocal.
    static std::unique_ptr<JsonDevice> load(const QString& dirPath);

    Status status() const;

    // All IDevice methods implemented by returning stored members.
    QString deviceName() const override;
    std::vector<uint16_t> productIds() const override;
    bool matchesPid(uint16_t pid) const override;
    QList<ControlDescriptor> controls() const override;
    QList<HotspotDescriptor> buttonHotspots() const override;
    QList<HotspotDescriptor> scrollHotspots() const override;
    FeatureSupport features() const override;
    QString frontImagePath() const override;
    QString sideImagePath() const override;
    QString backImagePath() const override;
    QMap<QString, ButtonAction> defaultGestures() const override;
    int minDpi() const override;
    int maxDpi() const override;
    int dpiStep() const override;
    QList<EasySwitchSlotPosition> easySwitchSlotPositions() const override;

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
```

### Loading and validation

`JsonDevice::load(dirPath)`:

1. Read `dirPath + "/descriptor.json"`, parse with `QJsonDocument`
2. Parse `status` string → `Status` enum
3. Parse all fields into members
4. Resolve image paths: `"front.png"` → `dirPath + "/front.png"`
5. Parse CID strings: `"0x0050"` → `uint16_t(0x0050)`
6. Validate based on status:
   - `Implemented` / `CommunityVerified`: require non-empty `name`, at least one PID, at least one control, at least one button hotspot, front image file must exist on disk
   - `Placeholder` / `CommunityLocal`: require non-empty `name` and at least one PID. Everything else can be empty/default.
7. Log warnings for missing optional fields (regardless of status)
8. Return `nullptr` if the file doesn't exist, isn't valid JSON, or fails strict validation

### FeatureSupport mapping

The JSON `features` object maps to the existing `FeatureSupport` struct. Known keys are read; unknown keys are ignored. Missing keys default to `false`.

```cpp
FeatureSupport fs;
fs.battery       = obj["battery"].toBool(false);
fs.adjustableDpi = obj["adjustableDpi"].toBool(false);
fs.smartShift    = obj["smartShift"].toBool(false);
// ... etc for all 28 feature flags
```

The `FeatureSupport` struct gains new boolean members for features not yet implemented in the app. These are stored but not acted on until the app has UI/logic for them.

---

## DeviceRegistry Changes

### Current state

```cpp
DeviceRegistry::DeviceRegistry() {
    registerDevice(std::make_unique<MxMaster3sDescriptor>());
    registerDevice(std::make_unique<MxMaster4Descriptor>());
}
```

### New state

```cpp
DeviceRegistry::DeviceRegistry() {
    // Load from all three directories, in reverse priority order
    // (later loads override earlier ones for the same PID)
    loadDirectory(systemDevicesDir());   // /usr/share/logitune/devices/
    loadDirectory(cacheDevicesDir());    // ~/.cache/logitune/devices/
    loadDirectory(userDevicesDir());     // ~/.local/share/logitune/devices/
}

void DeviceRegistry::loadDirectory(const QString& dir) {
    QDir d(dir);
    if (!d.exists()) return;
    for (const auto& entry : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto device = JsonDevice::load(d.filePath(entry));
        if (device)
            registerDevice(std::move(device));
    }
}
```

`registerDevice` overwrites any existing PID entry, so higher-priority directories win.

### Directory resolution

```cpp
QString DeviceRegistry::systemDevicesDir() {
    // Follows XDG: /usr/share/logitune/devices/ or /usr/local/share/logitune/devices/
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  "logitune/devices", QStandardPaths::LocateDirectory);
}

QString DeviceRegistry::userDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/logitune/devices";
}

QString DeviceRegistry::cacheDevicesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
           + "/logitune/devices";
}
```

---

## Image Path Migration

### Current state

Images are compiled into the Qt resource system:
```cpp
return QStringLiteral("qrc:/Logitune/qml/assets/mx-master-3s.png");
```

QML loads them via `source: DeviceModel.frontImage` which resolves `qrc://` URIs.

### New state

Images are on the filesystem:
```cpp
return m_frontImage; // "/usr/share/logitune/devices/mx-master-3s/front.png"
```

QML loads filesystem paths via `file://` URI. The `DeviceModel` getter prepends `file://` if the path doesn't already have a scheme:

```cpp
QString DeviceModel::frontImage() const {
    QString path = /* ... get from active device ... */;
    if (!path.isEmpty() && !path.startsWith("qrc:") && !path.startsWith("file:"))
        return "file://" + path;
    return path;
}
```

### Migration checklist

- Move `src/app/qml/assets/mx-master-3s*.png` to `devices/mx-master-3s/`
- Move `src/app/qml/assets/mx-master-4*.png` to `devices/mx-master-4/`
- Remove image entries from `qt_add_qml_module` RESOURCES in CMakeLists
- Add `install(DIRECTORY devices/ DESTINATION share/logitune/devices/)` to CMakeLists
- Update DeviceModel image getters to handle filesystem paths

---

## CMake / Packaging Changes

### Install rule

```cmake
# Install device descriptor directories
install(DIRECTORY ${CMAKE_SOURCE_DIR}/devices/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/logitune/devices)
```

### Build-time validation (optional, nice-to-have)

A CMake custom command that validates all JSON descriptors at build time:
```cmake
add_custom_target(validate-descriptors
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/validate-descriptors.py ${CMAKE_SOURCE_DIR}/devices/
    COMMENT "Validating device descriptors"
)
```

---

## Files Changed

### New files

- `src/core/devices/JsonDevice.h` — `JsonDevice` class declaration
- `src/core/devices/JsonDevice.cpp` — JSON loading + validation implementation
- `devices/mx-master-3s/descriptor.json` — MX Master 3S descriptor
- `devices/mx-master-3s/front.png` — moved from `src/app/qml/assets/`
- `devices/mx-master-3s/side.png` — moved
- `devices/mx-master-3s/back.png` — moved
- `devices/mx-master-4/descriptor.json` — MX Master 4 descriptor
- `devices/mx-master-4/front.png` — moved
- `devices/mx-master-4/side.png` — moved
- `devices/mx-master-4/back.png` — moved
- `tests/test_json_device.cpp` — unit tests for JSON loading + validation

### Modified files

- `src/core/DeviceRegistry.h` — add `loadDirectory()`, remove compiled descriptor includes
- `src/core/DeviceRegistry.cpp` — replace constructor with directory scanning
- `src/core/CMakeLists.txt` — remove compiled descriptor .cpp files, add JsonDevice.cpp
- `src/app/CMakeLists.txt` — remove image RESOURCES entries, add install rule
- `src/app/models/DeviceModel.cpp` — prepend `file://` to image paths
- `src/core/interfaces/IDevice.h` — extend `FeatureSupport` with all 28 mouse feature flags
- `tests/CMakeLists.txt` — add test_json_device.cpp

### Deleted files

- `src/core/devices/MxMaster3sDescriptor.h`
- `src/core/devices/MxMaster3sDescriptor.cpp`
- `src/core/devices/MxMaster4Descriptor.h`
- `src/core/devices/MxMaster4Descriptor.cpp`

---

## Testing Strategy

### Unit tests (test_json_device.cpp)

- Load a valid implemented descriptor → all fields populated correctly
- Load a valid placeholder with missing fields → defaults applied, no crash
- Load a missing file → returns nullptr
- Load invalid JSON → returns nullptr
- Load implemented descriptor with missing required fields → returns nullptr (strict validation)
- Load placeholder with missing required fields → loads successfully (lenient)
- CID string parsing: "0x0050" → 0x0050, "0xC3" → 0x00C3
- Image path resolution: "front.png" → absolute path
- PID matching: matchesPid works for all listed PIDs
- Unknown JSON keys are silently ignored

### Existing test validation

- All `test_device_registry.cpp` parameterized tests must pass against JSON-loaded descriptors
- All `test_app_controller.cpp` tests must pass (they use MockDevice, not real descriptors, but the DeviceModel path must work)

### Smoke test

- Build and launch with MX Master 3S connected
- Battery reads correctly
- SmartShift toggles correctly
- Button diversion works
- Device name shows "MX Master 3S"
- Device images render correctly in the UI

---

## Out of Scope

- Options+ installer extraction (Phase 2)
- Greyed-out placeholder rendering and carousel (Phase 3)
- Status corner badges (Phase 3)
- Device onboarding wizard (Phase 4)
- Community database fetching from GitHub (Phase 5)
- Multi-device support (issue #21)
- Adding new feature flags to FeatureSupport beyond the 28 defined here (done as-needed when UI is built for them)
