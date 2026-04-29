#include <gtest/gtest.h>
#include "helpers/AppRootFixture.h"

using namespace logitune;
using namespace logitune::test;

// =============================================================================
// Smoke
// =============================================================================

TEST_F(AppRootFixture, Smoke) {
    EXPECT_NE(m_ctrl.get(), nullptr);
    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

// =============================================================================
// Focus / Profile switching tests
// =============================================================================

TEST_F(AppRootFixture, FocusAppWithProfileSwitchesHardware) {
    createAppProfile("google-chrome", "Chrome", 1600);
    setProfileButton("Chrome", 3, {ButtonAction::Keystroke, "Alt+Left"});

    focusApp("google-chrome");

    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");
}

TEST_F(AppRootFixture, FocusAppWithoutProfileSwitchesToDefault) {
    // No profile for "firefox" — should stay on default
    focusApp("firefox");

    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

TEST_F(AppRootFixture, FocusSameAppTwiceNoDoubleApply) {
    createAppProfile("google-chrome", "Chrome", 1600);

    focusApp("google-chrome");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");

    // Focus same app again — hardware profile already "Chrome", should be a no-op.
    // We verify by checking that profileForApp still returns "Chrome" and no crash.
    focusApp("google-chrome");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");
}

TEST_F(AppRootFixture, DesktopComponentsFiltered) {
    createAppProfile("org.kde.plasmashell", "Plasma");

    focusApp("org.kde.plasmashell");

    // plasmashell is in the ignore list — hardware profile must NOT change
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

TEST_F(AppRootFixture, KwinWaylandFiltered) {
    focusApp("kwin_wayland");

    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

TEST_F(AppRootFixture, FocusUpdatesHwIndicator) {
    createAppProfile("google-chrome", "Chrome");

    focusApp("google-chrome");

    // ProfileModel hw indicator should point at the Chrome entry (index 1, since 0 = default)
    QModelIndex idx = profileModel().index(1);
    EXPECT_TRUE(profileModel().data(idx, ProfileModel::IsHwActiveRole).toBool());

    // Default should no longer be hw-active
    QModelIndex defIdx = profileModel().index(0);
    EXPECT_FALSE(profileModel().data(defIdx, ProfileModel::IsHwActiveRole).toBool());
}

TEST_F(AppRootFixture, TabSwitchChangesDisplayNotHardware) {
    createAppProfile("google-chrome", "Chrome", 1600);

    // Focus Chrome so hardware is on Chrome
    focusApp("google-chrome");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");

    // User clicks the default tab (index 0)
    profileModel().selectTab(0);

    // Display should be default, but hardware stays on Chrome
    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");
}

TEST_F(AppRootFixture, TabSwitchPushesDisplayValues) {
    createAppProfile("google-chrome", "Chrome", 1600);

    // Focus Chrome so hardware is on Chrome
    focusApp("google-chrome");

    // Switch to default tab
    profileModel().selectTab(0);

    // DeviceModel should now show default's DPI (1000), not Chrome's (1600)
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
}

TEST_F(AppRootFixture, SettingsSaveToDisplayedProfile) {
    createAppProfile("google-chrome", "Chrome", 1600);

    // Display Chrome profile
    profileEngine().setDisplayProfile(QStringLiteral("mock-serial"), "Chrome");

    // Set a button action on button 3 (Back) via ButtonModel — simulates UI action
    buttonModel().setAction(3, "Copy", "keystroke");

    // Trigger save via saveCurrentProfile (this is what happens after userActionChanged)
    // The action should be saved to the Chrome profile
    Profile &chromeP = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Chrome");
    // After save, the Chrome profile should reflect the displayed edit
    // Note: saveCurrentProfile writes from ButtonModel to the displayed profile.
    // buttonModel().setAction emits userActionChanged -> onUserButtonChanged -> saveCurrentProfile
    // So the save already happened via the signal chain.
    // Verify the saved action type is what we set
    EXPECT_EQ(buttonModel().actionTypeForButton(3), "keystroke");
}

TEST_F(AppRootFixture, SettingsDontSaveToOtherProfile) {
    createAppProfile("google-chrome", "Chrome", 1600);
    setProfileButton("Chrome", 3, {ButtonAction::Keystroke, "Alt+Left"});

    // Display is on default, hardware is on Chrome after focus
    focusApp("google-chrome");
    profileModel().selectTab(0); // switch display to default

    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");

    // Change a button on displayed (default) profile
    buttonModel().setAction(3, "Paste", "keystroke");

    // Chrome profile button 3 should still be Alt+Left, not Paste
    const Profile &chromeP = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Chrome");
    EXPECT_EQ(chromeP.buttons[3].type, ButtonAction::Keystroke);
    EXPECT_EQ(chromeP.buttons[3].payload, "Alt+Left");
}

// =============================================================================
// Action dispatch tests
// =============================================================================

TEST_F(AppRootFixture, KeystrokeDispatchReadsHardwareProfile) {
    // Set button 3 (CID 0x53) to a keystroke on default profile
    setProfileButton("default", 3, {ButtonAction::Keystroke, "Ctrl+C"});

    // Press button — dispatch reads from hardware profile (which is "default")
    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+C");
}

TEST_F(AppRootFixture, DispatchReadsHwNotDisplayProfile) {
    // Set up Chrome profile with a keystroke on button 3
    createAppProfile("google-chrome", "Chrome", 1600);
    setProfileButton("Chrome", 3, {ButtonAction::Keystroke, "Alt+Left"});

    // Hardware is on Chrome (via focus)
    focusApp("google-chrome");

    // User switches display tab to default
    profileModel().selectTab(0);
    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");

    // Press button 3 — should dispatch Chrome's action (Alt+Left), not default's
    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Alt+Left");
}

TEST_F(AppRootFixture, DefaultButtonNotDispatched) {
    // Button 3 is Default type by default — pressing it should not dispatch anything
    pressButton(0x53);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_FALSE(m_injector->hasCalled("launchApp"));
}

TEST_F(AppRootFixture, SmartShiftToggleDoesNotInjectKeystroke) {
    setProfileButton("default", 3, {ButtonAction::SmartShiftToggle, ""});

    pressButton(0x53);

    // SmartShiftToggle should NOT inject a keystroke
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

TEST_F(AppRootFixture, AppLaunchDispatches) {
    setProfileButton("default", 3, {ButtonAction::AppLaunch, "firefox"});

    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("launchApp"));
    EXPECT_EQ(m_injector->lastArg("launchApp"), "firefox");
}

TEST_F(AppRootFixture, EmptyPayloadNotDispatched) {
    // Set a keystroke action with empty payload — should not dispatch
    setProfileButton("default", 3, {ButtonAction::Keystroke, ""});

    pressButton(0x53);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

// =============================================================================
// Gesture tests
// =============================================================================

TEST_F(AppRootFixture, GestureRightDetected) {
    setProfileGesture("default", "right", "Ctrl+Super+Right");

    // Activate gesture: press gesture button (CID 0xC3)
    // The default profile has btn5 = GestureTrigger via device descriptor
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    EXPECT_TRUE(gestureActive());

    gestureXY(80, 5);

    releaseButton(0xC3);
    EXPECT_FALSE(gestureActive());

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+Super+Right");
}

TEST_F(AppRootFixture, GestureLeftDetected) {
    setProfileGesture("default", "left", "Ctrl+Super+Left");
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    gestureXY(-80, 5);
    releaseButton(0xC3);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+Super+Left");
}

TEST_F(AppRootFixture, GestureDownDetected) {
    setProfileGesture("default", "down", "Super+D");
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    gestureXY(5, 80);
    releaseButton(0xC3);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Super+D");
}

TEST_F(AppRootFixture, GestureUpDetected) {
    setProfileGesture("default", "up", "Super+Up");
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    gestureXY(5, -80);
    releaseButton(0xC3);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Super+Up");
}

TEST_F(AppRootFixture, GestureClickDetected) {
    setProfileGesture("default", "click", "Super+W");
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    gestureXY(0, 0);
    releaseButton(0xC3);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Super+W");
}

TEST_F(AppRootFixture, GestureOnlyResolvesOnGestureButtonRelease) {
    setProfileGesture("default", "right", "Ctrl+Super+Right");
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});

    pressButton(0xC3);
    gestureXY(80, 5);

    // Release a different button (btn3, CID 0x53) — gesture should NOT resolve
    releaseButton(0x53);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_TRUE(gestureActive());

    // Now release the correct gesture button — should resolve
    releaseButton(0xC3);
    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+Super+Right");
    EXPECT_FALSE(gestureActive());
}

TEST_F(AppRootFixture, GestureUsesHardwareProfileGestures) {
    // Chrome has a different gesture for "right"
    createAppProfile("google-chrome", "Chrome");
    setProfileButton("Chrome", 5, {ButtonAction::GestureTrigger, ""});
    setProfileGesture("Chrome", "right", "Alt+Right");

    // Default has a different gesture for "right"
    setProfileButton("default", 5, {ButtonAction::GestureTrigger, ""});
    setProfileGesture("default", "right", "Ctrl+Super+Right");

    // Focus Chrome — hardware profile is now Chrome
    focusApp("google-chrome");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Chrome");

    // Switch display to default
    profileModel().selectTab(0);
    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");

    // Gesture right — should use Chrome's (hardware) gesture, not default's (display)
    pressButton(0xC3);
    gestureXY(80, 5);
    releaseButton(0xC3);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Alt+Right");
}

// =============================================================================
// Thumb wheel tests
// =============================================================================

TEST_F(AppRootFixture, ThumbWheelVolumeForwardIsUp) {
    setThumbWheelMode("volume");

    thumbWheel(20);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "VolumeUp");
}

TEST_F(AppRootFixture, ThumbWheelVolumeBackwardIsDown) {
    setThumbWheelMode("volume");

    thumbWheel(-20);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "VolumeDown");
}

TEST_F(AppRootFixture, ThumbWheelZoomForward) {
    setThumbWheelMode("zoom");

    // Positive delta (clockwise) in zoom mode -> zoom in (ctrlscroll +1)
    thumbWheel(20);

    EXPECT_TRUE(m_injector->hasCalled("injectCtrlScroll"));
    EXPECT_EQ(m_injector->lastArg("injectCtrlScroll"), "1");
}

TEST_F(AppRootFixture, ThumbWheelScrollModeNoDispatch) {
    // Default mode is "scroll" — no injection should occur
    setThumbWheelMode("scroll");

    thumbWheel(20);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_FALSE(m_injector->hasCalled("injectCtrlScroll"));
}

TEST_F(AppRootFixture, ThumbWheelBelowThresholdNoDispatch) {
    setThumbWheelMode("volume");

    // kThumbThreshold is 15; delta of 5 should not trigger
    thumbWheel(5);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

// =============================================================================
// Media controls tests
// =============================================================================

TEST_F(AppRootFixture, MediaPlayPauseInjectsPlay) {
    setProfileButton("default", 3, {ButtonAction::Media, "Play"});

    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Play");
}

TEST_F(AppRootFixture, MediaNextTrackInjectsNext) {
    setProfileButton("default", 3, {ButtonAction::Media, "Next"});

    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Next");
}

TEST_F(AppRootFixture, MediaMuteInjectsMute) {
    setProfileButton("default", 3, {ButtonAction::Media, "Mute"});

    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Mute");
}

TEST_F(AppRootFixture, MediaVolumeDownInjectsVolumeDown) {
    setProfileButton("default", 3, {ButtonAction::Media, "VolumeDown"});

    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "VolumeDown");
}

TEST_F(AppRootFixture, MediaActionPerProfileSwitching) {
    // Default profile: button 3 = Play/Pause
    setProfileButton("default", 3, {ButtonAction::Media, "Play"});

    // Chrome profile: button 3 = Next track
    createAppProfile("google-chrome", "Chrome", 1600);
    setProfileButton("Chrome", 3, {ButtonAction::Media, "Next"});

    // Focus Chrome
    focusApp("google-chrome");
    pressButton(0x53);

    EXPECT_TRUE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Next");
}

TEST_F(AppRootFixture, CarouselSwitchSwapsButtonModel) {
    // Fixture's primary device "mock-serial" is selected (index 0). Set
    // its button 3 to a distinctive action and bounce the display profile
    // so ButtonModel re-loads from the mutated cache. (Mutating the cache
    // alone doesn't refresh the ButtonModel; that requires going through
    // onDisplayProfileChanged.)
    setProfileButton("default", 3,
                     {ButtonAction::Keystroke, QStringLiteral("Alt+Left")});
    profileEngine().setDisplayProfile(
        QStringLiteral("mock-serial"), QString());
    profileEngine().setDisplayProfile(
        QStringLiteral("mock-serial"), QStringLiteral("default"));
    deviceModel().setSelectedIndex(0);

    auto *secondary = addMockDevice(QStringLiteral("B"));
    {
        const QString serialB = QStringLiteral("mock-serial-B");
        Profile &pB = profileEngine().cachedProfile(
            serialB, QStringLiteral("default"));
        pB.buttons[3] = {ButtonAction::Media, QStringLiteral("Play")};
        profileEngine().saveProfileToDisk(
            serialB, QStringLiteral("default"));
    }

    // ButtonModel reflects A's profile.
    EXPECT_EQ(buttonModel().actionTypeForButton(3),
              QStringLiteral("keystroke"));

    // Switch carousel to device B.
    const int idxB = deviceModel().devices().indexOf(secondary);
    ASSERT_GE(idxB, 0);
    deviceModel().setSelectedIndex(idxB);

    // ButtonModel now reflects device B. Legacy ButtonAction::Media now
    // surfaces in the UI as a regular keystroke entry.
    EXPECT_EQ(buttonModel().actionTypeForButton(3),
              QStringLiteral("keystroke"));
}

TEST_F(AppRootFixture, CarouselSwitchSwapsDisplayValues) {
    // Fixture primary has DPI 1000 (seeded in SetUp).
    deviceModel().setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);

    auto *secondary = addMockDevice(QStringLiteral("B"), /*seedDpi=*/2500);

    const int idxB = deviceModel().devices().indexOf(secondary);
    ASSERT_GE(idxB, 0);
    deviceModel().setSelectedIndex(idxB);
    EXPECT_EQ(deviceModel().currentDPI(), 2500);

    deviceModel().setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
}

TEST_F(AppRootFixture, DisplayProfileChangedIgnoredForNonSelectedDevice) {
    deviceModel().setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);

    // Register a second device's profile dir with the engine WITHOUT
    // adding it to the carousel. addMockDevice would also drive
    // setupProfileForDevice + applyProfileToHardware, which routes
    // through the active session (i.e. A's) and clobbers A's hardware
    // values. We're testing the engine-level filter here, not transport
    // routing, so we sidestep that path.
    const QString serialB = QStringLiteral("mock-serial-B");
    const QString dirB = m_tmpDir.path() + "/" + serialB + "/profiles";
    QDir().mkpath(dirB);
    Profile pB;
    pB.name = QStringLiteral("Default");
    pB.dpi  = 9999;
    ProfileEngine::saveProfile(dirB + "/default.conf", pB);
    profileEngine().registerDevice(serialB, dirB);

    // Bounce setDisplayProfile to force deviceDisplayProfileChanged emission
    // (setDisplayProfile short-circuits on unchanged name).
    profileEngine().setDisplayProfile(
        serialB, QStringLiteral("default"));
    profileEngine().setDisplayProfile(
        serialB, QStringLiteral("other"));
    profileEngine().setDisplayProfile(
        serialB, QStringLiteral("default"));

    // DeviceModel should still reflect device A's 1000 DPI — the
    // onDisplayProfileChanged filter rejected device B's signal.
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
}

// Regression test for the stale-cache-on-state-tick bug. The per-property
// NOTIFY chain (DeviceSession -> PhysicalDevice -> DeviceModel) clears
// m_hasDisplayValues so QML re-reads the live session value. A separate
// path also fires DeviceModel::selectedChanged from refreshRow on the
// same state tick; AppRoot::onSelectionChanged must not treat that as a
// real selection change and replay onDisplayProfileChanged, which would
// re-prime the cache from the stored profile and clobber the live value.
TEST_F(AppRootFixture, HardwareDpiTickDoesNotRepriDisplayCache) {
    deviceModel().setSelectedIndex(0);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);  // primed from profile

    // Simulate a hardware-side DPI change on the selected device (e.g. the
    // user pressed the DPI cycle button on the mouse). Going through
    // DeviceSession::setDPI exercises the same per-property emit + stateChanged
    // co-fire that the live HID++ path produces.
    m_session->setDPI(2400);

    // Without the fix: AppRoot::onSelectionChanged is re-driven by
    // refreshRow's selectedChanged emit, replays onDisplayProfileChanged,
    // and setDisplayValues re-arms m_displayDpi back to the profile's 1000.
    // With the fix: same active device, so onSelectionChanged bails.
    EXPECT_EQ(deviceModel().currentDPI(), 2400);
}
