#include <gtest/gtest.h>
#include "helpers/AppControllerFixture.h"

using namespace logitune;
using namespace logitune::test;

// --- Profile creation guards ---

TEST_F(AppControllerFixture, CreateProfileDoesNotOverwriteExisting) {
    createAppProfile("google-chrome", "Google Chrome", 2000);
    // Modify the profile
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi = 3000;
    // Re-create — should NOT overwrite
    profileEngine().createProfileForApp(QStringLiteral("mock-serial"), "google-chrome", "Google Chrome");
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi, 3000);
}

TEST_F(AppControllerFixture, NewProfileCopiesFromDefault) {
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default").dpi = 1500;
    profileEngine().createProfileForApp(QStringLiteral("mock-serial"), "firefox", "Firefox");
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Firefox").dpi, 1500);
}

TEST_F(AppControllerFixture, RestoreProfileDoesNotTriggerCreate) {
    // Create Chrome profile, then manually set a custom DPI in the cache
    createAppProfile("google-chrome", "Google Chrome");
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi = 2500;
    // Simulate startup restore — should NOT overwrite the cache
    profileModel().restoreProfile("google-chrome", "Google Chrome");
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi, 2500);
}

// --- Profile isolation ---

TEST_F(AppControllerFixture, ProfilesAreIndependent) {
    createAppProfile("google-chrome", "Google Chrome", 1000);
    createAppProfile("org.kde.dolphin", "Dolphin", 1000);

    // Change Chrome's DPI
    profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi = 2000;
    // Dolphin should be unaffected
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Dolphin").dpi, 1000);
}

TEST_F(AppControllerFixture, DisplayAndHardwareCanDiffer) {
    createAppProfile("google-chrome", "Google Chrome", 2000);
    focusApp("google-chrome"); // hw = Chrome
    profileModel().selectTab(0); // display = default
    EXPECT_EQ(profileEngine().displayProfile(QStringLiteral("mock-serial")), "default");
    EXPECT_EQ(profileEngine().hardwareProfile(QStringLiteral("mock-serial")), "Google Chrome");
    EXPECT_EQ(deviceModel().currentDPI(), 1000); // display shows default's DPI
}

TEST_F(AppControllerFixture, DpiChangeSavesToDisplayedProfile) {
    createAppProfile("google-chrome", "Google Chrome");
    profileModel().selectTab(1); // display Chrome
    deviceModel().setDPI(2400);
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").dpi, 2400);
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default").dpi, 1000); // unchanged
}

TEST_F(AppControllerFixture, ThumbWheelModeSavesToDisplayedProfile) {
    createAppProfile("google-chrome", "Google Chrome");
    profileModel().selectTab(1);
    deviceModel().setThumbWheelMode("zoom");
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").thumbWheelMode, "zoom");
    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default").thumbWheelMode, "scroll");
}

// --- Profile removal ---

TEST_F(AppControllerFixture, RemoveProfileFallsToDefault) {
    createAppProfile("google-chrome", "Google Chrome");
    profileModel().selectTab(1); // display Chrome
    profileModel().removeProfile(1);
    EXPECT_EQ(profileModel().displayIndex(), 0);
}

TEST_F(AppControllerFixture, RemovedProfileWmClassResolvesToDefault) {
    createAppProfile("google-chrome", "Google Chrome");
    profileEngine().removeAppProfile(QStringLiteral("mock-serial"), "google-chrome");
    EXPECT_EQ(profileEngine().profileForApp(QStringLiteral("mock-serial"), "google-chrome"), "default");
}

// --- Focus + profile interaction ---

TEST_F(AppControllerFixture, FocusAfterProfileChangeAppliesNewSettings) {
    createAppProfile("google-chrome", "Google Chrome", 1000, "scroll");
    focusApp("google-chrome");

    // Change Chrome's thumb wheel while viewing it
    profileModel().selectTab(1);
    deviceModel().setThumbWheelMode("zoom");

    // Switch away and back — new setting should apply
    focusApp("kitty");
    focusApp("google-chrome");

    EXPECT_EQ(profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Google Chrome").thumbWheelMode, "zoom");
}
