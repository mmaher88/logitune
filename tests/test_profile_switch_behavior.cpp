#include <gtest/gtest.h>
#include "helpers/AppRootFixture.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(AppRootFixture, ProfileSwitchUpdatesDPI) {
    createAppProfile("chrome", "Chrome", 1600, "scroll", false);
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
    focusApp("chrome");
    EXPECT_EQ(deviceModel().currentDPI(), 1600);
}

TEST_F(AppRootFixture, ProfileSwitchUpdatesSmartShift) {
    createAppProfile("chrome", "Chrome", 1000, "scroll", false,
                     false, 50);
    EXPECT_TRUE(deviceModel().smartShiftEnabled());
    EXPECT_EQ(deviceModel().smartShiftThreshold(), 128);
    focusApp("chrome");
    EXPECT_FALSE(deviceModel().smartShiftEnabled());
    EXPECT_EQ(deviceModel().smartShiftThreshold(), 50);
}

TEST_F(AppRootFixture, ProfileSwitchUpdatesScrollDirection) {
    createAppProfile("chrome", "Chrome", 1000, "scroll", false,
                     true, 128, "natural");
    EXPECT_FALSE(deviceModel().scrollInvert());
    focusApp("chrome");
    EXPECT_TRUE(deviceModel().scrollInvert());
}

TEST_F(AppRootFixture, ProfileSwitchUpdatesThumbWheelMode) {
    createAppProfile("chrome", "Chrome", 1000, "volume", false);
    focusApp("chrome");
    EXPECT_EQ(deviceModel().thumbWheelMode(), "volume");
}

TEST_F(AppRootFixture, ProfileSwitchUpdatesThumbWheelInvert) {
    createAppProfile("chrome", "Chrome", 1000, "volume", true);
    EXPECT_FALSE(deviceModel().thumbWheelInvert());
    focusApp("chrome");
    EXPECT_TRUE(deviceModel().thumbWheelInvert());
}

TEST_F(AppRootFixture, SwitchBackToDefaultRestoresAllSettings) {
    createAppProfile("chrome", "Chrome", 1600, "volume", true,
                     false, 50, "natural", false);
    focusApp("chrome");
    EXPECT_EQ(deviceModel().currentDPI(), 1600);
    EXPECT_FALSE(deviceModel().smartShiftEnabled());
    EXPECT_TRUE(deviceModel().scrollInvert());
    EXPECT_EQ(deviceModel().thumbWheelMode(), "volume");
    EXPECT_TRUE(deviceModel().thumbWheelInvert());

    focusApp("unregistered-app");
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
    EXPECT_TRUE(deviceModel().smartShiftEnabled());
    EXPECT_FALSE(deviceModel().scrollInvert());
    EXPECT_EQ(deviceModel().thumbWheelMode(), "scroll");
    EXPECT_FALSE(deviceModel().thumbWheelInvert());
}

TEST_F(AppRootFixture, ThreeProfilesAreIndependent) {
    createAppProfile("chrome", "Chrome", 1600, "zoom", false);
    createAppProfile("kwrite", "KWrite", 800, "scroll", true);

    focusApp("chrome");
    EXPECT_EQ(deviceModel().currentDPI(), 1600);
    EXPECT_EQ(deviceModel().thumbWheelMode(), "zoom");

    focusApp("kwrite");
    EXPECT_EQ(deviceModel().currentDPI(), 800);
    EXPECT_EQ(deviceModel().thumbWheelMode(), "scroll");
    EXPECT_TRUE(deviceModel().thumbWheelInvert());

    focusApp("unregistered");
    EXPECT_EQ(deviceModel().currentDPI(), 1000);
    EXPECT_EQ(deviceModel().thumbWheelMode(), "scroll");
    EXPECT_FALSE(deviceModel().thumbWheelInvert());

    focusApp("chrome");
    EXPECT_EQ(deviceModel().currentDPI(), 1600);
    EXPECT_EQ(deviceModel().thumbWheelMode(), "zoom");
}
