#include <gtest/gtest.h>
#include <QFile>
#include "ProfileEngine.h"
#include "helpers/TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class ProfilePersistenceTest : public ProfileFixture {};

TEST_F(ProfilePersistenceTest, SaveAndReloadPreservesAllFields) {
    Profile p = makeDefaultProfile();
    p.dpi = 2400;
    p.smartShiftEnabled = false;
    p.smartShiftThreshold = 64;
    p.hiResScroll = false;
    p.scrollDirection = "natural";
    p.thumbWheelMode = "zoom";
    p.buttons[3] = {ButtonAction::Keystroke, "Alt+Left"};
    p.buttons[5] = {ButtonAction::GestureTrigger, {}};
    p.buttons[6] = {ButtonAction::SmartShiftToggle, {}};
    p.gestures["up"] = {ButtonAction::Keystroke, "Super+Up"};
    p.gestures["down"] = {ButtonAction::Keystroke, "Super+D"};

    QString path = tmpPath() + "/test.conf";
    ProfileEngine::saveProfile(path, p);

    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(loaded.dpi, 2400);
    EXPECT_FALSE(loaded.smartShiftEnabled);
    EXPECT_EQ(loaded.smartShiftThreshold, 64);
    EXPECT_FALSE(loaded.hiResScroll);
    EXPECT_EQ(loaded.scrollDirection, "natural");
    EXPECT_EQ(loaded.thumbWheelMode, "zoom");
    EXPECT_EQ(loaded.buttons[3].type, ButtonAction::Keystroke);
    EXPECT_EQ(loaded.buttons[3].payload, "Alt+Left");
    EXPECT_EQ(loaded.buttons[5].type, ButtonAction::GestureTrigger);
    EXPECT_EQ(loaded.buttons[6].type, ButtonAction::SmartShiftToggle);
    EXPECT_EQ(loaded.gestures.at("up").payload, "Super+Up");
    EXPECT_EQ(loaded.gestures.at("down").payload, "Super+D");
}

TEST_F(ProfilePersistenceTest, AllButtonActionTypesSurviveRoundTrip) {
    Profile p = makeDefaultProfile();
    p.buttons[0] = {ButtonAction::Default, {}};
    p.buttons[1] = {ButtonAction::Keystroke, "Ctrl+C"};
    p.buttons[2] = {ButtonAction::GestureTrigger, {}};
    p.buttons[3] = {ButtonAction::SmartShiftToggle, {}};
    p.buttons[4] = {ButtonAction::AppLaunch, "kcalc"};

    QString path = tmpPath() + "/roundtrip.conf";
    ProfileEngine::saveProfile(path, p);
    Profile loaded = ProfileEngine::loadProfile(path);

    EXPECT_EQ(loaded.buttons[0], p.buttons[0]);
    EXPECT_EQ(loaded.buttons[1], p.buttons[1]);
    EXPECT_EQ(loaded.buttons[2], p.buttons[2]);
    EXPECT_EQ(loaded.buttons[3], p.buttons[3]);
    EXPECT_EQ(loaded.buttons[4], p.buttons[4]);
}

TEST_F(ProfilePersistenceTest, AppBindingsRoundTrip) {
    QMap<QString, QString> bindings;
    bindings["google-chrome"] = "Google Chrome";
    bindings["org.kde.dolphin"] = "Dolphin";

    QString path = tmpPath() + "/app-bindings.conf";
    ProfileEngine::saveAppBindings(path, bindings);
    auto loaded = ProfileEngine::loadAppBindings(path);

    EXPECT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded.value("google-chrome"), "Google Chrome");
}

TEST_F(ProfilePersistenceTest, ClearIniPreventsDuplicates) {
    Profile p = makeDefaultProfile();
    p.dpi = 1000;
    QString path = tmpPath() + "/dup.conf";

    // Save twice — should not accumulate duplicate sections
    ProfileEngine::saveProfile(path, p);
    p.dpi = 2000;
    ProfileEngine::saveProfile(path, p);

    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(loaded.dpi, 2000);
}

TEST_F(ProfilePersistenceTest, MissingFileReturnsDefaults) {
    Profile loaded = ProfileEngine::loadProfile(tmpPath() + "/nonexistent.conf");
    EXPECT_EQ(loaded.dpi, 1000); // default
    EXPECT_TRUE(loaded.smartShiftEnabled); // default
    EXPECT_EQ(loaded.thumbWheelMode, "scroll"); // default
}

TEST_F(ProfilePersistenceTest, CreateProfileSavesToDisk) {
    const QString kSerial = QStringLiteral("test-serial");
    ProfileEngine engine;
    Profile def = makeDefaultProfile();
    ProfileEngine::saveProfile(tmpPath() + "/default.conf", def);
    engine.registerDevice(kSerial, tmpPath());

    engine.createProfileForApp(kSerial, "google-chrome", "Google Chrome");

    EXPECT_TRUE(QFile::exists(tmpPath() + "/Google Chrome.conf"));
}

TEST_F(ProfilePersistenceTest, RemoveProfileDeletesFile) {
    const QString kSerial = QStringLiteral("test-serial");
    ProfileEngine engine;
    Profile def = makeDefaultProfile();
    ProfileEngine::saveProfile(tmpPath() + "/default.conf", def);
    engine.registerDevice(kSerial, tmpPath());

    engine.createProfileForApp(kSerial, "google-chrome", "Google Chrome");
    EXPECT_TRUE(QFile::exists(tmpPath() + "/Google Chrome.conf"));

    engine.removeAppProfile(kSerial, "google-chrome");
    EXPECT_FALSE(QFile::exists(tmpPath() + "/Google Chrome.conf"));
}

TEST_F(ProfilePersistenceTest, SaveProfileToDiskWritesCache) {
    const QString kSerial = QStringLiteral("test-serial");
    ProfileEngine engine;
    Profile def = makeDefaultProfile();
    ProfileEngine::saveProfile(tmpPath() + "/default.conf", def);
    engine.registerDevice(kSerial, tmpPath());

    auto &p = engine.cachedProfile(kSerial, "default");
    p.dpi = 3200;
    engine.saveProfileToDisk(kSerial, "default");

    // Reload from disk and verify
    Profile reloaded = ProfileEngine::loadProfile(tmpPath() + "/default.conf");
    EXPECT_EQ(reloaded.dpi, 3200);
}

TEST_F(ProfilePersistenceTest, RegisterDeviceLoadsAllProfiles) {
    // Create two profile files on disk
    Profile p1 = makeDefaultProfile(); p1.name = "default"; p1.dpi = 1000;
    Profile p2 = makeDefaultProfile(); p2.name = "Chrome"; p2.dpi = 2000;
    ProfileEngine::saveProfile(tmpPath() + "/default.conf", p1);
    ProfileEngine::saveProfile(tmpPath() + "/Chrome.conf", p2);

    const QString kSerial = QStringLiteral("test-serial");
    ProfileEngine engine;
    engine.registerDevice(kSerial, tmpPath());

    EXPECT_EQ(engine.cachedProfile(kSerial, "default").dpi, 1000);
    EXPECT_EQ(engine.cachedProfile(kSerial, "Chrome").dpi, 2000);
}
