#include <gtest/gtest.h>

#include <QSettings>
#include <QTemporaryDir>

#include "ProfileEngine.h"
#include "RadialMenuConfig.h"

using namespace logitune;

TEST(RadialMenuConfig, DefaultsAreValid)
{
    const RadialMenuConfig config;
    EXPECT_TRUE(config.isValid());
    EXPECT_GE(config.items.size(), 2);
    EXPECT_LE(config.items.size(), 8);
}

TEST(RadialMenuConfig, RejectsRecursiveActions)
{
    RadialMenuConfig config;
    config.items[0].action = {ButtonAction::RadialMenu, {}};
    EXPECT_FALSE(config.isValid());

    config.items[0].action = {ButtonAction::GestureTrigger, {}};
    EXPECT_FALSE(config.isValid());
}

TEST(RadialMenuConfig, ProfileRoundTripUsesIniKeys)
{
    QTemporaryDir temp;
    ASSERT_TRUE(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("profile.conf"));

    Profile profile;
    profile.name = QStringLiteral("default");
    profile.radialMenu.items = {
        {QStringLiteral("Back"), {ButtonAction::Keystroke, QStringLiteral("Alt+Left")}},
        {QStringLiteral("Calculator"), {ButtonAction::PresetRef, QStringLiteral("calculator")}},
    };
    ProfileEngine::saveProfile(path, profile);

    QSettings raw(path, QSettings::IniFormat);
    EXPECT_EQ(raw.value(QStringLiteral("ActionRing/count")).toInt(), 2);
    EXPECT_EQ(raw.value(QStringLiteral("ActionRing/Items/0/label")).toString(),
              QStringLiteral("Back"));
    EXPECT_EQ(raw.value(QStringLiteral("ActionRing/Items/1/action")).toString(),
              QStringLiteral("preset:calculator"));

    const Profile loaded = ProfileEngine::loadProfile(path);
    ASSERT_EQ(loaded.radialMenu.items.size(), 2);
    EXPECT_EQ(loaded.radialMenu.items.at(0), profile.radialMenu.items.at(0));
    EXPECT_EQ(loaded.radialMenu.items.at(1), profile.radialMenu.items.at(1));
}

TEST(RadialMenuConfig, InvalidEntryDoesNotResetValidEntries)
{
    QTemporaryDir temp;
    ASSERT_TRUE(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("profile.conf"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("ActionRing/count"), 3);
    settings.setValue(QStringLiteral("ActionRing/Items/0/label"), QStringLiteral("Back"));
    settings.setValue(QStringLiteral("ActionRing/Items/0/action"), QStringLiteral("keystroke:Alt+Left"));
    settings.setValue(QStringLiteral("ActionRing/Items/1/label"), QStringLiteral("Broken"));
    settings.setValue(QStringLiteral("ActionRing/Items/1/action"), QStringLiteral("radial-menu"));
    settings.setValue(QStringLiteral("ActionRing/Items/2/label"), QStringLiteral("Mute"));
    settings.setValue(QStringLiteral("ActionRing/Items/2/action"), QStringLiteral("media:Mute"));
    settings.sync();

    const RadialMenuConfig loaded = RadialMenuConfig::load(settings);
    ASSERT_EQ(loaded.items.size(), 3);
    EXPECT_EQ(loaded.items.at(0).label, QStringLiteral("Back"));
    EXPECT_EQ(loaded.items.at(0).action.serialize(), QStringLiteral("keystroke:Alt+Left"));
    EXPECT_EQ(loaded.items.at(1).label, QStringLiteral("Broken"));
    EXPECT_NE(loaded.items.at(1).action.type, ButtonAction::RadialMenu);
    EXPECT_EQ(loaded.items.at(2).label, QStringLiteral("Mute"));
    EXPECT_EQ(loaded.items.at(2).action.serialize(), QStringLiteral("media:Mute"));
    EXPECT_TRUE(loaded.isValid());
}

TEST(RadialMenuConfig, SaveRepairsOnlyInvalidEntry)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("profile.conf")), QSettings::IniFormat);
    RadialMenuConfig config;
    config.items[0] = {QStringLiteral("Back"),
                       {ButtonAction::Keystroke, QStringLiteral("Alt+Left")}};
    config.items[1] = {QStringLiteral("Recursive"), {ButtonAction::RadialMenu, {}}};

    config.save(settings);
    settings.sync();
    const RadialMenuConfig loaded = RadialMenuConfig::load(settings);

    EXPECT_EQ(loaded.items.at(0), config.items.at(0));
    EXPECT_NE(loaded.items.at(1).action.type, ButtonAction::RadialMenu);
}
