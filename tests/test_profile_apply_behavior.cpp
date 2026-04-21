/// Tests for profile application behavior:
///   - Non-divertable buttons are skipped
///   - Thumb wheel accumulator resets on profile switch
///   - Command queue is flushed before applying a new profile
///   - Settings are applied before button diversions
#include "helpers/AppControllerFixture.h"

namespace logitune::test {

class ProfileApplyBehaviorTest : public AppControllerFixture {};

// ---------------------------------------------------------------------------
// Non-divertable button skip
// ---------------------------------------------------------------------------

// When a control has configurable=false (e.g. left/right click), the profile
// apply must skip it rather than trying to divert it via HID++.  Previously
// this caused HwError 0x04 responses that clogged the command queue.

TEST_F(ProfileApplyBehaviorTest, NonConfigurableButtonsAreSkippedDuringProfileApply) {
    // Mark buttons 0 and 1 (left/right click, CID 0x0050 / 0x0051) as non-configurable
    m_device.m_controls[0].configurable = false;
    m_device.m_controls[1].configurable = false;

    // Create a profile where those buttons have non-default actions
    // (this would fail if applyProfileToHardware tries to divert them)
    createAppProfile("firefox", "Firefox", 2000);
    setProfileButton("Firefox", 0, {ButtonAction::Keystroke, "Ctrl+C"});
    setProfileButton("Firefox", 1, {ButtonAction::Keystroke, "Ctrl+V"});

    // Focus should succeed without error — non-configurable buttons are skipped
    focusApp("firefox");

    // Verify profile was applied (DPI changed = hardware profile switched)
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Firefox");
}

TEST_F(ProfileApplyBehaviorTest, ConfigurableButtonsStillDivertedWhenNonConfigExist) {
    // Buttons 0,1 non-configurable, but button 2 (middle click) is configurable
    m_device.m_controls[0].configurable = false;
    m_device.m_controls[1].configurable = false;

    createAppProfile("code", "VSCode", 1600);
    setProfileButton("VSCode", 2, {ButtonAction::Keystroke, "Ctrl+Z"});

    focusApp("code");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "VSCode");

    // Switching back to default should also work
    focusApp("some-other-app");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

// ---------------------------------------------------------------------------
// Thumb wheel accumulator reset on profile switch
// ---------------------------------------------------------------------------

TEST_F(ProfileApplyBehaviorTest, ThumbWheelAccumulatorResetsOnProfileSwitch) {
    // Set up a profile with volume mode so thumb wheel is diverted
    createAppProfile("firefox", "Firefox", 1000, "volume");
    focusApp("firefox");

    // Feed some thumb wheel rotation — accumulates below threshold
    thumbWheel(10);
    EXPECT_NE(thumbAccum(), 0);

    // Switch profile — accumulator must reset to zero
    focusApp("some-other-app");
    EXPECT_EQ(thumbAccum(), 0);
}

TEST_F(ProfileApplyBehaviorTest, ThumbWheelAccumulatorResetsOnProfileSwitchToSameMode) {
    // Both profiles use volume mode
    createAppProfile("firefox", "Firefox", 1000, "volume");
    createAppProfile("code", "VSCode", 1600, "volume");

    focusApp("firefox");
    thumbWheel(10);
    EXPECT_NE(thumbAccum(), 0);

    // Even switching between two volume-mode profiles must reset the accumulator
    focusApp("code");
    EXPECT_EQ(thumbAccum(), 0);
}

// ---------------------------------------------------------------------------
// Command queue flush before profile apply
// ---------------------------------------------------------------------------

TEST_F(ProfileApplyBehaviorTest, ProfileSwitchDoesNotAccumulateStaleState) {
    // Rapid focus switching: create three profiles with different DPIs
    createAppProfile("firefox", "Firefox", 2000);
    createAppProfile("code", "VSCode", 3000);

    // Rapidly switch between profiles
    focusApp("firefox");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Firefox");

    focusApp("code");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "VSCode");

    focusApp("firefox");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Firefox");

    // Final state should be clean — no stale profile lingering
    focusApp("some-other-app");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

// ---------------------------------------------------------------------------
// Settings applied before button diversions (ordering)
// ---------------------------------------------------------------------------

// We verify ordering indirectly: if DPI/SmartShift/scroll were applied
// AFTER button diversions, a rapid switch could leave the device with
// stale settings but correct buttons.  Here we verify that all settings
// values are correctly reflected in DeviceModel after a profile switch.

TEST_F(ProfileApplyBehaviorTest, AllSettingsAppliedOnProfileSwitch) {
    createAppProfile("firefox", "Firefox", 2400, "volume", true,
                     false, 64, "natural", false);

    focusApp("firefox");

    // All profile settings should be reflected — this only works if
    // settings commands were sent (and not blocked by button diversions)
    auto &dm = deviceModel();
    EXPECT_EQ(dm.currentDPI(), 2400);
    EXPECT_FALSE(dm.smartShiftEnabled());
    EXPECT_EQ(dm.smartShiftThreshold(), 64);
    EXPECT_TRUE(dm.scrollInvert());   // "natural" = inverted
    EXPECT_FALSE(dm.scrollHiRes());
    EXPECT_EQ(dm.thumbWheelMode(), "volume");
    EXPECT_TRUE(dm.thumbWheelInvert());
}

TEST_F(ProfileApplyBehaviorTest, SettingsRevertOnSwitchBackToDefault) {
    createAppProfile("firefox", "Firefox", 2400, "volume", true,
                     false, 64, "natural", false);

    focusApp("firefox");
    focusApp("some-other-app"); // back to default

    auto &dm = deviceModel();
    EXPECT_EQ(dm.currentDPI(), 1000);
    EXPECT_TRUE(dm.smartShiftEnabled());
    EXPECT_EQ(dm.smartShiftThreshold(), 128);
    EXPECT_FALSE(dm.scrollInvert());  // "standard" = not inverted
    EXPECT_TRUE(dm.scrollHiRes());
    EXPECT_EQ(dm.thumbWheelMode(), "scroll");
    EXPECT_FALSE(dm.thumbWheelInvert());
}

} // namespace logitune::test
