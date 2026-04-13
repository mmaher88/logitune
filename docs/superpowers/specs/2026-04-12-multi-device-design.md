# Multi-Device Support — Design Spec

**Goal:** Support multiple simultaneously connected Logitech devices with a carousel home screen, per-device configuration, and concurrent per-app profile switching.

**Scope:** Issue #21. Covers DeviceSession extraction, DeviceManager refactor, DeviceModel list model, HomeView carousel with status badges and drag-to-reorder, greyed placeholder rendering, and per-device profile application on focus change.

**Dependencies:** Stacks on PR #23 (JSON descriptors) and PR #24 (community fetch).

---

## Architecture

### DeviceSession (new class)

A QObject extracted from DeviceManager that encapsulates one connected device's full lifecycle: transport, feature enumeration, command queue, capability dispatches, cached state, and notification handling.

```cpp
class DeviceSession : public QObject {
    Q_OBJECT
public:
    DeviceSession(std::unique_ptr<HidrawDevice> device,
                  uint8_t deviceIndex,
                  const QString &connectionType,
                  DeviceRegistry *registry,
                  QObject *parent = nullptr);

    // Identity
    QString deviceId() const;           // VID+PID+serial (stable across reconnects)
    const IDevice* descriptor() const;  // matched JSON descriptor (or nullptr for unknown)
    uint16_t devicePid() const;
    QString deviceSerial() const;

    // Connection
    bool isConnected() const;
    QString connectionType() const;     // "Bolt", "Bluetooth", "USB"

    // State getters
    QString deviceName() const;
    int batteryLevel() const;
    bool batteryCharging() const;
    int currentDPI() const;
    int minDPI() const;
    int maxDPI() const;
    int dpiStep() const;
    bool smartShiftEnabled() const;
    int smartShiftThreshold() const;
    bool scrollHiRes() const;
    bool scrollInvert() const;
    bool scrollRatchet() const;
    QString thumbWheelMode() const;
    bool thumbWheelInvert() const;
    int currentHost() const;
    int hostCount() const;

    // Setters (write to device via command queue)
    void setDPI(int value);
    void setSmartShift(bool enabled, int threshold);
    void setScrollConfig(bool hiRes, bool invert);
    void divertButton(uint16_t cid, bool divert, bool rawXY = false);
    void setThumbWheelMode(const QString &mode, bool invert = false);
    void flushCommandQueue();

    // Notification handling (called by DeviceManager from hidraw notifier)
    void handleNotification(const hidpp::Report &report);

    // Enumeration (called once after construction)
    void enumerateAndSetup();

signals:
    void setupComplete();
    void disconnected();
    void batteryChanged(int level, bool charging);
    void smartShiftChanged(bool enabled, int threshold);
    void scrollConfigChanged();
    void thumbWheelModeChanged();
    void divertedButtonPressed(uint16_t cid, bool pressed);
    void gestureRawXY(int16_t dx, int16_t dy);
    void thumbWheelRotation(int delta);
    void deviceWoke();

private:
    // All current DeviceManager per-device state moves here:
    // HidrawDevice, Transport, FeatureDispatcher, CommandQueue
    // Capability dispatches (BatteryVariant, SmartShiftVariant)
    // Cached state (battery, DPI, smartShift, scroll, thumbWheel, easySwitch)
    // Sleep/wake detection
    // Reconnect timer
};
```

### DeviceSession lifecycle

- **Created** when DeviceManager successfully opens a hidraw device and identifies it as a Logitech HID++ device
- **enumerateAndSetup()** called immediately — enumerates features, reads state, resolves capabilities, matches descriptor
- **Emits setupComplete()** when ready — DeviceManager adds to active sessions list
- **handleNotification()** called by DeviceManager whenever hidraw data arrives for this device
- **Destroyed** when the physical device disconnects (udev remove or DJ notification)
- **State does not persist** across reconnects — a new session is created on reconnect, profile data persists on disk

### Device ID format

`VID+PID+serial` formatted as `046d-b034-2332AP05UNV8`. Stable across reconnects and reboots. Used as the key for:
- Profile directory lookup
- Device order persistence
- DeviceModel selection
- Session lookup

---

## Refactored DeviceManager

DeviceManager shrinks to three responsibilities: udev monitoring, hidraw probing, and session lifecycle management.

```cpp
class DeviceManager : public QObject {
    Q_OBJECT
public:
    DeviceManager(DeviceRegistry *registry, QObject *parent = nullptr);
    void start();

    const std::vector<std::unique_ptr<DeviceSession>>& sessions() const;
    DeviceSession* sessionById(const QString &id) const;
    DeviceSession* sessionByPid(uint16_t pid) const;

    // Static helpers (unchanged)
    static bool isReceiver(uint16_t pid);

signals:
    void sessionAdded(const QString &deviceId);
    void sessionRemoved(const QString &deviceId);
    void unknownDeviceDetected(uint16_t pid);

private:
    void scanExistingDevices();
    void onUdevEvent(const QString &action, const QString &devNode);
    void probeDevice(const QString &devNode);
    void removeSession(const QString &devNode);

    DeviceRegistry *m_registry;
    std::vector<std::unique_ptr<DeviceSession>> m_sessions;

    struct udev *m_udev = nullptr;
    struct udev_monitor *m_udevMon = nullptr;
    QSocketNotifier *m_udevNotifier = nullptr;
};
```

### Key changes from current DeviceManager

- **No per-device state** — all moved into DeviceSession
- **No setter methods** — `setDPI()`, `setSmartShift()`, etc. are on DeviceSession
- **No notification handling logic** — routed to the correct DeviceSession
- **`probeDevice()` no longer early-exits** — removes `if (!m_connected) return` guard, creates a session for each device found
- **`scanExistingDevices()` probes ALL hidraw devices** — not just the first one
- **Receiver handling** — one receiver can have multiple devices on different slots, each gets its own DeviceSession
- **`sessionAdded`/`sessionRemoved` signals** — replace old `deviceConnectedChanged`/`deviceDisconnected`

---

## DeviceModel as QAbstractListModel

DeviceModel changes from a flat QObject to a list model that exposes all connected devices for the carousel, while maintaining backward-compatible flat properties for the selected device.

```cpp
class DeviceModel : public QAbstractListModel {
    Q_OBJECT

    // List model metadata
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId NOTIFY selectedChanged)

    // Selected device properties (backward-compatible with existing QML pages)
    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY selectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY selectedChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY selectedBatteryChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY selectedBatteryChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY selectedChanged)
    Q_PROPERTY(int currentDPI READ currentDPI NOTIFY selectedSettingsChanged)
    Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY selectedSettingsChanged)
    // ... all existing Q_PROPERTYs, reading from selected session
    Q_PROPERTY(QString frontImage READ frontImage NOTIFY selectedChanged)
    Q_PROPERTY(QString deviceStatus READ deviceStatus NOTIFY selectedChanged)
    Q_PROPERTY(bool smoothScrollSupported READ smoothScrollSupported NOTIFY selectedChanged)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        FrontImageRole,
        BatteryLevelRole,
        BatteryChargingRole,
        ConnectionTypeRole,
        StatusRole,
        IsSelectedRole,
    };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Selection
    int selectedIndex() const;
    void setSelectedIndex(int index);
    QString selectedDeviceId() const;

    // Session management (called by AppController)
    void addSession(DeviceSession *session);
    void removeSession(const QString &deviceId);
};
```

### Backward compatibility

All existing QML pages (ButtonsPage, PointScrollPage, EasySwitchPage, DetailPanel) continue to read from flat properties like `DeviceModel.deviceName`, `DeviceModel.batteryLevel`, etc. These properties now read from the *selected* session instead of a global singleton state. **Zero changes needed to existing QML config pages.**

Only HomeView.qml changes — it uses the list model roles for the carousel.

### Signal routing

When AppController connects a new DeviceSession's signals, it routes them through DeviceModel:
- `session->batteryChanged` → if session is selected, emit `selectedBatteryChanged`
- `session->smartShiftChanged` → if session is selected, emit `selectedSettingsChanged`
- Session added/removed → emit `countChanged`, update model rows

---

## HomeView Carousel

### QML structure

```qml
PathView {
    id: deviceCarousel
    model: DeviceModel
    currentIndex: DeviceModel.selectedIndex
    onCurrentIndexChanged: DeviceModel.selectedIndex = currentIndex

    pathItemCount: Math.min(model.count, 5)
    preferredHighlightBegin: 0.5
    preferredHighlightEnd: 0.5
    highlightRangeMode: PathView.StrictlyEnforceRange

    path: Path {
        startX: 0; startY: height / 2
        PathLine { x: deviceCarousel.width; y: height / 2 }
    }

    delegate: DeviceCard {
        // Scale/opacity based on PathView position
        scale: PathView.isCurrentItem ? 1.0 : 0.65
        opacity: PathView.isCurrentItem ? 1.0 : 0.5

        // Status badge (corner badge style)
        // Greyed + desaturated for placeholder status
        // Click non-selected → select; click selected → navigate to config
    }
}
```

### DeviceCard component

A reusable QML component that renders one device in the carousel:
- Device image (from `frontImage` role)
- Status badge (green ✓ / blue ★ / orange ✎ / grey ?)
- Device name label
- Battery level + connection type (for selected device)
- Greyed out + desaturated + "Setup needed" label for placeholder devices
- Click handler: select if not selected, navigate to config if already selected

### Drag to reorder

PathView supports drag-to-reorder with `DelegateModel` and `DragHandler`. When the user drags a device card to a new position:
1. DelegateModel rearranges the items
2. DeviceModel saves the new order to `~/.config/Logitune/device-order.json`
3. On next startup, DeviceModel reads the order file and sorts sessions accordingly

### Status badges

Corner badge at top-right of each device card:
- **Green ✓** — `status == "implemented"` (shipped with app, fully tested)
- **Blue ★** — `status == "community-verified"` (from community repo)
- **Orange ✎** — `status == "community-local"` (wizard completed locally)
- **Grey ?** — `status == "placeholder"` (recognized but not configured)

### Greyed placeholder rendering

Devices with `status == "placeholder"`:
- Device image has `filter: grayscale(100%)` and `opacity: 0.4`
- "Setup needed" label below device name
- Click does NOT navigate to config (nothing to configure yet)
- Future: click opens the onboarding wizard

### Dot indicators

Row of dots below the carousel. Accent color for selected device, dim for others. Hidden when count == 1.

### Empty state

When no devices are connected: show current "Connect your device" prompt (unchanged).

---

## Profile Application on Focus Change

When the focused window changes, ALL connected devices get their per-app profile applied:

```cpp
void AppController::onWindowFocusChanged(const QString &wmClass) {
    for (auto &session : m_deviceManager.sessions()) {
        if (!session->descriptor()) continue;  // skip unmatched devices

        QString profileName = m_profileEngine.profileForApp(wmClass);
        QString deviceConfigDir = profileDirForDevice(session->deviceId());

        Profile p = ProfileEngine::loadProfile(deviceConfigDir + "/" + profileName + ".conf");
        if (p.name.isEmpty())
            p = ProfileEngine::loadProfile(deviceConfigDir + "/default.conf");

        applyProfileToSession(session.get(), p);
    }
}
```

Each device has its own profile directory (`~/.config/Logitune/devices/<serial>/profiles/`). If a device doesn't have a profile for the focused app, it stays on its default profile. This provides per-device customization naturally without per-device app binding rules.

---

## Device Order Persistence

```json
// ~/.config/Logitune/device-order.json
{
    "order": [
        "046d-b034-2332AP05UNV8",
        "046d-405e-AB12CD34EF56",
        "046d-c08b-0000000000"
    ]
}
```

On startup, DeviceModel sorts sessions to match this order. New devices (not in the list) are appended at the end. Disconnected devices stay in the order list so their position is preserved when they reconnect.

---

## AppController Wiring

AppController becomes the orchestrator between DeviceManager sessions and the rest of the app:

```cpp
// When a new session is created
connect(&m_deviceManager, &DeviceManager::sessionAdded, this, [this](const QString &id) {
    auto *session = m_deviceManager.sessionById(id);

    // Connect session signals for the selected device routing
    connect(session, &DeviceSession::batteryChanged, this, ...);
    connect(session, &DeviceSession::divertedButtonPressed, this, ...);
    connect(session, &DeviceSession::gestureRawXY, this, ...);
    connect(session, &DeviceSession::thumbWheelRotation, this, ...);

    // Add to DeviceModel
    m_deviceModel.addSession(session);

    // Load and apply profile
    setupProfileForSession(session);

    // Auto-select if it's the first device
    if (m_deviceModel.count() == 1)
        m_deviceModel.setSelectedIndex(0);
});

// When a session is destroyed
connect(&m_deviceManager, &DeviceManager::sessionRemoved, this, [this](const QString &id) {
    m_deviceModel.removeSession(id);

    // If the selected device was removed, select another
    if (m_deviceModel.count() > 0 && m_deviceModel.selectedIndex() < 0)
        m_deviceModel.setSelectedIndex(0);
});
```

### Gesture/thumbwheel/button routing

Currently AppController has gesture accumulation state (`m_gestureAccumX/Y`), thumb wheel accumulation (`m_thumbAccum`), and button divert logic. In multi-device, these need to be per-device.

Two options:
1. Move gesture/thumb state into DeviceSession
2. Keep in AppController but keyed by device ID

Since gesture resolution and action execution involve ActionExecutor and ProfileEngine (which are AppController's concerns, not DeviceSession's), **keep the state in AppController keyed by device ID:**

```cpp
struct PerDeviceState {
    int gestureAccumX = 0;
    int gestureAccumY = 0;
    int thumbAccum = 0;
    bool gestureActive = false;
};
QMap<QString, PerDeviceState> m_perDeviceState;
```

---

## Files Changed

### New files
- `src/core/DeviceSession.h` — DeviceSession class declaration
- `src/core/DeviceSession.cpp` — full implementation (most code migrated from DeviceManager)
- `src/app/qml/components/DeviceCard.qml` — carousel device card component
- `tests/test_device_session.cpp` — unit tests for DeviceSession

### Modified files
- `src/core/DeviceManager.h` — strip to udev + session lifecycle
- `src/core/DeviceManager.cpp` — strip per-device code, add multi-session probing
- `src/core/CMakeLists.txt` — add DeviceSession.cpp
- `src/app/models/DeviceModel.h` — QAbstractListModel with roles + selected device properties
- `src/app/models/DeviceModel.cpp` — list model implementation + signal routing
- `src/app/AppController.h` — per-device state map, new signal connections
- `src/app/AppController.cpp` — session wiring, per-device profile apply, gesture/thumb routing
- `src/app/qml/HomeView.qml` — PathView carousel replacing single device view
- `src/app/CMakeLists.txt` — register DeviceCard.qml
- `tests/CMakeLists.txt` — add test_device_session.cpp
- `tests/mocks/MockDevice.h` — may need updates for multi-device testing

### Deleted files
- None (DeviceManager files are modified, not deleted)

---

## Testing Strategy

### Unit tests (test_device_session.cpp)
- Create DeviceSession with MockTransport, verify feature enumeration
- Verify battery/DPI/SmartShift state reads
- Verify setter methods enqueue correct commands
- Verify notification routing (battery change, button divert, ratchet switch)
- Verify device ID format (VID+PID+serial)

### Existing test adaptation
- `test_device_registry.cpp` — unchanged (tests registry, not DeviceManager)
- `test_app_controller.cpp` — needs updates for session-based wiring
- `test_device_model.cpp` — needs updates for list model API
- `test_notification_filtering.cpp` — needs updates for session-based notification routing

### Smoke test
- Connect MX Master 3S → appears in carousel, full functionality
- Verify second device connection (if available) → appears in carousel
- Verify device disconnect → removed from carousel, remaining device stays
- Verify profile switching applies to all connected devices on focus change
- Verify carousel selection changes config pages

---

## Out of Scope
- Device onboarding wizard (separate feature, depends on this)
- Per-device app binding rules (covered naturally by per-device profile directories)
- Keyboard support (mice only for now)
