#include <gtest/gtest.h>
#include "hidpp/features/SmartShift.h"
#include "hidpp/features/HiResWheel.h"
#include "hidpp/features/ThumbWheel.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::features;

// ---------------------------------------------------------------------------
// SmartShift
// ---------------------------------------------------------------------------

TEST(SmartShift, ParseConfigEnabled)
{
    Report r;
    r.params[0]   = 0x01;  // enabled
    r.params[1]   = 30;    // threshold
    r.paramLength = 4;
    auto cfg = SmartShift::parseConfig(r);
    EXPECT_TRUE(cfg.enabled);
    EXPECT_EQ(cfg.threshold, 30);
}

TEST(SmartShift, ParseConfigDisabled)
{
    Report r;
    r.params[0]   = 0x00;  // disabled
    r.params[1]   = 100;
    r.paramLength = 4;
    auto cfg = SmartShift::parseConfig(r);
    EXPECT_FALSE(cfg.enabled);
    EXPECT_EQ(cfg.threshold, 100);
}

TEST(SmartShift, BuildSetConfig)
{
    auto params = SmartShift::buildSetConfig(true, 50);
    ASSERT_EQ(params.size(), 3u);
    EXPECT_EQ(params[0], 0x02);
    EXPECT_EQ(params[1], 0x01);
    EXPECT_EQ(params[2], 50);
}

TEST(SmartShift, BuildSetConfigDisabled)
{
    auto params = SmartShift::buildSetConfig(false, 10);
    ASSERT_EQ(params.size(), 3u);
    EXPECT_EQ(params[0], 0x02);
    EXPECT_EQ(params[1], 0x00);
    EXPECT_EQ(params[2], 10);
}

TEST(SmartShift, ConstantValues)
{
    EXPECT_EQ(SmartShift::kFnGetConfig, 0x00);
    EXPECT_EQ(SmartShift::kFnSetConfig, 0x01);
}

// ---------------------------------------------------------------------------
// HiResWheel
// ---------------------------------------------------------------------------

TEST(HiResWheel, ParseConfigBits)
{
    Report r;
    // bit 0 = hiRes, bit 1 = invert
    r.params[0]   = 0x03;  // both set
    r.params[1]   = 0x01;  // ratchet
    r.paramLength = 4;
    auto cfg = HiResWheel::parseConfig(r);
    EXPECT_TRUE(cfg.hiRes);
    EXPECT_TRUE(cfg.invert);
    EXPECT_TRUE(cfg.ratchet);
}

TEST(HiResWheel, ParseConfigFreespin)
{
    Report r;
    r.params[0]   = 0x01;  // hiRes only
    r.params[1]   = 0x02;  // freespin
    r.paramLength = 4;
    auto cfg = HiResWheel::parseConfig(r);
    EXPECT_TRUE(cfg.hiRes);
    EXPECT_FALSE(cfg.invert);
    EXPECT_FALSE(cfg.ratchet);
}

TEST(HiResWheel, ParseConfigNoBitsSet)
{
    Report r;
    r.params[0]   = 0x00;
    r.params[1]   = 0x02;
    r.paramLength = 4;
    auto cfg = HiResWheel::parseConfig(r);
    EXPECT_FALSE(cfg.hiRes);
    EXPECT_FALSE(cfg.invert);
    EXPECT_FALSE(cfg.ratchet);
}

TEST(HiResWheel, BuildSetConfigBothSet)
{
    auto params = HiResWheel::buildSetConfig(true, true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x03);
}

TEST(HiResWheel, BuildSetConfigHiResOnly)
{
    auto params = HiResWheel::buildSetConfig(true, false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x01);
}

TEST(HiResWheel, BuildSetConfigInvertOnly)
{
    auto params = HiResWheel::buildSetConfig(false, true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x02);
}

TEST(HiResWheel, BuildSetConfigNone)
{
    auto params = HiResWheel::buildSetConfig(false, false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x00);
}

TEST(HiResWheel, ConstantValues)
{
    EXPECT_EQ(HiResWheel::kFnGetWheelMode,     0x00);
    EXPECT_EQ(HiResWheel::kFnSetWheelMode,     0x02);
    EXPECT_EQ(HiResWheel::kFnGetRatchetSwitch, 0x03);
}

// ---------------------------------------------------------------------------
// ThumbWheel
// ---------------------------------------------------------------------------

TEST(ThumbWheel, ParseConfigInverted)
{
    Report r;
    r.params[0]   = 0x01;  // invert bit set
    r.params[1]   = 75;    // resolution
    r.paramLength = 4;
    auto cfg = ThumbWheel::parseConfig(r);
    EXPECT_TRUE(cfg.invert);
    EXPECT_EQ(cfg.resolution, 75);
}

TEST(ThumbWheel, ParseConfigNotInverted)
{
    Report r;
    r.params[0]   = 0x00;
    r.params[1]   = 50;
    r.paramLength = 4;
    auto cfg = ThumbWheel::parseConfig(r);
    EXPECT_FALSE(cfg.invert);
    EXPECT_EQ(cfg.resolution, 50);
}

TEST(ThumbWheel, BuildSetConfigInvert)
{
    auto params = ThumbWheel::buildSetConfig(true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x01);
}

TEST(ThumbWheel, BuildSetConfigNoInvert)
{
    auto params = ThumbWheel::buildSetConfig(false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x00);
}

TEST(ThumbWheel, ConstantValues)
{
    EXPECT_EQ(ThumbWheel::kFnGetConfig, 0x00);
    EXPECT_EQ(ThumbWheel::kFnSetConfig, 0x01);
}
