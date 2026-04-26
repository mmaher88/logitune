# ActionsPanel per-Device Filtering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Insert a `QSortFilterProxyModel` between the static `ActionModel` and the `ActionsPanel` QML consumer that hides actions the selected device cannot execute (e.g. "Shift wheel mode" on MX Vertical). Closes #63.

**Architecture:** Add two new `*Supported` Q_PROPERTYs to `DeviceModel` (`adjustableDpiSupported`, `reprogControlsSupported`), create an `ActionFilterModel` proxy that reads the source row's `actionType` and maps it to a capability query on `DeviceModel`, and flip the QML context property for `ActionModel` to expose the proxy instead of the raw source. The raw source is unchanged and AppController's internal lookups continue to go through it.

**Tech Stack:** Qt 6 (`QSortFilterProxyModel`), C++20, CMake, GTest. Existing files at `src/app/models/{ActionModel,DeviceModel}.{h,cpp}`, `src/app/AppController.{h,cpp}`, `src/app/main.cpp`, `tests/test_action_model.cpp`.

---

## File Structure

### Files created

- `src/app/models/ActionFilterModel.h` — class declaration (~30 lines).
- `src/app/models/ActionFilterModel.cpp` — implementation (~40 lines).
- `tests/test_action_filter_model.cpp` — four GTest cases (~130 lines).

### Files modified

- `src/app/models/DeviceModel.h` — two new `Q_PROPERTY`s + getter declarations.
- `src/app/models/DeviceModel.cpp` — two new getter bodies (~16 lines).
- `src/app/AppController.h` — `m_actionFilterModel` member + `actionFilterModel()` accessor.
- `src/app/AppController.cpp` — construct + wire the proxy in the constructor.
- `src/app/main.cpp` — flip `qmlRegisterSingletonInstance` and `setContextProperty` for `ActionModel` to use the proxy.
- `src/app/CMakeLists.txt` — add `models/ActionFilterModel.cpp` to the target's sources.
- `tests/CMakeLists.txt` — add `test_action_filter_model.cpp` to `logitune-tests` sources.

---

## Task 1: `DeviceModel` capability getters

**Files:**
- Modify: `src/app/models/DeviceModel.h`
- Modify: `src/app/models/DeviceModel.cpp`

- [ ] **Step 1: Declare two Q_PROPERTYs in the header**

Open `src/app/models/DeviceModel.h`. Find the existing block that declares `smoothScrollSupported`, `thumbWheelSupported`, `smartShiftSupported` (around line 47-49). After the `smartShiftSupported` Q_PROPERTY, add:

```cpp
    Q_PROPERTY(bool adjustableDpiSupported READ adjustableDpiSupported NOTIFY selectedChanged)
    Q_PROPERTY(bool reprogControlsSupported READ reprogControlsSupported NOTIFY selectedChanged)
```

Also in the `public:` section where the existing getters are declared (around line 121-123 based on matching pattern), after `bool smartShiftSupported() const;`, add:

```cpp
    bool adjustableDpiSupported() const;
    bool reprogControlsSupported() const;
```

- [ ] **Step 2: Implement the getters**

Open `src/app/models/DeviceModel.cpp`. After the existing `smartShiftSupported()` implementation (around line 648-654), append:

```cpp
bool DeviceModel::adjustableDpiSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().adjustableDpi;
    return true;
}

bool DeviceModel::reprogControlsSupported() const
{
    auto *s = selectedDevice();
    if (s && s->descriptor())
        return s->descriptor()->features().reprogControls;
    return true;
}
```

The `return true` fallback matches the existing `smoothScrollSupported` / `thumbWheelSupported` / `smartShiftSupported` pattern: when no device is selected, be permissive.

- [ ] **Step 3: Build and run the full test suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`

Expected: baseline pass (exact number depends on what's on master when you start, should be unchanged from before this task). The new getters have no callers yet so no behavioral change is expected.

- [ ] **Step 4: Commit**

```bash
git add src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp
git commit -m "feat(device-model): adjustableDpiSupported + reprogControlsSupported Q_PROPERTYs

Matches the existing smoothScroll/thumbWheel/smartShift pattern:
read descriptor()->features().<flag>, fall back to true when no
device is selected. Consumed by ActionFilterModel in a follow-up
commit to hide capability-gated actions from the remap picker."
```

---

## Task 2: `ActionFilterModel` class

**Files:**
- Create: `src/app/models/ActionFilterModel.h`
- Create: `src/app/models/ActionFilterModel.cpp`
- Modify: `src/app/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `src/app/models/ActionFilterModel.h`:

```cpp
#pragma once
#include <QSortFilterProxyModel>

namespace logitune {

class DeviceModel;

/// Hides actions whose required device capability is absent on the
/// currently-selected device. Source model must be an ActionModel.
/// Reinvalidates its filter whenever DeviceModel::selectedChanged fires.
class ActionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ActionFilterModel(DeviceModel *deviceModel,
                               QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
};

} // namespace logitune
```

- [ ] **Step 2: Write the implementation**

Create `src/app/models/ActionFilterModel.cpp`:

```cpp
#include "ActionFilterModel.h"
#include "ActionModel.h"
#include "DeviceModel.h"

namespace logitune {

ActionFilterModel::ActionFilterModel(DeviceModel *deviceModel, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_deviceModel(deviceModel)
{
    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::selectedChanged,
                this, [this]() { invalidateFilter(); });
    }
}

bool ActionFilterModel::filterAcceptsRow(int sourceRow,
                                         const QModelIndex &sourceParent) const
{
    // Before any device is selected (e.g. startup before udev scan completes)
    // the picker should show every action. ActionsPanel only opens from a
    // hotspot click on a selected device, so this branch is mostly defensive.
    if (!m_deviceModel || m_deviceModel->selectedIndex() < 0)
        return true;

    const QString type = sourceModel()->data(
        sourceModel()->index(sourceRow, 0, sourceParent),
        ActionModel::ActionTypeRole).toString();

    if (type == QLatin1String("dpi-cycle"))
        return m_deviceModel->adjustableDpiSupported();
    if (type == QLatin1String("smartshift-toggle"))
        return m_deviceModel->smartShiftSupported();
    if (type == QLatin1String("gesture-trigger"))
        return m_deviceModel->reprogControlsSupported();
    if (type == QLatin1String("wheel-mode"))
        return m_deviceModel->thumbWheelSupported();
    return true;
}

} // namespace logitune
```

- [ ] **Step 3: Add to CMake target**

Open `src/app/CMakeLists.txt`. Find the list of sources for the `logitune-app-lib` target. Add `models/ActionFilterModel.cpp` to the same `add_library` / `target_sources` / `qt_add_library` list that contains `models/ActionModel.cpp`.

For example, if the file looks like:

```cmake
qt_add_library(logitune-app-lib STATIC
    AppController.cpp
    ...
    models/ActionModel.cpp
    models/ButtonModel.cpp
    models/DeviceModel.cpp
    ...
)
```

Change the `models/` block to include the new source:

```cmake
    models/ActionModel.cpp
    models/ActionFilterModel.cpp
    models/ButtonModel.cpp
    models/DeviceModel.cpp
```

(Find exact existing line structure with `grep -n "models/" src/app/CMakeLists.txt` before editing.)

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: clean build. Qt's AUTOMOC generates the MOC wrapper for the new QObject subclass automatically.

- [ ] **Step 5: Commit**

```bash
git add src/app/models/ActionFilterModel.h src/app/models/ActionFilterModel.cpp src/app/CMakeLists.txt
git commit -m "feat(app): ActionFilterModel proxy with capability-based filterAcceptsRow

New QSortFilterProxyModel that hides ActionModel rows whose
actionType requires a device capability the selected device lacks.
Mapping is inline in filterAcceptsRow for four action types:
dpi-cycle, smartshift-toggle, gesture-trigger, wheel-mode.
Everything else (keystrokes, app-launch, media, default, none)
passes the filter unconditionally.

invalidateFilter() is called on DeviceModel::selectedChanged so
carousel selection triggers a re-filter. No consumers yet; wiring
lands in a follow-up commit."
```

---

## Task 3: Filter tests

**Files:**
- Create: `tests/test_action_filter_model.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the four tests**

Create `tests/test_action_filter_model.cpp`:

```cpp
#include <gtest/gtest.h>
#include <memory>
#include <QCoreApplication>

#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"
#include "models/ActionModel.h"
#include "models/ActionFilterModel.h"
#include "models/DeviceModel.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "hidpp/HidrawDevice.h"

using namespace logitune;
using namespace logitune::test;

namespace {

// Count how many rows in the proxy have a given Name role. 0 = hidden.
int proxyCountByName(ActionFilterModel &proxy, const QString &name) {
    int count = 0;
    for (int i = 0; i < proxy.rowCount(); ++i) {
        const QString rowName = proxy.data(
            proxy.index(i, 0), ActionModel::NameRole).toString();
        if (rowName == name) count++;
    }
    return count;
}

// Build a PhysicalDevice + DeviceSession pair backed by a MockDevice
// with the given FeatureSupport flags. Added to the DeviceModel and
// selected, but NOT driven through AppController::onPhysicalDeviceAdded
// because this test does not exercise the profile engine.
PhysicalDevice* attachMockDevice(DeviceModel &model,
                                 MockDevice &mock,
                                 const QString &serial = QStringLiteral("mock"))
{
    auto *owner = &model;  // parent the heap objects to the model for cleanup
    auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
    auto *session = new DeviceSession(std::move(mockHidraw), 0xFF, "Bluetooth",
                                       nullptr, owner);
    session->m_connected = true;
    session->m_deviceName = QStringLiteral("Mock Device");
    session->m_activeDevice = &mock;

    auto *device = new PhysicalDevice(serial, owner);
    device->attachTransport(session);
    model.addPhysicalDevice(device);
    return device;
}

} // namespace

TEST(ActionFilterModel, EmptyDeviceModelShowsFullList) {
    ensureApp();
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    EXPECT_EQ(proxy.rowCount(), source.rowCount());
}

TEST(ActionFilterModel, FilterHidesUnsupportedActions) {
    ensureApp();
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mock;
    mock.setupMxControls();
    FeatureSupport f;
    f.adjustableDpi = true;
    f.smartShift    = false;   // no free-spin wheel
    f.thumbWheel    = false;   // no thumb wheel
    f.reprogControls = true;
    mock.setFeatures(f);

    attachMockDevice(dm, mock);
    dm.setSelectedIndex(0);

    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Shift wheel mode")), 0);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Gestures")), 1);
    EXPECT_LT(proxy.rowCount(), source.rowCount());
}

TEST(ActionFilterModel, SelectionChangeInvalidates) {
    ensureApp();
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mockA;
    mockA.setupMxControls();
    FeatureSupport fa;
    fa.adjustableDpi = true;
    fa.smartShift    = true;
    fa.thumbWheel    = true;
    fa.reprogControls = true;
    mockA.setFeatures(fa);

    MockDevice mockB;
    mockB.setupMxControls();
    FeatureSupport fb;  // all flags default false
    mockB.setFeatures(fb);

    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("mock-A"));
    auto *devB = attachMockDevice(dm, mockB, QStringLiteral("mock-B"));

    const int idxA = dm.devices().indexOf(devA);
    const int idxB = dm.devices().indexOf(devB);
    ASSERT_GE(idxA, 0);
    ASSERT_GE(idxB, 0);

    dm.setSelectedIndex(idxA);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 1);

    dm.setSelectedIndex(idxB);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 0);
}

TEST(ActionFilterModel, UnrestrictedActionsAlwaysVisible) {
    ensureApp();
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mock;
    mock.setupMxControls();
    FeatureSupport f;  // every capability flag false
    mock.setFeatures(f);

    attachMockDevice(dm, mock);
    dm.setSelectedIndex(0);

    // Even on a device with zero capabilities, keystroke/app-launch/media
    // /default/none actions remain visible.
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Keyboard shortcut")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Copy")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Paste")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Do nothing")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Media controls")), 1);
}
```

- [ ] **Step 2: Check `MockDevice::setFeatures` exists**

Run: `grep -n "setFeatures\|m_features" tests/mocks/MockDevice.h | head -5`

Expected output should show something like `void setFeatures(const FeatureSupport &f)` and `FeatureSupport m_features`. If the method does not exist, add it to `tests/mocks/MockDevice.h`:

```cpp
    void setFeatures(const FeatureSupport &f) { m_features = f; }
    FeatureSupport features() const override { return m_features; }
```

(with a matching `FeatureSupport m_features;` member).

If `features()` already overrides `IDevice::features()` but reads from somewhere else, adjust the test to use whatever pattern MockDevice already provides for configurable features. Report as DONE_WITH_CONCERNS if the mock is more rigid than expected.

- [ ] **Step 3: Add the test to CMake**

Open `tests/CMakeLists.txt`. Find the `logitune-tests` target's source list. Add `test_action_filter_model.cpp` in alphabetical order among the existing `test_action_model.cpp`, `test_app_controller.cpp`, etc. Example:

```cmake
    test_action_filter_model.cpp
    test_action_model.cpp
    test_app_controller.cpp
```

- [ ] **Step 4: Build and run the new tests**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='ActionFilterModel.*' 2>&1 | tail -15`

Expected: 4 tests, all pass.

- [ ] **Step 5: Run the full suite**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: baseline + 4 = 575/575 passing (or whatever the baseline count was + 4).

- [ ] **Step 6: Commit**

```bash
git add tests/test_action_filter_model.cpp tests/CMakeLists.txt
# If MockDevice.h was modified in Step 2, add it too:
# git add tests/mocks/MockDevice.h
git commit -m "test(action-filter): capability-based filter tests

Four tests covering: empty device model (full list), capability-based
hiding for smartShift/thumbWheel-absent devices, selectedChanged
invalidation, and unrestricted-actions-always-visible guard."
```

---

## Task 4: Wire the proxy into AppController and flip QML registration

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `src/app/main.cpp`

- [ ] **Step 1: Add the proxy member and accessor to AppController header**

Open `src/app/AppController.h`. Add the include at the top of the includes block:

```cpp
#include "models/ActionFilterModel.h"
```

In the `public:` section where `ActionModel *actionModel()` is declared (around line 51), add directly below it:

```cpp
    ActionFilterModel *actionFilterModel() { return m_actionFilterModel.get(); }
```

In the `private:` section where `ActionModel m_actionModel;` is declared (around line 92), add below it:

```cpp
    std::unique_ptr<ActionFilterModel> m_actionFilterModel;
```

Verify `#include <memory>` is already present near the top; if not, add it.

- [ ] **Step 2: Construct the proxy in AppController's constructor**

Open `src/app/AppController.cpp`. Find the constructor body (after the member-initializer list). At the end of the constructor's body, add:

```cpp
    m_actionFilterModel = std::make_unique<ActionFilterModel>(&m_deviceModel, this);
    m_actionFilterModel->setSourceModel(&m_actionModel);
```

The constructor already wires `m_actionExecutor.setInjector(m_injector)` and similar setup — put these two lines near that block.

- [ ] **Step 3: Flip the QML registration in main.cpp**

Open `src/app/main.cpp`. Find the two sites that register `ActionModel`:

```cpp
qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",    controller.actionModel());
...
engine.rootContext()->setContextProperty("ActionModel",    controller.actionModel());
```

Change both to use the filter proxy:

```cpp
qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",    controller.actionFilterModel());
...
engine.rootContext()->setContextProperty("ActionModel",    controller.actionFilterModel());
```

QML sees an `ActionFilterModel` instead of an `ActionModel`, but the role names / data layout are identical (proxy forwards them through from the source), so `ActionsPanel.qml` and other QML consumers don't need changes.

- [ ] **Step 4: Build + run the full test suite**

Run: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3 && QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3`

Expected: all tests still pass (core at baseline+4, QML at 72/72).

- [ ] **Step 5: Smoke-launch the app locally**

Run:

```bash
pkill -9 -f 'build/src/app/logitune' 2>/dev/null
rm -f /tmp/logitune.lock /tmp/logitune-sim.log
nohup ./build/src/app/logitune --simulate-all > /tmp/logitune-sim.log 2>&1 & disown
sleep 4
ps aux | grep -v grep | grep 'build/src/app/logitune' || echo not-running
```

Expected: app launches without QML warnings related to `ActionModel`. Navigate to MX Vertical in the carousel, click any configurable button (e.g. Back), and confirm the ActionsPanel slides in without `"Shift wheel mode"` in the action list. Then switch to MX Master 3S, open the same panel, confirm `"Shift wheel mode"` reappears.

Kill the app when done:

```bash
pkill -9 -f 'build/src/app/logitune'
```

- [ ] **Step 6: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp src/app/main.cpp
git commit -m "refactor(app): QML ActionModel context property points at filter proxy

AppController now owns an ActionFilterModel that wraps the raw
ActionModel. QML consumers see the proxy (which forwards role names
and row data transparently), so every action picker in the app
filters capability-gated actions automatically. Internal C++
consumers (AppController lookups by payload/name in
restoreButtonModelFromProfile etc.) keep going through m_actionModel
directly.

Closes #63."
```

---

## Final verification

- [ ] Run `git log --oneline origin/master..HEAD` — expect 5 commits on the branch (spec doc + 4 task commits). If only 4, the spec commit happened on a prior branch; that's fine.
- [ ] Run the full local test suite once more: `cmake --build build -j$(nproc) && QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`. Must be green.
- [ ] Draft the PR body in conversation for user review before `gh pr create`. Do NOT open the PR directly from the plan.

---

## Self-review summary

Every spec section has a matching task:

- **Architecture (ActionFilterModel class)** → Task 2.
- **Capability mapping (four action types)** → Task 2 Step 2.
- **DeviceModel extensions (two Q_PROPERTYs)** → Task 1.
- **Wiring (AppController + main.cpp)** → Task 4.
- **Tests (four GTest cases)** → Task 3.

No placeholders. All code blocks are complete. Method signatures (`adjustableDpiSupported`, `reprogControlsSupported`, `actionFilterModel`, `filterAcceptsRow`) are consistent across every task that references them.
