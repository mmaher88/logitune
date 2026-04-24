#include <gtest/gtest.h>

#include "ActionModel.h"
#include "ButtonAction.h"
#include "helpers/TestFixtures.h"

using logitune::ActionModel;
using logitune::ButtonAction;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ActionModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();
    }

    ActionModel model;
};

// ---------------------------------------------------------------------------
// Row count
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, HasEntries) {
    EXPECT_GT(model.rowCount(), 0);
}

// ---------------------------------------------------------------------------
// Roles — index(0)
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, AllRolesReturn) {
    QModelIndex idx = model.index(0);
    EXPECT_FALSE(model.data(idx, ActionModel::NameRole).toString().isEmpty());
    EXPECT_FALSE(model.data(idx, ActionModel::DescriptionRole).toString().isEmpty());
    EXPECT_FALSE(model.data(idx, ActionModel::ActionTypeRole).toString().isEmpty());
    // payload may be empty — just check it returns a valid QVariant
    EXPECT_TRUE(model.data(idx, ActionModel::PayloadRole).isValid());
}

// ---------------------------------------------------------------------------
// payloadForName — known entries
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, PayloadForNameFindsBack) {
    EXPECT_EQ(model.payloadForName(QStringLiteral("Back")), QStringLiteral("Alt+Left"));
}

TEST_F(ActionModelTest, PayloadForNameFindsForward) {
    EXPECT_EQ(model.payloadForName(QStringLiteral("Forward")), QStringLiteral("Alt+Right"));
}

TEST_F(ActionModelTest, PayloadForNameFindsCopy) {
    EXPECT_EQ(model.payloadForName(QStringLiteral("Copy")), QStringLiteral("Ctrl+C"));
}

// ---------------------------------------------------------------------------
// payloadForName — missing entry
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, PayloadForNameMissReturnsEmpty) {
    EXPECT_TRUE(model.payloadForName(QStringLiteral("NonExistent")).isEmpty());
}

// ---------------------------------------------------------------------------
// payloadForName — entry with intentionally empty payload
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, KeyboardShortcutHasEmptyPayload) {
    EXPECT_TRUE(model.payloadForName(QStringLiteral("Keyboard shortcut")).isEmpty());
}

// ---------------------------------------------------------------------------
// actionType checks
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, CalculatorIsPreset) {
    int idx = model.indexForName(QStringLiteral("Calculator"));
    ASSERT_GE(idx, 0);
    QModelIndex midx = model.index(idx);
    EXPECT_EQ(model.data(midx, ActionModel::ActionTypeRole).toString(),
              QStringLiteral("preset"));
}

TEST_F(ActionModelTest, DoNothingIsNoneType) {
    int idx = model.indexForName(QStringLiteral("Do nothing"));
    ASSERT_GE(idx, 0);
    QModelIndex midx = model.index(idx);
    EXPECT_EQ(model.data(midx, ActionModel::ActionTypeRole).toString(),
              QStringLiteral("none"));
}

// ---------------------------------------------------------------------------
// roleNames
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, RoleNamesContainExpectedKeys) {
    const QHash<int, QByteArray> roles = model.roleNames();
    const QList<QByteArray> values = roles.values();
    EXPECT_TRUE(values.contains("name"));
    EXPECT_TRUE(values.contains("description"));
    EXPECT_TRUE(values.contains("actionType"));
}

// ---------------------------------------------------------------------------
// buttonActionToName — domain -> UI display name
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, ButtonActionToNameDefaultReturnsEmpty) {
    ButtonAction ba{ButtonAction::Default, {}};
    EXPECT_TRUE(model.buttonActionToName(ba).isEmpty());
}

TEST_F(ActionModelTest, ButtonActionToNameGestureTrigger) {
    ButtonAction ba{ButtonAction::GestureTrigger, {}};
    EXPECT_EQ(model.buttonActionToName(ba), QStringLiteral("Gestures"));
}

TEST_F(ActionModelTest, ButtonActionToNameForKeystrokeKnownPayload) {
    // Copy has payload "Ctrl+C" — lookup should resolve back to "Copy".
    ButtonAction ba{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")};
    EXPECT_EQ(model.buttonActionToName(ba), QStringLiteral("Copy"));
}

TEST_F(ActionModelTest, ButtonActionToNameForKeystrokeUnknownFallsBackToPayload) {
    ButtonAction ba{ButtonAction::Keystroke, QStringLiteral("Ctrl+Alt+Unknown")};
    EXPECT_EQ(model.buttonActionToName(ba), QStringLiteral("Ctrl+Alt+Unknown"));
}

TEST_F(ActionModelTest, ButtonActionToNameForAppLaunch) {
    // AppLaunch branch falls through to returning the payload directly.
    ButtonAction ba{ButtonAction::AppLaunch, QStringLiteral("kcalc")};
    EXPECT_EQ(model.buttonActionToName(ba), QStringLiteral("kcalc"));
}

// ---------------------------------------------------------------------------
// buttonEntryToAction — UI (type, name) -> domain
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, ButtonEntryToActionDefault) {
    auto ba = model.buttonEntryToAction(QStringLiteral("default"), QStringLiteral("Middle click"));
    EXPECT_EQ(ba.type, ButtonAction::Default);
    EXPECT_TRUE(ba.payload.isEmpty());
}

TEST_F(ActionModelTest, ButtonEntryToActionGestureTrigger) {
    auto ba = model.buttonEntryToAction(QStringLiteral("gesture-trigger"), QStringLiteral("Gestures"));
    EXPECT_EQ(ba.type, ButtonAction::GestureTrigger);
}

TEST_F(ActionModelTest, ButtonEntryToActionSmartShiftToggle) {
    auto ba = model.buttonEntryToAction(QStringLiteral("smartshift-toggle"), QStringLiteral("Shift wheel mode"));
    EXPECT_EQ(ba.type, ButtonAction::SmartShiftToggle);
}

TEST_F(ActionModelTest, ButtonEntryToActionDpiCycle) {
    auto ba = model.buttonEntryToAction(QStringLiteral("dpi-cycle"), QStringLiteral("DPI cycle"));
    EXPECT_EQ(ba.type, ButtonAction::DpiCycle);
}

TEST_F(ActionModelTest, ButtonEntryToActionMediaControlsMapsDisplayNameToKey) {
    auto ba = model.buttonEntryToAction(QStringLiteral("media-controls"), QStringLiteral("Next track"));
    EXPECT_EQ(ba.type, ButtonAction::Media);
    EXPECT_EQ(ba.payload, QStringLiteral("Next"));
}

TEST_F(ActionModelTest, ButtonEntryToActionKeystrokeResolvesPayloadFromName) {
    auto ba = model.buttonEntryToAction(QStringLiteral("keystroke"), QStringLiteral("Copy"));
    EXPECT_EQ(ba.type, ButtonAction::Keystroke);
    EXPECT_EQ(ba.payload, QStringLiteral("Ctrl+C"));
}

TEST_F(ActionModelTest, ButtonEntryToActionKeystrokeFallsBackToNameWhenUnknown) {
    // Unknown name, but type is keystroke — treat the name as the literal payload.
    auto ba = model.buttonEntryToAction(QStringLiteral("keystroke"), QStringLiteral("Ctrl+Alt+T"));
    EXPECT_EQ(ba.type, ButtonAction::Keystroke);
    EXPECT_EQ(ba.payload, QStringLiteral("Ctrl+Alt+T"));
}

TEST_F(ActionModelTest, ButtonEntryToActionKeyboardShortcutHasEmptyPayload) {
    // "Keyboard shortcut" is the placeholder entry with an intentionally empty payload;
    // the translator should preserve that rather than echo the name back.
    auto ba = model.buttonEntryToAction(QStringLiteral("keystroke"), QStringLiteral("Keyboard shortcut"));
    EXPECT_EQ(ba.type, ButtonAction::Keystroke);
    EXPECT_TRUE(ba.payload.isEmpty());
}

TEST_F(ActionModelTest, ButtonEntryToActionAppLaunchFallsBackToName) {
    // "app-launch" branch: payloadForName returns whatever the model stores.
    // Calculator is now a preset entry so payloadForName returns "calculator".
    auto ba = model.buttonEntryToAction(QStringLiteral("app-launch"), QStringLiteral("Calculator"));
    EXPECT_EQ(ba.type, ButtonAction::AppLaunch);
    EXPECT_EQ(ba.payload, QStringLiteral("calculator"));
}

TEST_F(ActionModelTest, ButtonEntryToActionUnknownTypeReturnsDefault) {
    auto ba = model.buttonEntryToAction(QStringLiteral("nonsense-type"), QStringLiteral("whatever"));
    EXPECT_EQ(ba.type, ButtonAction::Default);
    EXPECT_TRUE(ba.payload.isEmpty());
}

// ---------------------------------------------------------------------------
// Round trips
// ---------------------------------------------------------------------------

TEST_F(ActionModelTest, RoundTripKeystrokeCopy) {
    ButtonAction original{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")};
    QString name = model.buttonActionToName(original);
    auto recovered = model.buttonEntryToAction(QStringLiteral("keystroke"), name);
    EXPECT_EQ(recovered, original);
}

TEST_F(ActionModelTest, RoundTripMediaNextTrack) {
    // Media is serialized with display names ("Next track") in the UI, so we
    // can't round-trip by starting from ButtonActionToName (it just returns the
    // payload). Instead, round-trip the UI -> domain -> UI direction matters
    // less here; ensure the domain side parses correctly for the known key.
    auto ba = model.buttonEntryToAction(QStringLiteral("media-controls"), QStringLiteral("Play/Pause"));
    EXPECT_EQ(ba.type, ButtonAction::Media);
    EXPECT_EQ(ba.payload, QStringLiteral("Play"));
}

// ---------------------------------------------------------------------------
// PresetRef entries (Task 9)
// ---------------------------------------------------------------------------

TEST(ActionModel, ShowDesktopEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Show desktop");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "show-desktop");
}

TEST(ActionModel, TaskSwitcherEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Task switcher");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "task-switcher");
}

TEST(ActionModel, CalculatorEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Calculator");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "calculator");
}

TEST(ActionModel, BackEntryStaysRawKeystroke) {
    ActionModel m;
    int idx = m.indexForName("Back");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "keystroke");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "Alt+Left");
}

TEST(ActionModel, buttonEntryToActionPresetReturnsPresetRef) {
    ActionModel m;
    ButtonAction ba = m.buttonEntryToAction("preset", "Show desktop");
    EXPECT_EQ(ba.type, ButtonAction::PresetRef);
    EXPECT_EQ(ba.payload, "show-desktop");
}

TEST(ActionModel, buttonActionToNamePresetLooksUpLabel) {
    ActionModel m;
    EXPECT_EQ(m.buttonActionToName(ButtonAction{ButtonAction::PresetRef, "show-desktop"}),
              "Show desktop");
}
