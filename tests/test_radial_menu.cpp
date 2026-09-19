#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include "RadialMenuConfig.h"
#include "ProfileEngine.h"
#include "services/RadialMenuController.h"
#include "helpers/TestFixtures.h"
using namespace logitune;

TEST(RadialMenu, SelectionAndDeadZone) {
    EXPECT_EQ(RadialMenuConfig::selection(0, 0, 8), -1);
    EXPECT_EQ(RadialMenuConfig::selection(30, 30, 8), -1);
    EXPECT_EQ(RadialMenuConfig::selection(0, -100, 4), 0);
    EXPECT_EQ(RadialMenuConfig::selection(100, 0, 4), 1);
    EXPECT_EQ(RadialMenuConfig::selection(0, 100, 4), 2);
    EXPECT_EQ(RadialMenuConfig::selection(-100, 0, 4), 3);
    EXPECT_EQ(RadialMenuConfig::selection(100, -100, 8), 1);
    EXPECT_EQ(RadialMenuConfig::selection(-100, -100, 8), 7);
    EXPECT_EQ(RadialMenuConfig::selection(100, 0, 9), -1);
}
TEST(RadialMenu, ProfileRoundTripAndInvalidFallback) {
    QTemporaryDir dir;
    Profile p;
    p.buttons[5] = {ButtonAction::RadialMenu, {}};
    p.radialMenu.items[0] = QVariantMap{{"label", QString::fromUtf8("Браузер")}, {"action", "app-launch:firefox --new-window"}};
    p.radialMenu.click = {{"label", "Copy"}, {"action", "keystroke:Ctrl+C"}};
    ProfileEngine::saveProfile(dir.filePath("profile.ini"), p);
    const auto loaded = ProfileEngine::loadProfile(dir.filePath("profile.ini"));
    EXPECT_EQ(loaded.buttons[5].type, ButtonAction::RadialMenu);
    EXPECT_EQ(loaded.radialMenu.toJson(), p.radialMenu.toJson());
    EXPECT_EQ(RadialMenuConfig::fromJson("broken").items.size(), 2);
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "radial-menu"}}));
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "gesture-trigger"}}));
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "keystroke:"}}));
}
TEST(RadialMenu, ShortPressHoldAndCenterCancel) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    RadialMenuConfig config;
    config.click = {{"label", "Undo"}, {"action", "keystroke:Ctrl+Z"}};
    QStringList fired;
    QObject::connect(&menu, &RadialMenuController::triggered, &menu,
        [&](const ButtonAction &a) { EXPECT_FALSE(menu.visible()); fired << a.serialize(); });
    menu.begin(config);
    menu.release();
    EXPECT_EQ(fired, QStringList{"keystroke:Ctrl+Z"});
    fired.clear();
    menu.begin(config);
    QTest::qWait(300);
    EXPECT_TRUE(menu.visible());
    menu.release();
    EXPECT_TRUE(fired.isEmpty());
    menu.begin(config);
    menu.move(0, -100);
    EXPECT_TRUE(menu.visible());
    menu.move(0, 100);
    EXPECT_EQ(menu.selected(), -1);
    menu.release();
    EXPECT_TRUE(fired.isEmpty());
    menu.begin(config);
    menu.move(0, -100);
    menu.release();
    EXPECT_EQ(fired, QStringList{"keystroke:Ctrl+C"});
}
TEST(RadialMenu, CancellationStopsPendingTimerAndRelease) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    QSignalSpy fired(&menu, &RadialMenuController::triggered);
    menu.begin({});
    menu.move(0, -100);
    menu.cancel();
    menu.release();
    QTest::qWait(300);
    EXPECT_FALSE(menu.visible());
    EXPECT_EQ(fired.count(), 0);
}

#include "input/UinputInjector.h"
#include <QFile>
#include <QStandardPaths>
TEST(RadialMenu, LaunchCommandPreservesQuotedArguments) {
    logitune::test::ensureApp();
    const auto touch = QStandardPaths::findExecutable("touch");
    ASSERT_FALSE(touch.isEmpty());
    QTemporaryDir dir;
    const auto file = dir.filePath("argument with spaces");
    UinputInjector injector;
    injector.launchApp(touch + " \"" + file + "\"");
    for (int i = 0; i < 100 && !QFile::exists(file); ++i) QTest::qWait(10);
    EXPECT_TRUE(QFile::exists(file));
}

TEST(RadialMenu, RejectsInvalidBindingsAndItemCounts) {
    RadialMenuConfig config;
    config.items.removeLast();
    EXPECT_FALSE(config.isValid());
    while (config.items.size() < 8) config.items.append(config.items.first());
    EXPECT_TRUE(config.isValid());
    config.items.append(config.items.first());
    EXPECT_FALSE(config.isValid());
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "keystroke:Ctrl+NotAKey"}}));
    EXPECT_TRUE(RadialMenuConfig::validEntry({{"action", "keystroke:Ctrl+Shift+C"}}));
}
