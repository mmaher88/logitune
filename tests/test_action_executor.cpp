#include <gtest/gtest.h>
#include "ActionExecutor.h"

#include <linux/input-event-codes.h>

using namespace logitune;

// ---------------------------------------------------------------------------
// parseKeystroke
// ---------------------------------------------------------------------------

TEST(ActionExecutor, ParseKeystrokeSimple) {
    auto keys = ActionExecutor::parseKeystroke("Back");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_BACK);
}

TEST(ActionExecutor, ParseKeystrokeCombo) {
    auto keys = ActionExecutor::parseKeystroke("Ctrl+Shift+A");
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_LEFTSHIFT);
    EXPECT_EQ(keys[2], KEY_A);
}

TEST(ActionExecutor, ParseKeystrokeCtrlC) {
    auto keys = ActionExecutor::parseKeystroke("Ctrl+C");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_C);
}

TEST(ActionExecutor, ParseKeystrokeTab) {
    auto keys = ActionExecutor::parseKeystroke("Tab");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_TAB);
}

TEST(ActionExecutor, ParseKeystrokeAltTab) {
    auto keys = ActionExecutor::parseKeystroke("Alt+Tab");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTALT);
    EXPECT_EQ(keys[1], KEY_TAB);
}

TEST(ActionExecutor, ParseKeystrokeForward) {
    auto keys = ActionExecutor::parseKeystroke("Forward");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_FORWARD);
}

TEST(ActionExecutor, ParseKeystrokeVolumeUp) {
    auto keys = ActionExecutor::parseKeystroke("VolumeUp");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_VOLUMEUP);
}

TEST(ActionExecutor, ParseKeystrokeVolumeDown) {
    auto keys = ActionExecutor::parseKeystroke("VolumeDown");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_VOLUMEDOWN);
}

TEST(ActionExecutor, ParseKeystrokeSuper) {
    auto keys = ActionExecutor::parseKeystroke("Super");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_LEFTMETA);
}

TEST(ActionExecutor, ParseKeystrokeSpace) {
    auto keys = ActionExecutor::parseKeystroke("Space");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_SPACE);
}

TEST(ActionExecutor, ParseKeystrokeEnter) {
    auto keys = ActionExecutor::parseKeystroke("Enter");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_ENTER);
}

TEST(ActionExecutor, ParseKeystrokeEscape) {
    auto keys = ActionExecutor::parseKeystroke("Escape");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_ESC);
}

TEST(ActionExecutor, ParseKeystrokeDelete) {
    auto keys = ActionExecutor::parseKeystroke("Delete");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0], KEY_DELETE);
}

TEST(ActionExecutor, ParseKeystrokeArrows) {
    EXPECT_EQ(ActionExecutor::parseKeystroke("Up")[0],    KEY_UP);
    EXPECT_EQ(ActionExecutor::parseKeystroke("Down")[0],  KEY_DOWN);
    EXPECT_EQ(ActionExecutor::parseKeystroke("Left")[0],  KEY_LEFT);
    EXPECT_EQ(ActionExecutor::parseKeystroke("Right")[0], KEY_RIGHT);
}

TEST(ActionExecutor, ParseKeystrokeFKeys) {
    EXPECT_EQ(ActionExecutor::parseKeystroke("F1")[0],  KEY_F1);
    EXPECT_EQ(ActionExecutor::parseKeystroke("F5")[0],  KEY_F5);
    EXPECT_EQ(ActionExecutor::parseKeystroke("F12")[0], KEY_F12);
}

TEST(ActionExecutor, ParseKeystrokeAllLetters) {
    // Spot-check a few letters
    EXPECT_EQ(ActionExecutor::parseKeystroke("A")[0], KEY_A);
    EXPECT_EQ(ActionExecutor::parseKeystroke("Z")[0], KEY_Z);
    EXPECT_EQ(ActionExecutor::parseKeystroke("M")[0], KEY_M);
}

TEST(ActionExecutor, ParseKeystrokeEmpty) {
    // Empty or unrecognised token produces empty vector
    auto keys = ActionExecutor::parseKeystroke("");
    EXPECT_TRUE(keys.empty());
}

// ---------------------------------------------------------------------------
// parseDBusAction
// ---------------------------------------------------------------------------

TEST(ActionExecutor, ParseDBusAction) {
    auto call = ActionExecutor::parseDBusAction(
        "org.kde.KWin,/VirtualDesktopManager,org.kde.KWin.VirtualDesktopManager,nextDesktop");
    EXPECT_EQ(call.service,    "org.kde.KWin");
    EXPECT_EQ(call.path,       "/VirtualDesktopManager");
    EXPECT_EQ(call.interface,  "org.kde.KWin.VirtualDesktopManager");
    EXPECT_EQ(call.method,     "nextDesktop");
}

TEST(ActionExecutor, ParseDBusActionInvalid) {
    auto call = ActionExecutor::parseDBusAction("only,two,parts");
    EXPECT_TRUE(call.method.isEmpty());  // invalid — needs 4 parts
}

TEST(ActionExecutor, ParseDBusActionTooMany) {
    auto call = ActionExecutor::parseDBusAction("a,b,c,d,e");
    EXPECT_TRUE(call.method.isEmpty());
}

TEST(ActionExecutor, ParseDBusActionEmpty) {
    auto call = ActionExecutor::parseDBusAction("");
    EXPECT_TRUE(call.method.isEmpty());
}

// ---------------------------------------------------------------------------
// gestureDirectionName
// ---------------------------------------------------------------------------

TEST(ActionExecutor, GestureDirectionName) {
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::None),  "None");
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::Up),    "Up");
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::Down),  "Down");
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::Left),  "Left");
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::Right), "Right");
    EXPECT_EQ(ActionExecutor::gestureDirectionName(GestureDirection::Click), "Click");
}

// ---------------------------------------------------------------------------
// GestureDetector
// ---------------------------------------------------------------------------

TEST(GestureDetector, NoDisplacement) {
    GestureDetector gd;
    gd.addDelta(2, -1);
    gd.addDelta(-1, 1);
    auto result = gd.resolve();
    EXPECT_EQ(result, GestureDirection::Click);
}

TEST(GestureDetector, SwipeLeft) {
    GestureDetector gd;
    gd.addDelta(-80, 5);
    auto result = gd.resolve();
    EXPECT_EQ(result, GestureDirection::Left);
}

TEST(GestureDetector, SwipeRight) {
    GestureDetector gd;
    gd.addDelta(60, 10);
    EXPECT_EQ(gd.resolve(), GestureDirection::Right);
}

TEST(GestureDetector, SwipeUp) {
    GestureDetector gd;
    gd.addDelta(5, -70);
    EXPECT_EQ(gd.resolve(), GestureDirection::Up);
}

TEST(GestureDetector, SwipeDown) {
    GestureDetector gd;
    gd.addDelta(-3, 55);
    EXPECT_EQ(gd.resolve(), GestureDirection::Down);
}

TEST(GestureDetector, Reset) {
    GestureDetector gd;
    gd.addDelta(-100, 0);
    gd.reset();
    gd.addDelta(5, 5);
    EXPECT_EQ(gd.resolve(), GestureDirection::Click);
}

TEST(GestureDetector, DominantAxis) {
    GestureDetector gd;
    gd.addDelta(-80, 60);  // both over threshold, but dx is dominant
    EXPECT_EQ(gd.resolve(), GestureDirection::Left);
}

TEST(GestureDetector, ExactThresholdIsClick) {
    GestureDetector gd;
    gd.addDelta(50, 0);  // exactly at threshold — not over it
    EXPECT_EQ(gd.resolve(), GestureDirection::Click);
}

TEST(GestureDetector, JustOverThreshold) {
    GestureDetector gd;
    gd.addDelta(51, 0);  // just over threshold
    EXPECT_EQ(gd.resolve(), GestureDirection::Right);
}

TEST(GestureDetector, MultipleAccumulatedDeltas) {
    GestureDetector gd;
    gd.addDelta(20, 0);
    gd.addDelta(20, 0);
    gd.addDelta(20, 0);  // total = 60 > 50
    EXPECT_EQ(gd.resolve(), GestureDirection::Right);
}
