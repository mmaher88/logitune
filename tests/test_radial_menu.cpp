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
    EXPECT_EQ(RadialMenuConfig::fromJson("broken").items.size(), 8);
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "radial-menu"}}));
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "gesture-trigger"}}));
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "keystroke:"}}));
}
TEST(RadialMenu, OpensOnClickAndRunsOnlyExplicitSelection) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    RadialMenuConfig config;
    config.click = {{"label", "Old short press"}, {"action", "keystroke:Ctrl+Z"}};
    QStringList fired;
    QObject::connect(&menu, &RadialMenuController::triggered, &menu,
        [&](const ButtonAction &a) { EXPECT_FALSE(menu.visible()); fired << a.serialize(); });
    menu.begin(config);
    EXPECT_TRUE(menu.visible());
    menu.select(0);
    QTest::qWait(300);
    EXPECT_TRUE(menu.visible());
    EXPECT_TRUE(fired.isEmpty());
    menu.activate(-1);
    menu.activate(8);
    EXPECT_TRUE(fired.isEmpty());
    EXPECT_TRUE(menu.visible());
    menu.activate(0);
    EXPECT_EQ(fired, QStringList{"keystroke:Ctrl+C"});
    EXPECT_FALSE(menu.visible());
    menu.activate(0);
    EXPECT_EQ(fired.size(), 1);
}
TEST(RadialMenu, CancelPreventsLaterActivation) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    QSignalSpy fired(&menu, &RadialMenuController::triggered);
    menu.begin({});
    menu.cancel();
    menu.activate(0);
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
    config.items = {config.items.first()};
    EXPECT_FALSE(config.isValid());
    while (config.items.size() < 8) config.items.append(config.items.first());
    EXPECT_TRUE(config.isValid());
    config.items.append(config.items.first());
    EXPECT_FALSE(config.isValid());
    EXPECT_FALSE(RadialMenuConfig::validEntry({{"action", "keystroke:Ctrl+NotAKey"}}));
    EXPECT_TRUE(RadialMenuConfig::validEntry({{"action", "keystroke:Ctrl+Shift+C"}}));
}

TEST(RadialMenu, PlacementUsesRingCenterAndKeepsWindowOnCursorOutput) {
    const QSize size(380, 380);
    const QPoint center(190, 165);
    EXPECT_EQ(RadialMenuController::placement({800, 500}, {0, 0, 1920, 1080}, size, center),
              QPoint(610, 335));
    EXPECT_EQ(RadialMenuController::placement({5, 5}, {0, 0, 1920, 1080}, size, center),
              QPoint(0, 0));
    EXPECT_EQ(RadialMenuController::placement({1919, 1079}, {0, 0, 1920, 1080}, size, center),
              QPoint(1540, 700));
    // A second monitor left of the primary, with a panel reducing usable height.
    EXPECT_EQ(RadialMenuController::placement({-1000, 400}, {-1920, 30, 1920, 1050}, size, center),
              QPoint(-1190, 235));
    EXPECT_EQ(RadialMenuController::placement({-5, 1070}, {-1920, 30, 1920, 1050}, size, center),
              QPoint(-380, 700));
    // An output smaller than the menu must not create an invalid clamp interval.
    EXPECT_EQ(RadialMenuController::placement({30, 30}, {0, 0, 300, 300}, size, center), QPoint(0, 0));
}

TEST(RadialMenu, PointerMovementDoesNotRepositionOpenMenu) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    menu.begin({});
    const auto origin = menu.overlayPosition();
    menu.select(0);
    menu.select(1);
    EXPECT_EQ(menu.overlayPosition(), origin);
}

TEST(RadialMenu, CursorProviderIgnoresRepliesAfterCancellation) {
    logitune::test::ensureApp();
    CursorPositionProvider provider;
    QSignalSpy ready(&provider, &CursorPositionProvider::positionReady);
    provider.cancel();
    provider.receivePosition(1, 500, 500);
    EXPECT_EQ(ready.count(), 0);
}

TEST(RadialMenu, MigratesOldSlotsAndPreservesCustomBindings) {
    const auto config = RadialMenuConfig::fromJson(R"({"items":[{"label":"Browser","action":"app-launch:firefox --new-window"},{"label":"Copy","action":"keystroke:Ctrl+C"}],"click":{"label":"","action":"default"}})");
    ASSERT_EQ(config.items.size(), 8);
    EXPECT_EQ(config.items[0].toMap().value("action"), "app-launch:firefox --new-window");
    EXPECT_EQ(config.items[2].toMap().value("kind"), "empty");
    EXPECT_EQ(RadialMenuConfig::fromJson(config.toJson()).toJson(), config.toJson());
}

TEST(RadialMenu, FolderRoundTripAndValidation) {
    RadialMenuConfig config;
    QVariantList children{QVariantMap{{"label", "Undo"}, {"action", "keystroke:Ctrl+Z"}, {"icon", "undo"}}};
    QVariantMap folder{{"kind", "folder"}, {"label", "Editing"}, {"children", children}, {"icon", "folder"}};
    config.items[0] = folder;
    ASSERT_TRUE(config.isValid());
    EXPECT_EQ(RadialMenuConfig::fromJson(config.toJson()).toJson(), config.toJson());
    folder["children"] = QVariantList{folder};
    config.items[0] = folder;
    EXPECT_FALSE(config.isValid());
    folder["children"] = RadialMenuConfig::padded(children, 10);
    config.items[0] = folder;
    EXPECT_FALSE(config.isValid());
    folder["children"] = children;
    folder["icon"] = "../../unexpected";
    config.items[0] = folder;
    EXPECT_FALSE(config.isValid());
    config.items[0] = QVariantMap{{"kind", "adjustment"}, {"label", "Volume"}, {"parameter", "unknown"}};
    EXPECT_FALSE(config.isValid());
}

TEST(RadialMenu, FolderNavigationDoesNotExecuteUntilChildClicked) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    RadialMenuConfig config;
    config.items[0] = QVariantMap{{"kind", "folder"}, {"label", "Editing"},
        {"children", QVariantList{QVariantMap{{"label", "Undo"}, {"action", "keystroke:Ctrl+Z"}}}}};
    QSignalSpy fired(&menu, &RadialMenuController::triggered);
    menu.begin(config);
    menu.activate(0);
    ASSERT_TRUE(menu.inFolder());
    EXPECT_EQ(menu.items().size(), 9);
    EXPECT_EQ(menu.folderName(), "Editing");
    EXPECT_EQ(fired.count(), 0);
    menu.activate(8); // Empty slot must not close or inject anything.
    EXPECT_TRUE(menu.visible());
    EXPECT_EQ(fired.count(), 0);
    menu.back();
    EXPECT_FALSE(menu.inFolder());
    EXPECT_TRUE(menu.visible());
    menu.activate(0);
    menu.activate(0);
    EXPECT_FALSE(menu.visible());
    ASSERT_EQ(fired.count(), 1);
    EXPECT_EQ(qvariant_cast<ButtonAction>(fired.first().first()).serialize(), "keystroke:Ctrl+Z");
    menu.begin(config);
    EXPECT_FALSE(menu.inFolder());
    menu.back();
    EXPECT_FALSE(menu.visible());
}

TEST(RadialMenu, AdjustmentsKeepMenuOpenAndCancelStopsFurtherInput) {
    logitune::test::ensureApp();
    RadialMenuController menu;
    RadialMenuConfig config;
    config.items[0] = QVariantMap{{"kind", "adjustment"}, {"label", "Volume"}, {"parameter", "volume"}};
    config.items[1] = QVariantMap{{"kind", "adjustment"}, {"label", "Brightness"}, {"parameter", "brightness"}};
    QStringList fired;
    QObject::connect(&menu, &RadialMenuController::triggered, &menu,
        [&](const ButtonAction &a) { EXPECT_TRUE(menu.visible()); fired << a.serialize(); });
    menu.begin(config);
    menu.activate(0);
    EXPECT_TRUE(fired.isEmpty());
    EXPECT_TRUE(menu.adjust(0, 2));
    EXPECT_EQ(fired, (QStringList{"keystroke:VolumeUp", "keystroke:VolumeUp"}));
    EXPECT_EQ(menu.adjustmentSteps(), 2);
    EXPECT_TRUE(menu.adjust(1, -1));
    EXPECT_EQ(fired.last(), "keystroke:BrightnessDown");
    EXPECT_EQ(menu.adjustmentSteps(), -1);
    EXPECT_FALSE(menu.adjust(7, 1));
    menu.cancel();
    EXPECT_FALSE(menu.adjust(0, 1));
    EXPECT_EQ(fired.size(), 3);
}
