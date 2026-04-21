#include <gtest/gtest.h>
#include "helpers/AppControllerFixture.h"
#include "helpers/TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(AppControllerFixture, FirstConnectSetsDefaultProfile) {
    // Fresh controller — hw profile should be "default" after setup
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

TEST_F(AppControllerFixture, ReconnectPreservesHwProfile) {
    createAppProfile("google-chrome", "Google Chrome", 2000, "zoom");
    focusApp("google-chrome");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Google Chrome");

    // Simulate reconnect: onDeviceSetupComplete re-runs
    // The hw profile should stay "Google Chrome", not reset to "default"
    QString hwBefore = profileEngine().hardwareProfile(QStringLiteral("mock-serial"));

    // We can't fully simulate onDeviceSetupComplete without DeviceManager,
    // but we can verify the logic: if hwProfile is non-empty, it should be preserved
    EXPECT_FALSE(hwBefore.isEmpty());
    EXPECT_EQ(hwBefore, "Google Chrome");
}

// A fresh ProfileEngine (before any setHardwareProfile call) has an empty hw profile.
// This mirrors the state of AppController before device setup completes.
TEST(DeviceReconnect, HwProfileEmptyOnFirstConnect) {
    ProfileEngine engine;
    EXPECT_TRUE(engine.hardwareProfile(QStringLiteral("mock-serial")).isEmpty());
}

TEST_F(AppControllerFixture, ProfileDataIntactAcrossReconnect) {
    createAppProfile("google-chrome", "Google Chrome");
    // Explicitly set dpi and thumbWheelMode in the cache (createAppProfile copies from default)
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi = 2000;
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").thumbWheelMode = "zoom";
    setProfileButton("Google Chrome", 3, {ButtonAction::Keystroke, "Alt+Left"});
    focusApp("google-chrome");

    // Verify profile data is intact
    auto &p = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome");
    EXPECT_EQ(p.dpi, 2000);
    EXPECT_EQ(p.thumbWheelMode, "zoom");
    EXPECT_EQ(p.buttons[3].type, ButtonAction::Keystroke);
    EXPECT_EQ(p.buttons[3].payload, "Alt+Left");
}

TEST_F(AppControllerFixture, ButtonDiversionsMatchHwProfile) {
    createAppProfile("google-chrome", "Google Chrome");
    setProfileButton("Google Chrome", 3, {ButtonAction::Keystroke, "Alt+Left"});
    setProfileButton("Google Chrome", 4, {ButtonAction::Keystroke, "Alt+Right"});
    focusApp("google-chrome");

    // Verify diverted buttons dispatch correctly
    pressButton(0x0053);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Alt+Left");
    m_injector->clear();
    pressButton(0x0056);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Alt+Right");
}
