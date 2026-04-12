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
