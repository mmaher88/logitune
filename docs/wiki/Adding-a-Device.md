# Adding a Device

This guide walks through adding support for a new Logitech HID++ 2.0 device to Logitune. We use the MX Master 3S descriptor as the reference implementation and work through a hypothetical "MX Anywhere 3S" as a complete example.

## Prerequisites

Before starting, you need:

- The device's **Product ID (PID)** — from `lsusb`, `/sys/class/hidraw/*/device/`, or Solaar
- The device's **Control IDs (CIDs)** — from ReprogControlsV4 feature enumeration
- The device's **DPI range** — from AdjustableDPI feature
- The device's **supported features** — from feature enumeration
- **Device images** — front, side, and back photos/renders (PNG)

## Step 1: Understand the IDevice Interface

Every device descriptor implements `IDevice` (defined in `src/core/interfaces/IDevice.h`):

```cpp
class IDevice {
public:
    virtual ~IDevice() = default;

    virtual QString deviceName() const = 0;
    virtual std::vector<uint16_t> productIds() const = 0;
    virtual bool matchesPid(uint16_t pid) const = 0;
    virtual QList<ControlDescriptor> controls() const = 0;
    virtual QList<HotspotDescriptor> buttonHotspots() const = 0;
    virtual QList<HotspotDescriptor> scrollHotspots() const = 0;
    virtual FeatureSupport features() const = 0;
    virtual QString frontImagePath() const = 0;
    virtual QString sideImagePath() const = 0;
    virtual QString backImagePath() const = 0;
    virtual QMap<QString, ButtonAction> defaultGestures() const = 0;
    virtual int minDpi() const = 0;
    virtual int maxDpi() const = 0;
    virtual int dpiStep() const = 0;
    virtual QList<EasySwitchSlotPosition> easySwitchSlotPositions() const = 0;
};
```

### Key Structs

**ControlDescriptor** — describes a button or control:

```cpp
struct ControlDescriptor {
    uint16_t controlId;       // HID++ CID (e.g., 0x0050 for left click)
    int buttonIndex;          // 0-based index into Profile::buttons array
    QString defaultName;      // Display name (e.g., "Left click")
    QString defaultActionType; // "default", "gesture-trigger", "smartshift-toggle"
    bool configurable;        // Can the user reassign this button?
};
```

**HotspotDescriptor** — positions interactive dots on the device render:

```cpp
struct HotspotDescriptor {
    int buttonIndex;      // Matches ControlDescriptor::buttonIndex, or negative for scroll zones
    double xPct;          // X position as fraction of image width (0.0 - 1.0)
    double yPct;          // Y position as fraction of image height (0.0 - 1.0)
    QString side;         // "left" or "right" — which side of the hotspot the label appears
    double labelOffsetYPct; // Vertical offset for the label (prevents overlap)
};
```

**FeatureSupport** — which HID++ features the device supports:

```cpp
struct FeatureSupport {
    bool battery = false;
    bool adjustableDpi = false;
    bool smartShift = false;
    bool hiResWheel = false;
    bool thumbWheel = false;
    bool reprogControls = false;
    bool gestureV2 = false;
};
```

> **Note:** You don't need to specify *which variant* of a feature your device uses. For example, set `battery = true` regardless of whether the device uses Battery Unified (0x1004) or Battery Status (0x1000). DeviceManager auto-detects the variant at runtime via capability dispatch tables (see `src/core/hidpp/capabilities/`). Same applies to SmartShift V1 vs Enhanced.

## Step 2: Find the Device's PIDs and Features

### Using hidraw tools

List Logitech hidraw devices:

```bash
# Find all Logitech hidraw devices
for f in /sys/class/hidraw/hidraw*/device/; do
    vendor=$(cat "$f/../../modalias" 2>/dev/null | grep -oP '(?<=v0000046D)p[0-9A-F]+' | sed 's/p/0x/')
    if [ -n "$vendor" ]; then
        echo "$(basename $(dirname $f)): PID=$vendor"
    fi
done
```

### Using Solaar

Solaar's command-line interface provides detailed device info:

```bash
solaar show
# Look for "Product ID", "Serial Number", and "Features"

solaar config <device-name>
# Shows current settings
```

### Using logiops / libratbag

```bash
# logiops debug output shows feature enumeration
sudo logiops -v debug

# ratbagctl for device capabilities
ratbagctl list
ratbagctl <device> info
```

### Manual HID++ enumeration

You can use the Logitune debug log to enumerate features. Connect the new device and run:

```bash
./build/src/app/logitune --debug 2>&1 | grep -E "(feature|PID|CID|DPI)"
```

The log output will show all discovered features and their indices.

## Step 3: Create the Descriptor

Create two files in `src/core/devices/`:

### Header: `MxAnywhere3sDescriptor.h`

```cpp
#pragma once
#include "interfaces/IDevice.h"

namespace logitune {

class MxAnywhere3sDescriptor : public IDevice {
public:
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
};

} // namespace logitune
```

### Implementation: `MxAnywhere3sDescriptor.cpp`

```cpp
#include "MxAnywhere3sDescriptor.h"

namespace logitune {

QString MxAnywhere3sDescriptor::deviceName() const
{
    return QStringLiteral("MX Anywhere 3S");
}

std::vector<uint16_t> MxAnywhere3sDescriptor::productIds() const
{
    // Include all known PIDs: Bolt receiver PID, Bluetooth PID, USB PID
    return { 0xb037 };  // Example PID — replace with actual
}

bool MxAnywhere3sDescriptor::matchesPid(uint16_t pid) const
{
    for (auto id : productIds()) {
        if (id == pid)
            return true;
    }
    return false;
}

QList<ControlDescriptor> MxAnywhere3sDescriptor::controls() const
{
    return {
        // CID,    index, name,                  defaultAction,          configurable
        { 0x0050, 0, QStringLiteral("Left click"),       QStringLiteral("default"),          false },
        { 0x0051, 1, QStringLiteral("Right click"),      QStringLiteral("default"),          false },
        { 0x0052, 2, QStringLiteral("Middle click"),     QStringLiteral("default"),          true  },
        { 0x0053, 3, QStringLiteral("Back"),             QStringLiteral("default"),          true  },
        { 0x0056, 4, QStringLiteral("Forward"),          QStringLiteral("default"),          true  },
        // The MX Anywhere 3S has no gesture button or thumb wheel — adjust accordingly
        { 0x00C4, 5, QStringLiteral("Shift wheel mode"), QStringLiteral("smartshift-toggle"),true  },
    };
}

QList<HotspotDescriptor> MxAnywhere3sDescriptor::buttonHotspots() const
{
    // Position hotspots relative to the device image
    // Use an image editor to find x/y percentages
    return {
        { 2, 0.65, 0.20, QStringLiteral("right"), 0.0 },  // Middle click
        { 5, 0.75, 0.38, QStringLiteral("right"), 0.0 },  // Shift wheel mode
        { 4, 0.30, 0.40, QStringLiteral("left"),  0.0 },  // Forward
        { 3, 0.40, 0.55, QStringLiteral("left"),  0.0 },  // Back
    };
}

QList<HotspotDescriptor> MxAnywhere3sDescriptor::scrollHotspots() const
{
    return {
        { -1, 0.68, 0.18, QStringLiteral("right"), 0.0 },  // Main wheel
    };
}

FeatureSupport MxAnywhere3sDescriptor::features() const
{
    FeatureSupport f;
    f.battery        = true;
    f.adjustableDpi  = true;
    f.smartShift     = true;
    f.hiResWheel     = true;
    f.thumbWheel     = false;  // MX Anywhere 3S has no thumb wheel
    f.reprogControls = true;
    f.gestureV2      = false;
    return f;
}

QString MxAnywhere3sDescriptor::frontImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-anywhere-3s.png");
}

QString MxAnywhere3sDescriptor::sideImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-anywhere-3s-side.png");
}

QString MxAnywhere3sDescriptor::backImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-anywhere-3s-back.png");
}

QMap<QString, ButtonAction> MxAnywhere3sDescriptor::defaultGestures() const
{
    // No gesture button on MX Anywhere 3S — return empty
    return {};
}

int MxAnywhere3sDescriptor::minDpi() const  { return 200; }
int MxAnywhere3sDescriptor::maxDpi() const  { return 8000; }
int MxAnywhere3sDescriptor::dpiStep() const { return 50; }

QList<EasySwitchSlotPosition> MxMaster3sDescriptor::easySwitchSlotPositions() const
{
    return {
        { 0.325, 0.658 }, // 1
        { 0.384, 0.642 }, // 2
        { 0.443, 0.643 }, // 3
    };
}

} // namespace logitune
```

## Step 4: Add Device Images

Place device images in `src/app/qml/assets/`:

- `mx-anywhere-3s.png` — front view (used as the main device render on the Buttons page)
- `mx-anywhere-3s-side.png` — side view
- `mx-anywhere-3s-back.png` — back view

Then register them in `src/app/CMakeLists.txt` under the `RESOURCES` section of `qt_add_qml_module`:

```cmake
RESOURCES
    qml/assets/mx-master-3s.png
    qml/assets/mx-master-3s-back.png
    qml/assets/mx-master-3s-side.png
    qml/assets/mx-anywhere-3s.png        # Add these
    qml/assets/mx-anywhere-3s-side.png
    qml/assets/mx-anywhere-3s-back.png
```

### Image Guidelines

- **Format**: PNG with transparency
- **Resolution**: High enough for 2x display scaling (~800px wide is good)
- **Orientation**: Front view should show the device from the right side, angled slightly
- **Background**: Transparent

## Step 5: Position Hotspots

Hotspot positions are defined as fractions of the image dimensions (0.0 to 1.0). To find the right positions:

1. Open the front image in an image editor
2. Note the pixel coordinates of each button/control center
3. Divide by image width/height to get the percentage

For each hotspot, decide:

- **side**: `"left"` or `"right"` — which side of the dot the label line extends to
- **labelOffsetYPct**: If two hotspots are close vertically, offset one label to prevent overlap

The MX Master 3S example:

```cpp
QList<HotspotDescriptor> MxMaster3sDescriptor::buttonHotspots() const
{
    return {
        { 2, 0.71, 0.15,  QStringLiteral("right"), 0.0  },  // Middle click
        { 6, 0.81, 0.34,  QStringLiteral("right"), 0.0  },  // Shift wheel mode
        { 7, 0.55, 0.515, QStringLiteral("right"), 0.0  },  // Thumb wheel
        { 4, 0.35, 0.43,  QStringLiteral("left"),  0.0  },  // Forward
        { 3, 0.45, 0.60,  QStringLiteral("left"),  0.20 },  // Back (offset label)
        { 5, 0.08, 0.58,  QStringLiteral("left"),  0.0  },  // Gesture button
    };
}
```

Scroll hotspots use negative `buttonIndex` values to indicate they are not buttons:

- `-1` — main scroll wheel
- `-2` — thumb wheel (horizontal)
- `-3` — scroll wheel side (mode switch area)

## Step 6: Register in DeviceRegistry

Edit `src/core/DeviceRegistry.cpp`:

```cpp
#include "DeviceRegistry.h"
#include "devices/MxMaster3sDescriptor.h"
#include "devices/MxAnywhere3sDescriptor.h"  // Add this

namespace logitune {

DeviceRegistry::DeviceRegistry() {
    registerDevice(std::make_unique<MxMaster3sDescriptor>());
    registerDevice(std::make_unique<MxAnywhere3sDescriptor>());  // Add this
}

// ... rest unchanged
```

## Step 7: Add to CMakeLists.txt

Edit `src/core/CMakeLists.txt` to include the new descriptor source:

```cmake
target_sources(logitune-core PRIVATE
    # ... existing files ...
    devices/MxMaster3sDescriptor.cpp
    devices/MxAnywhere3sDescriptor.cpp  # Add this
)
```

## Step 8: Testing

### Unit Test

Create a test to verify the descriptor returns valid data:

```cpp
// In tests/test_device_registry.cpp
static const DeviceSpec kDevices[] = {
    ...
    {
        .pid = 0xb034,
        .name = "MX Anywhere 3S",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 6, .scrollHotspots = 3,
        .minControls = 7,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "gesture-trigger",
        .control6ActionType = "smartshift-toggle",
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Keystroke,
        .gestureDownPayload = "Super+D",
        .gestureUpType = ButtonAction::Default,
    },
};
```

### Mock Device Setup

Add a helper to `MockDevice` (in `tests/mocks/MockDevice.h`) for the new device:

```cpp
void setupMxAnywhereControls() {
    m_deviceName  = QStringLiteral("MX Anywhere 3S");
    m_productIds  = { 0xb037 };
    m_features.reprogControls = true;
    m_features.battery        = true;
    m_features.adjustableDpi  = true;
    m_features.smartShift     = true;
    m_features.hiResWheel     = true;
    m_features.thumbWheel     = false;
    // ... populate m_controls
}
```

### Hardware Test

If you have the physical device, enable hardware tests:

```bash
cmake -B build -DBUILD_HW_TESTING=ON
cmake --build build
./build/tests/hw/logitune-hw-tests
```

The `HardwareFixture` will detect the device and run integration tests. Make sure the device is connected and awake.

### Manual Testing Checklist

- [ ] Device is detected on Bolt receiver connection
- [ ] Device is detected on Bluetooth connection
- [ ] Battery level reads correctly
- [ ] DPI changes apply and persist across profile switches
- [ ] SmartShift toggle works
- [ ] All configurable buttons divert and un-divert correctly
- [ ] Button remapping works (keystroke injection)
- [ ] Profile switching works on window focus change
- [ ] Device images render correctly in the Buttons page
- [ ] Hotspots align with the correct buttons
- [ ] Disconnect/reconnect works cleanly

## Step 9: Notes on PIDs

A single device may have multiple PIDs:

- **Bolt receiver PID** — reported by the receiver when the device is connected wirelessly via Bolt
- **Bluetooth PID** — the PID seen when connected directly via Bluetooth
- **USB PID** — if the device supports wired USB connection

For Bolt connections, `DeviceManager` matches by PID first, then falls back to matching by device name (read via the DeviceName HID++ feature). So even if the Bolt PID differs from the Bluetooth PID, the device will be found as long as either:

1. One of the PIDs is in `productIds()`, or
2. The device name matches via `DeviceRegistry::findByName()`

Include all known PIDs in `productIds()` for the most reliable matching.

## Reference: MX Master 3S Control Map

For reference, here is the complete control map of the MX Master 3S:

| CID | Index | Name | Default Action | Configurable |
|-----|-------|------|---------------|-------------|
| `0x0050` | 0 | Left click | default | No |
| `0x0051` | 1 | Right click | default | No |
| `0x0052` | 2 | Middle click | default | Yes |
| `0x0053` | 3 | Back | default | Yes |
| `0x0056` | 4 | Forward | default | Yes |
| `0x00C3` | 5 | Gesture button | gesture-trigger | Yes |
| `0x00C4` | 6 | Shift wheel mode | smartshift-toggle | Yes |
| `0x0000` | 7 | Thumb wheel | default | Yes |

Button index 7 (thumb wheel) uses CID `0x0000` because it is a virtual entry — the thumb wheel is controlled via the ThumbWheel feature (`0x2150`), not ReprogControlsV4.
