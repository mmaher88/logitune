#include <gtest/gtest.h>
#include "desktop/KDeDesktop.h"
#include "desktop/GnomeDesktop.h"
#include "desktop/HyprlandDesktop.h"
#include "desktop/GenericDesktop.h"

using namespace logitune;

TEST(DesktopFactory, KDeDesktopReportsKDE) {
    KDeDesktop kde;
    EXPECT_EQ(kde.desktopName(), "KDE");
}

TEST(DesktopFactory, GnomeDesktopReportsGNOME) {
    GnomeDesktop gnome;
    EXPECT_EQ(gnome.desktopName(), "GNOME");
}

TEST(DesktopFactory, HyprlandDesktopReportsHyprland) {
    HyprlandDesktop hyprland;
    EXPECT_EQ(hyprland.desktopName(), "Hyprland");
}

TEST(DesktopFactory, GenericDesktopReportsGeneric) {
    GenericDesktop generic;
    EXPECT_EQ(generic.desktopName(), "Generic");
}

TEST(DesktopFactory, GenericDesktopIsAlwaysAvailable) {
    GenericDesktop generic;
    EXPECT_TRUE(generic.available());
}

TEST(DesktopFactory, RunningApplicationsReturnsSortedList) {
    GenericDesktop generic;
    auto apps = generic.runningApplications();
    // May be empty in CI containers with no .desktop files — that's OK
    for (int i = 1; i < apps.size(); ++i) {
        QString prev = apps[i-1].toMap()["title"].toString().toLower();
        QString curr = apps[i].toMap()["title"].toString().toLower();
        EXPECT_LE(prev, curr);
    }
}

TEST(DesktopFactory, KDeVariantKeyIsKde) {
    KDeDesktop d;
    EXPECT_EQ(d.variantKey(), "kde");
}

TEST(DesktopFactory, GnomeVariantKeyIsGnome) {
    GnomeDesktop d;
    EXPECT_EQ(d.variantKey(), "gnome");
}

TEST(DesktopFactory, HyprlandVariantKeyIsHyprland) {
    HyprlandDesktop d;
    EXPECT_EQ(d.variantKey(), "hyprland");
}

TEST(DesktopFactory, GenericVariantKeyIsGeneric) {
    GenericDesktop d;
    EXPECT_EQ(d.variantKey(), "generic");
}
