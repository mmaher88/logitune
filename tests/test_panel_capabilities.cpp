#include <gtest/gtest.h>
#include "hidpp/capabilities/HapticsCapability.h"
#include "hidpp/capabilities/ForceSensingCapability.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::capabilities;

static Report replyWith(std::initializer_list<uint8_t> bytes)
{
    Report report;
    size_t index = 0;
    for (uint8_t byte : bytes)
        report.params[index++] = byte;
    return report;
}

// Replies below are what the user's MX Master 4 sent when probed read-only.

TEST(HapticsCapability, ParsesConfig) {
    const auto &haptics = kHapticsVariants[0];
    const auto config = haptics.parseConfig(replyWith({0x01, 0x19, 0x00}));
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.level, 25);
    EXPECT_FALSE(config.fourLevelsOnly);

    const auto off = haptics.parseConfig(replyWith({0x00, 0x32, 0x01}));
    EXPECT_FALSE(off.enabled);
    EXPECT_TRUE(off.fourLevelsOnly);
}

TEST(HapticsCapability, BuildsSetConfig) {
    const auto &haptics = kHapticsVariants[0];
    EXPECT_EQ(haptics.getConfigFn, 1);
    EXPECT_EQ(haptics.setConfigFn, 2);
    EXPECT_EQ(haptics.buildSetConfig(true, 60), (std::vector<uint8_t>{0x01, 60}));
    // Turning haptics off still sends a level; the device keeps it for when it's switched back on.
    EXPECT_EQ(haptics.buildSetConfig(false, 60), (std::vector<uint8_t>{0x00, 60}));
    EXPECT_EQ(haptics.buildSetConfig(true, 140), (std::vector<uint8_t>{0x01, 100}));
}

TEST(ForceSensingCapability, ParsesButtonInfo) {
    const auto &force = kForceSensingVariants[0];
    EXPECT_EQ(force.feature, FeatureId::ForceSensingButton);
    const auto info = force.parseInfo(replyWith({0x00, 0x01, 0x12, 0xc3, 0x18, 0xf4, 0x0f, 0x02}));
    EXPECT_TRUE(info.changeable);
    EXPECT_EQ(info.defaultForce, 4803);
    EXPECT_EQ(info.maxForce, 6388);
    EXPECT_EQ(info.minForce, 3842);
}

TEST(ForceSensingCapability, ParsesCurrentAndBuildsSet) {
    const auto &force = kForceSensingVariants[0];
    EXPECT_EQ(force.parseCurrent(replyWith({0x0f, 0x02})), 3842);
    EXPECT_EQ(force.buildSet(0, 4803), (std::vector<uint8_t>{0x00, 0x12, 0xc3}));
    EXPECT_EQ(force.countFn, 0);
    EXPECT_EQ(force.infoFn, 1);
    EXPECT_EQ(force.currentFn, 2);
    EXPECT_EQ(force.setFn, 3);
}
