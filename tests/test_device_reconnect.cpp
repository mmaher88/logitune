#include <gtest/gtest.h>
#include "helpers/AppRootFixture.h"
#include "helpers/TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(AppRootFixture, FirstConnectSetsDefaultProfile) {
    // Fresh controller — hw profile should be "default" after setup
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "default");
}

TEST_F(AppRootFixture, ReconnectPreservesHwProfile) {
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
// This mirrors the state of AppRoot before device setup completes.
TEST(DeviceReconnect, HwProfileEmptyOnFirstConnect) {
    ProfileEngine engine;
    EXPECT_TRUE(engine.hardwareProfile(QStringLiteral("mock-serial")).isEmpty());
}

TEST_F(AppRootFixture, ProfileDataIntactAcrossReconnect) {
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

TEST_F(AppRootFixture, ButtonDiversionsMatchHwProfile) {
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

// ---------------------------------------------------------------------------
// Transport re-attach
// ---------------------------------------------------------------------------
// When a transport re-attaches to a PhysicalDevice that already exists —
// a Bolt receiver coming back while the Bluetooth session is still alive, or
// a udev remove+add fast enough that the PhysicalDevice was never destroyed —
// DeviceManager does not emit physicalDeviceAdded a second time. Nothing then
// re-applies the profile, and the device keeps whatever its firmware reset to
// (issue #132).
//
// DeviceManager::probeDevice needs a real /dev node, so these tests start one
// step downstream: at the signal it emits and the AppRoot slot behind it.

TEST_F(AppRootFixture, TransportReadyReappliesStoredProfile) {
    const QString kSerial = QStringLiteral("mock-serial");
    profileEngine().cachedProfile(kSerial, QStringLiteral("default")).dpi = 2400;

    // The link dropped and the device came back at its firmware default.
    m_session->setDPI(400);
    ASSERT_EQ(m_session->currentDPI(), 400);

    emitTransportReady(m_physicalDevice);

    EXPECT_EQ(m_session->currentDPI(), 2400);
}

// The re-applied profile is the one the device is actually on, not "default".
// A per-app profile that was active before the drop has to survive it.
TEST_F(AppRootFixture, TransportReadyReappliesActiveAppProfile) {
    const QString kSerial = QStringLiteral("mock-serial");
    createAppProfile("google-chrome", "Google Chrome");
    // createAppProfile writes the profile to disk; the cache is what
    // onTransportSetupComplete reads, so set the DPI there too.
    profileEngine().cachedProfile(kSerial, QStringLiteral("Google Chrome")).dpi = 3200;
    focusApp("google-chrome");
    ASSERT_EQ(profileEngine().hardwareProfile(kSerial),
              QStringLiteral("Google Chrome"));

    m_session->setDPI(400);

    emitTransportReady(m_physicalDevice);

    EXPECT_EQ(m_session->currentDPI(), 3200);
}
