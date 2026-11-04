#include <gtest/gtest.h>
#include <QTemporaryDir>
#include "PanelSettings.h"

using namespace logitune;

TEST(PanelSettings, MissingFileHasNothingStored) {
    QTemporaryDir dir;
    const auto stored = loadPanelSettings(dir.filePath("devices/ABC/panel.conf"));
    EXPECT_FALSE(stored.hapticsEnabled.has_value());
    EXPECT_FALSE(stored.hapticsLevel.has_value());
    EXPECT_FALSE(stored.panelForce.has_value());
}

TEST(PanelSettings, SavesAndLoadsEachSettingSeparately) {
    QTemporaryDir dir;
    const QString file = dir.filePath("devices/ABC/panel.conf");

    // Changing only the panel force must not invent a haptics value, and vice versa.
    savePanelForce(file, 4803);
    auto stored = loadPanelSettings(file);
    EXPECT_EQ(stored.panelForce, 4803);
    EXPECT_FALSE(stored.hapticsLevel.has_value());

    saveHaptics(file, false, 60);
    stored = loadPanelSettings(file);
    EXPECT_EQ(stored.hapticsEnabled, false);
    EXPECT_EQ(stored.hapticsLevel, 60);
    EXPECT_EQ(stored.panelForce, 4803);
}

TEST(PanelSettings, PathIsPerDevice) {
    EXPECT_TRUE(panelSettingsPath("2616ZAR5CYK8").endsWith("/devices/2616ZAR5CYK8/panel.conf"));
}
