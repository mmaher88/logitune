#include <gtest/gtest.h>
#include "hidpp/FeatureDispatcher.h"

using namespace logitune::hidpp;

// ---------------------------------------------------------------------------
// setFeatureTable / featureIndex / hasFeature
// ---------------------------------------------------------------------------

TEST(FeatureDispatcher, MapFeatureIdToIndex) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::Root, 0x00},
        {FeatureId::FeatureSet, 0x01},
        {FeatureId::BatteryUnified, 0x02},
        {FeatureId::AdjustableDPI, 0x03},
    });
    EXPECT_EQ(fd.featureIndex(FeatureId::BatteryUnified), 0x02);
    EXPECT_EQ(fd.featureIndex(FeatureId::AdjustableDPI), 0x03);
    EXPECT_EQ(fd.featureIndex(FeatureId::SmartShift), std::nullopt);
}

TEST(FeatureDispatcher, FeatureAvailable) {
    FeatureDispatcher fd;
    fd.setFeatureTable({{FeatureId::BatteryUnified, 0x02}});
    EXPECT_TRUE(fd.hasFeature(FeatureId::BatteryUnified));
    EXPECT_FALSE(fd.hasFeature(FeatureId::GestureV2));
}

TEST(FeatureDispatcher, EmptyTableReturnsNullopt) {
    FeatureDispatcher fd;
    EXPECT_EQ(fd.featureIndex(FeatureId::Root), std::nullopt);
    EXPECT_FALSE(fd.hasFeature(FeatureId::Root));
}

TEST(FeatureDispatcher, RootAtIndexZero) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::Root, 0x00},
        {FeatureId::FeatureSet, 0x01},
    });
    EXPECT_EQ(fd.featureIndex(FeatureId::Root), 0x00);
}

TEST(FeatureDispatcher, AllKnownFeaturesCanBeSet) {
    FeatureDispatcher fd;
    fd.setFeatureTable({
        {FeatureId::Root,             0x00},
        {FeatureId::FeatureSet,       0x01},
        {FeatureId::DeviceName,       0x02},
        {FeatureId::BatteryUnified,   0x03},
        {FeatureId::ReprogControlsV4, 0x04},
        {FeatureId::SmartShift,       0x05},
        {FeatureId::HiResWheel,       0x06},
        {FeatureId::ThumbWheel,       0x07},
        {FeatureId::AdjustableDPI,    0x08},
        {FeatureId::GestureV2,        0x09},
    });
    EXPECT_EQ(fd.featureIndex(FeatureId::GestureV2), 0x09);
    EXPECT_EQ(fd.featureIndex(FeatureId::SmartShift), 0x05);
    EXPECT_TRUE(fd.hasFeature(FeatureId::HiResWheel));
}

TEST(FeatureDispatcher, SetFeatureTableOverwritesPrevious) {
    FeatureDispatcher fd;
    fd.setFeatureTable({{FeatureId::BatteryUnified, 0x02}});
    EXPECT_TRUE(fd.hasFeature(FeatureId::BatteryUnified));

    fd.setFeatureTable({{FeatureId::AdjustableDPI, 0x03}});
    EXPECT_FALSE(fd.hasFeature(FeatureId::BatteryUnified));
    EXPECT_TRUE(fd.hasFeature(FeatureId::AdjustableDPI));
}

// ---------------------------------------------------------------------------
// call() — no transport needed; missing feature returns nullopt
// ---------------------------------------------------------------------------

TEST(FeatureDispatcher, CallMissingFeatureReturnsNullopt) {
    FeatureDispatcher fd;
    fd.setFeatureTable({{FeatureId::Root, 0x00}});
    // BatteryUnified not in table — call() must return nullopt without touching transport
    auto result = fd.call(/*transport=*/nullptr, /*deviceIndex=*/0x01,
                          FeatureId::BatteryUnified, 0x00);
    EXPECT_FALSE(result.has_value());
}

TEST(FeatureDispatcher, CallEmptyTableReturnsNullopt) {
    FeatureDispatcher fd;
    auto result = fd.call(nullptr, 0x01, FeatureId::AdjustableDPI, 0x02);
    EXPECT_FALSE(result.has_value());
}
