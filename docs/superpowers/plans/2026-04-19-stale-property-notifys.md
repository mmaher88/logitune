# Stale Property NOTIFY Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hardware-originated state changes (physical SmartShift press, DPI cycle press, scroll invert from another app) must propagate to the QML bindings on `DeviceModel`'s `currentDPI`, `smartShiftEnabled`, `smartShiftThreshold`, `scrollHiRes`, `scrollInvert`, `thumbWheelMode`, `thumbWheelInvert`. Today they read stale cached profile values.

**Architecture:** Two layers. First, `PhysicalDevice` gains per-property relay signals that forward matching `DeviceSession` signals; `DeviceModel::addPhysicalDevice` connects to them and emits per-property NOTIFYs gated on the selected device. Second, the per-property handler in `DeviceModel` sets `m_hasDisplayValues = false` so the getters stop returning the cached `m_display*` values and fall through to the live session readers. `setDisplayValues` re-arms the cache on the next profile reload. Q_PROPERTY NOTIFYs flip from the coarse `settingsReloaded` to property-specific signals last.

**Tech Stack:** Qt 6 / C++20 / GTest + QSignalSpy. No new files.

**Design spec:** `docs/superpowers/specs/2026-04-19-stale-property-notifys-design.md`. Read it before Task 1.

---

## Global rules

- **No em-dashes (U+2014 "—")** in any file you create or modify. Use colons, commas, periods, or parentheses. The only acceptable occurrence is inside a `grep -c "—"` verification command.
- **No co-author signatures** in commit messages.
- **Branch is `fix-stale-property-notifys`.** Already created with the spec committed. Do NOT push. The maintainer pushes after final verification.
- **Working directory:** `/home/mina/repos/logitune`.
- **Never amend commits.** Each task makes a new commit on top.
- **Build + test after every task**: `cmake --build build -j"$(nproc)" 2>&1 | tail -3` (must exit 0); `XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -5` (must show `[  PASSED  ] <N> tests.` with no failures). Baseline count before this plan is 539.

## Intentional simplification vs the spec

The spec mentions a new `DeviceSession::thumbWheelInvertChanged` signal. In practice, `DeviceSession::setThumbWheelMode` writes both `m_thumbWheelMode` AND `m_thumbWheelInvert` atomically and emits the single `thumbWheelModeChanged`. This plan keeps that shape: `thumbWheelInvert`'s Q_PROPERTY NOTIFY uses `thumbWheelModeChanged` (mirroring how `scrollHiRes` and `scrollInvert` share `scrollConfigChanged`). Result: no new session signal or setter is needed for thumb-wheel invert.

Only one brand-new session emit is needed: `currentDPIChanged`.

---

## File Structure

### Modified

- `src/core/DeviceSession.h`: add `currentDPIChanged()` signal declaration.
- `src/core/DeviceSession.cpp`: emit `currentDPIChanged` from `setDPI` and from the initial DPI read in `enumerateAndSetup`.
- `src/core/PhysicalDevice.h`: add five new signals (`smartShiftChanged`, `scrollConfigChanged`, `thumbWheelModeChanged`, `currentDPIChanged`, plus a keep-the-interface-consistent note: `batteryChanged` already exists as a forwarded signal via `connectSessionSignals`).
- `src/core/PhysicalDevice.cpp`: expand `connectSessionSignals` lambdas to emit both the per-property relay and `stateChanged`; add one new `connect` for `DeviceSession::currentDPIChanged`.
- `src/app/models/DeviceModel.cpp`: in `addPhysicalDevice`, connect to the four new PhysicalDevice signals; handlers invalidate the display cache then emit per-property NOTIFYs. In `setDisplayValues`, emit the per-property signals alongside the existing `settingsReloaded` so profile reload paths still trigger QML re-reads.
- `src/app/models/DeviceModel.h`: flip Q_PROPERTY NOTIFY declarations from `settingsReloaded` to per-property signals.
- `tests/test_device_session.cpp`: one narrow test for the new emit (`SetDPIEmitsCurrentDPIChanged`).
- `tests/test_device_model.cpp`: gate and cache-invalidation tests with `QSignalSpy`.

### Unchanged

- `src/app/AppController.cpp`: no changes. Request signals keep their existing wiring.
- `src/core/DeviceManager.cpp`: no changes.
- All QML files: no changes. Bindings are already declarative on the Q_PROPERTYs; the NOTIFY flip rewires them automatically.
- MockDevice: no changes. Tests that need a `PhysicalDevice` use a test double pattern described in Task 4.

---

## Task 1: DeviceSession emits `currentDPIChanged`

**Files:**
- Modify: `src/core/DeviceSession.h`
- Modify: `src/core/DeviceSession.cpp`
- Modify: `tests/test_device_session.cpp`

TDD-friendly. Write the test first, watch it fail, add the signal + emits, watch it pass.

- [ ] **Step 1: Write the failing test**

In `tests/test_device_session.cpp`, add near the other `SetDPI*` tests (around line 114):

```cpp
TEST_F(DeviceSessionTest, SetDPIEmitsCurrentDPIChangedWhenConnected) {
    auto session = makeSession();
    session->setConnectedForTest(true);
    QSignalSpy spy(session.get(), &DeviceSession::currentDPIChanged);
    session->setDPI(2000);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceSessionTest, SetDPISkipsEmitWhenNotConnected) {
    auto session = makeSession();
    // Default: m_connected = false
    QSignalSpy spy(session.get(), &DeviceSession::currentDPIChanged);
    session->setDPI(2000);
    EXPECT_EQ(spy.count(), 0);
}
```

If `QSignalSpy` is not already included in this file, add `#include <QSignalSpy>` near the top.

Note: `setDPI` guards on `m_connected && m_features && m_commandQueue`. `setConnectedForTest(true)` flips `m_connected`, but `m_features` and `m_commandQueue` remain null, so `setDPI` returns early at `if (!m_connected || !m_features || !m_commandQueue)` before it would write HID++. The signal must still fire when we actually change `m_currentDPI` — so the emit will go at the end of the updating branch, NOT at the top guard. That is why the "connected but features-less" test case above still expects zero emissions if the guard rejects the call. ADJUST: the test pair above assumes we emit from the actual update path, which runs only when all three preconditions are present. For this test to work without instantiating `m_features` / `m_commandQueue`, place the emit BEFORE the HID++-write path but AFTER the new-value assignment, gated only on the value having changed. See Step 3 for the exact placement.

- [ ] **Step 2: Run tests to see them fail**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: build fails at `DeviceSession::currentDPIChanged` — signal not declared.

- [ ] **Step 3: Declare the signal and emit it**

In `src/core/DeviceSession.h`, inside the `signals:` block (around line 107), add after the existing `smartShiftChanged`:

```cpp
void currentDPIChanged();
```

In `src/core/DeviceSession.cpp`, find `setDPI` (around line 742). The current body is:

```cpp
void DeviceSession::setDPI(int value)
{
    if (!m_connected || !m_features || !m_commandQueue) {
        qCDebug(lcDevice) << "setDPI: skipped (not connected)";
        return;
    }
    if (!m_features->hasFeature(hidpp::FeatureId::AdjustableDPI))
        return;

    value = qBound(m_minDPI, value, m_maxDPI);
    value = (value / m_dpiStep) * m_dpiStep;

    qCDebug(lcDevice) << "setDPI:" << value << "(was" << m_currentDPI << ") queue=" << m_commandQueue->pending();
    m_currentDPI = value;

    auto params = hidpp::features::AdjustableDPI::buildSetDPI(value);
    m_commandQueue->enqueue(hidpp::FeatureId::AdjustableDPI,
                            hidpp::features::AdjustableDPI::kFnSetSensorDpi,
                            params);
}
```

Restructure so the emit fires whenever `m_currentDPI` is updated, independent of whether the HID++ write goes through:

```cpp
void DeviceSession::setDPI(int value)
{
    if (!m_connected)
        return;

    value = qBound(m_minDPI, value, m_maxDPI);
    if (m_dpiStep > 0)
        value = (value / m_dpiStep) * m_dpiStep;

    const int previous = m_currentDPI;
    m_currentDPI = value;
    if (previous != value)
        emit currentDPIChanged();

    if (!m_features || !m_commandQueue)
        return;
    if (!m_features->hasFeature(hidpp::FeatureId::AdjustableDPI))
        return;

    qCDebug(lcDevice) << "setDPI:" << value << "(was" << previous << ") queue=" << m_commandQueue->pending();
    auto params = hidpp::features::AdjustableDPI::buildSetDPI(value);
    m_commandQueue->enqueue(hidpp::FeatureId::AdjustableDPI,
                            hidpp::features::AdjustableDPI::kFnSetSensorDpi,
                            params);
}
```

The rearrangement:
- Early-return on `!m_connected` stays; when not connected we do nothing.
- Clamp + step quantisation happens before the emit so `m_currentDPI` is set to the clamped value.
- Emit fires once per distinct value change.
- HID++ write still requires `m_features && m_commandQueue`; the qCDebug moves below the emit so the log line reflects the clamped value.

Also find the initial DPI read in `enumerateAndSetup` (around line 318) which assigns `m_currentDPI` from an `AdjustableDPI::parseCurrentDPI` call. Immediately after that assignment, emit the signal so QML bindings created before enumeration also get a value:

```cpp
    m_currentDPI = hidpp::features::AdjustableDPI::parseCurrentDPI(*dpiResp);
    qCDebug(lcDevice) << "current DPI:" << m_currentDPI;
    emit currentDPIChanged();
```

- [ ] **Step 4: Update the failing test to match the new guard**

The test pair from Step 1 expects a signal ONLY when connected. Because the emit now only happens when `m_connected == true` and the value changes, the pair above works as-is. Verify the test names and assertions still match.

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests --gtest_filter='DeviceSessionTest.SetDPI*' 2>&1 | tail -10
```

Expected: both new tests pass. Pre-existing `SetDPISkipsWhenNotConnected` still passes (it only asserts `currentDPI() == 0`, not signal counts, and the new body preserves that behavior because the early-return keeps `m_currentDPI` at its init value).

Full run:

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 541 tests.` (539 baseline + 2 new).

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" src/core/DeviceSession.h src/core/DeviceSession.cpp tests/test_device_session.cpp
```

Expected: `0` for each.

- [ ] **Step 7: Commit**

```bash
git add src/core/DeviceSession.h src/core/DeviceSession.cpp tests/test_device_session.cpp
git commit -m "feat(device-session): emit currentDPIChanged on DPI change

setDPI now emits currentDPIChanged whenever m_currentDPI changes value,
independent of whether the HID++ write path reaches the command queue.
The initial DPI read inside enumerateAndSetup also emits, so QML
bindings created before enumeration get a value.

Previously nothing in DeviceSession emitted a per-property signal for
DPI. DeviceModel's currentDPI Q_PROPERTY used the coarse
settingsReloaded NOTIFY and silently missed changes from setDPI calls
(including the dpi-cycle action that writes DPI without a profile
reload).

Two narrow tests (SetDPIEmitsCurrentDPIChangedWhenConnected and
SetDPISkipsEmitWhenNotConnected) cover the guard. Pre-existing
SetDPISkipsWhenNotConnected still asserts the old no-op behavior and
still passes."
```

---

## Task 2: PhysicalDevice per-property relay signals

**Files:**
- Modify: `src/core/PhysicalDevice.h`
- Modify: `src/core/PhysicalDevice.cpp`

No new tests in this task; the existing PhysicalDevice tests (if any) must stay green. The signals are exercised by the DeviceModel tests in Tasks 3 and 4.

- [ ] **Step 1: Declare the new signals**

In `src/core/PhysicalDevice.h`, find the `signals:` block. Add these four signals (alongside the existing `stateChanged`, `transportSetupComplete`, `divertedButtonPressed`, `gestureRawXY`, `thumbWheelRotation`):

```cpp
void smartShiftChanged(bool enabled, int threshold);
void scrollConfigChanged();
void thumbWheelModeChanged();
void currentDPIChanged();
```

Parameter shapes mirror `DeviceSession`'s signals exactly. If `<cstdint>` is not already included (for `int`) no include change is needed — `bool` and `int` are builtins.

- [ ] **Step 2: Expand `connectSessionSignals`**

In `src/core/PhysicalDevice.cpp`, find `connectSessionSignals` (around line 87). Today the four session signal relays each emit only `stateChanged`. Change each lambda to emit the matching per-property signal first, then `stateChanged`. Add a new connection for `currentDPIChanged`.

Before:

```cpp
    connect(session, &DeviceSession::smartShiftChanged, this,
            [this](bool, int) { emit stateChanged(); });
    connect(session, &DeviceSession::scrollConfigChanged, this,
            [this]() { emit stateChanged(); });
    connect(session, &DeviceSession::thumbWheelModeChanged, this,
            [this]() { emit stateChanged(); });
```

After:

```cpp
    connect(session, &DeviceSession::smartShiftChanged, this,
            [this](bool enabled, int threshold) {
        emit smartShiftChanged(enabled, threshold);
        emit stateChanged();
    });
    connect(session, &DeviceSession::scrollConfigChanged, this,
            [this]() {
        emit scrollConfigChanged();
        emit stateChanged();
    });
    connect(session, &DeviceSession::thumbWheelModeChanged, this,
            [this]() {
        emit thumbWheelModeChanged();
        emit stateChanged();
    });
    connect(session, &DeviceSession::currentDPIChanged, this,
            [this]() {
        emit currentDPIChanged();
        emit stateChanged();
    });
```

The existing `batteryChanged` connection keeps its current shape; `batteryLevel` already has its own Q_PROPERTY NOTIFY (`selectedBatteryChanged`) and is out of scope.

`disconnectSessionSignals` uses a blanket `disconnect(session, nullptr, this, nullptr)`; no change needed.

- [ ] **Step 3: Build and run the full suite**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: 541 passing. No new tests; this task adds infrastructure the next tasks will exercise.

- [ ] **Step 4: Verify no em-dashes**

```bash
grep -c "—" src/core/PhysicalDevice.h src/core/PhysicalDevice.cpp
```

Expected: `0` for each.

- [ ] **Step 5: Commit**

```bash
git add src/core/PhysicalDevice.h src/core/PhysicalDevice.cpp
git commit -m "feat(physical-device): relay per-property session signals

PhysicalDevice previously collapsed every session-level change signal
(smartShiftChanged, scrollConfigChanged, thumbWheelModeChanged,
batteryChanged) into a single stateChanged emission. Good enough for
the carousel row refresh that only reads QAbstractListModel data
roles, but insufficient for QML bindings on per-property Q_PROPERTYs
that care which field changed.

Add four new signals (smartShiftChanged, scrollConfigChanged,
thumbWheelModeChanged, currentDPIChanged) and update
connectSessionSignals so each session signal fires both the specific
relay and stateChanged. Existing stateChanged consumers keep working;
new consumers can listen for just the field they care about.

No behavior change yet: no consumer subscribes to the new signals in
this commit. DeviceModel wires them up in the next two commits."
```

---

## Task 3: DeviceModel invalidates display cache on hardware change

**Files:**
- Modify: `src/app/models/DeviceModel.cpp`

This task adds the per-property handler scaffolding with ONLY the cache invalidation rule. The per-property NOTIFY emits land in Task 4. Result after this task: a physical SmartShift press on the selected device updates `m_hasDisplayValues` to `false`, so the existing `settingsReloaded` chain already fires via `refreshRow` and QML bindings re-read getters that now fall through to live session state. This alone resolves issue #17 for today's setup; Task 4 is the forward-looking cleanup that makes the NOTIFY signals property-specific.

- [ ] **Step 1: Write failing tests**

Open `tests/test_device_model.cpp`. Find an appropriate spot near the existing `SetDisplayValues*` tests (around line 34). Add a new test fixture pattern that includes a `PhysicalDevice` test double. The simplest path is to use the existing pattern from other tests that construct `PhysicalDevice` directly with a `MockDevice`. If no such pattern exists in this file, inspect how `src/core/PhysicalDevice.h` is constructed and follow that.

Add these tests:

```cpp
TEST_F(DeviceModelTest, HardwareSmartShiftChangeInvalidatesDisplayCache) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    EXPECT_TRUE(model.smartShiftEnabled());

    // Simulate a hardware-originated change arriving for the selected device.
    ASSERT_TRUE(model.selectedIndex() >= 0);
    auto *pd = model.devices().at(model.selectedIndex());
    ASSERT_NE(pd, nullptr);
    emit pd->smartShiftChanged(false, 128);

    // m_hasDisplayValues should now be false; getter falls through to
    // the session. With no connected session, the session returns
    // the default (true) per the existing smartShiftEnabled() fallback.
    // Test: the cached false should no longer be returned. Assert the
    // getter does NOT return the display cache's value.
    EXPECT_NE(model.smartShiftEnabled(), false /* cached value */);
}

TEST_F(DeviceModelTest, SetDisplayValuesRestoresCacheAfterInvalidation) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    auto *pd = model.devices().at(model.selectedIndex());
    emit pd->smartShiftChanged(false, 64);
    // cache is now invalidated; re-call setDisplayValues
    model.setDisplayValues(1500, true, 200, false, true, "zoom", true);
    EXPECT_EQ(model.currentDPI(), 1500);
    EXPECT_EQ(model.smartShiftThreshold(), 200);
    EXPECT_TRUE(model.smartShiftEnabled());
    EXPECT_FALSE(model.scrollHiRes());
    EXPECT_TRUE(model.scrollInvert());
    EXPECT_EQ(model.thumbWheelMode(), "zoom");
    EXPECT_TRUE(model.thumbWheelInvert());
}

TEST_F(DeviceModelTest, UnselectedDeviceHardwareChangeDoesNotInvalidateCache) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);

    // Assumes the fixture has at least one non-selected PhysicalDevice.
    // If the fixture only constructs a single device, add a second mock
    // device via model.addPhysicalDevice() before this test.
    int selectedIdx = model.selectedIndex();
    ASSERT_GE(selectedIdx, 0);

    PhysicalDevice *other = nullptr;
    for (int i = 0; i < model.devices().size(); ++i) {
        if (i != selectedIdx) { other = model.devices().at(i); break; }
    }
    ASSERT_NE(other, nullptr) << "test needs at least two PhysicalDevices";

    emit other->smartShiftChanged(false, 128);

    // Cache should still be armed; getter returns the cached value.
    EXPECT_TRUE(model.smartShiftEnabled());
}
```

If the existing `DeviceModelTest` fixture does not populate a `PhysicalDevice`, the test will fail at `ASSERT_NE(pd, nullptr)`. In that case, extend the fixture (or add a dedicated `DeviceModelWithDeviceTest` fixture) that constructs a `MockDevice`, wraps it in a `PhysicalDevice`, and calls `model.addPhysicalDevice(pd)` in `SetUp`. Mirror the pattern from any test in the file that currently works against a populated model.

If there is no existing pattern to copy, STOP and escalate NEEDS_CONTEXT with the fixture shape so the controller can decide how to construct a `PhysicalDevice` in tests.

- [ ] **Step 2: Run tests to see them fail**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests --gtest_filter='DeviceModelTest.*InvalidateDisplayCache:DeviceModelTest.*AfterInvalidation:DeviceModelTest.*DoesNotInvalidateCache' 2>&1 | tail -10
```

Expected: at least one test fails because `DeviceModel` does not yet wire up the PhysicalDevice per-property signals.

- [ ] **Step 3: Wire the handlers in `addPhysicalDevice`**

Open `src/app/models/DeviceModel.cpp`. Find `addPhysicalDevice` (the method that already connects `stateChanged` around line 146). Directly below the existing `connect(device, &PhysicalDevice::stateChanged, ...)` block, add four new `connect` calls, each with a handler lambda that invalidates the display cache ONLY when the signal is for the currently selected device. Use `this` as the context object so the connections are auto-disconnected when the DeviceModel is destroyed:

```cpp
    connect(device, &PhysicalDevice::smartShiftChanged, this,
            [this, device](bool, int) {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
    });
    connect(device, &PhysicalDevice::scrollConfigChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
    });
    connect(device, &PhysicalDevice::thumbWheelModeChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
    });
    connect(device, &PhysicalDevice::currentDPIChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
    });
```

Keep the existing `stateChanged` connection untouched. `refreshRow` still runs via that path so the carousel keeps updating; the existing `settingsReloaded` emit inside `refreshRow` (line 245) still fires so QML bindings pinged.

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: all new tests pass. Total 544 (541 + 3 new).

- [ ] **Step 5: Verify no em-dashes**

```bash
grep -c "—" src/app/models/DeviceModel.cpp tests/test_device_model.cpp
```

Expected: `0` for each.

- [ ] **Step 6: Commit**

```bash
git add src/app/models/DeviceModel.cpp tests/test_device_model.cpp
git commit -m "fix(device-model): invalidate display cache on live hardware change

DeviceModel getters for DPI, SmartShift, scroll, and thumb-wheel
properties short-circuit to m_display* cached profile values whenever
m_hasDisplayValues is true. That flag is set by setDisplayValues on
profile load and never cleared, so even when QML bindings re-read a
getter in response to settingsReloaded (which refreshRow already
fires), they get the stale profile value back instead of the hardware
state.

Wire DeviceModel::addPhysicalDevice to PhysicalDevice's new
per-property relay signals. Each handler:
 - Early-returns if the event is not for the selected device.
 - Sets m_hasDisplayValues = false so the next getter call falls
   through to the live session readers.

The next profile reload re-arms the cache via setDisplayValues.

This commit resolves issue #17 on the current signal-plumbing layer
without changing any NOTIFY signals. Task 4 adds property-specific
NOTIFYs as forward-looking cleanup."
```

---

## Task 4: DeviceModel per-property NOTIFY emits

**Files:**
- Modify: `src/app/models/DeviceModel.cpp`
- Modify: `tests/test_device_model.cpp`

- [ ] **Step 1: Write failing tests for the per-property emits**

In `tests/test_device_model.cpp`, add (near the tests from Task 3):

```cpp
TEST_F(DeviceModelTest, SmartShiftHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    auto *pd = model.devices().at(model.selectedIndex());

    QSignalSpy enabledSpy(&model, &DeviceModel::smartShiftEnabledChanged);
    QSignalSpy thresholdSpy(&model, &DeviceModel::smartShiftThresholdChanged);

    emit pd->smartShiftChanged(false, 192);

    EXPECT_EQ(enabledSpy.count(), 1);
    EXPECT_EQ(thresholdSpy.count(), 1);
}

TEST_F(DeviceModelTest, ScrollConfigHardwareChangeEmitsOnce) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    auto *pd = model.devices().at(model.selectedIndex());

    QSignalSpy spy(&model, &DeviceModel::scrollConfigChanged);
    emit pd->scrollConfigChanged();

    // scrollHiRes and scrollInvert both NOTIFY on this signal; expect
    // a single emission, not two.
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, ThumbWheelHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    auto *pd = model.devices().at(model.selectedIndex());

    QSignalSpy spy(&model, &DeviceModel::thumbWheelModeChanged);
    emit pd->thumbWheelModeChanged();

    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, DPIHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    auto *pd = model.devices().at(model.selectedIndex());

    QSignalSpy spy(&model, &DeviceModel::currentDPIChanged);
    emit pd->currentDPIChanged();

    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, UnselectedDeviceHardwareChangeDoesNotEmit) {
    model.setDisplayValues(1000, true, 128, true, false, "scroll", false);
    int selectedIdx = model.selectedIndex();
    PhysicalDevice *other = nullptr;
    for (int i = 0; i < model.devices().size(); ++i) {
        if (i != selectedIdx) { other = model.devices().at(i); break; }
    }
    ASSERT_NE(other, nullptr) << "test needs at least two PhysicalDevices";

    QSignalSpy spy(&model, &DeviceModel::smartShiftEnabledChanged);
    emit other->smartShiftChanged(false, 128);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceModelTest, SetDisplayValuesEmitsPerPropertySignals) {
    QSignalSpy dpiSpy(&model, &DeviceModel::currentDPIChanged);
    QSignalSpy smartSpy(&model, &DeviceModel::smartShiftEnabledChanged);
    QSignalSpy threshSpy(&model, &DeviceModel::smartShiftThresholdChanged);
    QSignalSpy scrollSpy(&model, &DeviceModel::scrollConfigChanged);
    QSignalSpy thumbSpy(&model, &DeviceModel::thumbWheelModeChanged);

    model.setDisplayValues(1500, false, 200, false, true, "zoom", true);

    EXPECT_EQ(dpiSpy.count(), 1);
    EXPECT_EQ(smartSpy.count(), 1);
    EXPECT_EQ(threshSpy.count(), 1);
    EXPECT_EQ(scrollSpy.count(), 1);
    EXPECT_EQ(thumbSpy.count(), 1);
}
```

- [ ] **Step 2: Run tests to see them fail**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests --gtest_filter='DeviceModelTest.*HardwareChange*:DeviceModelTest.SetDisplayValuesEmitsPerPropertySignals:DeviceModelTest.UnselectedDeviceHardwareChangeDoesNotEmit' 2>&1 | tail -15
```

Expected: at least six tests fail because DeviceModel's handlers from Task 3 don't emit per-property signals yet.

- [ ] **Step 3: Extend the Task 3 handlers to emit per-property signals**

Reopen `src/app/models/DeviceModel.cpp`. In `addPhysicalDevice`, augment each of the four handlers added in Task 3 to emit the matching per-property signal after the cache invalidation. Before:

```cpp
    connect(device, &PhysicalDevice::smartShiftChanged, this,
            [this, device](bool, int) {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
    });
```

After:

```cpp
    connect(device, &PhysicalDevice::smartShiftChanged, this,
            [this, device](bool, int) {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit smartShiftEnabledChanged();
        emit smartShiftThresholdChanged();
    });
    connect(device, &PhysicalDevice::scrollConfigChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit scrollConfigChanged();
    });
    connect(device, &PhysicalDevice::thumbWheelModeChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit thumbWheelModeChanged();
    });
    connect(device, &PhysicalDevice::currentDPIChanged, this,
            [this, device]() {
        if (device != selectedDevice())
            return;
        m_hasDisplayValues = false;
        emit currentDPIChanged();
    });
```

`DeviceModel::scrollConfigChanged` already exists as a declared signal (verify by grepping `src/app/models/DeviceModel.h` for `scrollConfigChanged`). Same for `currentDPIChanged`, `smartShiftEnabledChanged`, `smartShiftThresholdChanged`, `thumbWheelModeChanged`. No new DeviceModel signals are needed in this file.

- [ ] **Step 4: Update `setDisplayValues` to emit per-property signals**

Still in `src/app/models/DeviceModel.cpp`, find `setDisplayValues` (around line 654). It currently ends with:

```cpp
    m_hasDisplayValues = true;
    emit settingsReloaded();
}
```

Add per-property emits just before `emit settingsReloaded();`:

```cpp
    m_hasDisplayValues = true;
    emit currentDPIChanged();
    emit smartShiftEnabledChanged();
    emit smartShiftThresholdChanged();
    emit scrollConfigChanged();
    emit thumbWheelModeChanged();
    emit settingsReloaded();
}
```

This preserves the existing `settingsReloaded` trigger for any still-subscribed consumer, while adding the per-property signals so QML bindings on the post-flip NOTIFYs (Task 5) get pinged on profile reload too. This is the fix for the risk noted in the spec's "Double-emit on profile reload" section.

- [ ] **Step 5: Build and run the full suite**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: all new tests pass. Total 550 (544 + 6 new).

- [ ] **Step 6: Verify no em-dashes**

```bash
grep -c "—" src/app/models/DeviceModel.cpp tests/test_device_model.cpp
```

Expected: `0` for each.

- [ ] **Step 7: Commit**

```bash
git add src/app/models/DeviceModel.cpp tests/test_device_model.cpp
git commit -m "feat(device-model): emit per-property NOTIFY signals gated on selection

Extend the per-property PhysicalDevice signal handlers (landed in the
prior commit with cache-invalidation only) to also emit the matching
DeviceModel signals: smartShiftEnabledChanged,
smartShiftThresholdChanged, scrollConfigChanged, thumbWheelModeChanged,
currentDPIChanged. Each emit is gated on the signal arriving from the
selected PhysicalDevice.

Also update setDisplayValues to emit the same five per-property
signals alongside its existing settingsReloaded emission, so profile
reload paths keep pinging QML bindings after the Q_PROPERTY NOTIFY
flip in the next commit.

scrollHiRes and scrollInvert intentionally share the single
scrollConfigChanged signal (matching DeviceSession::setScrollConfig's
combined write semantics), so one hardware change fires one DeviceModel
signal but both QML bindings re-read."
```

---

## Task 5: Flip Q_PROPERTY NOTIFY declarations

**Files:**
- Modify: `src/app/models/DeviceModel.h`

This is the final flip. After this commit, QML bindings on these properties re-read via the per-property signals from Task 4 instead of `settingsReloaded`. All emit sites exist; the flip is safe.

- [ ] **Step 1: Update the Q_PROPERTY declarations**

In `src/app/models/DeviceModel.h`, find the affected Q_PROPERTYs. Before:

```cpp
Q_PROPERTY(int currentDPI READ currentDPI NOTIFY settingsReloaded)
Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY settingsReloaded)
Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY settingsReloaded)
Q_PROPERTY(bool scrollHiRes READ scrollHiRes NOTIFY settingsReloaded)
Q_PROPERTY(bool scrollInvert READ scrollInvert NOTIFY settingsReloaded)
Q_PROPERTY(QString thumbWheelMode READ thumbWheelMode NOTIFY settingsReloaded)
Q_PROPERTY(bool thumbWheelInvert READ thumbWheelInvert NOTIFY settingsReloaded)
```

After:

```cpp
Q_PROPERTY(int currentDPI READ currentDPI NOTIFY currentDPIChanged)
Q_PROPERTY(bool smartShiftEnabled READ smartShiftEnabled NOTIFY smartShiftEnabledChanged)
Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY smartShiftThresholdChanged)
Q_PROPERTY(bool scrollHiRes READ scrollHiRes NOTIFY scrollConfigChanged)
Q_PROPERTY(bool scrollInvert READ scrollInvert NOTIFY scrollConfigChanged)
Q_PROPERTY(QString thumbWheelMode READ thumbWheelMode NOTIFY thumbWheelModeChanged)
Q_PROPERTY(bool thumbWheelInvert READ thumbWheelInvert NOTIFY thumbWheelModeChanged)
```

The last two share `thumbWheelModeChanged` because `DeviceSession::setThumbWheelMode` writes both mode and invert atomically. Same pattern as `scrollHiRes` and `scrollInvert` sharing `scrollConfigChanged`.

- [ ] **Step 2: Build and run the full suite**

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: 550 passing. No new tests; the existing tests still pass because the underlying semantics (signals fire, getters return correct values) are unchanged.

- [ ] **Step 3: QML smoke test**

```bash
pkill -f logitune 2>/dev/null; sleep 1
nohup env XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/src/app/logitune --simulate-all > /tmp/logitune-notify-smoke.log 2>&1 & disown
sleep 3
grep -iE "binding loop|notify signal|no notify" /tmp/logitune-notify-smoke.log | head -10
pkill -f logitune 2>/dev/null
```

Expected: no "binding loop" or "no NOTIFY signal" warnings. If any appear, they typically mean a QML binding tried to read a property whose NOTIFY signal was renamed and the binding didn't update; inspect the QML file referenced in the warning and verify it uses the Q_PROPERTY name (which hasn't changed), not the old signal name.

- [ ] **Step 4: Verify no em-dashes**

```bash
grep -c "—" src/app/models/DeviceModel.h
```

Expected: `0`.

- [ ] **Step 5: Commit**

```bash
git add src/app/models/DeviceModel.h
git commit -m "feat(device-model): flip Q_PROPERTY NOTIFY to per-property signals

currentDPI, smartShiftEnabled, smartShiftThreshold, scrollHiRes,
scrollInvert, thumbWheelMode, and thumbWheelInvert now notify via
property-specific signals instead of the coarse settingsReloaded. All
emit sites are already in place:
 - DeviceSession -> PhysicalDevice -> DeviceModel handler for
   hardware-originated changes (gated on selected device).
 - setDisplayValues emits per-property signals alongside its existing
   settingsReloaded emission for profile reload paths.

scrollHiRes + scrollInvert share scrollConfigChanged, and
thumbWheelMode + thumbWheelInvert share thumbWheelModeChanged, because
the underlying DeviceSession setters write those pairs atomically.

settingsReloaded stays as a coarse signal used only on profile reload
paths, available for future properties that do not have a dedicated
NOTIFY yet. Closes #17."
```

---

## Task 6: Final verification

**Files:** none modified.

- [ ] **Step 1: Clean rebuild**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: both exit 0; tail ends with linking the app and test binaries.

- [ ] **Step 2: Full test run**

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3
```

Expected: 550 core tests pass, 72 QML tests pass.

- [ ] **Step 3: Real-hardware sanity check**

With your MX Master 3S or MX Vertical connected, launch normally:

```bash
pkill -f logitune 2>/dev/null; sleep 1
nohup /usr/bin/logitune > /tmp/logitune-live.log 2>&1 & disown
```

Then:
- Open the Point & Scroll page on the connected mouse.
- Press the physical SmartShift button on MX Master 3S (above the thumb rest). The SmartShift toggle and scroll-wheel readout in the UI should flip within a frame.
- Press the DPI cycle button on MX Vertical. The DPI value on the Point & Scroll page should step through the ring.
- Switch focus to a different app that has a saved profile. The UI should restore the profile's values (proving setDisplayValues re-arms the cache).

If anything stays stale, re-inspect the relevant connect() in `addPhysicalDevice` to verify the signal names match and the `device != selectedDevice()` guard is not accidentally reversed.

- [ ] **Step 4: Branch commit list**

```bash
git log --oneline master..HEAD
```

Expected: the spec commit plus five new commits in the order defined above (no amendments).

- [ ] **Step 5: Em-dash scan on touched files**

```bash
git diff --name-only master..HEAD \
  | grep -vE '\.(png|svg)$' \
  | xargs -I{} sh -c 'printf "%s: " "{}"; grep -c "—" "{}" 2>/dev/null || echo "N/A"'
```

Expected: `0` on every C++/header/test file. `docs/superpowers/specs/*` may print non-zero (pre-existing).

Do NOT push the branch. Maintainer pushes and opens the PR.

---

## Done criteria

- Clean Debug rebuild from scratch: 0 errors, 0 warnings introduced by this plan.
- 550 core tests pass (539 baseline after v0.3.1-beta.1 plus 11 new: 2 in Task 1, 3 in Task 3, 6 in Task 4). 72 QML tests pass.
- Real-hardware smoke: physical SmartShift button press updates the Point & Scroll SmartShift toggle; DPI cycle button updates the DPI readout; profile reload still restores profile values.
- Five implementation commits on the branch, each reviewable in isolation, none amended.
- `grep -c "—"` on every touched C++/JSON/test file prints `0`.
