# Comprehensive Test Plan for Logitune

## Goal

Production-grade test coverage across all 31 modules with unit, integration, system, and UI tests. ~313 new tests bringing the total from 133 to ~446.

## Architecture Change: Dependency Injection

AppController and DeviceManager currently hardcode their dependencies. To test the critical orchestration paths, they need dependency injection.

### AppController

```cpp
// Constructor accepts optional dependencies. nullptr = create real implementation.
AppController::AppController(
    IDesktopIntegration *desktop = nullptr,
    IInputInjector *injector = nullptr,
    QObject *parent = nullptr);
```

- Tests inject `MockDesktop`, `MockInjector`
- Production code passes nothing (creates `KDeDesktop`, `UinputInjector` internally)
- No behavior change in production

### DeviceManager

```cpp
DeviceManager::DeviceManager(DeviceRegistry *registry,
                              ITransport *transport = nullptr,
                              QObject *parent = nullptr);
```

- Tests inject `MockTransport` for canned HID++ responses
- Production code passes nothing (creates real `Transport` on device probe)

## Mock Library (`tests/mocks/`)

### MockDesktop (`IDesktopIntegration`)

```cpp
class MockDesktop : public IDesktopIntegration {
public:
    void start() override {}
    bool available() const override { return true; }
    QString desktopName() const override { return "Mock"; }
    QStringList detectedCompositors() const override { return {"MockWM"}; }
    QVariantList runningApplications() const override { return m_apps; }
    void blockGlobalShortcuts(bool) override {}

    // Test helpers
    void simulateFocus(const QString &wmClass, const QString &title = {}) {
        emit activeWindowChanged(wmClass, title);
    }
    void setRunningApps(const QVariantList &apps) { m_apps = apps; }

private:
    QVariantList m_apps;
};
```

### MockInjector (`IInputInjector`)

```cpp
class MockInjector : public IInputInjector {
public:
    bool init() override { return true; }
    void shutdown() override {}

    void injectKeystroke(const QString &combo) override {
        m_calls.append({"keystroke", combo});
    }
    void injectCtrlScroll(int direction) override {
        m_calls.append({"ctrlscroll", QString::number(direction)});
    }
    void launchApp(const QString &cmd) override {
        m_calls.append({"launch", cmd});
    }

    struct Call { QString method; QString arg; };
    const QVector<Call> &calls() const { return m_calls; }
    void clearCalls() { m_calls.clear(); }
    Call lastCall() const { return m_calls.isEmpty() ? Call{} : m_calls.last(); }

private:
    QVector<Call> m_calls;
};
```

### MockTransport (`ITransport`)

```cpp
class MockTransport : public ITransport {
public:
    std::optional<hidpp::Report> sendRequest(const hidpp::Report &req, int timeout) override {
        if (m_responses.contains(req.featureIndex))
            return m_responses[req.featureIndex];
        return std::nullopt;
    }

    // Test helpers
    void setResponse(uint8_t featureIndex, const hidpp::Report &resp) {
        m_responses[featureIndex] = resp;
    }
    void simulateNotification(const hidpp::Report &report) {
        emit notificationReceived(report);
    }
    void simulateDisconnect() { emit deviceDisconnected(); }

private:
    QMap<uint8_t, hidpp::Report> m_responses;
};
```

### MockDevice (`IDevice`)

```cpp
class MockDevice : public IDevice {
public:
    // Configurable via setters, returns canned data
    QString deviceName() const override { return m_name; }
    std::vector<uint16_t> productIds() const override { return m_pids; }
    std::vector<ControlDescriptor> controls() const override { return m_controls; }
    // ... all virtual methods with configurable return values
};
```

## Test Helpers (`tests/helpers/`)

### SignalSpy.h

Thin wrapper around `QSignalSpy` that works inside GTest assertions:

```cpp
#define EXPECT_SIGNAL(obj, signal, count) \
    { QSignalSpy spy(obj, signal); \
      QCoreApplication::processEvents(); \
      EXPECT_EQ(spy.count(), count); }
```

### TestFixtures.h

- `QCoreApplication` singleton (created once per test binary)
- Temp dir for profile files (cleaned after each test)
- Helper to build a minimal `Profile` struct

## Layer 1: Unit Tests

No I/O, no Qt event loop needed. Fast.

### test_keystroke_parser.cpp (~30 tests)

Tests `UinputInjector::parseKeystroke()` static method.

- Single keys: A-Z, 0-9, F1-F12
- Modifiers: Ctrl, Shift, Alt, Super, Meta
- Combos: Ctrl+C, Ctrl+Shift+Z, Alt+F4, Super+D, Ctrl+Super+Left
- Special keys: Tab, Space, Enter, Escape, Delete, Up/Down/Left/Right
- Media keys: Mute, Play, VolumeUp, VolumeDown, BrightnessUp, BrightnessDown
- Navigation: Back, Forward, Home, End, PageUp, PageDown, Print
- Symbols: -, =, [, ], ;, comma, period, /, \, `, '
- Edge cases: bare "+", empty string, unknown key name, extra whitespace
- All returned keycodes verified against linux/input-event-codes.h constants

### test_button_action.cpp (~20 tests)

Tests `ButtonAction::parse()` and `ButtonAction::serialize()`.

- Round-trip for every type: Default, GestureTrigger, SmartShiftToggle, Keystroke, Media, DBus, AppLaunch
- Parse edge cases: empty string, "default", unknown prefix, no colon
- Legacy migration: "keystroke:smartshift-toggle" parses as SmartShiftToggle
- Serialize SmartShiftToggle produces "smartshift-toggle"
- Keystroke payloads with special chars: "Ctrl+Shift+Z", bare "-", bare "+"

### test_button_model.cpp (~15 tests)

- `setAction()` emits `dataChanged` AND `userActionChanged`
- `loadFromProfile()` emits `dataChanged` but NOT `userActionChanged`
- `loadFromProfile()` with fewer items than model size — remaining untouched
- `actionNameForButton()` / `actionTypeForButton()` lookup by buttonId
- Lookup for nonexistent buttonId returns empty
- Initial state has 8 buttons with correct defaults

### test_profile_model.cpp (~20 tests)

- `addProfile()` prevents duplicate wmClass (case-insensitive)
- `addProfile()` emits `profileAdded` and calls `selectTab`
- `restoreProfile()` does NOT emit `profileAdded` or call `selectTab`
- `removeProfile()` shifts `m_displayIndex` correctly
- `removeProfile()` index 0 rejected (can't remove Defaults)
- `removeProfile()` falls back to index 0 when removing displayed tab
- `selectTab()` emits `profileSwitched` with correct name
- `selectTab()` emits "default" for index 0 (not "Defaults")
- `selectTab()` no-op when already selected (no duplicate signal)
- `setHwActiveByProfileName()` updates `IsHwActiveRole` without emitting `profileSwitched`
- `setHwActiveByProfileName()` case-insensitive match
- `setHwActiveByProfileName()` unknown name defaults to index 0
- `data()` returns correct values for all roles
- `rowCount()` reflects add/remove

### test_action_model.cpp (~10 tests)

- `rowCount()` matches entry count
- `data()` returns correct name, description, actionType, payload for each role
- `payloadForName()` finds existing entry
- `payloadForName()` returns empty for unknown name
- `roleNames()` maps all roles

### test_device_model.cpp (~15 tests)

- `batteryStatusText()` formatting: "Battery: 85%", "Battery: 100% (charging)"
- `currentDPI()` returns display value when `m_hasDisplayValues` is true
- `currentDPI()` returns DeviceManager value when `m_hasDisplayValues` is false
- `setDisplayValues()` emits `settingsReloaded` (single signal)
- `setDPI()` emits `dpiChangeRequested` (does NOT change internal state)
- `setSmartShift()` emits `smartShiftChangeRequested`
- `setScrollConfig()` emits `scrollConfigChangeRequested`
- `setThumbWheelMode()` emits `thumbWheelModeChangeRequested`
- `setGestureAction()` emits both `gestureChanged` and `userGestureChanged`
- `loadGesturesFromProfile()` emits `gestureChanged` but NOT `userGestureChanged`
- `gestureActionName()` / `gestureKeystroke()` lookup
- `setActiveProfileName()` emits `activeProfileNameChanged`, no-op on same value

### test_wmclass_resolution.cpp (~10 tests)

Tests `ProfileEngine::profileForApp()`.

- Exact match: "google-chrome" → "Google Chrome"
- Case-insensitive: "Google-Chrome" → "Google Chrome"
- Short-class fallback: "org.kde.dolphin" → "Dolphin" (via last component "dolphin")
- No match returns "default"
- Empty wmClass returns "default"
- Multiple bindings, correct one selected
- Short-class doesn't false-match partial strings

### Extend existing test files (~25 tests)

- `test_action_executor.cpp`: +10 — dispatch for every ButtonAction type, launchApp, empty payload
- `test_transport.cpp`: +8 — sendRequest retry, timeout, async send, bad fd
- `test_features.cpp` / `test_scroll_features.cpp` / `test_button_features.cpp`: +7 — boundary values (DPI min/max, SmartShift 0/255, battery 0%)

## Layer 2: Integration Tests

Mocked I/O, requires Qt event loop. Tests signal chains end-to-end.

### test_app_controller.cpp (~35 tests)

AppController wired with MockDesktop, MockInjector, MockDevice.

**Profile switching:**
- Focus app with profile → hardware profile changes
- Focus app without profile → default applied
- Focus same app twice → no duplicate apply
- Desktop components (plasmashell, kwin) filtered
- Focus change updates `setHwActiveByProfileName`

**Action dispatch:**
- Diverted button press → reads from hardware profile cache, NOT ButtonModel
- Keystroke action → MockInjector receives correct combo
- SmartShiftToggle → DeviceManager.setSmartShift called with toggled value
- GestureTrigger → sets gestureActive, tracks controlId
- AppLaunch → MockInjector receives correct command
- Default action → no dispatch
- Empty payload → no dispatch

**Gesture detection:**
- dx=0, dy=0 → "click"
- dx=80, dy=5 → "right"
- dx=-80, dy=5 → "left"
- dx=5, dy=80 → "down"
- dx=5, dy=-80 → "up"
- Gesture resolves only on the gesture button's release (not other buttons)
- Gesture keystroke looked up from hardware profile gestures

**Thumb wheel:**
- Mode "volume": forward rotation → VolumeUp, backward → VolumeDown
- Mode "zoom": forward → injectCtrlScroll(+1), backward → injectCtrlScroll(-1)
- Mode "scroll": no dispatch (native)
- Accumulator threshold: deltas below 15 don't fire

**User button change:**
- setAction from QML → userActionChanged → saveCurrentProfile to displayed profile
- Button divert only when editing active hardware profile

**Tab switching:**
- selectTab → setDisplayProfile → onDisplayProfileChanged → pushes values to DeviceModel + restores ButtonModel
- Tab switch closes side panel (selectedButton = -1)

### test_profile_switching.cpp (~20 tests)

- `createProfileForApp()` doesn't overwrite existing cache entry
- `createProfileForApp()` creates new profile as copy of default
- `restoreProfile()` at startup doesn't trigger `createProfileForApp`
- Display profile and hardware profile can differ simultaneously
- New profile auto-switches to new tab
- Profile removal falls back to default
- Profile removal updates hw indicator if removed profile was hw-active
- `profileForApp()` returns correct profile after add/remove cycles
- Settings saved to displayed profile, not hardware profile
- DPI/SmartShift/scroll changes save to correct profile

### test_profile_persistence.cpp (~15 tests)

Uses temp directory for profile files.

- Save profile → reload from disk → all fields match
- All ButtonAction types survive save/load round-trip
- Gesture assignments survive round-trip
- App bindings save/load round-trip
- Profile with all defaults saves correctly
- Clear INI before save prevents duplicate sections
- Missing config dir created automatically
- Corrupted INI: missing sections → defaults used
- Corrupted INI: invalid values → defaults used
- createProfileForApp saves to disk immediately
- removeAppProfile removes file, cache entry, and binding

### test_device_reconnect.cpp (~10 tests)

- onDeviceSetupComplete re-applies current hw profile (not default)
- First connect applies default profile
- touchResponseTime prevents false sleep/wake re-enumeration
- Button diversions restored after reconnect
- Thumb wheel mode restored after reconnect

### test_device_manager.cpp (~25 tests)

DeviceManager with MockTransport.

- setDPI sends correct HID++ params
- setSmartShift sends correct mode byte and threshold
- setScrollConfig sends correct hiRes + invert flags
- setThumbWheelMode "scroll" → divert=false, others → divert=true
- divertButton sends correct CID with divert/rawXY flags
- handleNotification routes battery events correctly
- handleNotification routes diverted button press/release
- handleNotification routes thumb wheel rotation
- handleNotification routes gesture rawXY
- Sleep/wake detection disabled (no re-enumeration on notification gap)
- deviceConnected state tracks connect/disconnect
- Multiple scan cycles don't duplicate devices

## Layer 3: System Tests

Separate binary, requires hardware and/or KDE. Not built by default.

### test_hidraw_io.cpp (~8 tests)

- Open real hidraw device for Logitech mouse
- Send IRoot ping, receive valid response
- Read with timeout returns within timeout period
- Write to wrong interface returns error (not crash)
- Handle device disconnect mid-read gracefully
- Multiple open/close cycles don't leak fds
- Invalid devNode path returns error on open
- Concurrent read/write doesn't crash

### test_uinput_io.cpp (~10 tests)

- Init creates /dev/input/eventN device
- Inject single key (KEY_A) → readable from evdev
- Inject combo (Ctrl+C) → correct key sequence (ctrl down, c down, c up, ctrl up)
- Inject Ctrl+Scroll → correct EV_REL events
- Inject bare "+" → KEY_KPPLUS
- Inject bare "-" → KEY_MINUS
- All registered keys produce events (no missing registrations)
- Shutdown removes virtual device
- Double init doesn't crash
- Inject after shutdown is no-op

### test_kde_focus.cpp (~7 tests)

- KWin script installs via D-Bus
- Focus callback fires on window switch
- Correct wmClass reported for KDE apps (org.kde.dolphin)
- Correct wmClass for non-KDE apps (google-chrome)
- Duplicate filter suppresses same-window repeat
- Poll timer stops after script installs
- blockGlobalShortcuts sends D-Bus call

### test_desktop_apps.cpp (~5 tests)

- runningApplications returns entries from /usr/share/applications
- completeBaseName used (not baseName) — org.kde.dolphin not "org"
- NoDisplay=true apps excluded
- Type != Application excluded
- Deduplication by wmClass works

### test_app_startup.cpp (~5 tests)

- App launches without crash
- QML root objects created
- System tray icon appears
- Device detected and profile applied
- App handles no-device-connected state

## Layer 4: QML/UI Tests

Qt Quick Test framework. Separate binary with mock C++ models.

### test_AppProfileBar.qml (~8 tests)

- Chips render from ProfileModel data
- Click chip calls ProfileModel.selectTab
- Right-click chip opens context menu
- Scroll arrows appear when chips overflow
- Scroll arrows hidden when chips fit
- Mouse wheel scrolls chip area
- Add button opens dropdown popup
- Search filters app list

### test_ActionsPanel.qml (~7 tests)

- Action list highlights matching currentAction
- Custom keystroke highlights "Keyboard shortcut" entry
- Keystroke capture pre-fills with current custom keystroke
- Wheel mode list reflects DeviceModel.thumbWheelMode
- Wheel mode click emits wheelModeSelected (user-only)
- Action click emits actionSelected
- Gesture preset click sets all 5 directions

### test_DetailPanel.qml (~5 tests)

- DPI slider commits on release only (onPressedChanged, not onValueChanged)
- Radio buttons use onClicked (not onCheckedChanged)
- Toggles use onToggled (not onCheckedChanged)
- SmartShift slider visible only when toggle is on
- Smooth scrolling toggle calls setScrollConfig

## Build Configuration

### CMakeLists.txt changes

```cmake
# tests/CMakeLists.txt
# Layer 1 + 2: always built
add_executable(logitune-tests
    # ... existing test files ...
    # New unit tests
    test_keystroke_parser.cpp
    test_button_action.cpp
    test_button_model.cpp
    test_profile_model.cpp
    test_action_model.cpp
    test_device_model.cpp
    test_wmclass_resolution.cpp
    # New integration tests
    test_app_controller.cpp
    test_profile_switching.cpp
    test_profile_persistence.cpp
    test_device_reconnect.cpp
    test_device_manager.cpp
)
target_link_libraries(logitune-tests PRIVATE
    logitune-core logitune-app-lib GTest::gtest_main Qt6::Core Qt6::Quick Qt6::Test)

# Layer 3: opt-in
option(BUILD_SYSTEM_TESTS "Build system tests (needs hardware)" OFF)
if(BUILD_SYSTEM_TESTS)
    add_subdirectory(system)
endif()

# Layer 4: opt-in
option(BUILD_QML_TESTS "Build QML tests (needs display)" OFF)
if(BUILD_QML_TESTS)
    add_subdirectory(qml)
endif()
```

## Success Criteria

- All Layer 1+2 tests pass in CI with no hardware
- Every bug fixed in this session has a regression test
- Every public method on AppController, models, and ProfileEngine has at least one test
- Every ActionModel entry has a dispatch test
- Every ButtonAction type has a parse/serialize round-trip test
- Every key name in parseKeystroke has a mapping test
- Test run completes in < 5 seconds (Layer 1+2)
