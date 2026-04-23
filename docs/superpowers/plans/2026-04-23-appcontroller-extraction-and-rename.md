# AppController Extraction and Rename to AppRoot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract behavior out of `AppController` into 4 focused services (`DeviceSelection`, `DeviceCommands`, `ButtonActionDispatcher`, `ProfileOrchestrator`), move translation helpers to `ActionModel`, add a contract docstring, and rename `AppController` to `AppRoot`. Preserve all existing behavior.

**Architecture:** `AppRoot` becomes a pure composition root: owns singletons, wires signals, handles runtime device lifecycle, exposes ViewModels for QML registration. All user-facing behavior lives in services. Services hold pointers only to models, engines, and `DeviceSelection` — never to other services. All `connect()` calls live in `AppRoot`.

**Tech Stack:** C++20, Qt 6, GoogleTest, CMake

**Spec:** `docs/superpowers/specs/2026-04-23-appcontroller-extraction-design.md`

---

## File Structure

Files created across all tasks:

- `src/app/services/DeviceSelection.h` / `.cpp` (Task 1)
- `src/app/services/DeviceCommands.h` / `.cpp` (Task 2)
- `src/app/services/ButtonActionDispatcher.h` / `.cpp` (Task 3)
- `src/app/services/ProfileOrchestrator.h` / `.cpp` (Task 4)
- `tests/services/DeviceSelectionFixture.h` (Task 1)
- `tests/services/DeviceCommandsFixture.h` (Task 2)
- `tests/services/ButtonActionDispatcherFixture.h` (Task 3)
- `tests/services/ProfileOrchestratorFixture.h` (Task 4)
- `tests/services/test_device_selection.cpp` (Task 1)
- `tests/services/test_device_commands.cpp` (Task 2)
- `tests/services/test_button_action_dispatcher.cpp` (Task 3)
- `tests/services/test_profile_orchestrator.cpp` (Task 4)

Files modified:

- `src/app/AppController.h` / `.cpp` — Tasks 1-6 (code removal + integration); renamed to `AppRoot` in Task 7
- `src/app/CMakeLists.txt` — Tasks 1-4 (add services); Task 7 (rename source)
- `tests/CMakeLists.txt` — Tasks 1-4 (add test sources); Task 7 (rename test source and fixture references)
- `src/app/models/ActionModel.h` / `.cpp` — Task 5 (add translation helpers)
- `src/app/main.cpp` — Task 7 (rename)
- `tests/helpers/AppControllerFixture.h` — Task 7 (renamed to `AppRootFixture.h`)
- All `.cpp` files that `#include "AppController.h"` — Task 7

Dependency order between tasks:

- Task 1 blocks 2, 3, 4 (they use `DeviceSelection`)
- Task 4 depends on 2 and 3 (needs their signals for wiring)
- Task 5 depends on 4 (updates `ProfileOrchestrator` callers)
- Task 6 depends on 5
- Task 7 is last (pure rename)

---

## Task 1: Extract `DeviceSelection` service

**Files:**
- Create: `src/app/services/DeviceSelection.h`
- Create: `src/app/services/DeviceSelection.cpp`
- Create: `tests/services/DeviceSelectionFixture.h`
- Create: `tests/services/test_device_selection.cpp`
- Modify: `src/app/AppController.h` (remove `selectedDevice/Session/Serial`, `onSelectedDeviceChanged`)
- Modify: `src/app/AppController.cpp` (construct `m_deviceSelection`, redirect callers of old helpers)
- Modify: `src/app/CMakeLists.txt` (add new sources)
- Modify: `tests/CMakeLists.txt` (add new test sources)

### Step 1: Create `src/app/services/DeviceSelection.h`

- [ ] **Step 1: Write the service header**

```cpp
#pragma once
#include <QObject>
#include <QString>

namespace logitune {

class DeviceManager;
class DeviceModel;
class PhysicalDevice;
class DeviceSession;
class ProfileModel;

/// Resolves the currently selected PhysicalDevice / DeviceSession / serial
/// from ProfileModel's selection index and DeviceManager's device list.
/// Emits selectionChanged whenever the resolution changes.
///
/// Read-only, single source of truth. Other services hold a pointer to this
/// and either query on demand (active*()) or subscribe to selectionChanged.
class DeviceSelection : public QObject {
    Q_OBJECT
public:
    DeviceSelection(DeviceManager *deviceManager,
                    DeviceModel *deviceModel,
                    ProfileModel *profileModel,
                    QObject *parent = nullptr);

    PhysicalDevice *activeDevice() const;
    DeviceSession  *activeSession() const;
    QString         activeSerial() const;

signals:
    void selectionChanged();

public slots:
    /// Called when the ProfileModel selection index changes. Recomputes
    /// the active pointers and emits selectionChanged if they changed.
    void onSelectionIndexChanged();

private:
    DeviceManager *m_deviceManager;
    DeviceModel   *m_deviceModel;
    ProfileModel  *m_profileModel;
};

} // namespace logitune
```

- [ ] **Step 2: Write the implementation at `src/app/services/DeviceSelection.cpp`**

```cpp
#include "DeviceSelection.h"
#include "DeviceManager.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "models/DeviceModel.h"
#include "models/ProfileModel.h"

namespace logitune {

DeviceSelection::DeviceSelection(DeviceManager *deviceManager,
                                 DeviceModel *deviceModel,
                                 ProfileModel *profileModel,
                                 QObject *parent)
    : QObject(parent)
    , m_deviceManager(deviceManager)
    , m_deviceModel(deviceModel)
    , m_profileModel(profileModel)
{}

PhysicalDevice *DeviceSelection::activeDevice() const
{
    if (!m_deviceModel) return nullptr;
    const int idx = m_deviceModel->selectedIndex();
    if (idx < 0) return nullptr;
    const auto list = m_deviceManager ? m_deviceManager->physicalDevices() : QList<PhysicalDevice *>{};
    if (idx >= list.size()) return nullptr;
    return list.at(idx);
}

DeviceSession *DeviceSelection::activeSession() const
{
    auto *d = activeDevice();
    return d ? d->primary() : nullptr;
}

QString DeviceSelection::activeSerial() const
{
    auto *d = activeDevice();
    return d ? d->deviceSerial() : QString();
}

void DeviceSelection::onSelectionIndexChanged()
{
    emit selectionChanged();
}

} // namespace logitune
```

Note: Check the actual `DeviceModel` and `DeviceManager` API — the `selectedIndex()` accessor name may differ. Match it to whatever `AppController::selectedDevice()` currently uses.

- [ ] **Step 3: Update `src/app/CMakeLists.txt` — add to the `logitune-app-lib` source list**

```cmake
add_library(logitune-app-lib STATIC
    AppController.cpp
    services/DeviceSelection.cpp      # ← add
    models/DeviceModel.cpp
    ...
)
```

Also extend include directories if needed:

```cmake
target_include_directories(logitune-app-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services   # ← add
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)
```

- [ ] **Step 4: Write `tests/services/DeviceSelectionFixture.h`**

```cpp
#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <memory>
#include "services/DeviceSelection.h"
#include "DeviceManager.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"

namespace logitune::test {

class DeviceSelectionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        m_deviceManager  = std::make_unique<DeviceManager>();
        m_deviceModel    = std::make_unique<DeviceModel>();
        m_profileModel   = std::make_unique<ProfileModel>();
        m_selection = std::make_unique<DeviceSelection>(
            m_deviceManager.get(), m_deviceModel.get(), m_profileModel.get());
    }

    std::unique_ptr<DeviceManager>   m_deviceManager;
    std::unique_ptr<DeviceModel>     m_deviceModel;
    std::unique_ptr<ProfileModel>    m_profileModel;
    std::unique_ptr<DeviceSelection> m_selection;
};

} // namespace logitune::test
```

- [ ] **Step 5: Write `tests/services/test_device_selection.cpp`**

```cpp
#include "services/DeviceSelectionFixture.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(DeviceSelectionFixture, NoDevicesReturnsNulls) {
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
    EXPECT_EQ(m_selection->activeSession(), nullptr);
    EXPECT_TRUE(m_selection->activeSerial().isEmpty());
}

TEST_F(DeviceSelectionFixture, OutOfRangeIndexReturnsNulls) {
    m_deviceModel->setSelectedIndex(99);
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
}

TEST_F(DeviceSelectionFixture, SelectionChangedEmitsOnSlot) {
    QSignalSpy spy(m_selection.get(), &DeviceSelection::selectionChanged);
    m_selection->onSelectionIndexChanged();
    EXPECT_EQ(spy.count(), 1);
}
```

- [ ] **Step 6: Add the test source to `tests/CMakeLists.txt`**

```cmake
add_executable(logitune-tests
    test_main.cpp
    ...
    services/test_device_selection.cpp   # ← add
)
```

- [ ] **Step 7: Build and run the new tests**

```bash
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='DeviceSelectionFixture.*'
```

Expected: all 3 tests pass.

- [ ] **Step 8: Integrate into `AppController`**

In `src/app/AppController.h`:

- Add `#include "services/DeviceSelection.h"` near the top
- Add member: `DeviceSelection m_deviceSelection{&m_deviceManager, &m_deviceModel, &m_profileModel, this};` (or construct in ctor body to control ordering)
- Remove declarations of `selectedDevice()`, `selectedSession()`, `selectedSerial()`, `onSelectedDeviceChanged()`

In `src/app/AppController.cpp`:

- Delete the bodies of `selectedDevice()`, `selectedSession()`, `selectedSerial()`, `onSelectedDeviceChanged()`
- Replace every internal call to `selectedDevice()` → `m_deviceSelection.activeDevice()`
- Replace every internal call to `selectedSession()` → `m_deviceSelection.activeSession()`
- Replace every internal call to `selectedSerial()` → `m_deviceSelection.activeSerial()`
- In `wireSignals()`, add: `connect(&m_profileModel, &ProfileModel::selectedDeviceIndexChanged, &m_deviceSelection, &DeviceSelection::onSelectionIndexChanged);` (check actual `ProfileModel` signal name and wire accordingly)

- [ ] **Step 9: Build and run the full test suite**

```bash
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests
```

Expected: all tests pass, including existing `AppControllerFixture` tests (which still exercise the full graph).

- [ ] **Step 10: Smoke test on real hardware (MX Master 3S)**

```bash
pkill -x logitune || true
cmake --build build -j$(nproc)
nohup ./build/src/app/logitune --debug > /tmp/logitune-task1.log 2>&1 & disown
```

Manually verify: device is detected, switching devices in the sidebar still works, DPI slider still applies, profile tabs still switch correctly.

- [ ] **Step 11: Commit**

```bash
git add src/app/services/DeviceSelection.h src/app/services/DeviceSelection.cpp \
        tests/services/DeviceSelectionFixture.h tests/services/test_device_selection.cpp \
        src/app/AppController.h src/app/AppController.cpp \
        src/app/CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor(app): extract DeviceSelection service from AppController

Resolves active PhysicalDevice / DeviceSession / serial from ProfileModel
selection + DeviceManager. Replaces the three selectedDevice/Session/Serial
helpers and onSelectedDeviceChanged slot on AppController.

Part of #107."
```

---

## Task 2: Extract `DeviceCommands` service

**Files:**
- Create: `src/app/services/DeviceCommands.h` / `.cpp`
- Create: `tests/services/DeviceCommandsFixture.h`
- Create: `tests/services/test_device_commands.cpp`
- Modify: `src/app/AppController.h` (remove the 5 `*ChangeRequested` slots)
- Modify: `src/app/AppController.cpp` (remove slot bodies, wire DeviceCommands into signal graph)
- Modify: `src/app/CMakeLists.txt` and `tests/CMakeLists.txt`

### Steps

- [ ] **Step 1: Write `src/app/services/DeviceCommands.h`**

```cpp
#pragma once
#include <QObject>
#include <QString>

namespace logitune {

class DeviceSelection;

/// Routes UI change requests (from DeviceModel signals) to the active
/// DeviceSession. Emits userChangedSomething() after each mutation so
/// ProfileOrchestrator can trigger a save.
///
/// No-op if there is no active session (DeviceSelection returns null).
class DeviceCommands : public QObject {
    Q_OBJECT
public:
    explicit DeviceCommands(DeviceSelection *selection, QObject *parent = nullptr);

public slots:
    void requestDpi(int value);
    void requestSmartShift(bool enabled, int threshold);
    void requestScrollConfig(bool hiRes, bool invert);
    void requestThumbWheelMode(const QString &mode);
    void requestThumbWheelInvert(bool invert);

signals:
    /// Emitted after any successful mutation. Subscribers (ProfileOrchestrator)
    /// should call saveCurrentProfile() in response.
    void userChangedSomething();

private:
    DeviceSelection *m_selection;
};

} // namespace logitune
```

- [ ] **Step 2: Write `src/app/services/DeviceCommands.cpp`**

```cpp
#include "DeviceCommands.h"
#include "DeviceSelection.h"
#include "DeviceSession.h"

namespace logitune {

DeviceCommands::DeviceCommands(DeviceSelection *selection, QObject *parent)
    : QObject(parent)
    , m_selection(selection)
{}

void DeviceCommands::requestDpi(int value)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setDPI(value);
    emit userChangedSomething();
}

void DeviceCommands::requestSmartShift(bool enabled, int threshold)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setSmartShift(enabled, threshold);
    emit userChangedSomething();
}

void DeviceCommands::requestScrollConfig(bool hiRes, bool invert)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setScrollConfig(hiRes, invert);
    emit userChangedSomething();
}

void DeviceCommands::requestThumbWheelMode(const QString &mode)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    // Match the behavior at AppController.cpp:654-673 — reset thumb
    // accumulator is done by ButtonActionDispatcher on profileApplied,
    // not here. This slot only forwards.
    session->setThumbWheelMode(mode, session->thumbWheelInvert());
    emit userChangedSomething();
}

void DeviceCommands::requestThumbWheelInvert(bool invert)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setThumbWheelMode(session->thumbWheelMode(), invert);
    emit userChangedSomething();
}

} // namespace logitune
```

Cross-reference against `AppController.cpp:604-688` to confirm the two-arg shape of `setThumbWheelMode` / existing getter names; adjust if the real `DeviceSession` API differs.

- [ ] **Step 3: Add `services/DeviceCommands.cpp` to `src/app/CMakeLists.txt`**

- [ ] **Step 4: Write `tests/services/DeviceCommandsFixture.h`**

```cpp
#pragma once
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <memory>
#include "services/DeviceCommands.h"
#include "services/DeviceSelection.h"
#include "DeviceManager.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"
#include "hidpp/HidrawDevice.h"

namespace logitune::test {

class DeviceCommandsFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        m_deviceManager = std::make_unique<DeviceManager>();
        m_deviceModel   = std::make_unique<DeviceModel>();
        m_profileModel  = std::make_unique<ProfileModel>();
        m_selection = std::make_unique<DeviceSelection>(
            m_deviceManager.get(), m_deviceModel.get(), m_profileModel.get());
        m_commands = std::make_unique<DeviceCommands>(m_selection.get());
    }

    /// Attach a mock session so the commands have a target. Mirrors the
    /// mock-session pattern in AppControllerFixture.
    void attachMockSession() {
        m_device.setupMxControls();
        auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_session = new DeviceSession(std::move(hidraw), 0xFF, "Bluetooth", nullptr);
        m_session->m_connected = true;
        m_session->m_activeDevice = &m_device;
        m_physical = new PhysicalDevice(QStringLiteral("mock-serial"));
        m_physical->attachTransport(m_session);
        m_deviceManager->addPhysicalDevice(m_physical);
        m_deviceModel->addPhysicalDevice(m_physical);
        m_deviceModel->setSelectedIndex(0);
    }

    std::unique_ptr<DeviceManager>   m_deviceManager;
    std::unique_ptr<DeviceModel>     m_deviceModel;
    std::unique_ptr<ProfileModel>    m_profileModel;
    std::unique_ptr<DeviceSelection> m_selection;
    std::unique_ptr<DeviceCommands>  m_commands;
    MockDevice       m_device;
    DeviceSession   *m_session  = nullptr;
    PhysicalDevice  *m_physical = nullptr;
};

} // namespace logitune::test
```

- [ ] **Step 5: Write `tests/services/test_device_commands.cpp`**

```cpp
#include "services/DeviceCommandsFixture.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(DeviceCommandsFixture, NullSessionNoOpDoesNotEmit) {
    QSignalSpy spy(m_commands.get(), &DeviceCommands::userChangedSomething);
    m_commands->requestDpi(1600);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceCommandsFixture, RequestDpiCallsSetDPI) {
    attachMockSession();
    m_commands->requestDpi(1600);
    EXPECT_EQ(m_session->currentDPI(), 1600);
}

TEST_F(DeviceCommandsFixture, RequestDpiEmitsUserChangedExactlyOnce) {
    attachMockSession();
    QSignalSpy spy(m_commands.get(), &DeviceCommands::userChangedSomething);
    m_commands->requestDpi(1600);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceCommandsFixture, RequestSmartShiftForwardsBothArgs) {
    attachMockSession();
    m_commands->requestSmartShift(true, 200);
    EXPECT_TRUE(m_session->smartShiftEnabled());
    EXPECT_EQ(m_session->smartShiftThreshold(), 200);
}

TEST_F(DeviceCommandsFixture, RequestScrollConfigForwardsBothArgs) {
    attachMockSession();
    m_commands->requestScrollConfig(true, false);
    // Verify via session getters; exact assertions depend on DeviceSession API
}

TEST_F(DeviceCommandsFixture, RequestThumbWheelModeForwardsString) {
    attachMockSession();
    m_commands->requestThumbWheelMode(QStringLiteral("hscroll"));
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("hscroll"));
}

TEST_F(DeviceCommandsFixture, RequestThumbWheelInvertForwardsBool) {
    attachMockSession();
    m_commands->requestThumbWheelInvert(true);
    EXPECT_TRUE(m_session->thumbWheelInvert());
}
```

Add `services/test_device_commands.cpp` to `tests/CMakeLists.txt` alongside `test_device_selection.cpp`.

- [ ] **Step 6: Integrate into `AppController`**

In `AppController.h`:
- Add `#include "services/DeviceCommands.h"`
- Add member: `DeviceCommands m_deviceCommands{&m_deviceSelection, this};`
- Remove the 5 `on*ChangeRequested` slot declarations from `private slots:`

In `AppController.cpp`:
- Delete the 5 slot bodies (lines 604-688)
- In `wireSignals()`, replace `connect(&m_deviceModel, &DeviceModel::dpiChangeRequested, this, &AppController::onDpiChangeRequested);` (and the 4 siblings) with `connect(&m_deviceModel, &DeviceModel::dpiChangeRequested, &m_deviceCommands, &DeviceCommands::requestDpi);` etc.
- Add: `connect(&m_deviceCommands, &DeviceCommands::userChangedSomething, this, &AppController::saveCurrentProfile);` (temporary; gets redirected to `ProfileOrchestrator` in Task 4)

- [ ] **Step 7: Build, run all tests, smoke test, commit**

```bash
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests
# Smoke test: change DPI, SmartShift, scroll config in UI; confirm each still applies and saves to the profile.
git add -A && git commit -m "refactor(app): extract DeviceCommands service from AppController

Move the 5 *ChangeRequested passthrough slots (DPI, SmartShift, scroll,
thumbwheel mode, thumbwheel invert) into a dedicated service that holds
a DeviceSelection pointer and emits userChangedSomething after each
successful mutation.

AppController temporarily subscribes to userChangedSomething -> saveCurrentProfile;
this wiring moves to ProfileOrchestrator in a later commit.

Part of #107."
```

---

## Task 3: Extract `ButtonActionDispatcher` service

**Files:**
- Create: `src/app/services/ButtonActionDispatcher.h` / `.cpp`
- Create: `tests/services/ButtonActionDispatcherFixture.h`
- Create: `tests/services/test_button_action_dispatcher.cpp`
- Modify: `src/app/AppController.h` (remove `onDivertedButtonPressed`, `onThumbWheelRotation`, `PerDeviceState`, `m_perDeviceState`, `kGestureThreshold`, `kThumbThreshold`)
- Modify: `src/app/AppController.cpp` (remove slot bodies, rewire per-device lambdas in `onPhysicalDeviceAdded`)
- Modify: `src/app/CMakeLists.txt` and `tests/CMakeLists.txt`

### Steps

- [ ] **Step 1: Write `src/app/services/ButtonActionDispatcher.h`**

```cpp
#pragma once
#include <QMap>
#include <QObject>
#include <QString>
#include <cstdint>

namespace logitune {

class ActionExecutor;
class DeviceSelection;
class ProfileEngine;
class IDevice;

/// Turns raw HID++ input events (gestureRawXY, divertedButtonPressed,
/// thumbWheelRotation) into high-level actions (SmartShift toggle, DPI
/// cycle, keystroke injection, gesture direction, app launch).
///
/// Owns the per-device gesture + thumb wheel accumulator state. Resets
/// thumbAccum for a serial when ProfileOrchestrator emits profileApplied.
class ButtonActionDispatcher : public QObject {
    Q_OBJECT
public:
    ButtonActionDispatcher(ProfileEngine *profileEngine,
                           ActionExecutor *actionExecutor,
                           DeviceSelection *selection,
                           QObject *parent = nullptr);

    void onDeviceRemoved(const QString &serial);

public slots:
    void onGestureRaw(int16_t dx, int16_t dy);
    void onDivertedButtonPressed(uint16_t controlId, bool pressed);
    void onThumbWheelRotation(int delta);
    void onProfileApplied(const QString &serial);
    void onCurrentDeviceChanged(const IDevice *device);

private:
    struct PerDeviceState {
        int gestureAccumX = 0;
        int gestureAccumY = 0;
        int thumbAccum = 0;
        bool gestureActive = false;
        uint16_t gestureControlId = 0;
    };
    static constexpr int kGestureThreshold = 50;
    static constexpr int kThumbThreshold = 15;

    ProfileEngine   *m_profileEngine;
    ActionExecutor  *m_actionExecutor;
    DeviceSelection *m_selection;
    const IDevice   *m_currentDevice = nullptr;
    QMap<QString, PerDeviceState> m_state;
};

} // namespace logitune
```

- [ ] **Step 2: Write `src/app/services/ButtonActionDispatcher.cpp`**

Move the bodies of `AppController::onDivertedButtonPressed` (`AppController.cpp:690-762`), `AppController::onThumbWheelRotation` (`:763-800`), and the `gestureRawXY` lambda inner logic (`AppController.cpp:189-196`) into the corresponding slots. Replace `selectedSession()`/`selectedSerial()` with `m_selection->activeSession()`/`m_selection->activeSerial()`. Replace `m_currentDevice` with the locally tracked `m_currentDevice` (updated by `onCurrentDeviceChanged`). Replace `m_actionExecutor.injectKeystroke(...)` with `m_actionExecutor->injectKeystroke(...)`.

Key implementation detail — `onProfileApplied` just clears `thumbAccum`:

```cpp
void ButtonActionDispatcher::onProfileApplied(const QString &serial)
{
    auto it = m_state.find(serial);
    if (it != m_state.end()) it->thumbAccum = 0;
}

void ButtonActionDispatcher::onDeviceRemoved(const QString &serial)
{
    m_state.remove(serial);
}
```

- [ ] **Step 3: Write fixture and tests**

`tests/services/ButtonActionDispatcherFixture.h` constructs: `ProfileEngine` (real), `MockInjector` (existing), `ActionExecutor` (real, using mock), `DeviceSelection` with a mock device setup, `ButtonActionDispatcher`.

Tests in `tests/services/test_button_action_dispatcher.cpp`:

```cpp
TEST_F(ButtonActionDispatcherFixture, GestureStartAccumulatesXY) { ... }
TEST_F(ButtonActionDispatcherFixture, GestureReleaseBelowThresholdDispatchesClick) { ... }
TEST_F(ButtonActionDispatcherFixture, GestureReleaseUpDownLeftRight) { ... }  // parameterized
TEST_F(ButtonActionDispatcherFixture, ThumbWheelAccumulatesBelowThreshold) { ... }
TEST_F(ButtonActionDispatcherFixture, ThumbWheelEmitsStepsAtThreshold) { ... }
TEST_F(ButtonActionDispatcherFixture, SmartShiftTogglesOnButtonPress) { ... }
TEST_F(ButtonActionDispatcherFixture, DpiCycleTriggersOnButtonPress) { ... }
TEST_F(ButtonActionDispatcherFixture, KeystrokeButtonInjectsKeystroke) { ... }
TEST_F(ButtonActionDispatcherFixture, AppLaunchButtonInvokesExecutor) { ... }
TEST_F(ButtonActionDispatcherFixture, OnProfileAppliedClearsThumbAccum) { ... }
TEST_F(ButtonActionDispatcherFixture, OnDeviceRemovedDropsEntry) { ... }
```

- [ ] **Step 4: Integrate into `AppController`**

In `AppController.h`:
- Add `#include "services/ButtonActionDispatcher.h"`
- Add member: `ButtonActionDispatcher m_buttonDispatcher{&m_profileEngine, &m_actionExecutor, &m_deviceSelection, this};`
- Remove `onDivertedButtonPressed`, `onThumbWheelRotation` declarations
- Remove `PerDeviceState` struct, `m_perDeviceState`, `kGestureThreshold`, `kThumbThreshold`

In `AppController.cpp::onPhysicalDeviceAdded` (line ~178):
- Replace the `gestureRawXY` lambda with: `connect(device, &PhysicalDevice::gestureRawXY, &m_buttonDispatcher, &ButtonActionDispatcher::onGestureRaw);`
- Replace the `divertedButtonPressed` lambda with: `connect(device, &PhysicalDevice::divertedButtonPressed, &m_buttonDispatcher, &ButtonActionDispatcher::onDivertedButtonPressed);`
- Replace the `thumbWheelRotation` lambda with: `connect(device, &PhysicalDevice::thumbWheelRotation, &m_buttonDispatcher, &ButtonActionDispatcher::onThumbWheelRotation);`
- In the `transportSetupComplete` lambda, after the profile apply, the dispatcher's thumbAccum reset happens via signal from ProfileOrchestrator (wired in Task 4); for now, call `m_buttonDispatcher.onProfileApplied(serial);` directly until Task 4 wires the signal.

In `AppController.cpp::onPhysicalDeviceRemoved`:
- Add: `m_buttonDispatcher.onDeviceRemoved(deviceId);`

In `AppController.cpp::applyProfileToHardware` (line ~529):
- Remove the two lines `auto &state = m_perDeviceState[...]; state.thumbAccum = 0;` (they'll move into `ButtonActionDispatcher::onProfileApplied` which is called post-apply).

In `AppController.cpp::wireSignals()`:
- Add a wiring for `m_currentDevice` propagation: when a device is selected, tell the dispatcher. For now, update `m_buttonDispatcher.onCurrentDeviceChanged(m_currentDevice)` inside existing device-selection code paths. (In Task 4, this becomes a signal.)

- [ ] **Step 5: Build, run all tests, smoke test, commit**

Smoke test must include: gesture button press-and-drag in all 4 directions triggers the right keystrokes; thumb wheel rotation produces correct number of scroll events; SmartShift toggle button works; DPI cycle button works.

```bash
git add -A && git commit -m "refactor(app): extract ButtonActionDispatcher service from AppController

Move onDivertedButtonPressed, onThumbWheelRotation, PerDeviceState, and
the gestureRawXY aggregation lambda into a dedicated service. The service
owns the per-device gesture + thumb accumulator state as private
implementation detail. Gesture and thumb thresholds become private
static constants.

AppController's onPhysicalDeviceAdded now connects PhysicalDevice signals
directly to dispatcher slots instead of to AppController lambdas.

Part of #107."
```

---

## Task 4: Extract `ProfileOrchestrator` service

**Files:**
- Create: `src/app/services/ProfileOrchestrator.h` / `.cpp`
- Create: `tests/services/ProfileOrchestratorFixture.h`
- Create: `tests/services/test_profile_orchestrator.cpp`
- Modify: `src/app/AppController.h` (remove the 9 profile methods/slots, plus `m_currentDevice` state)
- Modify: `src/app/AppController.cpp` (remove their bodies; wire the signal graph)
- Modify: `src/app/CMakeLists.txt` and `tests/CMakeLists.txt`

### Steps

- [ ] **Step 1: Write `src/app/services/ProfileOrchestrator.h`**

```cpp
#pragma once
#include <QObject>
#include <QString>
#include "ButtonAction.h"
#include "Profile.h"

namespace logitune {

class ActionExecutor;
class ButtonModel;
class DeviceModel;
class DeviceSelection;
class IDevice;
class IDesktopIntegration;
class PhysicalDevice;
class ProfileEngine;
class ProfileModel;

/// Owns the save / apply / push / window-focus flow. Holds no device
/// state of its own; reads from models and engines, writes to them, and
/// emits profileApplied(serial) after every hardware apply.
class ProfileOrchestrator : public QObject {
    Q_OBJECT
public:
    ProfileOrchestrator(ProfileEngine *profileEngine,
                        ActionExecutor *actionExecutor,
                        DeviceSelection *selection,
                        DeviceModel *deviceModel,
                        ButtonModel *buttonModel,
                        ProfileModel *profileModel,
                        QObject *parent = nullptr);

    void setupProfileForDevice(PhysicalDevice *device);
    void applyProfileToHardware(const Profile &p);
    void saveCurrentProfile();

public slots:
    void onUserButtonChanged(int buttonId, const QString &actionName, const QString &actionType);
    void onTabSwitched(const QString &profileName);
    void onDisplayProfileChanged(const QString &serial, const Profile &profile);
    void onWindowFocusChanged(const QString &wmClass, const QString &title);
    void onTransportSetupComplete(PhysicalDevice *device);
    void onCurrentDeviceChanged(const IDevice *device);

signals:
    /// Emitted after applyProfileToHardware finishes. ButtonActionDispatcher
    /// subscribes to clear its thumbAccum for the given serial.
    void profileApplied(const QString &serial);

    /// Emitted on every m_currentDevice update so ButtonActionDispatcher
    /// can update its own m_currentDevice pointer.
    void currentDeviceChanged(const IDevice *device);

private:
    void pushDisplayValues(const Profile &p);
    void restoreButtonModelFromProfile(const Profile &p);

    ProfileEngine   *m_profileEngine;
    ActionExecutor  *m_actionExecutor;
    DeviceSelection *m_selection;
    DeviceModel     *m_deviceModel;
    ButtonModel     *m_buttonModel;
    ProfileModel    *m_profileModel;
    const IDevice   *m_currentDevice = nullptr;
};

} // namespace logitune
```

- [ ] **Step 2: Write `src/app/services/ProfileOrchestrator.cpp`**

Move the bodies of these `AppController` methods verbatim, replacing `selectedSession()`/`selectedSerial()`/`selectedDevice()` with `m_selection->activeSession()`/`activeSerial()`/`activeDevice()`, and replacing `m_profileEngine` (value) with `m_profileEngine` (pointer) access:

- `applyProfileToHardware` (at `AppController.cpp:529`) — at the end, `emit profileApplied(serial);`
- `saveCurrentProfile` (`:565`)
- `pushDisplayValues` (`:596`)
- `restoreButtonModelFromProfile` (`:442`)
- `setupProfileForDevice` (`:256`)
- `onUserButtonChanged` (`:360`)
- `onTabSwitched` (`:387`)
- `onDisplayProfileChanged` (`:394`)
- `onWindowFocusChanged` (`:409`)

New helper `onTransportSetupComplete` that replaces the inline lambda in `onPhysicalDeviceAdded`:

```cpp
void ProfileOrchestrator::onTransportSetupComplete(PhysicalDevice *device)
{
    m_currentDevice = device->descriptor();
    emit currentDeviceChanged(m_currentDevice);
    const QString serial = device->deviceSerial();
    Profile &p = m_profileEngine->cachedProfile(serial,
                                                 m_profileEngine->hardwareProfile(serial));
    applyProfileToHardware(p);
}
```

Note: `buttonActionToName` and `buttonEntryToAction` are used by `saveCurrentProfile` and `restoreButtonModelFromProfile`. For this task, inline them as private member functions of `ProfileOrchestrator` (copy the code); Task 5 moves them to `ActionModel`.

- [ ] **Step 3: Write fixture and tests**

`tests/services/ProfileOrchestratorFixture.h` clones the mock setup pattern from `tests/helpers/AppControllerFixture.h` (mock desktop, mock injector, mock hidraw session, seeded default profile in `QTemporaryDir`), but constructs the orchestrator directly with real `ProfileEngine`, real `DeviceModel`/`ButtonModel`/`ProfileModel`, a real `DeviceSelection`, and a `MockDesktop` reference. The fixture attaches one mock `PhysicalDevice` via `DeviceManager::addPhysicalDevice` so `DeviceSelection::activeDevice()` returns a valid pointer for tests.

Tests:

```cpp
TEST_F(ProfileOrchestratorFixture, SaveCurrentProfilePersistsChanges) { ... }
TEST_F(ProfileOrchestratorFixture, ApplyProfileToHardwareSetsAllFields) { ... }
TEST_F(ProfileOrchestratorFixture, ApplyEmitsProfileAppliedWithCorrectSerial) { ... }
TEST_F(ProfileOrchestratorFixture, WindowFocusSwitchesHardwareProfile) { ... }
TEST_F(ProfileOrchestratorFixture, WindowFocusNoProfileFallsBackToDefault) { ... }
TEST_F(ProfileOrchestratorFixture, TabSwitchChangesDisplayNotHardware) { ... }
TEST_F(ProfileOrchestratorFixture, DisplayProfileChangedPushesToModels) { ... }
```

Add to `tests/CMakeLists.txt`.

- [ ] **Step 4: Integrate into `AppController`**

In `AppController.h`:
- Add `#include "services/ProfileOrchestrator.h"`
- Add member: `ProfileOrchestrator m_profileOrchestrator{&m_profileEngine, &m_actionExecutor, &m_deviceSelection, &m_deviceModel, &m_buttonModel, &m_profileModel, this};`
- Remove the 9 method/slot declarations
- Remove `m_currentDevice` (moves to ProfileOrchestrator and ButtonActionDispatcher)

In `AppController.cpp`:
- Delete the 9 method bodies
- In `wireSignals()`:

```cpp
// DeviceCommands save signal now goes to orchestrator
connect(&m_deviceCommands, &DeviceCommands::userChangedSomething,
        &m_profileOrchestrator, &ProfileOrchestrator::saveCurrentProfile);

// ButtonModel signal goes directly to orchestrator
connect(&m_buttonModel, &ButtonModel::userActionChanged,
        &m_profileOrchestrator, &ProfileOrchestrator::onUserButtonChanged);

// Window focus
connect(m_desktop, &IDesktopIntegration::activeWindowChanged,
        &m_profileOrchestrator, &ProfileOrchestrator::onWindowFocusChanged);

// Profile tab switch
connect(&m_profileModel, &ProfileModel::profileSwitched,
        &m_profileOrchestrator, &ProfileOrchestrator::onTabSwitched);

// Display profile change (from ProfileEngine)
connect(&m_profileEngine, &ProfileEngine::displayProfileChanged,
        &m_profileOrchestrator, &ProfileOrchestrator::onDisplayProfileChanged);

// profileApplied -> dispatcher resets thumbAccum
connect(&m_profileOrchestrator, &ProfileOrchestrator::profileApplied,
        &m_buttonDispatcher, &ButtonActionDispatcher::onProfileApplied);

// currentDevice -> dispatcher stays in sync
connect(&m_profileOrchestrator, &ProfileOrchestrator::currentDeviceChanged,
        &m_buttonDispatcher, &ButtonActionDispatcher::onCurrentDeviceChanged);

// Gesture save (DeviceModel)
connect(&m_deviceModel, &DeviceModel::userGestureChanged,
        &m_profileOrchestrator, &ProfileOrchestrator::saveCurrentProfile);

// Profile add/remove (ProfileModel)
connect(&m_profileModel, &ProfileModel::profileAdded,
        &m_profileEngine, &ProfileEngine::createProfileForApp);
connect(&m_profileModel, &ProfileModel::profileRemoved,
        &m_profileEngine, &ProfileEngine::removeAppProfile);

// DeviceManager setup signals
connect(&m_deviceManager, &DeviceManager::deviceSetupComplete,
        this, &AppController::onDeviceSetupComplete);
```

In `AppController::onPhysicalDeviceAdded`:
- Replace the inline `transportSetupComplete` lambda with: `connect(device, &PhysicalDevice::transportSetupComplete, &m_profileOrchestrator, [this, device](){ m_profileOrchestrator.onTransportSetupComplete(device); });`
- Replace the direct `setupProfileForDevice(device)` call with `m_profileOrchestrator.setupProfileForDevice(device);`

Remove the temporary wiring from Task 2 (`DeviceCommands::userChangedSomething -> AppController::saveCurrentProfile`).

- [ ] **Step 5: Build, run all tests, smoke test, commit**

Smoke test focus: full window-focus-to-profile-switch-to-hardware-commands flow. Switch between Chrome and Firefox with different profiles; confirm each app's DPI/SmartShift/etc. applies on focus. Edit a button assignment, confirm it saves. Switch profile tabs, confirm display updates without hardware writes.

```bash
git add -A && git commit -m "refactor(app): extract ProfileOrchestrator service from AppController

Move the 9 profile-related methods (applyProfileToHardware, saveCurrentProfile,
pushDisplayValues, restoreButtonModelFromProfile, setupProfileForDevice,
onUserButtonChanged, onTabSwitched, onDisplayProfileChanged, onWindowFocusChanged)
into a dedicated service.

ProfileOrchestrator emits profileApplied(serial) after every hardware apply;
ButtonActionDispatcher subscribes to reset thumbAccum. currentDeviceChanged
signal keeps the dispatcher's IDevice pointer in sync. AppController is now
the only place with connect() calls.

Part of #107."
```

---

## Task 5: Move translation helpers to `ActionModel`

**Files:**
- Modify: `src/app/models/ActionModel.h` / `.cpp` (add `buttonActionToName`, `buttonEntryToAction`)
- Modify: `src/app/services/ProfileOrchestrator.cpp` (delete local copies, call `ActionModel`)

### Steps

- [ ] **Step 1: Add the two methods to `ActionModel`**

In `src/app/models/ActionModel.h`, add:

```cpp
public:
    QString buttonActionToName(const ButtonAction &ba) const;
    ButtonAction buttonEntryToAction(const QString &actionType, const QString &actionName) const;
```

In `src/app/models/ActionModel.cpp`, move the implementations from the private copies currently in `ProfileOrchestrator.cpp` (which came from `AppController.cpp:801` and `:821`).

- [ ] **Step 2: Update `ProfileOrchestrator`**

- Delete the two private member function definitions in `ProfileOrchestrator.cpp` and their declarations in `.h`
- Update callers in `saveCurrentProfile` and `restoreButtonModelFromProfile` to call `m_actionModel->buttonActionToName(...)` and `m_actionModel->buttonEntryToAction(...)`
- Add `ActionModel *m_actionModel;` member and constructor parameter; update `AppController` to pass `&m_actionModel`

- [ ] **Step 3: Add unit tests**

In `tests/test_action_model.cpp`, add:

```cpp
TEST_F(ActionModelFixture, ButtonActionToNameRoundTripForKeystroke) { ... }
TEST_F(ActionModelFixture, ButtonActionToNameRoundTripForAppLaunch) { ... }
TEST_F(ActionModelFixture, ButtonEntryToActionDefaultTypeReturnsDefault) { ... }
TEST_F(ActionModelFixture, ButtonActionNameCoverageForAllTypes) { ... }
```

- [ ] **Step 4: Build, run all tests, smoke test, commit**

```bash
git add -A && git commit -m "refactor(app): move ButtonAction translation helpers to ActionModel

buttonActionToName and buttonEntryToAction now live on ActionModel where
they belong (domain<->UI translation for actions). ProfileOrchestrator
calls them via its ActionModel pointer.

Part of #107."
```

---

## Task 6: Cleanup commit

**Files:**
- Modify: `src/app/AppController.h` (add contract docstring)
- Modify: `src/app/AppController.cpp` (remove unused includes)

### Steps

- [ ] **Step 1: Add contract docstring at the top of `AppController.h`**

```cpp
/// Composition root for the Logitune application.
///
/// Owns long-lived singletons (ViewModels, services, engines), wires the
/// signal graph between them at startup, and attaches PhysicalDevice
/// instances into the graph at runtime. Exposes ViewModels via accessors
/// for QML registration in main.cpp.
///
/// This class does not implement user-facing behavior. Profile flow lives
/// in ProfileOrchestrator, input interpretation in ButtonActionDispatcher,
/// hardware command relays in DeviceCommands, and active-device resolution
/// in DeviceSelection. If you find yourself adding a method here that
/// responds to a user event or mutates application state, it belongs in
/// a service instead.
class AppController : public QObject {
```

- [ ] **Step 2: Audit `#include`s in `AppController.h` and `.cpp`**

Remove any includes that are no longer used after extraction. Likely candidates: `ActionExecutor.h`, `ButtonAction.h`, possibly `DeviceSession.h` depending on what remains. Verify by compiling after each removal.

- [ ] **Step 3: Remove any dead code**

Look for leftover helper methods, unused forward declarations, stale comments referencing moved behavior.

- [ ] **Step 4: Build, run all tests, smoke test, commit**

```bash
git add -A && git commit -m "refactor(app): add AppController contract docstring and clean up includes

State the composition-root role explicitly so future changes stay on the
right side of the line. Drop includes that are no longer needed after
extraction.

Part of #107."
```

---

## Task 7: Rename `AppController` to `AppRoot`

**Files (all modified):**
- Rename: `src/app/AppController.h` → `src/app/AppRoot.h`
- Rename: `src/app/AppController.cpp` → `src/app/AppRoot.cpp`
- Rename: `tests/helpers/AppControllerFixture.h` → `tests/helpers/AppRootFixture.h`
- Rename: `tests/test_app_controller.cpp` → `tests/test_app_root.cpp`
- Modify: `src/app/main.cpp`
- Modify: `src/app/CMakeLists.txt`, `tests/CMakeLists.txt`
- Modify: every `.cpp`/`.h` that `#include`s `AppController.h` or `AppControllerFixture.h`
- Modify: any wiki page / diagram / docstring that references `AppController`

### Steps

- [ ] **Step 1: Rename the source files**

```bash
cd /home/mina/repos/logitune-wt-appcontroller-extract
git mv src/app/AppController.h src/app/AppRoot.h
git mv src/app/AppController.cpp src/app/AppRoot.cpp
git mv tests/helpers/AppControllerFixture.h tests/helpers/AppRootFixture.h
git mv tests/test_app_controller.cpp tests/test_app_root.cpp
```

- [ ] **Step 2: Rename the class and all references via `sed`**

```bash
# Class/symbol renames
grep -rl 'AppController' src/ tests/ docs/ | xargs sed -i 's/AppController/AppRoot/g'

# File include renames
grep -rl 'AppController\.h' src/ tests/ | xargs sed -i 's/AppController\.h/AppRoot.h/g'
grep -rl 'AppControllerFixture\.h' src/ tests/ | xargs sed -i 's/AppControllerFixture\.h/AppRootFixture.h/g'
```

- [ ] **Step 3: Update CMakeLists.txt source references**

```bash
sed -i 's/AppController\.cpp/AppRoot.cpp/g' src/app/CMakeLists.txt
sed -i 's/test_app_controller\.cpp/test_app_root.cpp/g' tests/CMakeLists.txt
```

- [ ] **Step 4: Update docs/wiki references**

```bash
grep -rl 'AppController' docs/ | xargs sed -i 's/AppController/AppRoot/g'
```

Re-render any diagrams that changed. System-overview SVG likely referenced `AppController`:

```bash
grep -n 'AppController' docs/wiki/diagrams/system-overview.svg
# If found, edit the SVG, then:
rsvg-convert -w 1600 -h 1120 docs/wiki/diagrams/system-overview.svg -o docs/wiki/diagrams/system-overview.png
```

- [ ] **Step 5: Review the diff carefully**

```bash
git diff --stat
git diff | head -200
```

Expected: entirely `s/AppController/AppRoot/`. No behavior changes. If you see any diff hunk that isn't a rename, investigate before committing.

- [ ] **Step 6: Build, run all tests, smoke test, commit**

```bash
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests
# Smoke test on real hardware
git add -A && git commit -m "refactor(app): rename AppController to AppRoot

The class's final role after the extraction in prior commits is
composition root: owns singletons, wires signals, handles runtime device
lifecycle, exposes ViewModels for QML registration. The 'Controller'
name in an MVVM stack is misleading because it implies MVC-style
behavior that now lives in ProfileOrchestrator, ButtonActionDispatcher,
and DeviceCommands.

Closes #107."
```

- [ ] **Step 7: Push the branch and open the PR**

```bash
git push -u origin refactor-appcontroller-extract
gh pr create --title "Refactor: extract behavior from AppController and rename to AppRoot (closes #107)" \
    --body-file docs/superpowers/specs/2026-04-23-appcontroller-extraction-design.md
```
