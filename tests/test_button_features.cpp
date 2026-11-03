#include <gtest/gtest.h>
#include "hidpp/features/ReprogControls.h"
#include "hidpp/features/GestureV2.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::features;

TEST(ReprogControls, HeldButtonsTrackIndividualReleases) {
    DivertedButtons buttons;
    Report report;
    report.paramLength = 8;
    report.params[0] = 1; report.params[1] = 0xa0;
    report.params[2] = 0; report.params[3] = 0x53;
    auto events = buttons.update(report);
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0], (std::pair<uint16_t, bool>{0x1a0, true}));
    EXPECT_TRUE(buttons.update(report).empty());
    report.params[0] = 0; report.params[1] = 0x53;
    report.params[2] = 0; report.params[3] = 0;
    events = buttons.update(report);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], (std::pair<uint16_t, bool>{0x1a0, false}));
    report.params[1] = 0;
    events = buttons.update(report);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], (std::pair<uint16_t, bool>{0x53, false}));
}

// ---------------------------------------------------------------------------
// ReprogControls
// ---------------------------------------------------------------------------

TEST(ReprogControls, ParseControlCount) {
    Report r;
    r.params[0] = 7;
    EXPECT_EQ(ReprogControls::parseControlCount(r), 7);
}

TEST(ReprogControls, ParseControlInfo) {
    Report r;
    r.params[0] = 0x00; r.params[1] = 0x50; // controlId = 0x0050
    r.params[2] = 0x00; r.params[3] = 0x38; // taskId = 0x0038
    r.params[4] = 0x03; // divertable + persist
    auto info = ReprogControls::parseControlInfo(r);
    EXPECT_EQ(info.controlId, 0x0050);
    EXPECT_EQ(info.taskId, 0x0038);
    EXPECT_TRUE(info.divertable);
    EXPECT_TRUE(info.persist);
}

TEST(ReprogControls, ParseControlInfoDivertableOnly) {
    Report r;
    r.params[0] = 0x00; r.params[1] = 0x52;
    r.params[2] = 0x00; r.params[3] = 0x3A;
    r.params[4] = 0x01; // divertable only
    auto info = ReprogControls::parseControlInfo(r);
    EXPECT_TRUE(info.divertable);
    EXPECT_FALSE(info.persist);
}

TEST(ReprogControls, BuildDivertRequest) {
    auto params = ReprogControls::buildSetDivert(0x0050, true);
    EXPECT_EQ(params[0], 0x00);
    EXPECT_EQ(params[1], 0x50);
    EXPECT_EQ(params[2], 0x03);
}

TEST(ReprogControls, BuildUndivertRequest) {
    auto params = ReprogControls::buildSetDivert(0x0050, false);
    EXPECT_EQ(params[0], 0x00);
    EXPECT_EQ(params[1], 0x50);
    // 0x02 = ChangeTemporaryDivert set, TemporaryDiverted clear (undivert)
    EXPECT_EQ(params[2], 0x02);
}

TEST(ReprogControls, ParseDivertedButtonEvent) {
    Report r;
    r.params[0] = 0x00; r.params[1] = 0x56;
    EXPECT_EQ(ReprogControls::parseDivertedButtonEvent(r), 0x0056);
}

TEST(ReprogControls, ParseDivertedButtonEventHighByte) {
    Report r;
    r.params[0] = 0x01; r.params[1] = 0x23;
    EXPECT_EQ(ReprogControls::parseDivertedButtonEvent(r), 0x0123);
}

// ---------------------------------------------------------------------------
// GestureV2
// ---------------------------------------------------------------------------

TEST(GestureV2, ParseGestureEvent) {
    Report r;
    r.params[0] = 0xFF; r.params[1] = 0xB0; // dx = -80 (signed)
    r.params[2] = 0x00; r.params[3] = 0x05; // dy = 5
    r.params[4] = 0x00; // not released
    auto evt = GestureV2::parseGestureEvent(r);
    EXPECT_EQ(evt.dx, -80);
    EXPECT_EQ(evt.dy, 5);
    EXPECT_FALSE(evt.released);
}

TEST(GestureV2, ParseGestureRelease) {
    Report r;
    r.params[0] = 0x00; r.params[1] = 0x00;
    r.params[2] = 0x00; r.params[3] = 0x00;
    r.params[4] = 0x01; // released
    auto evt = GestureV2::parseGestureEvent(r);
    EXPECT_EQ(evt.dx, 0);
    EXPECT_EQ(evt.dy, 0);
    EXPECT_TRUE(evt.released);
}

TEST(GestureV2, ParseGesturePositiveDelta) {
    Report r;
    r.params[0] = 0x00; r.params[1] = 0x64; // dx = 100
    r.params[2] = 0xFF; r.params[3] = 0x9C; // dy = -100
    r.params[4] = 0x00;
    auto evt = GestureV2::parseGestureEvent(r);
    EXPECT_EQ(evt.dx, 100);
    EXPECT_EQ(evt.dy, -100);
    EXPECT_FALSE(evt.released);
}

TEST(GestureV2, BuildSetGestureEnable) {
    auto params = GestureV2::buildSetGestureEnable(true);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x01);
}

TEST(GestureV2, BuildSetGestureDisable) {
    auto params = GestureV2::buildSetGestureEnable(false);
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0], 0x00);
}
