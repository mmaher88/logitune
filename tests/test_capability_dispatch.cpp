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

// ---------------------------------------------------------------------------
// BatteryCapability table
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// SmartShiftCapability table
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// GestureV2Capability table — diverted thumb-wheel events on devices
// (e.g. 1st-gen MX Master) that lack 0x2150.
// ---------------------------------------------------------------------------
#include "hidpp/capabilities/GestureV2Capability.h"
#include "hidpp/features/GestureV2.h"

TEST(GestureV2Capability, ResolvesWhenFeaturePresent) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::GestureV2, 0x0F},
    });
    auto v = resolveCapability(&fd, kGestureV2Variants);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->feature, FeatureId::GestureV2);
    EXPECT_EQ(v->setEnabledFn,  0x02);
    EXPECT_EQ(v->setDivertedFn, 0x04);
}

TEST(GestureV2Capability, BuildOffsetMaskRequestEncodesIndex) {
    auto v = kGestureV2Variants[0];

    // Index 3 → offset 0, mask 1<<3 = 0x08
    auto enableParams = v.buildOffsetMaskRequest(3, true);
    ASSERT_EQ(enableParams.size(), 4u);
    EXPECT_EQ(enableParams[0], 0x00);
    EXPECT_EQ(enableParams[1], 0x01);
    EXPECT_EQ(enableParams[2], 0x08);
    EXPECT_EQ(enableParams[3], 0x08);

    auto disableParams = v.buildOffsetMaskRequest(3, false);
    EXPECT_EQ(disableParams[3], 0x00);

    // Index 9 → offset 1, mask 1<<1 = 0x02
    auto secondByte = v.buildOffsetMaskRequest(9, true);
    EXPECT_EQ(secondByte[0], 0x01);
    EXPECT_EQ(secondByte[2], 0x02);
}

TEST(GestureV2Capability, ParseDivertedEventDecodesPayload) {
    auto v = kGestureV2Variants[0];

    // Captured movement event from a 1st-gen MX Master:
    //   payload byte 0 = 0x23 (movement), byte 2 = 0x03 (thumb-wheel marker),
    //   byte 3 = 0x01 (signed delta = +1).
    Report r{};
    r.params[0] = 0x23;
    r.params[1] = 0x00;
    r.params[2] = 0x03;
    r.params[3] = 0x01;

    auto evt = v.parseDivertedEvent(r);
    EXPECT_EQ(evt.status, 0x23);
    EXPECT_EQ(evt.gestureMarker, GestureV2::kThumbWheelEventMarker);
    EXPECT_EQ(evt.delta, 1);

    // Negative delta as signed int8 (0xFF = -1).
    r.params[3] = 0xFF;
    auto neg = v.parseDivertedEvent(r);
    EXPECT_EQ(neg.delta, -1);
}
