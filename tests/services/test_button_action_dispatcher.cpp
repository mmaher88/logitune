#include "services/ButtonActionDispatcherFixture.h"

using namespace logitune;
using namespace logitune::test;

namespace {

// MX Master 3S CIDs from MockDevice::setupMxControls()
constexpr uint16_t kGestureCid     = 0x00C3; // button index 5, "gesture-trigger"
constexpr uint16_t kBackCid        = 0x0053; // button index 3
constexpr uint16_t kForwardCid     = 0x0056; // button index 4
constexpr uint16_t kMiddleCid      = 0x0052; // button index 2

/// Write a button action into the cached hardware profile at the given
/// buttonIndex. The dispatcher reads from that profile on press.
void setProfileButton(Profile &p, int idx, ButtonAction ba) {
    if (idx >= 0 && idx < static_cast<int>(p.buttons.size()))
        p.buttons[static_cast<std::size_t>(idx)] = std::move(ba);
}

} // namespace

// --- Gesture accumulation + resolution --------------------------------------

TEST_F(ButtonActionDispatcherFixture, NullSessionGestureIsNoOp) {
    // No attach — no device. onGestureRaw must not crash or insert state.
    m_dispatcher->onGestureRaw(m_physical, 10, 20);
    EXPECT_FALSE(hasStateFor(QStringLiteral("mock-serial")));
}

TEST_F(ButtonActionDispatcherFixture, GestureStartAccumulatesXY) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    EXPECT_TRUE(gestureActive());

    m_dispatcher->onGestureRaw(m_physical, 10, 20);
    m_dispatcher->onGestureRaw(m_physical, 5, -5);
    EXPECT_EQ(gestureAccumX(), 15);
    EXPECT_EQ(gestureAccumY(), 15);
}

TEST_F(ButtonActionDispatcherFixture, GestureAccumulationIgnoredWhenNotActive) {
    attachMockSession();
    m_dispatcher->onGestureRaw(m_physical, 100, 100);
    EXPECT_EQ(gestureAccumX(), 0);
    EXPECT_EQ(gestureAccumY(), 0);
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseBelowThresholdDispatchesClick) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    hwProfile().gestures["click"] = ButtonAction{ButtonAction::Keystroke,
                                                 QStringLiteral("Super+W")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, 5, 5); // well under kGestureThreshold (50)
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_FALSE(gestureActive());
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Super+W"));
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseRightDispatchesRightKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    hwProfile().gestures["right"] = ButtonAction{ButtonAction::Keystroke,
                                                 QStringLiteral("Ctrl+Super+Right")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, 80, 5);   // dx dominant, over threshold
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("injectKeystroke"),
              QStringLiteral("Ctrl+Super+Right"));
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseLeftDispatchesLeftKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    hwProfile().gestures["left"] = ButtonAction{ButtonAction::Keystroke,
                                                QStringLiteral("Ctrl+Super+Left")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, -80, 3);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("injectKeystroke"),
              QStringLiteral("Ctrl+Super+Left"));
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseDownDispatchesDownKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    hwProfile().gestures["down"] = ButtonAction{ButtonAction::Keystroke,
                                                QStringLiteral("Super+D")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, 5, 80);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Super+D"));
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseUpDispatchesUpKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    hwProfile().gestures["up"] = ButtonAction{ButtonAction::Keystroke,
                                              QStringLiteral("Super+Up")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, -3, -80);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Super+Up"));
}

TEST_F(ButtonActionDispatcherFixture, GestureReleaseMissingDirectionDoesNotInject) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    // no gestures map entries

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    m_dispatcher->onGestureRaw(m_physical, 80, 0);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

// --- Thumb wheel accumulator -----------------------------------------------

TEST_F(ButtonActionDispatcherFixture, ThumbWheelAccumulatesBelowThreshold) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("scroll"), false);
    m_injector->clear();

    m_dispatcher->onThumbWheelRotation(m_physical, 5);
    m_dispatcher->onThumbWheelRotation(m_physical, 5);
    // 10 total, under kThumbThreshold (15)

    EXPECT_EQ(thumbAccum(), 10);
    EXPECT_FALSE(m_injector->hasCalled("injectHorizontalScroll"));
}

TEST_F(ButtonActionDispatcherFixture, ThumbWheelEmitsHorizontalScrollAtThreshold) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("scroll"), false);
    m_injector->clear();

    m_dispatcher->onThumbWheelRotation(m_physical, 15);
    EXPECT_TRUE(m_injector->hasCalled("injectHorizontalScroll"));
    EXPECT_EQ(m_injector->lastArg("injectHorizontalScroll"),
              QStringLiteral("1"));
    EXPECT_EQ(thumbAccum(), 0);
}

TEST_F(ButtonActionDispatcherFixture, ThumbWheelEmitsVolumeKeystrokeInVolumeMode) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("volume"), false);
    m_injector->clear();

    m_dispatcher->onThumbWheelRotation(m_physical, 15);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"),
              QStringLiteral("VolumeUp"));

    m_dispatcher->onThumbWheelRotation(m_physical, -15);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"),
              QStringLiteral("VolumeDown"));
}

TEST_F(ButtonActionDispatcherFixture, ThumbWheelEmitsCtrlScrollInZoomMode) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("zoom"), false);
    m_injector->clear();

    m_dispatcher->onThumbWheelRotation(m_physical, 15);
    EXPECT_TRUE(m_injector->hasCalled("injectCtrlScroll"));
    EXPECT_EQ(m_injector->lastArg("injectCtrlScroll"), QStringLiteral("1"));
}

// --- onProfileApplied / onDeviceRemoved ------------------------------------

TEST_F(ButtonActionDispatcherFixture, OnProfileAppliedClearsThumbAccum) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("scroll"), false);
    m_dispatcher->onThumbWheelRotation(m_physical, 5);
    ASSERT_EQ(thumbAccum(), 5);

    m_dispatcher->onProfileApplied(QStringLiteral("mock-serial"));
    EXPECT_EQ(thumbAccum(), 0);
}

TEST_F(ButtonActionDispatcherFixture, OnProfileAppliedForUnknownSerialIsNoOp) {
    attachMockSession();
    // No crash when serial has no entry yet.
    m_dispatcher->onProfileApplied(QStringLiteral("never-seen"));
    EXPECT_FALSE(hasStateFor(QStringLiteral("never-seen")));
}

TEST_F(ButtonActionDispatcherFixture, OnDeviceRemovedDropsEntry) {
    attachMockSession();
    m_session->setThumbWheelMode(QStringLiteral("scroll"), false);
    m_dispatcher->onThumbWheelRotation(m_physical, 5);
    ASSERT_TRUE(hasStateFor(QStringLiteral("mock-serial")));

    m_dispatcher->onDeviceRemoved(QStringLiteral("mock-serial"));
    EXPECT_FALSE(hasStateFor(QStringLiteral("mock-serial")));
}

// --- Button action dispatch ------------------------------------------------

TEST_F(ButtonActionDispatcherFixture, SmartShiftButtonTogglesSession) {
    attachMockSession();
    setProfileButton(hwProfile(), 2,
        ButtonAction{ButtonAction::SmartShiftToggle, {}});
    // DeviceSession::setSmartShift bails without a SmartShift dispatcher,
    // which requires enumeration we don't do in tests. We verify the
    // dispatch reached session by checking it did NOT reach the executor.
    m_dispatcher->onDivertedButtonPressed(m_physical, kMiddleCid, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

TEST_F(ButtonActionDispatcherFixture, DpiCycleButtonDoesNotInjectKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::DpiCycle, {}});
    // cycleDpi() writes to the session; no executor path. Verify we didn't
    // accidentally fall into the keystroke branch.
    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

TEST_F(ButtonActionDispatcherFixture, KeystrokeActionInjectsViaExecutor) {
    attachMockSession();
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Ctrl+C"));
}

TEST_F(ButtonActionDispatcherFixture, RadialMenuReleaseLeavesOverlayOpenAndActivatesItem) {
    attachMockSession();
    m_desktop->setSupportsActionRing(true);
    setProfileButton(hwProfile(), 3, {ButtonAction::RadialMenu, {}});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    ASSERT_TRUE(m_dispatcher->radialMenu()->visible());

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, false);
    EXPECT_TRUE(m_dispatcher->radialMenu()->visible());

    m_dispatcher->radialMenu()->activate(0);
    EXPECT_FALSE(m_dispatcher->radialMenu()->visible());
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Ctrl+C"));
}

TEST_F(ButtonActionDispatcherFixture, MediaActionInjectsViaExecutor) {
    attachMockSession();
    setProfileButton(hwProfile(), 4,
        ButtonAction{ButtonAction::Media, QStringLiteral("Play")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kForwardCid, true);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Play"));
}

TEST_F(ButtonActionDispatcherFixture, AppLaunchActionInvokesExecutor) {
    attachMockSession();
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::AppLaunch, QStringLiteral("firefox")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_EQ(m_injector->lastArg("launchApp"), QStringLiteral("firefox"));
}

TEST_F(ButtonActionDispatcherFixture, ReleaseWithoutGestureActiveIsNoOp) {
    attachMockSession();
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")});

    // Release of the same button as pressed. Dispatcher only dispatches on
    // press (the only exception is the gesture-release path which checks
    // gestureActive first).
    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, false);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

TEST_F(ButtonActionDispatcherFixture, DefaultButtonIsNoOp) {
    attachMockSession();
    // button 3 stays default in the profile
    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_FALSE(m_injector->hasCalled("launchApp"));
}

TEST_F(ButtonActionDispatcherFixture, UnknownControlIdIsIgnored) {
    attachMockSession();
    // CID 0xFFFF does not appear in MockDevice controls — idx stays -1.
    m_dispatcher->onDivertedButtonPressed(m_physical, 0xFFFF, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

TEST_F(ButtonActionDispatcherFixture, DeviceWithoutDescriptorIgnoresButtons) {
    attachMockSession();
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")});

    // Same serial (so the profile above is found), but its transport has no
    // descriptor: control IDs cannot be mapped to button slots, so the
    // press must be ignored rather than guessed.
    DeviceSession session(std::make_unique<hidpp::HidrawDevice>("/dev/null"),
                          0xFF, QStringLiteral("Bluetooth"), nullptr);
    PhysicalDevice unknown(QStringLiteral("mock-serial"));
    unknown.attachTransport(&session);

    m_dispatcher->onDivertedButtonPressed(&unknown, kBackCid, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
}

// ---------------------------------------------------------------------------
// PresetRef resolution via IDesktopIntegration
// ---------------------------------------------------------------------------

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvesViaDesktopAndFiresKeystroke) {
    attachMockSession();
    m_desktop->scriptResolve("show-desktop",
                             ButtonAction{ButtonAction::Keystroke, "Super+D"});
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Super+D"));
}

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvedToDBusFiresDBus) {
    attachMockSession();
    m_desktop->scriptResolve("show-desktop",
                             ButtonAction{ButtonAction::DBus,
                                          "org.x,/path,org.x.Iface,method"});
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_EQ(m_injector->lastArg("sendDBusCall"),
              QStringLiteral("org.x,/path,org.x.Iface,method"));
}

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvedToAppLaunchFiresLaunch) {
    attachMockSession();
    m_desktop->scriptResolve("calculator",
                             ButtonAction{ButtonAction::AppLaunch, "gnome-calculator"});
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("calculator")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_EQ(m_injector->lastArg("launchApp"), QStringLiteral("gnome-calculator"));
}

TEST_F(ButtonActionDispatcherFixture, PresetRefUnresolvedLogsAndFiresNothing) {
    attachMockSession();
    // No scriptResolve -> resolveNamedAction returns nullopt
    setProfileButton(hwProfile(), 3,
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")});

    m_dispatcher->onDivertedButtonPressed(m_physical, kBackCid, true);
    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_FALSE(m_injector->hasCalled("sendDBusCall"));
    EXPECT_FALSE(m_injector->hasCalled("launchApp"));
}

// ---------------------------------------------------------------------------
// Gesture-direction PresetRef resolution. Mirrors the press-path PresetRef
// tests above but exercises the gesture-release branch in the dispatcher.
// ---------------------------------------------------------------------------

TEST_F(ButtonActionDispatcherFixture, GesturePresetResolvesViaDesktopAndFiresKeystroke) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    m_desktop->scriptResolve("show-desktop",
                             ButtonAction{ButtonAction::Keystroke, "Super+D"});
    hwProfile().gestures["down"] =
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")};

    // Trigger a gesture-down release: press the gesture button, accumulate
    // dy past kGestureThreshold, release. Direction should resolve to "down"
    // and the preset should be resolved via the desktop integration before
    // firing.
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    for (int i = 0; i < 10; ++i)
        m_dispatcher->onGestureRaw(m_physical, 0, 20);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), QStringLiteral("Super+D"));
}

TEST_F(ButtonActionDispatcherFixture, GesturePresetResolvedToDBusFiresDBus) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    m_desktop->scriptResolve("show-desktop",
                             ButtonAction{ButtonAction::DBus,
                                          "org.x,/path,org.x.Iface,method"});
    hwProfile().gestures["down"] =
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    for (int i = 0; i < 10; ++i)
        m_dispatcher->onGestureRaw(m_physical, 0, 20);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg("sendDBusCall"),
              QStringLiteral("org.x,/path,org.x.Iface,method"));
}

TEST_F(ButtonActionDispatcherFixture, GesturePresetUnresolvedFiresNothing) {
    attachMockSession();
    setProfileButton(hwProfile(), 5,
        ButtonAction{ButtonAction::GestureTrigger, {}});
    // No scriptResolve -> nullopt
    hwProfile().gestures["down"] =
        ButtonAction{ButtonAction::PresetRef, QStringLiteral("show-desktop")};

    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, true);
    for (int i = 0; i < 10; ++i)
        m_dispatcher->onGestureRaw(m_physical, 0, 20);
    m_dispatcher->onDivertedButtonPressed(m_physical, kGestureCid, false);

    EXPECT_FALSE(m_injector->hasCalled("injectKeystroke"));
    EXPECT_FALSE(m_injector->hasCalled("sendDBusCall"));
    EXPECT_FALSE(m_injector->hasCalled("launchApp"));
}
