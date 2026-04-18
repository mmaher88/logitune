#include <gtest/gtest.h>

#include "ButtonAction.h"

using logitune::ButtonAction;

// ---------------------------------------------------------------------------
// Parse tests
// ---------------------------------------------------------------------------

TEST(ButtonAction, ParseDefaultKeyword) {
    auto a = ButtonAction::parse("default");
    EXPECT_EQ(a.type, ButtonAction::Default);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseEmptyStringIsDefault) {
    auto a = ButtonAction::parse("");
    EXPECT_EQ(a.type, ButtonAction::Default);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseGestureTriggerKeyword) {
    auto a = ButtonAction::parse("gesture-trigger");
    EXPECT_EQ(a.type, ButtonAction::GestureTrigger);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseSmartShiftToggleKeyword) {
    auto a = ButtonAction::parse("smartshift-toggle");
    EXPECT_EQ(a.type, ButtonAction::SmartShiftToggle);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseDpiCycleKeyword) {
    auto a = ButtonAction::parse("dpi-cycle");
    EXPECT_EQ(a.type, ButtonAction::DpiCycle);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, SerializeDpiCycleType) {
    ButtonAction a{ButtonAction::DpiCycle, {}};
    EXPECT_EQ(a.serialize(), "dpi-cycle");
}

TEST(ButtonAction, ParseKeystrokeCtrlC) {
    auto a = ButtonAction::parse("keystroke:Ctrl+C");
    EXPECT_EQ(a.type, ButtonAction::Keystroke);
    EXPECT_EQ(a.payload, "Ctrl+C");
}

TEST(ButtonAction, ParseMediaVolumeUp) {
    auto a = ButtonAction::parse("media:VolumeUp");
    EXPECT_EQ(a.type, ButtonAction::Media);
    EXPECT_EQ(a.payload, "VolumeUp");
}

TEST(ButtonAction, ParseDBusWithSpaces) {
    auto a = ButtonAction::parse("dbus:org.kde.kwin /KWin toggle");
    EXPECT_EQ(a.type, ButtonAction::DBus);
    EXPECT_EQ(a.payload, "org.kde.kwin /KWin toggle");
}

TEST(ButtonAction, ParseAppLaunchKcalc) {
    auto a = ButtonAction::parse("app-launch:kcalc");
    EXPECT_EQ(a.type, ButtonAction::AppLaunch);
    EXPECT_EQ(a.payload, "kcalc");
}

TEST(ButtonAction, ParseUnknownPrefixFallsToDefault) {
    auto a = ButtonAction::parse("foobar:test");
    EXPECT_EQ(a.type, ButtonAction::Default);
}

TEST(ButtonAction, ParseUnknownBareKeywordFallsToDefault) {
    auto a = ButtonAction::parse("something");
    EXPECT_EQ(a.type, ButtonAction::Default);
}

TEST(ButtonAction, ParseLegacyKeystrokeSmartShiftToggle) {
    // Migration: "keystroke:smartshift-toggle" → SmartShiftToggle
    auto a = ButtonAction::parse("keystroke:smartshift-toggle");
    EXPECT_EQ(a.type, ButtonAction::SmartShiftToggle);
    EXPECT_TRUE(a.payload.isEmpty());
}

// ---------------------------------------------------------------------------
// Serialize tests
// ---------------------------------------------------------------------------

TEST(ButtonAction, SerializeDefaultType) {
    ButtonAction a{ButtonAction::Default, {}};
    EXPECT_EQ(a.serialize(), "default");
}

TEST(ButtonAction, SerializeGestureTriggerType) {
    ButtonAction a{ButtonAction::GestureTrigger, {}};
    EXPECT_EQ(a.serialize(), "gesture-trigger");
}

TEST(ButtonAction, SerializeSmartShiftToggleType) {
    ButtonAction a{ButtonAction::SmartShiftToggle, {}};
    EXPECT_EQ(a.serialize(), "smartshift-toggle");
}

TEST(ButtonAction, SerializeKeystrokeAltF4) {
    ButtonAction a{ButtonAction::Keystroke, "Alt+F4"};
    EXPECT_EQ(a.serialize(), "keystroke:Alt+F4");
}

TEST(ButtonAction, SerializeAppLaunchFirefox) {
    ButtonAction a{ButtonAction::AppLaunch, "firefox"};
    EXPECT_EQ(a.serialize(), "app-launch:firefox");
}

// ---------------------------------------------------------------------------
// Round-trip tests
// ---------------------------------------------------------------------------

TEST(ButtonAction, RoundTripKeystrokeCtrlShiftZ) {
    ButtonAction orig{ButtonAction::Keystroke, "Ctrl+Shift+Z"};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, RoundTripSmartShiftToggle) {
    ButtonAction orig{ButtonAction::SmartShiftToggle, {}};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, RoundTripGestureTrigger) {
    ButtonAction orig{ButtonAction::GestureTrigger, {}};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, RoundTripAppLaunchKcalc) {
    ButtonAction orig{ButtonAction::AppLaunch, "kcalc"};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, RoundTripMediaPlay) {
    ButtonAction orig{ButtonAction::Media, "Play"};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, RoundTripMediaNext) {
    ButtonAction orig{ButtonAction::Media, "Next"};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}

TEST(ButtonAction, SerializeMediaVolumeUp) {
    ButtonAction a{ButtonAction::Media, "VolumeUp"};
    EXPECT_EQ(a.serialize(), "media:VolumeUp");
}
