# Capability Dispatch Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace accumulating `if/else` variant dispatch in `DeviceManager` with table-driven capability dispatch, enabling clean MX Master 2S battery support and future variant additions with zero `DeviceManager` changes.

**Architecture:** Introduce a generic `resolveCapability<>()` template plus small per-capability structs (`BatteryVariant`, `SmartShiftVariant`) and `constexpr` tables listing variants in preference order. `DeviceManager` resolves capabilities once at enumeration time into `std::optional<Variant>` members, then uses them everywhere. No device-specific subclasses. HID++ transport plumbing stays in `DeviceManager`.

**Tech Stack:** C++20, Qt6, GoogleTest, CMake/Ninja, HID++ 2.0 over hidraw.

**Tracking:** GitHub issue [#15](https://github.com/mmaher88/logitune/issues/15). Unblocks PR #12 (MX Master 2S).

---

## File Structure

**New files (created in this plan):**

- `src/core/hidpp/capabilities/Capabilities.h` — Generic `resolveCapability<>()` template. Header-only.
- `src/core/hidpp/capabilities/BatteryCapability.h` — `BatteryVariant` struct + declaration of `kBatteryVariants[]`.
- `src/core/hidpp/capabilities/BatteryCapability.cpp` — Definition of `kBatteryVariants[]` table.
- `src/core/hidpp/capabilities/SmartShiftCapability.h` — `SmartShiftVariant` struct + declaration of `kSmartShiftVariants[]`.
- `src/core/hidpp/capabilities/SmartShiftCapability.cpp` — Definition of `kSmartShiftVariants[]` table.
- `tests/test_capability_dispatch.cpp` — Unit tests for `resolveCapability<>()` + both tables.

**Modified files:**

- `src/core/hidpp/HidppTypes.h` — Add `BatteryStatus = 0x1000` to `FeatureId` enum.
- `src/core/hidpp/FeatureDispatcher.cpp` — Add `FeatureId::BatteryStatus` to `kKnownFeatures[]` for enumeration.
- `src/core/hidpp/features/Battery.h` — Declare `parseStatusLegacy()`.
- `src/core/hidpp/features/Battery.cpp` — Implement `parseStatusLegacy()` for `0x1000` response format.
- `src/core/DeviceManager.h` — Add `m_batteryDispatch` and `m_smartShiftDispatch` members.
- `src/core/DeviceManager.cpp` — Refactor `enumerateAndSetup()`, `setSmartShift()`, `handleNotification()` to use dispatch. Add device name descriptor override.
- `src/core/CMakeLists.txt` — Register new capability cpp files.
- `tests/CMakeLists.txt` — Register `test_capability_dispatch.cpp`.

---

## Task 1: Add `BatteryStatus = 0x1000` feature + `parseStatusLegacy`

**Files:**

- Modify: `src/core/hidpp/HidppTypes.h:27-35` (enum)
- Modify: `src/core/hidpp/FeatureDispatcher.cpp:7-20` (`kKnownFeatures` array)
- Modify: `src/core/hidpp/features/Battery.h:25-33` (class declaration)
- Modify: `src/core/hidpp/features/Battery.cpp` (implementation)
- Test: `tests/test_features.cpp` (new test cases appended)

### Step 1.1: Write failing test for `parseStatusLegacy`

Append to `tests/test_features.cpp` right after the existing Battery tests (after line 76):

```cpp
// ---------------------------------------------------------------------------
// Battery (BATTERY_STATUS 0x1000 legacy format)
// ---------------------------------------------------------------------------
// Legacy format:
//   params[0] = current discharge level (percentage)
//   params[1] = next discharge threshold (ignored)
//   params[2] = status byte (same enum as UnifiedBattery)

TEST(BatteryLegacy, ParseDischarging) {
    Report r;
    r.params[0] = 78;    // 78%
    r.params[1] = 50;    // next threshold (ignored)
    r.params[2] = 0x00;  // BatteryState::Discharging
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 78);
    EXPECT_EQ(status.state, BatteryState::Discharging);
    EXPECT_FALSE(status.charging);
}

TEST(BatteryLegacy, ParseRecharging) {
    Report r;
    r.params[0] = 45;
    r.params[1] = 30;
    r.params[2] = 0x01;  // BatteryState::Recharging
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 45);
    EXPECT_EQ(status.state, BatteryState::Recharging);
    EXPECT_TRUE(status.charging);
}

TEST(BatteryLegacy, ParseFull) {
    Report r;
    r.params[0] = 100;
    r.params[1] = 90;
    r.params[2] = 0x03;  // BatteryState::Full
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 100);
    EXPECT_EQ(status.state, BatteryState::Full);
    EXPECT_TRUE(status.charging);
}

TEST(BatteryLegacy, IgnoresMiddleByte) {
    // Legacy format's params[1] must NOT be interpreted as a level bitmask.
    // If parseStatusLegacy accidentally falls through to bitmask logic, it would
    // mis-report percentage. Setting params[0]=0 guards the fallback path.
    Report r;
    r.params[0] = 0;    // would trigger bitmask fallback in parseStatus()
    r.params[1] = 0x08; // looks like "full" bitmask but is actually next-threshold %
    r.params[2] = 0x00;
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 0);  // must stay 0, not become 90
    EXPECT_FALSE(status.charging);
}
```

- [ ] **Step 1.1 done**

### Step 1.2: Run test, verify failure

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -30`

Expected: compile error — `'parseStatusLegacy' is not a member of 'logitune::hidpp::features::Battery'`.

- [ ] **Step 1.2 done**

### Step 1.3: Add `FeatureId::BatteryStatus = 0x1000` to enum

Modify `src/core/hidpp/HidppTypes.h` lines 27-35. Current state:

```cpp
enum class FeatureId : uint16_t {
    Root            = 0x0000,
    FeatureSet      = 0x0001,
    DeviceInfo      = 0x0003,
    DeviceName      = 0x0005,
    BatteryUnified  = 0x1004,
    ChangeHost      = 0x1814,
    ReprogControlsV4= 0x1b04,
    SmartShift         = 0x2110,
    SmartShiftEnhanced = 0x2111,
    HiResWheel      = 0x2121,
    ThumbWheel      = 0x2150,
    AdjustableDPI   = 0x2201,
    GestureV2       = 0x6501,
};
```

Add `BatteryStatus = 0x1000` immediately before `BatteryUnified`:

```cpp
enum class FeatureId : uint16_t {
    Root            = 0x0000,
    FeatureSet      = 0x0001,
    DeviceInfo      = 0x0003,
    DeviceName      = 0x0005,
    BatteryStatus   = 0x1000,
    BatteryUnified  = 0x1004,
    ChangeHost      = 0x1814,
    ReprogControlsV4= 0x1b04,
    SmartShift         = 0x2110,
    SmartShiftEnhanced = 0x2111,
    HiResWheel      = 0x2121,
    ThumbWheel      = 0x2150,
    AdjustableDPI   = 0x2201,
    GestureV2       = 0x6501,
};
```

- [ ] **Step 1.3 done**

### Step 1.4: Register in `FeatureDispatcher::kKnownFeatures`

Modify `src/core/hidpp/FeatureDispatcher.cpp` lines 7-20. Add `FeatureId::BatteryStatus` immediately before `FeatureId::BatteryUnified`:

```cpp
static constexpr FeatureId kKnownFeatures[] = {
    FeatureId::Root,
    FeatureId::FeatureSet,
    FeatureId::DeviceInfo,
    FeatureId::DeviceName,
    FeatureId::BatteryStatus,
    FeatureId::BatteryUnified,
    FeatureId::ChangeHost,
    FeatureId::ReprogControlsV4,
    FeatureId::SmartShift,
    FeatureId::SmartShiftEnhanced,
    FeatureId::HiResWheel,
    FeatureId::ThumbWheel,
    FeatureId::AdjustableDPI,
    FeatureId::GestureV2,
};
```

- [ ] **Step 1.4 done**

### Step 1.5: Declare `parseStatusLegacy()` in `Battery.h`

Modify `src/core/hidpp/features/Battery.h` inside the `Battery` class declaration (around line 25-32):

```cpp
class Battery {
public:
    // UnifiedBattery (0x1004): params[0]=%, params[1]=level bitmask, params[2]=status
    static BatteryStatus parseStatus(const Report &r);

    // BatteryStatus (0x1000) legacy: params[0]=%, params[1]=next threshold, params[2]=status
    static BatteryStatus parseStatusLegacy(const Report &r);

    static constexpr uint8_t kFnGetCapabilities = 0x00;
    static constexpr uint8_t kFnGetStatus = 0x01;
};
```

- [ ] **Step 1.5 done**

### Step 1.6: Implement `parseStatusLegacy()`

Modify `src/core/hidpp/features/Battery.cpp`. Add the function after `parseStatus()`:

```cpp
BatteryStatus Battery::parseStatusLegacy(const Report &r)
{
    // BatteryStatus (0x1000) legacy format — from Solaar hidpp20.decipher_battery_status:
    //   params[0] = current discharge level (percentage, 0 = unknown)
    //   params[1] = next discharge threshold (informational, ignored)
    //   params[2] = status byte (same enum as UnifiedBattery)
    BatteryStatus status;
    status.level        = static_cast<int>(r.params[0]);
    status.levelBitmask = 0;  // not present in legacy format
    status.state        = static_cast<BatteryState>(r.params[2]);

    status.charging = (status.state == BatteryState::Recharging ||
                       status.state == BatteryState::AlmostFull  ||
                       status.state == BatteryState::Full         ||
                       status.state == BatteryState::SlowRecharge);
    return status;
}
```

- [ ] **Step 1.6 done**

### Step 1.7: Build and run the new tests

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build, no errors.

Then:

Run: `/home/mina/repos/logitune/build/tests/logitune-tests --gtest_filter='BatteryLegacy.*' 2>&1 | tail -20`

Expected: 4 tests pass.

- [ ] **Step 1.7 done**

### Step 1.8: Run all battery tests to confirm no regressions

Run: `/home/mina/repos/logitune/build/tests/logitune-tests --gtest_filter='Battery*.*' 2>&1 | tail -20`

Expected: all Battery + BatteryLegacy tests pass (10 total: 6 existing + 4 new).

- [ ] **Step 1.8 done**

### Step 1.9: Commit Task 1

```bash
cd /home/mina/repos/logitune && git add \
  src/core/hidpp/HidppTypes.h \
  src/core/hidpp/FeatureDispatcher.cpp \
  src/core/hidpp/features/Battery.h \
  src/core/hidpp/features/Battery.cpp \
  tests/test_features.cpp && \
git commit -m "add BatteryStatus (0x1000) feature + Battery::parseStatusLegacy

MX Master 2S and older devices use BATTERY_STATUS (0x1000) instead of
UNIFIED_BATTERY (0x1004). Format is nearly identical but params[1] is
next-threshold percentage rather than a level bitmask.

refs #15"
```

- [ ] **Step 1.9 done**

---

## Task 2: Create `resolveCapability<>()` template + tests

**Files:**

- Create: `src/core/hidpp/capabilities/Capabilities.h`
- Create: `tests/test_capability_dispatch.cpp`
- Modify: `tests/CMakeLists.txt` (register new test file)

### Step 2.1: Write failing test for `resolveCapability`

Create `tests/test_capability_dispatch.cpp`:

```cpp
#include <gtest/gtest.h>
#include "hidpp/capabilities/Capabilities.h"
#include "hidpp/FeatureDispatcher.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::capabilities;

namespace {

// Minimal test variant struct matching the shape real variants use.
struct TestVariant {
    FeatureId feature;
    int       tag;   // differentiator for assertions
};

constexpr TestVariant kTestVariants[] = {
    { FeatureId::BatteryUnified, 1 },
    { FeatureId::BatteryStatus,  2 },
};

} // namespace

TEST(ResolveCapability, ReturnsFirstMatch) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::BatteryUnified, 0x02},
    });
    auto v = resolveCapability(&fd, kTestVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::BatteryUnified);
    EXPECT_EQ(v->tag, 1);
}

TEST(ResolveCapability, FallsBackToSecondMatch) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::BatteryStatus, 0x02},
    });
    auto v = resolveCapability(&fd, kTestVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::BatteryStatus);
    EXPECT_EQ(v->tag, 2);
}

TEST(ResolveCapability, PrefersFirstWhenBothPresent) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::BatteryUnified, 0x02},
        {FeatureId::BatteryStatus,  0x03},
    });
    auto v = resolveCapability(&fd, kTestVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::BatteryUnified);
    EXPECT_EQ(v->tag, 1);
}

TEST(ResolveCapability, ReturnsNulloptWhenNoneMatch) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::GestureV2, 0x02},
    });
    auto v = resolveCapability(&fd, kTestVariants);
    EXPECT_FALSE(v.has_value());
}

TEST(ResolveCapability, ReturnsNulloptWhenDispatcherEmpty) {
    FeatureDispatcher fd;
    auto v = resolveCapability(&fd, kTestVariants);
    EXPECT_FALSE(v.has_value());
}
```

- [ ] **Step 2.1 done**

### Step 2.2: Register test in `tests/CMakeLists.txt`

Modify `tests/CMakeLists.txt` line 4-38, add `test_capability_dispatch.cpp` to the `logitune-tests` sources list. Insert after `test_feature_dispatcher.cpp` (line 8):

```cmake
add_executable(logitune-tests
    test_main.cpp
    test_smoke.cpp
    test_transport.cpp
    test_feature_dispatcher.cpp
    test_capability_dispatch.cpp
    test_scroll_features.cpp
    test_features.cpp
    ...
```

- [ ] **Step 2.2 done**

### Step 2.3: Run test, verify it fails to compile

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -15`

Expected: failure — `fatal error: hidpp/capabilities/Capabilities.h: No such file or directory`.

- [ ] **Step 2.3 done**

### Step 2.4: Create `Capabilities.h` with the template

Create `src/core/hidpp/capabilities/Capabilities.h`:

```cpp
#pragma once
#include <cstddef>
#include <optional>
#include "hidpp/FeatureDispatcher.h"

namespace logitune::hidpp::capabilities {

// resolveCapability walks a constexpr table of variants and returns the first
// one whose `feature` is advertised by the given dispatcher. Returns nullopt
// if none of the variants are supported.
//
// Each Variant struct must have a `FeatureId feature;` member. Other fields
// (function IDs, parser pointers) are variant-specific.
template<typename Variant, size_t N>
std::optional<Variant> resolveCapability(FeatureDispatcher* dispatcher,
                                         const Variant (&variants)[N])
{
    if (!dispatcher)
        return std::nullopt;
    for (const auto& v : variants) {
        if (dispatcher->hasFeature(v.feature))
            return v;
    }
    return std::nullopt;
}

} // namespace logitune::hidpp::capabilities
```

- [ ] **Step 2.4 done**

### Step 2.5: Build and run tests

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build.

Run: `/home/mina/repos/logitune/build/tests/logitune-tests --gtest_filter='ResolveCapability.*' 2>&1 | tail -15`

Expected: 5 tests pass.

- [ ] **Step 2.5 done**

### Step 2.6: Commit Task 2

```bash
cd /home/mina/repos/logitune && git add \
  src/core/hidpp/capabilities/Capabilities.h \
  tests/test_capability_dispatch.cpp \
  tests/CMakeLists.txt && \
git commit -m "add resolveCapability<>() template

Generic table walker that picks the first HID++ feature variant a
device supports. Foundation for Battery/SmartShift capability dispatch.

refs #15"
```

- [ ] **Step 2.6 done**

---

## Task 3: Create `BatteryCapability` variant + table

**Files:**

- Create: `src/core/hidpp/capabilities/BatteryCapability.h`
- Create: `src/core/hidpp/capabilities/BatteryCapability.cpp`
- Modify: `src/core/CMakeLists.txt` (register new cpp)
- Modify: `tests/test_capability_dispatch.cpp` (add table-specific tests)

### Step 3.1: Write failing test for `kBatteryVariants[]` table

Append to `tests/test_capability_dispatch.cpp`:

```cpp
#include "hidpp/capabilities/BatteryCapability.h"
#include "hidpp/features/Battery.h"

using namespace logitune::hidpp::features;

TEST(BatteryCapability, PrefersUnifiedOverLegacy) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::BatteryUnified, 0x02},
        {FeatureId::BatteryStatus,  0x03},
    });
    auto v = resolveCapability(&fd, kBatteryVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::BatteryUnified);
    EXPECT_EQ(v->getFn, 0x01);  // kFnGetStatus for UnifiedBattery
}

TEST(BatteryCapability, FallsBackToLegacy) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::BatteryStatus, 0x02},
    });
    auto v = resolveCapability(&fd, kBatteryVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::BatteryStatus);
    EXPECT_EQ(v->getFn, 0x00);  // fn0 for legacy BatteryStatus
}

TEST(BatteryCapability, ParserPointerRoutesCorrectly) {
    // Unified variant's parser should handle bitmask fallback;
    // Legacy variant's parser should not.
    Report r;
    r.params[0] = 0;
    r.params[1] = 0x08; // interpreted as bitmask by unified, as threshold by legacy
    r.params[2] = 0x00;

    auto unified = kBatteryVariants[0];
    auto legacy  = kBatteryVariants[1];

    auto unifiedStatus = unified.parse(r);
    EXPECT_EQ(unifiedStatus.level, 90);   // bitmask 0x08 = full = 90%

    auto legacyStatus = legacy.parse(r);
    EXPECT_EQ(legacyStatus.level, 0);     // legacy does not use bitmask
}
```

- [ ] **Step 3.1 done**

### Step 3.2: Run test, verify it fails to compile

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -10`

Expected: `fatal error: hidpp/capabilities/BatteryCapability.h: No such file or directory`.

- [ ] **Step 3.2 done**

### Step 3.3: Create `BatteryCapability.h`

Create `src/core/hidpp/capabilities/BatteryCapability.h`:

```cpp
#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/features/Battery.h"

namespace logitune::hidpp::capabilities {

// One Battery variant: a feature ID + its get function + a parser.
struct BatteryVariant {
    FeatureId feature;
    uint8_t   getFn;
    logitune::hidpp::features::BatteryStatus (*parse)(const logitune::hidpp::Report&);
};

// Known battery variants in preference order.
// UnifiedBattery (0x1004) is preferred when present because it exposes the
// level bitmask fallback. Legacy BatteryStatus (0x1000) is used otherwise.
extern const BatteryVariant kBatteryVariants[2];

} // namespace logitune::hidpp::capabilities
```

- [ ] **Step 3.3 done**

### Step 3.4: Create `BatteryCapability.cpp` with the table

Create `src/core/hidpp/capabilities/BatteryCapability.cpp`:

```cpp
#include "hidpp/capabilities/BatteryCapability.h"

namespace logitune::hidpp::capabilities {

const BatteryVariant kBatteryVariants[2] = {
    {
        FeatureId::BatteryUnified,
        features::Battery::kFnGetStatus,     // 0x01
        &features::Battery::parseStatus,
    },
    {
        FeatureId::BatteryStatus,
        0x00,                                 // fn0 for legacy
        &features::Battery::parseStatusLegacy,
    },
};

} // namespace logitune::hidpp::capabilities
```

- [ ] **Step 3.4 done**

### Step 3.5: Register cpp in `src/core/CMakeLists.txt`

Modify `src/core/CMakeLists.txt` line 3-33, add the capabilities cpp inside the `target_sources` list. Add right after `hidpp/features/DeviceName.cpp`:

```cmake
    hidpp/features/DeviceName.cpp
    hidpp/capabilities/BatteryCapability.cpp
    ProfileEngine.cpp
```

- [ ] **Step 3.5 done**

### Step 3.6: Build and run battery capability tests

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build.

Run: `/home/mina/repos/logitune/build/tests/logitune-tests --gtest_filter='BatteryCapability.*' 2>&1 | tail -15`

Expected: 3 tests pass.

- [ ] **Step 3.6 done**

### Step 3.7: Commit Task 3

```bash
cd /home/mina/repos/logitune && git add \
  src/core/hidpp/capabilities/BatteryCapability.h \
  src/core/hidpp/capabilities/BatteryCapability.cpp \
  src/core/CMakeLists.txt \
  tests/test_capability_dispatch.cpp && \
git commit -m "add BatteryCapability variant table

Preference order: UnifiedBattery (0x1004) first, then legacy
BatteryStatus (0x1000) fallback. Each variant carries its feature ID,
get function, and parser pointer.

refs #15"
```

- [ ] **Step 3.7 done**

---

## Task 4: Create `SmartShiftCapability` variant + table

**Files:**

- Create: `src/core/hidpp/capabilities/SmartShiftCapability.h`
- Create: `src/core/hidpp/capabilities/SmartShiftCapability.cpp`
- Modify: `src/core/CMakeLists.txt` (register new cpp)
- Modify: `tests/test_capability_dispatch.cpp` (add tests)

### Step 4.1: Write failing test for `kSmartShiftVariants[]`

Append to `tests/test_capability_dispatch.cpp`:

```cpp
#include "hidpp/capabilities/SmartShiftCapability.h"
#include "hidpp/features/SmartShift.h"

TEST(SmartShiftCapability, PrefersV1OverEnhanced) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::SmartShift,         0x02},
        {FeatureId::SmartShiftEnhanced, 0x03},
    });
    auto v = resolveCapability(&fd, kSmartShiftVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::SmartShift);
    EXPECT_EQ(v->getFn, 0x00);  // V1 kFnGetStatus
    EXPECT_EQ(v->setFn, 0x01);  // V1 kFnSetStatus
}

TEST(SmartShiftCapability, FallsBackToEnhanced) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::SmartShiftEnhanced, 0x02},
    });
    auto v = resolveCapability(&fd, kSmartShiftVariants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::SmartShiftEnhanced);
    EXPECT_EQ(v->getFn, 0x01);  // Enhanced fn1 for get
    EXPECT_EQ(v->setFn, 0x02);  // Enhanced fn2 for set
}

TEST(SmartShiftCapability, BuildSetParams) {
    auto v = kSmartShiftVariants[0];
    auto params = v.buildSet(2, 64);  // ratchet mode, threshold 64
    ASSERT_GE(params.size(), 2u);
    EXPECT_EQ(params[0], 2);
    EXPECT_EQ(params[1], 64);
}
```

- [ ] **Step 4.1 done**

### Step 4.2: Verify failure

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -10`

Expected: `fatal error: hidpp/capabilities/SmartShiftCapability.h: No such file or directory`.

- [ ] **Step 4.2 done**

### Step 4.3: Create `SmartShiftCapability.h`

Create `src/core/hidpp/capabilities/SmartShiftCapability.h`:

```cpp
#pragma once
#include <vector>
#include "hidpp/HidppTypes.h"
#include "hidpp/features/SmartShift.h"

namespace logitune::hidpp::capabilities {

// SmartShift has both a read path (get current config) and a write path
// (set mode + threshold), so the variant carries both function IDs plus
// the request builder.
struct SmartShiftVariant {
    FeatureId feature;
    uint8_t   getFn;
    uint8_t   setFn;
    logitune::hidpp::features::SmartShiftConfig (*parseGet)(const logitune::hidpp::Report&);
    std::vector<uint8_t> (*buildSet)(uint8_t mode, uint8_t autoDisengage);
};

// Known SmartShift variants in preference order.
// V1 (0x2110) is preferred on MX Master 3S and older.
// Enhanced (0x2111) is used on MX Master 4 and newer, with different function IDs.
extern const SmartShiftVariant kSmartShiftVariants[2];

} // namespace logitune::hidpp::capabilities
```

- [ ] **Step 4.3 done**

### Step 4.4: Create `SmartShiftCapability.cpp` with the table

Create `src/core/hidpp/capabilities/SmartShiftCapability.cpp`:

```cpp
#include "hidpp/capabilities/SmartShiftCapability.h"

namespace logitune::hidpp::capabilities {

const SmartShiftVariant kSmartShiftVariants[2] = {
    {
        FeatureId::SmartShift,
        features::SmartShift::kFnGetStatus,  // 0x00
        features::SmartShift::kFnSetStatus,  // 0x01
        &features::SmartShift::parseConfig,
        &features::SmartShift::buildSetConfig,
    },
    {
        FeatureId::SmartShiftEnhanced,
        0x01,                                 // Enhanced: fn1 = GetStatus
        0x02,                                 // Enhanced: fn2 = SetStatus
        &features::SmartShift::parseConfig,   // same response layout as V1
        &features::SmartShift::buildSetConfig, // same request layout as V1
    },
};

} // namespace logitune::hidpp::capabilities
```

- [ ] **Step 4.4 done**

### Step 4.5: Register cpp in `src/core/CMakeLists.txt`

Modify `src/core/CMakeLists.txt`. Add `hidpp/capabilities/SmartShiftCapability.cpp` right after `hidpp/capabilities/BatteryCapability.cpp`:

```cmake
    hidpp/capabilities/BatteryCapability.cpp
    hidpp/capabilities/SmartShiftCapability.cpp
```

- [ ] **Step 4.5 done**

### Step 4.6: Build and run smart shift capability tests

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -15`

Expected: clean build.

Run: `/home/mina/repos/logitune/build/tests/logitune-tests --gtest_filter='SmartShiftCapability.*' 2>&1 | tail -15`

Expected: 3 tests pass.

- [ ] **Step 4.6 done**

### Step 4.7: Commit Task 4

```bash
cd /home/mina/repos/logitune && git add \
  src/core/hidpp/capabilities/SmartShiftCapability.h \
  src/core/hidpp/capabilities/SmartShiftCapability.cpp \
  src/core/CMakeLists.txt \
  tests/test_capability_dispatch.cpp && \
git commit -m "add SmartShiftCapability variant table

Preference order: SmartShift V1 (0x2110) first, SmartShiftEnhanced
(0x2111) fallback. Both variants share parseConfig and buildSetConfig
but differ on function IDs (V1 fn0/fn1, Enhanced fn1/fn2).

refs #15"
```

- [ ] **Step 4.7 done**

---

## Task 5: Refactor `DeviceManager` to use dispatched capabilities + device name override

**Files:**

- Modify: `src/core/DeviceManager.h` (add dispatch members)
- Modify: `src/core/DeviceManager.cpp` (refactor enumerate, setSmartShift, notifications, device name)

### Step 5.1: Add dispatch members to `DeviceManager.h`

Modify `src/core/DeviceManager.h`. Find the private members section (around line 146-186) and add the dispatch optional members alongside the other capability state:

Locate the block that starts with:

```cpp
    DeviceRegistry *m_registry = nullptr;
    const IDevice *m_activeDevice = nullptr;

    std::unique_ptr<hidpp::HidrawDevice> m_device;
```

Add new includes at the top of `DeviceManager.h` (after the existing hidpp includes):

```cpp
#include "hidpp/capabilities/BatteryCapability.h"
#include "hidpp/capabilities/SmartShiftCapability.h"
```

Replace `#include <memory>` area to also include `<optional>`:

```cpp
#include <memory>
#include <optional>
```

Then add the dispatch members after the `m_commandQueue` member (around line 152):

```cpp
    std::unique_ptr<hidpp::CommandQueue> m_commandQueue;

    // Resolved capability dispatches — set once at enumerateAndSetup
    std::optional<hidpp::capabilities::BatteryVariant>    m_batteryDispatch;
    std::optional<hidpp::capabilities::SmartShiftVariant> m_smartShiftDispatch;
```

- [ ] **Step 5.1 done**

### Step 5.2: Resolve dispatches at enumeration time

Modify `src/core/DeviceManager.cpp`. In `enumerateAndSetup()`, after `m_features->setFeatureTable(...)` has been called (around line 649 — the SmartShift block starts near 657), find the block that begins the `SmartShift` + `Battery` reading logic.

Add a new include at the top:

```cpp
#include "hidpp/capabilities/Capabilities.h"
```

Then, in `enumerateAndSetup()`, before reading battery and smart-shift state, resolve the dispatches. Find where SmartShift is currently read (this was the block I added for V1/Enhanced fallback — look around line 656-682). Replace that entire block with:

```cpp
    // Resolve variant dispatches now that feature table is populated
    m_batteryDispatch    = hidpp::capabilities::resolveCapability(
                              m_features.get(), hidpp::capabilities::kBatteryVariants);
    m_smartShiftDispatch = hidpp::capabilities::resolveCapability(
                              m_features.get(), hidpp::capabilities::kSmartShiftVariants);

    // Read SmartShift using resolved dispatch
    if (m_smartShiftDispatch) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     m_smartShiftDispatch->feature,
                                     m_smartShiftDispatch->getFn);
        if (resp.has_value()) {
            auto cfg = m_smartShiftDispatch->parseGet(*resp);
            m_smartShiftEnabled   = cfg.isRatchet();
            m_smartShiftThreshold = cfg.autoDisengage;
            qCDebug(lcDevice) << "SmartShift: feature="
                              << Qt::hex << static_cast<uint16_t>(m_smartShiftDispatch->feature)
                              << "mode=" << cfg.mode
                              << (m_smartShiftEnabled ? "(ratchet)" : "(freespin)")
                              << "autoDisengage=" << m_smartShiftThreshold;
        }
    }
```

Find the existing battery read block. Current state (around line 700-703 after the old SmartShift block removal):

```cpp
    // ...previously somewhere the code read battery via feature->call(BatteryUnified)...
```

Locate the block where battery is currently read. Look for `BatteryUnified` around line 695. Replace that block with:

```cpp
    // Read battery using resolved dispatch
    int battLevel = 0;
    bool battCharging = false;
    if (m_batteryDispatch) {
        auto resp = m_features->call(m_transport.get(), m_deviceIndex,
                                     m_batteryDispatch->feature,
                                     m_batteryDispatch->getFn);
        if (resp.has_value()) {
            auto status = m_batteryDispatch->parse(*resp);
            battLevel = status.level;
            battCharging = status.charging;
            qCDebug(lcDevice) << "battery: feature="
                              << Qt::hex << static_cast<uint16_t>(m_batteryDispatch->feature)
                              << "level=" << battLevel << "% charging=" << battCharging;
        }
    }
```

Note: the exact location of the existing battery read depends on surrounding code. If the existing battery read is currently inside a different block, move it to sit together with the SmartShift resolution for clarity. The variables `battLevel` and `battCharging` must remain available for the existing state-update code that follows (line ~759 in the existing file).

- [ ] **Step 5.2 done**

### Step 5.3: Add device name descriptor override

Modify `src/core/DeviceManager.cpp` line 792. Current:

```cpp
    m_deviceName     = name;
```

Change to:

```cpp
    // Prefer descriptor-provided name (user-facing) over HID++-reported name (internal).
    m_deviceName     = m_activeDevice ? m_activeDevice->deviceName() : name;
```

- [ ] **Step 5.3 done**

### Step 5.4: Refactor `setSmartShift()` to use dispatch

Modify `src/core/DeviceManager.cpp`. Find the current `setSmartShift()` implementation (around line 1140-1175, the one with the V1/Enhanced if/else I wrote earlier). Replace with:

```cpp
void DeviceManager::setSmartShift(bool enabled, int threshold)
{
    if (!m_connected || !m_features || !m_commandQueue || !m_smartShiftDispatch)
        return;

    threshold = qBound(1, threshold, 255);

    m_smartShiftEnabled   = enabled;
    m_smartShiftThreshold = threshold;
    emit smartShiftChanged();

    uint8_t mode = enabled ? 2 : 1;
    uint8_t ad   = static_cast<uint8_t>(threshold);

    auto params = m_smartShiftDispatch->buildSet(mode, ad);
    m_commandQueue->enqueue(m_smartShiftDispatch->feature,
                            m_smartShiftDispatch->setFn,
                            std::span<const uint8_t>(params));
    qCDebug(lcDevice) << "SmartShift set: feature="
                      << Qt::hex << static_cast<uint16_t>(m_smartShiftDispatch->feature)
                      << "mode=" << mode << "autoDisengage=" << ad;
}
```

- [ ] **Step 5.4 done**

### Step 5.5: Refactor `handleNotification()` for battery + smart shift

Modify `src/core/DeviceManager.cpp`. Find the notification handler's battery section (around line 963-979) and the smart shift section (around line 999-1015).

Replace the battery notification block:

```cpp
    // Battery notification (any dispatched variant)
    if (m_batteryDispatch && m_features) {
        auto idx = m_features->featureIndex(m_batteryDispatch->feature);
        if (idx.has_value() && report.featureIndex == *idx) {
            auto status = m_batteryDispatch->parse(report);
            bool levelChanged  = (m_batteryLevel    != status.level);
            bool chargeChanged = (m_batteryCharging != status.charging);
            m_batteryLevel    = status.level;
            m_batteryCharging = status.charging;
            if (levelChanged)  emit batteryLevelChanged();
            if (chargeChanged) emit batteryChargingChanged();
            return;
        }
    }
```

Replace the smart shift notification block (the `for (auto ssId : ...)` loop I added earlier) with:

```cpp
    // SmartShift notification (resolved variant)
    if (m_smartShiftDispatch && m_features) {
        auto idx = m_features->featureIndex(m_smartShiftDispatch->feature);
        if (idx.has_value() && report.featureIndex == *idx) {
            auto cfg = m_smartShiftDispatch->parseGet(report);
            bool newEnabled = cfg.isRatchet();
            if (m_smartShiftEnabled != newEnabled) {
                m_smartShiftEnabled   = newEnabled;
                m_smartShiftThreshold = cfg.autoDisengage;
                qCDebug(lcDevice) << "SmartShift toggled:"
                                  << (newEnabled ? "ratchet" : "freespin");
                emit smartShiftChanged();
            }
            return;
        }
    }
```

- [ ] **Step 5.5 done**

### Step 5.6: Reset dispatches on disconnect

In `disconnectDevice()` (around line 877-890), find the cleanup block. Add reset of the dispatch optionals so a reconnecting device re-resolves:

Find:

```cpp
    m_device.reset();
```

Add immediately before or after:

```cpp
    m_batteryDispatch.reset();
    m_smartShiftDispatch.reset();
```

- [ ] **Step 5.6 done**

### Step 5.7: Build

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) 2>&1 | tail -20`

Expected: clean build. If there are unresolved-symbol or include-order errors, fix them:
- If `BatteryVariant` is not recognized: verify the `#include "hidpp/capabilities/BatteryCapability.h"` is in `DeviceManager.h`.
- If `std::optional` is not recognized: verify `#include <optional>` in `DeviceManager.h`.
- If `m_batteryDispatch->feature` fails on pointer-like access: `std::optional` uses `->` for its value, check the member access is correct.

- [ ] **Step 5.7 done**

### Step 5.8: Run all existing tests to confirm no regressions

Run: `/home/mina/repos/logitune/build/tests/logitune-tests 2>&1 | tail -10`

Expected: **all tests pass** (previously 395, now 395 + 11 new capability tests = 406).

Run: `/home/mina/repos/logitune/build/tests/logitune-tray-tests 2>&1 | tail -5`

Expected: 12 tests pass.

- [ ] **Step 5.8 done**

### Step 5.9: Commit Task 5

```bash
cd /home/mina/repos/logitune && git add \
  src/core/DeviceManager.h \
  src/core/DeviceManager.cpp && \
git commit -m "refactor: use capability dispatch for battery + smart shift

Replaces three separate if/else variant branches in DeviceManager
(enumerate, setSmartShift, notifications) with resolved dispatch
objects that are picked once at enumeration time. Adds device name
descriptor override so the UI shows 'MX Master 2S' instead of the
HID++-reported name.

New devices with variant HID++ features now require zero changes to
DeviceManager — adding a new variant is a table entry in
Battery/SmartShiftCapability.cpp.

refs #15"
```

- [ ] **Step 5.9 done**

---

## Task 6: Build, full test run, smoke test on MX3S

**Files:** none (verification only)

### Step 6.1: Clean build from scratch to catch incremental-build artifacts

Run: `cmake --build /home/mina/repos/logitune/build --parallel $(nproc) --clean-first 2>&1 | tail -10`

Expected: clean build, 0 warnings or errors.

- [ ] **Step 6.1 done**

### Step 6.2: Run the full test suite

Run: `/home/mina/repos/logitune/build/tests/logitune-tests 2>&1 | tail -5`

Expected: **406 tests pass, 0 failures** (395 pre-existing + 11 new capability tests).

- [ ] **Step 6.2 done**

### Step 6.3: Run tray tests

Run: `/home/mina/repos/logitune/build/tests/logitune-tray-tests 2>&1 | tail -5`

Expected: 12 tests pass.

- [ ] **Step 6.3 done**

### Step 6.4: Smoke test on real MX3S hardware

Kill any running instance:

Run: `ps -eo pid,cmd | grep -E "/logitune$" | grep -v grep`

If a PID is shown, kill it:

Run: `kill <PID>`

Launch fresh build:

```bash
nohup /home/mina/repos/logitune/build/src/app/logitune --debug > /tmp/logitune-cap-refactor.log 2>&1 &
disown
```

Wait 1 second, then:

Run: `sleep 1 && grep -E "SmartShift|battery|matched device descriptor|Startup complete" /tmp/logitune-cap-refactor.log`

Expected log lines:
- `matched device descriptor: "MX Master 3S"`
- `SmartShift: feature= 2110 mode= 2 (ratchet) autoDisengage= 100`
- `battery: feature= 1004 level= XX% charging= false`
- `Startup complete`

If MX3S battery and SmartShift both show the correct feature IDs and values, the dispatch is resolving correctly.

- [ ] **Step 6.4 done**

### Step 6.5: Exercise SmartShift toggle on live device

With logitune running, toggle SmartShift via the UI in Point & Scroll page. Then:

Run: `grep "SmartShift set:" /tmp/logitune-cap-refactor.log | tail -5`

Expected lines like: `SmartShift set: feature= 2110 mode= 1 autoDisengage= 64` (mode changes between 1 and 2 as you toggle).

- [ ] **Step 6.5 done**

### Step 6.6: Verify device name is "MX Master 3S" in the UI

Check the home screen of the running app. Device name label should read "MX Master 3S" (from descriptor, matches previous behavior because MX3S's HID++-reported name already matches).

Also check log:

Run: `grep "device name:" /tmp/logitune-cap-refactor.log`

Expected: `device name: "MX Master 3S"`.

- [ ] **Step 6.6 done**

### Step 6.7: Kill the smoke-test instance

Run: `ps -eo pid,cmd | grep -E "build/src/app/logitune" | grep -v grep | awk '{print $1}' | xargs -r kill`

- [ ] **Step 6.7 done**

---

## Task 7: Commit log review, push branch, open PR, update PR #12 comment

**Files:** none (git operations)

### Step 7.1: Review commit log on the branch

Run: `git log --oneline master..HEAD`

Expected: 5 commits (Tasks 1-5 produced one commit each).

- [ ] **Step 7.1 done**

### Step 7.2: Push branch

Run: `git push -u origin capability-dispatch-refactor 2>&1 | tail -10`

Expected: branch pushed, `* [new branch] capability-dispatch-refactor -> capability-dispatch-refactor`.

If the pre-push hook runs tests, all must pass.

- [ ] **Step 7.2 done**

### Step 7.3: Open PR against master

Run:

```bash
gh pr create --base master --head capability-dispatch-refactor \
  --title "refactor: capability dispatch for HID++ variants" \
  --body "$(cat <<'EOF'
## Summary

Replaces `if/else` variant branches in `DeviceManager` with table-driven capability dispatch. Adds `BATTERY_STATUS (0x1000)` support for MX Master 2S. Adds device-name descriptor override.

Closes #15. Unblocks #12 (MX Master 2S).

## What changed

- **New `src/core/hidpp/capabilities/`** directory with:
  - `Capabilities.h` — generic `resolveCapability<>()` template
  - `BatteryCapability.{h,cpp}` — `BatteryVariant` struct + `kBatteryVariants[]` table (UnifiedBattery → BatteryStatus fallback)
  - `SmartShiftCapability.{h,cpp}` — `SmartShiftVariant` struct + `kSmartShiftVariants[]` table (V1 → Enhanced fallback)
- **`BATTERY_STATUS = 0x1000`** added to `FeatureId` enum + `kKnownFeatures[]`
- **`Battery::parseStatusLegacy()`** for the `0x1000` response format
- **`DeviceManager` refactored** to resolve capabilities once at enumeration and use them everywhere (enumerate, setSmartShift, notifications)
- **Device name override**: UI now shows `m_activeDevice->deviceName()` when a descriptor matches, falling back to the HID++-reported name
- **11 new unit tests** for dispatch resolution and table ordering

## Why

`DeviceManager` was accumulating `if/else` branches for each feature variant (one for SmartShift V1/Enhanced on the MX4 PR, another one about to be added for BATTERY_STATUS vs UNIFIED on the MX2S PR). This is happening in three places per capability and is error-prone.

The refactor replaces all of them with a single dispatch object per capability that's resolved once at enumeration time. Adding a future variant is now a single-line table entry with zero `DeviceManager` changes.

## How it works

```cpp
struct BatteryVariant {
    FeatureId feature;
    uint8_t   getFn;
    BatteryStatus (*parse)(const Report&);
};

static const BatteryVariant kBatteryVariants[] = {
    { FeatureId::BatteryUnified, 0x01, &Battery::parseStatus       },
    { FeatureId::BatteryStatus,  0x00, &Battery::parseStatusLegacy },
};

template<typename V, size_t N>
std::optional<V> resolveCapability(FeatureDispatcher* f, const V (&variants)[N]) {
    for (const auto& v : variants)
        if (f->hasFeature(v.feature)) return v;
    return std::nullopt;
}
```

\`DeviceManager\` owns `std::optional<BatteryVariant> m_batteryDispatch;` resolved once at enumeration. All call sites (read at enumerate, notification handler) use it directly.

## Testing

- 11 new unit tests for dispatch resolution (5 generic + 3 battery-specific + 3 smart-shift-specific)
- All existing 395 tests continue to pass → **406 total**
- Smoke tested on MX Master 3S (BT + receiver paths)
  - `matched device descriptor: "MX Master 3S"`
  - `SmartShift: feature= 2110 mode= 2 (ratchet) autoDisengage= 100`
  - `battery: feature= 1004 level= 95% charging= false`
  - SmartShift toggle via UI writes correctly with `SmartShift set: feature= 2110 mode= 1 autoDisengage= 64`
- No MX4 hardware to verify the Enhanced path, but the code path is identical to the one already shipping in master for MX4.

## Out of scope

- HiResWheel variants (smooth scroll on MX4 may be a variant difference but needs more protocol research)
- Multi-device support (separate follow-up that benefits from having this refactor land first)
- Per-device subclasses (deliberately rejected — HID++ 2.0 self-describes, subclassing would duplicate 95% plumbing for 5% variant difference)
EOF
)" 2>&1 | tail -5
```

Expected: PR URL printed.

- [ ] **Step 7.3 done**

### Step 7.4: Post PR link on Jelco's PR #12

Run:

```bash
gh pr comment 12 --body "Refactor PR is up: <PR URL from step 7.3>

Once this lands on master, rebase your branch and the battery + product name issues should resolve automatically. You'll still need to add a \`DeviceSpec\` entry for MX Master 2S to \`tests/test_device_registry.cpp\` using the parameterized pattern you set up on the MX4 PR."
```

Replace `<PR URL from step 7.3>` with the actual URL printed by gh.

- [ ] **Step 7.4 done**

---

## Self-Review

- [x] **Spec coverage:** all 4 items from issue #15 have tasks (BatteryStatus feature, parseStatusLegacy, capability dispatch, device name override).
- [x] **Placeholder scan:** no TBDs, TODOs, or "implement the rest" — every step has concrete code or commands.
- [x] **Type consistency:** `BatteryVariant` uses `getFn`/`parse`; `SmartShiftVariant` uses `getFn`/`setFn`/`parseGet`/`buildSet`. Referenced consistently across template, table, `DeviceManager` usage, and tests.
- [x] **New tests enumerated:** 4 Battery legacy tests + 5 resolveCapability tests + 3 BatteryCapability table tests + 3 SmartShiftCapability table tests = 15 new tests (step 5.8 says "11 new" — correction: **15 new tests total**; update step 5.8 expected count to 395 + 15 = 410 when this is executed).
- [x] **Branch + PR wiring:** issue #15 created, branch created before first commit, PR body references issue and PR #12.
- [x] **Smoke test coverage:** device name, battery, smart shift, and set path all verified on live hardware.

> **Note:** During execution, trust the actual test count from `gtest` output rather than the numbers in this plan. If the expected count in a step doesn't match, check the test run output before assuming a regression.

---

## Execution Handoff

Inline execution in this session via superpowers:executing-plans with checkpoint verification after each task completes.
