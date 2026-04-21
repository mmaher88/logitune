#include <gtest/gtest.h>
#include <QSignalSpy>
#include "helpers/AppControllerFixture.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(AppControllerFixture, DpiChangeSavesToDisplayedProfileOnly) {
    createAppProfile("chrome", "Chrome", 1600, "scroll", false);
    focusApp("unregistered");  // hardware=default
    profileModel().selectTab(1);  // display=Chrome

    deviceModel().setDPI(2000);

    Profile &chrome = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Chrome");
    EXPECT_EQ(chrome.dpi, 2000);

    Profile &def = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default");
    EXPECT_EQ(def.dpi, 1000);  // unchanged
}

TEST_F(AppControllerFixture, SmartShiftToggleSavesAndUpdatesDisplay) {
    deviceModel().setSmartShift(false, 50);

    Profile &def = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default");
    EXPECT_FALSE(def.smartShiftEnabled);
    EXPECT_EQ(def.smartShiftThreshold, 50);

    EXPECT_FALSE(deviceModel().smartShiftEnabled());
    EXPECT_EQ(deviceModel().smartShiftThreshold(), 50);
}

TEST_F(AppControllerFixture, ScrollDirectionChangeSaves) {
    deviceModel().setScrollConfig(true, true);  // hiRes=true, invert=true (natural)

    Profile &def = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default");
    EXPECT_EQ(def.scrollDirection, "natural");
    EXPECT_TRUE(def.hiResScroll);
}

TEST_F(AppControllerFixture, ThumbWheelInvertChangeSaves) {
    deviceModel().setThumbWheelInvert(true);

    Profile &def = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "default");
    EXPECT_TRUE(def.thumbWheelInvert);
    EXPECT_TRUE(deviceModel().thumbWheelInvert());
}

TEST_F(AppControllerFixture, ThumbWheelInvertDoesNotAffectOtherProfile) {
    createAppProfile("chrome", "Chrome", 1000, "volume", false);

    deviceModel().setThumbWheelInvert(true);

    Profile &chrome = profileEngine().cachedProfile(QStringLiteral("mock-serial"), "Chrome");
    EXPECT_FALSE(chrome.thumbWheelInvert);
}
