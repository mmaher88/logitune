# Multi-Device Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support multiple simultaneously connected Logitech devices with a carousel home screen, per-device configuration, and concurrent per-app profile switching.

**Architecture:** Extract a `DeviceSession` QObject from DeviceManager that encapsulates one device's lifecycle (transport, features, commands, state). DeviceManager becomes a thin coordinator for udev + session lifecycle. DeviceModel becomes a QAbstractListModel for the carousel, with selected-device properties for backward-compatible QML pages. HomeView gets a PathView carousel with status badges and drag-to-reorder.

**Tech Stack:** C++20, Qt6 (Quick, QAbstractListModel, PathView), GoogleTest

**Spec:** `docs/superpowers/specs/2026-04-12-multi-device-design.md`

**Stacks on:** `community-device-fetch` branch (PR #24)

---

## File Structure

**New files:**
- `src/core/DeviceSession.h` — QObject encapsulating one device
- `src/core/DeviceSession.cpp` — migrated from DeviceManager per-device code
- `src/app/qml/components/DeviceCard.qml` — carousel card component
- `tests/test_device_session.cpp` — DeviceSession unit tests

**Modified files:**
- `src/core/DeviceManager.h` — strip to udev + session lifecycle
- `src/core/DeviceManager.cpp` — strip per-device code, multi-session probing
- `src/core/CMakeLists.txt` — add DeviceSession.cpp
- `src/app/models/DeviceModel.h` — QAbstractListModel + selected device properties
- `src/app/models/DeviceModel.cpp` — list model + signal routing
- `src/app/AppController.h` — per-device state, session wiring
- `src/app/AppController.cpp` — multi-session orchestration
- `src/app/qml/HomeView.qml` — PathView carousel
- `src/app/CMakeLists.txt` — register DeviceCard.qml
- `tests/CMakeLists.txt` — add test_device_session.cpp
- `tests/mocks/MockDevice.h` — updates for session testing
- `tests/test_app_controller.cpp` — adapt for sessions
- `tests/test_device_model.cpp` — adapt for list model

---

### Task 1: Create DeviceSession (extract from DeviceManager)

**Files:**
- Create: `src/core/DeviceSession.h`
- Create: `src/core/DeviceSession.cpp`
- Modify: `src/core/CMakeLists.txt`

This is the largest task. DeviceSession takes over ALL per-device responsibilities from DeviceManager.

- [ ] **Step 1.1: Create DeviceSession.h**

Read `src/core/DeviceManager.h` to understand the current per-device state and methods. Create `DeviceSession.h` with:

**Identity:**
- `QString deviceId() const` — format: `"046d-PPPP-SERIAL"` (VID-PID-serial)
- `const IDevice* descriptor() const`
- `uint16_t devicePid() const`
- `QString deviceSerial() const`
- `QString deviceName() const`
- `QString connectionType() const`
- `bool isConnected() const`

**State getters** — copy ALL state-related getters from DeviceManager.h:
- `batteryLevel`, `batteryCharging`, `currentDPI`, `minDPI`, `maxDPI`, `dpiStep`
- `smartShiftEnabled`, `smartShiftThreshold`
- `scrollHiRes`, `scrollInvert`, `scrollRatchet`
- `thumbWheelMode`, `thumbWheelInvert`, `thumbWheelDefaultDirection`
- `currentHost`, `hostCount`, `isHostPaired`
- `deviceSerial`, `firmwareVersion`

**Setters** — copy from DeviceManager.h:
- `setDPI`, `setSmartShift`, `setScrollConfig`, `divertButton`, `setThumbWheelMode`
- `flushCommandQueue`, `touchResponseTime`

**Signals** — these are NEW (DeviceManager currently emits these as flat signals):
- `setupComplete()`
- `disconnected()`
- `batteryChanged(int level, bool charging)`
- `smartShiftChanged(bool enabled, int threshold)`
- `scrollConfigChanged()`
- `thumbWheelModeChanged()`
- `divertedButtonPressed(uint16_t cid, bool pressed)`
- `gestureRawXY(int16_t dx, int16_t dy)`
- `thumbWheelRotation(int delta)`
- `deviceWoke()`

**Private members** — move from DeviceManager.h:
- `HidrawDevice`, `Transport`, `FeatureDispatcher`, `CommandQueue` unique_ptrs
- Capability dispatches (`BatteryVariant`, `SmartShiftVariant`)
- All cached state fields (battery, DPI, smartShift, scroll, thumbWheel, easySwitch)
- `QSocketNotifier` for hidraw
- Sleep/wake state (`m_lastResponseTime`, `m_enumerating`)
- `DeviceRegistry*` for descriptor lookup
- `const IDevice* m_activeDevice`

**Constructor:** takes `unique_ptr<HidrawDevice>`, `deviceIndex`, `connectionType`, `DeviceRegistry*`.

**Public method:** `void enumerateAndSetup()` — called once after construction.
**Public method:** `void handleNotification(const Report&)` — called by DeviceManager.

- [ ] **Step 1.2: Create DeviceSession.cpp**

Read `src/core/DeviceManager.cpp` carefully. The following methods/blocks MOVE to DeviceSession.cpp:

**Move entirely (rename `DeviceManager::` to `DeviceSession::`):**
- `enumerateAndSetup()` — the entire function (~150 lines)
- `handleNotification()` — the entire function (~150 lines)
- `setDPI()`, `setSmartShift()`, `setScrollConfig()`, `divertButton()`, `setThumbWheelMode()`
- `flushCommandQueue()`, `touchResponseTime()`
- `checkSleepWake()` — the sleep/wake detection
- All state getter implementations (`batteryLevel()`, `deviceConnected()`, etc.)
- `isDirectDevice()` — becomes a private helper

**New method — `deviceId()`:**
```cpp
QString DeviceSession::deviceId() const
{
    return QStringLiteral("%1-%2-%3")
        .arg(m_deviceVid, 4, 16, QLatin1Char('0'))
        .arg(m_devicePid, 4, 16, QLatin1Char('0'))
        .arg(m_deviceSerial);
}
```

**New — disconnectCleanup():** Extracted from DeviceManager::disconnectDevice(). Resets transport, features, command queue, cached state. Emits `disconnected()`.

**Constructor:** takes ownership of HidrawDevice, stores deviceIndex and connectionType, stores registry pointer. Creates Transport, connects hidraw QSocketNotifier.

**Key changes from DeviceManager code:**
- Replace `emit deviceConnectedChanged()` with `emit setupComplete()` at end of enumerateAndSetup
- Replace `emit deviceDisconnected()` with `emit disconnected()`
- Replace `emit batteryLevelChanged()` + `emit batteryChargingChanged()` with `emit batteryChanged(level, charging)`
- Replace `emit smartShiftChanged()` with `emit smartShiftChanged(enabled, threshold)`
- Remove all udev-related code (stays in DeviceManager)
- Remove `probeDevice()`, `scanExistingDevices()`, `onUdevReady()`, `onUdevEvent()` (stay in DeviceManager)
- Remove `isReceiver()`, `deviceIndexForDirect()`, `deviceIndexForReceiver()` (stay in DeviceManager)

**Includes:** Copy all includes from DeviceManager.cpp that relate to HID++, features, capabilities, logging. Do NOT include udev headers.

- [ ] **Step 1.3: Add DeviceSession.cpp to CMakeLists**

In `src/core/CMakeLists.txt`, add `DeviceSession.cpp` to `target_sources`.

- [ ] **Step 1.4: Build (expect failures — DeviceManager still references moved code)**

Run: `cmake --build build --parallel $(nproc) 2>&1 | tail -20`

Expected: compilation errors in DeviceManager.cpp because methods/members have been moved. This is expected — Task 2 fixes DeviceManager.

**Do NOT fix DeviceManager in this task.** Just verify DeviceSession.cpp compiles on its own.

To verify DeviceSession compiles: temporarily add a `#if 0` around DeviceManager.cpp's body, build, then remove it. Or just check that DeviceSession.o is produced.

- [ ] **Step 1.5: Commit**

```bash
git add src/core/DeviceSession.h src/core/DeviceSession.cpp src/core/CMakeLists.txt
git commit -m "add DeviceSession class (extracted from DeviceManager)

Encapsulates one connected device's lifecycle: transport, features,
command queue, capability dispatches, cached state, and notification
handling. QObject with per-device signals.

refs #21"
```

---

### Task 2: Strip DeviceManager to session lifecycle

**Files:**
- Modify: `src/core/DeviceManager.h`
- Modify: `src/core/DeviceManager.cpp`

- [ ] **Step 2.1: Rewrite DeviceManager.h**

Read the spec for the new DeviceManager interface. Replace the current header with:

**Keep:**
- `start()`, `isReceiver()` (static), `deviceIndexForDirect()`, `deviceIndexForReceiver()`
- udev members (`m_udev`, `m_udevMon`, `m_udevNotifier`)
- `DeviceRegistry*`

**Add:**
- `std::vector<std::unique_ptr<DeviceSession>> m_sessions`
- `sessions()` getter, `sessionById()`, `sessionByPid()`
- Signals: `sessionAdded(QString deviceId)`, `sessionRemoved(QString deviceId)`, `unknownDeviceDetected(uint16_t pid)`

**Remove:**
- ALL per-device state members (battery, DPI, smartShift, scroll, etc.)
- ALL per-device Q_PROPERTYs
- ALL setter methods (setDPI, setSmartShift, etc.)
- `handleNotification()` (moved to DeviceSession)
- `enumerateAndSetup()` (moved to DeviceSession)
- `features()`, `transport()`, `deviceIndex()` accessors
- `m_device`, `m_transport`, `m_features`, `m_commandQueue`
- `m_hidrawNotifier`, `m_receiverNotifier`, `m_receiverDevice`
- `m_batteryPollTimer`, `m_reconnectTimer`
- Sleep/wake members
- Capability dispatch members
- `AppControllerFixture` friend declaration

**Keep `activeDevice()` temporarily** as a convenience that returns the first session's descriptor (for backward compat during migration).

- [ ] **Step 2.2: Rewrite DeviceManager.cpp**

**Keep and adapt:**
- Constructor: initialize udev only (no device state)
- `start()` → `scanExistingDevices()`
- `scanExistingDevices()` — iterate hidraw devices, call `probeDevice()` for ALL Logitech devices (remove `if (!m_connected)` guard)
- `onUdevReady()` / `onUdevEvent()` — handle add/remove, create/destroy sessions
- `probeDevice()` — open hidraw, determine if receiver or direct, create DeviceSession, call `session->enumerateAndSetup()`, emit `sessionAdded`
- `isReceiver()`, `deviceIndexForDirect()`, `deviceIndexForReceiver()`

**New — probeDevice() flow:**
```
1. Open hidraw, check report descriptor for HID++
2. Check vendor == Logitech
3. If receiver: probe slots 1-6, for each found device → create DeviceSession
4. If direct: create DeviceSession with index 0xFF
5. Connect session signals
6. Call session->enumerateAndSetup()
7. On setupComplete: add to m_sessions, emit sessionAdded(id)
8. If session->descriptor() is null: emit unknownDeviceDetected(pid)
```

**New — device disconnect handling:**
```
On udev remove: find session by devNode, emit sessionRemoved(id), destroy session
On DJ disconnect notification: find session by receiver slot, destroy session
```

**New — hidraw notification routing:**
When data arrives on a hidraw fd, route to the correct DeviceSession:
```cpp
// In the QSocketNotifier lambda for each session:
auto bytes = session->device()->readReport(0);
auto report = Report::parse(bytes);
if (report) session->handleNotification(*report);
```

- [ ] **Step 2.3: Build and fix compilation errors**

Run: `cmake --build build --parallel $(nproc) 2>&1`

Fix any remaining compilation errors. Common issues:
- AppController references to old DeviceManager methods → temporarily stub or comment
- Test files reference old DeviceManager API → temporarily comment
- DeviceModel references to DeviceManager state → temporarily stub

The goal is a clean build. Tests may fail at this stage — that's OK.

- [ ] **Step 2.4: Commit**

```bash
git add src/core/DeviceManager.h src/core/DeviceManager.cpp
git commit -m "strip DeviceManager to udev + session lifecycle

DeviceManager now creates/destroys DeviceSession objects and routes
hidraw notifications. All per-device state, feature handling, and
command queuing lives in DeviceSession.

refs #21"
```

---

### Task 3: Update DeviceModel to QAbstractListModel

**Files:**
- Modify: `src/app/models/DeviceModel.h`
- Modify: `src/app/models/DeviceModel.cpp`

- [ ] **Step 3.1: Rewrite DeviceModel.h**

Change base class from QObject to QAbstractListModel. Read the spec for exact interface.

**Add:**
- Roles enum: `DeviceIdRole`, `DeviceNameRole`, `FrontImageRole`, `BatteryLevelRole`, `BatteryChargingRole`, `ConnectionTypeRole`, `StatusRole`, `IsSelectedRole`
- `rowCount()`, `data()`, `roleNames()` overrides
- `count`, `selectedIndex`, `selectedDeviceId` Q_PROPERTYs
- `addSession(DeviceSession*)`, `removeSession(QString deviceId)` methods
- `QList<DeviceSession*> m_sessions` member
- `int m_selectedIndex` member

**Keep all existing Q_PROPERTYs** but change their NOTIFY signals to `selectedChanged` or `selectedBatteryChanged` / `selectedSettingsChanged`. Their getter implementations will read from `m_sessions[m_selectedIndex]` instead of `m_dm`.

**Remove:** direct `DeviceManager*` dependency for state reads. Keep it only for `activeDevice()` backward compat if needed.

- [ ] **Step 3.2: Rewrite DeviceModel.cpp**

Implement QAbstractListModel methods:
- `rowCount()` → `m_sessions.size()`
- `data()` → switch on role, read from `m_sessions[index.row()]`
- `roleNames()` → map roles to QML names

Implement selection:
- `setSelectedIndex(int)` → validate range, emit `selectedChanged`
- All flat property getters read from `m_sessions[m_selectedIndex]` (with null check)

Implement session management:
- `addSession()` → `beginInsertRows`, append, `endInsertRows`, emit `countChanged`
- `removeSession()` → find by ID, `beginRemoveRows`, remove, `endRemoveRows`, adjust `m_selectedIndex`

Image getters: prepend `file://` as before, but read from selected session's descriptor.

- [ ] **Step 3.3: Build and fix**

Build. Fix any compilation errors from the model refactor. Focus on getting a clean build — tests may still fail.

- [ ] **Step 3.4: Commit**

```bash
git add src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp
git commit -m "refactor DeviceModel to QAbstractListModel

Exposes all connected devices via list model roles for the carousel.
Selected device properties remain as flat Q_PROPERTYs for backward
compatibility with existing QML config pages.

refs #21"
```

---

### Task 4: Update AppController for multi-session wiring

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`

- [ ] **Step 4.1: Update AppController.h**

**Add:**
- `#include "DeviceSession.h"`
- `QMap<QString, PerDeviceState> m_perDeviceState` for gesture/thumb accumulation
- `struct PerDeviceState { int gestureAccumX=0, gestureAccumY=0, thumbAccum=0; bool gestureActive=false; }`
- Remove direct DeviceManager state reads (DPI, smartShift, etc.)

**Keep:** DeviceManager member (for session lifecycle), ProfileEngine, ActionExecutor, models, desktop integration.

- [ ] **Step 4.2: Update AppController.cpp**

**Replace wireSignals():**

Old: connected to DeviceManager's flat signals (deviceSetupComplete, batteryLevelChanged, etc.)
New: connect to DeviceManager's session lifecycle signals:

```cpp
connect(&m_deviceManager, &DeviceManager::sessionAdded, this, &AppController::onSessionAdded);
connect(&m_deviceManager, &DeviceManager::sessionRemoved, this, &AppController::onSessionRemoved);
```

**New — onSessionAdded(deviceId):**
```cpp
void AppController::onSessionAdded(const QString &deviceId)
{
    auto *session = m_deviceManager.sessionById(deviceId);
    if (!session) return;

    // Add to DeviceModel
    m_deviceModel.addSession(session);

    // Connect per-device signals
    connect(session, &DeviceSession::batteryChanged, this, [this, deviceId](...) { ... });
    connect(session, &DeviceSession::divertedButtonPressed, this, [this, deviceId](...) { ... });
    connect(session, &DeviceSession::gestureRawXY, this, [this, deviceId](...) { ... });
    connect(session, &DeviceSession::thumbWheelRotation, this, [this, deviceId](...) { ... });

    // Auto-select first device
    if (m_deviceModel.count() == 1)
        m_deviceModel.setSelectedIndex(0);

    // Load and apply profile
    setupProfileForSession(session);
}
```

**New — onSessionRemoved(deviceId):**
```cpp
void AppController::onSessionRemoved(const QString &deviceId)
{
    m_deviceModel.removeSession(deviceId);
    m_perDeviceState.remove(deviceId);

    if (m_deviceModel.count() > 0 && m_deviceModel.selectedIndex() < 0)
        m_deviceModel.setSelectedIndex(0);
}
```

**Update onWindowFocusChanged:** Apply profile to ALL sessions, not just one:
```cpp
for (auto *session : m_deviceModel.sessions()) {
    if (!session->descriptor()) continue;
    QString profileName = m_profileEngine.profileForApp(wmClass);
    // Load profile from session's device config dir
    // Apply to session
}
```

**Update gesture/thumb/button handlers:** Use `m_perDeviceState[deviceId]` instead of flat accumulators.

**Update onDpiChangeRequested, onSmartShiftChangeRequested, etc.:** Route to the selected session instead of DeviceManager:
```cpp
void AppController::onDpiChangeRequested(int value)
{
    auto *session = selectedSession();
    if (!session) return;
    // ... apply to session
}
```

- [ ] **Step 4.3: Build and fix all compilation errors**

This is the integration point where everything comes together. Fix all remaining compilation errors across the codebase.

- [ ] **Step 4.4: Run tests — fix what's broken**

Run: `XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests 2>&1`

Some tests will fail due to API changes. Fix them:
- `test_app_controller.cpp` — update for session-based API
- `test_device_model.cpp` — update for list model API
- `test_notification_filtering.cpp` — update for session notification routing
- Other tests that reference DeviceManager state directly

- [ ] **Step 4.5: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp \
        tests/test_app_controller.cpp tests/test_device_model.cpp \
        tests/test_notification_filtering.cpp
git commit -m "wire AppController for multi-session device management

Connects to DeviceManager session lifecycle signals. Per-device
gesture/thumb/button state. Profile switching applies to all
connected sessions on focus change. Existing tests adapted.

refs #21"
```

---

### Task 5: Create DeviceCard.qml + rewrite HomeView.qml

**Files:**
- Create: `src/app/qml/components/DeviceCard.qml`
- Modify: `src/app/qml/HomeView.qml`
- Modify: `src/app/CMakeLists.txt` (register DeviceCard.qml)

- [ ] **Step 5.1: Create DeviceCard.qml**

A reusable carousel card component showing one device:

```qml
import QtQuick
import Logitune 1.0

Item {
    id: root

    required property int index
    required property string deviceId
    required property string deviceName
    required property string frontImage
    required property int batteryLevel
    required property bool batteryCharging
    required property string connectionType
    required property string status
    required property bool isSelected

    signal clicked()

    width: 180; height: 280

    // Device image
    Image {
        id: deviceImg
        anchors.centerIn: parent
        width: parent.width - 20
        height: parent.height - 60
        source: root.frontImage
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true

        // Greyed out for placeholder
        opacity: root.status === "placeholder" ? 0.4 : 1.0
        layer.enabled: root.status === "placeholder"
        layer.effect: ShaderEffect {
            // Desaturation handled via opacity for simplicity
        }
    }

    // Status badge (top-right corner)
    Rectangle {
        anchors { top: parent.top; right: parent.right; margins: -4 }
        width: 22; height: 22; radius: 11
        z: 2
        border { width: 2; color: "#111" }

        color: {
            switch (root.status) {
            case "implemented": return "#22c55e";
            case "community-verified": return "#3b82f6";
            case "community-local": return "#f59e0b";
            default: return "#666";
            }
        }

        Text {
            anchors.centerIn: parent
            font.pixelSize: 12; font.bold: true
            color: root.status === "community-local" ? "#222" : "#fff"
            text: {
                switch (root.status) {
                case "implemented": return "✓";
                case "community-verified": return "★";
                case "community-local": return "✎";
                default: return "?";
                }
            }
        }
    }

    // Device name
    Text {
        anchors { top: deviceImg.bottom; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        text: root.deviceName
        font { pixelSize: root.isSelected ? 13 : 11; bold: root.isSelected }
        color: root.status === "placeholder" ? "#666" : (root.isSelected ? Theme.text : "#888")
    }

    // Battery + connection (selected device only)
    Row {
        anchors { top: deviceImg.bottom; topMargin: 26; horizontalCenter: parent.horizontalCenter }
        spacing: 6
        visible: root.isSelected && root.status !== "placeholder"

        Text {
            text: "🔋 " + root.batteryLevel + "%"
            font.pixelSize: 11
            color: root.batteryLevel > 20 ? "#22c55e" : "#ef4444"
        }
        Text {
            text: "· " + root.connectionType
            font.pixelSize: 10
            color: "#888"
        }
    }

    // "Setup needed" label for placeholders
    Text {
        anchors { top: deviceImg.bottom; topMargin: 26; horizontalCenter: parent.horizontalCenter }
        visible: root.status === "placeholder"
        text: "Setup needed"
        font.pixelSize: 9
        color: "#666"
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
```

- [ ] **Step 5.2: Rewrite HomeView.qml**

Replace the current single-device layout with a PathView carousel. Read the spec for layout details.

Key elements:
- PathView with model: DeviceModel
- Delegate: DeviceCard component
- Scale/opacity based on `PathView.isCurrentItem`
- Click non-selected → `DeviceModel.selectedIndex = index`
- Click selected → `root.deviceClicked()` (navigate to config)
- Dot indicators below carousel
- Empty state when count === 0 (keep current "Connect your device" prompt)
- Accent border on selected device

```qml
PathView {
    id: carousel
    model: DeviceModel
    currentIndex: DeviceModel.selectedIndex
    onCurrentIndexChanged: DeviceModel.selectedIndex = currentIndex
    visible: DeviceModel.count > 0

    pathItemCount: Math.min(DeviceModel.count, 5)
    preferredHighlightBegin: 0.5
    preferredHighlightEnd: 0.5
    highlightRangeMode: PathView.StrictlyEnforceRange

    path: Path {
        startX: 0; startY: carousel.height / 2
        PathLine { x: carousel.width; y: carousel.height / 2 }
    }

    delegate: DeviceCard {
        required property int index
        required property string deviceId
        required property string deviceName
        required property string frontImage
        required property int batteryLevel
        required property bool batteryCharging
        required property string connectionType
        required property string status
        required property bool isSelected

        scale: PathView.isCurrentItem ? 1.0 : 0.65
        opacity: PathView.isCurrentItem ? 1.0 : 0.5
        z: PathView.isCurrentItem ? 2 : 1

        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 250 } }

        onClicked: {
            if (PathView.isCurrentItem)
                root.deviceClicked()
            else
                carousel.currentIndex = index
        }
    }
}
```

- [ ] **Step 5.3: Register DeviceCard.qml in CMakeLists**

Add `qml/components/DeviceCard.qml` to the QML module RESOURCES.

- [ ] **Step 5.4: Build and verify QML loads**

Build and run the app. Verify the carousel shows connected device(s).

- [ ] **Step 5.5: Commit**

```bash
git add src/app/qml/components/DeviceCard.qml src/app/qml/HomeView.qml src/app/CMakeLists.txt
git commit -m "add device carousel with PathView + DeviceCard component

Home screen shows all connected devices in a scrollable carousel.
Selected device is full-size with accent border. Status badges show
implemented/community/placeholder state. Greyed rendering for
placeholder devices.

refs #21"
```

---

### Task 6: Device order persistence

**Files:**
- Modify: `src/app/models/DeviceModel.h`
- Modify: `src/app/models/DeviceModel.cpp`

- [ ] **Step 6.1: Add order persistence methods**

```cpp
void DeviceModel::saveDeviceOrder() const
{
    QJsonArray order;
    for (auto *session : m_sessions)
        order.append(session->deviceId());

    QJsonObject root;
    root["order"] = order;

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                   + "/device-order.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

QStringList DeviceModel::loadDeviceOrder() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                   + "/device-order.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QJsonArray order = QJsonDocument::fromJson(f.readAll()).object()["order"].toArray();
    QStringList result;
    for (const auto &v : order)
        result.append(v.toString());
    return result;
}
```

- [ ] **Step 6.2: Sort sessions on add using saved order**

In `addSession()`, after inserting, sort `m_sessions` to match the persisted order. New devices go at the end.

- [ ] **Step 6.3: Add drag-to-reorder support**

Add `Q_INVOKABLE void moveDevice(int from, int to)` that reorders `m_sessions` and calls `saveDeviceOrder()`.

Wire this from QML drag handlers in the carousel (can be simplified to click-based reorder if drag is too complex for PathView).

- [ ] **Step 6.4: Build and test**

- [ ] **Step 6.5: Commit**

```bash
git add src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp
git commit -m "add device order persistence

Saves carousel order to device-order.json. Restored on startup.
New devices appended at end. Drag-to-reorder triggers save.

refs #21"
```

---

### Task 7: Add DeviceSession unit tests

**Files:**
- Create: `tests/test_device_session.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 7.1: Write DeviceSession tests**

Using MockTransport, create a DeviceSession and verify:

1. **DeviceIdFormat** — verify `deviceId()` returns `"046d-PPPP-SERIAL"` format
2. **EnumerateSuccess** — mock feature responses, verify `setupComplete` emits and state is populated
3. **BatteryRead** — verify battery level/charging parsed correctly
4. **SmartShiftRead** — verify smartShift state parsed
5. **SetDPI** — verify `setDPI()` enqueues correct command
6. **SetSmartShift** — verify `setSmartShift()` uses capability dispatch
7. **NotificationRouting** — send a battery notification, verify `batteryChanged` signal emits
8. **ButtonDivert** — verify `divertButton()` enqueues correct command
9. **DisconnectCleanup** — verify disconnect clears state and emits `disconnected`

- [ ] **Step 7.2: Register test file**

Add `test_device_session.cpp` to `tests/CMakeLists.txt`.

- [ ] **Step 7.3: Build and run**

Run: `XDG_DATA_DIRS=$(pwd)/build ./build/tests/logitune-tests --gtest_filter='DeviceSession*'`

All DeviceSession tests must pass.

- [ ] **Step 7.4: Run full test suite**

All tests must pass.

- [ ] **Step 7.5: Commit**

```bash
git add tests/test_device_session.cpp tests/CMakeLists.txt
git commit -m "add DeviceSession unit tests

Tests for device ID format, feature enumeration, state reads,
command enqueuing, notification routing, and disconnect cleanup.

refs #21"
```

---

### Task 8: Full verification + smoke test

**Files:** none (verification only)

- [ ] **Step 8.1: Clean build**
- [ ] **Step 8.2: Run full test suite** — all tests pass
- [ ] **Step 8.3: Run tray tests** — all pass
- [ ] **Step 8.4: Smoke test with MX3S** — carousel shows one device, selecting navigates to config, all buttons/DPI/smartshift work
- [ ] **Step 8.5: Verify profile switching** — change window focus, verify profile applies
- [ ] **Step 8.6: Verify device disconnect/reconnect** — unplug, carousel updates, replug, device reappears

---

### Task 9: Push + open stacked PR

- [ ] **Step 9.1: Review commit log**

Run: `git log --oneline community-device-fetch..HEAD`

- [ ] **Step 9.2: Push**

```bash
git push -u origin multi-device-support
```

- [ ] **Step 9.3: Open PR targeting community-device-fetch**

```bash
gh pr create --base community-device-fetch --head multi-device-support \
  --title "feat: multi-device support with carousel home screen" \
  --body "..."
```

---

## Self-Review

**1. Spec coverage:**
- [x] DeviceSession extraction — Task 1
- [x] DeviceManager refactor — Task 2
- [x] DeviceModel QAbstractListModel — Task 3
- [x] AppController multi-session — Task 4
- [x] HomeView carousel + DeviceCard — Task 5
- [x] Status badges — Task 5 (DeviceCard)
- [x] Greyed placeholder rendering — Task 5 (DeviceCard)
- [x] Device order persistence — Task 6
- [x] Per-device profile switching — Task 4
- [x] Device ID (VID+PID+serial) — Task 1
- [x] Unit tests — Task 7
- [x] Smoke test — Task 8

**2. Placeholder scan:** No TBDs. Task 4 step 4.2 has specific method descriptions rather than full 500-line code blocks — this is intentional as the subagent needs to read current AppController.cpp and adapt rather than paste wholesale.

**3. Type consistency:**
- `DeviceSession::deviceId()` returns `QString` — used consistently as key in DeviceModel, AppController, PerDeviceState map
- `sessionAdded(QString deviceId)` / `sessionRemoved(QString deviceId)` — matching parameter names
- `DeviceModel::addSession(DeviceSession*)` / `removeSession(QString deviceId)` — consistent with session lifecycle
- Roles enum names match roleNames() QML exposure
