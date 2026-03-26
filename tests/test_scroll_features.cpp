#include <gtest/gtest.h>
#include "hidpp/features/SmartShift.h"
#include "hidpp/features/HiResWheel.h"
#include "hidpp/features/ThumbWheel.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::features;

// ---------------------------------------------------------------------------
// SmartShift
// ---------------------------------------------------------------------------

// SmartShift V1 (0x2110) — from logid/Solaar source
// GetStatus response: [mode(1=freespin,2=ratchet), autoDisengage, defaultAutoDisengage]

TEST(SmartShift, ParseRatchetMode)
{
    Report r;
    r.params[0] = 2;    // mode=ratchet
    r.params[1] = 100;  // autoDisengage threshold
    r.params[2] = 100;  // default
    auto cfg = SmartShift::parseConfig(r);
    EXPECT_TRUE(cfg.isRatchet());
    EXPECT_FALSE(cfg.isFreespin());
    EXPECT_EQ(cfg.mode, 2);
    EXPECT_EQ(cfg.autoDisengage, 100);
    EXPECT_EQ(cfg.defaultAutoDisengage, 100);
}

TEST(SmartShift, ParseFreespinMode)
{
    Report r;
    r.params[0] = 1;    // mode=freespin
    r.params[1] = 50;
    r.params[2] = 100;
    auto cfg = SmartShift::parseConfig(r);
    EXPECT_FALSE(cfg.isRatchet());
    EXPECT_TRUE(cfg.isFreespin());
    EXPECT_EQ(cfg.mode, 1);
    EXPECT_EQ(cfg.autoDisengage, 50);
}

TEST(SmartShift, BuildSetRatchet)
{
    // mode=2 (ratchet), autoDisengage=50
    auto params = SmartShift::buildSetConfig(2, 50);
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params[0], 2);   // ratchet
    EXPECT_EQ(params[1], 50);  // threshold
}

TEST(SmartShift, BuildSetFreespin)
{
    // mode=1 (freespin), autoDisengage=0 (don't change)
    auto params = SmartShift::buildSetConfig(1, 0);
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params[0], 1);
    EXPECT_EQ(params[1], 0);
}

TEST(SmartShift, BuildSetThresholdOnly)
{
    // mode=0 (don't change), autoDisengage=80
    auto params = SmartShift::buildSetConfig(0, 80);
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params[0], 0);
    EXPECT_EQ(params[1], 80);
}

TEST(SmartShift, ConstantValues)
{
    EXPECT_EQ(SmartShift::kFnGetStatus, 0x00);
    EXPECT_EQ(SmartShift::kFnSetStatus, 0x01);
}

// ---------------------------------------------------------------------------
// HiResWheel
// ---------------------------------------------------------------------------

TEST(HiResWheel, ParseConfigBits)
{
    Report r;
    // bit 1 = hiRes, bit 2 = invert (per Solaar 0x2121)
    r.params[0] = 0x06;  // bits 1+2 set
    auto cfg = HiResWheel::parseWheelMode(r);
    EXPECT_TRUE(cfg.hiRes);
    EXPECT_TRUE(cfg.invert);
}

TEST(HiResWheel, ParseConfigHiResOnly)
{
    Report r;
    r.params[0] = 0x02;  // bit 1 only
    auto cfg = HiResWheel::parseWheelMode(r);
    EXPECT_TRUE(cfg.hiRes);
    EXPECT_FALSE(cfg.invert);
}

TEST(HiResWheel, ParseConfigNoBitsSet)
{
    Report r;
    r.params[0] = 0x00;
    auto cfg = HiResWheel::parseWheelMode(r);
    EXPECT_FALSE(cfg.hiRes);
    EXPECT_FALSE(cfg.invert);
}

TEST(HiResWheel, ParseRatchetSwitch)
{
    Report r;
    r.params[0] = 0x01;
    EXPECT_TRUE(HiResWheel::parseRatchetSwitch(r));
    r.params[0] = 0x00;
    EXPECT_FALSE(HiResWheel::parseRatchetSwitch(r));
}

TEST(HiResWheel, BuildSetBothSet)
{
    // hiRes=bit1, invert=bit2 → 0x06
    auto params = HiResWheel::buildSetWheelMode(0x00, true, true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x06);
}

TEST(HiResWheel, BuildSetHiResOnly)
{
    auto params = HiResWheel::buildSetWheelMode(0x00, true, false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x02);
}

TEST(HiResWheel, BuildSetInvertOnly)
{
    auto params = HiResWheel::buildSetWheelMode(0x00, false, true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x04);
}

TEST(HiResWheel, BuildPreservesDiversionBit)
{
    // currentMode has diversion bit set (0x01)
    auto params = HiResWheel::buildSetWheelMode(0x01, true, false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x03); // diversion(0x01) + hiRes(0x02)
}

TEST(HiResWheel, ConstantValues)
{
    EXPECT_EQ(HiResWheel::kFnGetCapabilities,  0x00);
    EXPECT_EQ(HiResWheel::kFnGetWheelMode,     0x01);
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
