#include <gtest/gtest.h>
#include "DeviceSession.h"   // forward-declares the public API

using namespace logitune;

// effectiveDpiRing: returns curated ring when present, otherwise [min, mid, max]
// quantised to step when adjustableDpi is true, otherwise empty.

TEST(DpiCycleRing, UsesCuratedRingWhenPresent) {
    std::vector<int> curated{400, 1000, 1750, 4000};
    auto ring = DeviceSession::effectiveDpiRing(curated, true, 400, 4000, 100);
    EXPECT_EQ(ring, curated);
}

TEST(DpiCycleRing, ComputesFallbackWhenCuratedEmpty) {
    auto ring = DeviceSession::effectiveDpiRing({}, true, 200, 8000, 50);
    ASSERT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring[0], 200);
    EXPECT_EQ(ring[2], 8000);
    // Midpoint 4100 is already a multiple of 50 from 200
    EXPECT_EQ(ring[1], 4100);
}

TEST(DpiCycleRing, FallbackQuantisesMidpointToStep) {
    // min=200, max=4000, step=100 -> mid approx 2100 (from 200) which IS a multiple of 100
    auto ring = DeviceSession::effectiveDpiRing({}, true, 200, 4000, 100);
    ASSERT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring[0], 200);
    EXPECT_EQ(ring[1], 2100);
    EXPECT_EQ(ring[2], 4000);
}

TEST(DpiCycleRing, FallbackDegradesToTwoEntriesWhenStepTooLarge) {
    // min=200, max=300, step=200: quantised mid snaps below min or above max
    // depending on rounding; the correct policy is a two-entry ring.
    auto ring = DeviceSession::effectiveDpiRing({}, true, 200, 300, 200);
    ASSERT_EQ(ring.size(), 2u);
    EXPECT_EQ(ring[0], 200);
    EXPECT_EQ(ring[1], 300);
}

TEST(DpiCycleRing, FallbackDegradesToTwoEntriesWhenStepEqualsRange) {
    // min=200, max=400, step=200: single step spans the whole range; no
    // valid interior midpoint exists.
    auto ring = DeviceSession::effectiveDpiRing({}, true, 200, 400, 200);
    ASSERT_EQ(ring.size(), 2u);
    EXPECT_EQ(ring[0], 200);
    EXPECT_EQ(ring[1], 400);
}

TEST(DpiCycleRing, EmptyWhenAdjustableDpiFalse) {
    auto ring = DeviceSession::effectiveDpiRing({}, false, 200, 8000, 50);
    EXPECT_TRUE(ring.empty());
}

// nextDpiInRing: advances to next slot, wraps, tolerates off-preset current values

TEST(DpiCycleRing, NextAdvancesWithinRing) {
    std::vector<int> ring{400, 1000, 1750, 4000};
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 400), 1000);
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 1000), 1750);
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 1750), 4000);
}

TEST(DpiCycleRing, NextWrapsAtEnd) {
    std::vector<int> ring{400, 1000, 1750, 4000};
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 4000), 400);
}

TEST(DpiCycleRing, NextSnapsOffPresetToNearestThenAdvances) {
    std::vector<int> ring{400, 1000, 1750, 4000};
    // 1500 is closer to 1750 than 1000, so next is 4000
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 1500), 4000);
    // 1200 is closer to 1000 than 1750, so next is 1750
    EXPECT_EQ(DeviceSession::nextDpiInRing(ring, 1200), 1750);
}

TEST(DpiCycleRing, NextOnEmptyRingReturnsZero) {
    // Zero is the sentinel; caller checks for empty() before calling setDPI
    EXPECT_EQ(DeviceSession::nextDpiInRing({}, 1000), 0);
}
