#include <gtest/gtest.h>

#include "ActionModel.h"
#include "helpers/TestFixtures.h"

using logitune::ActionModel;

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

TEST_F(ActionModelTest, CalculatorIsAppLaunch) {
    int idx = model.indexForName(QStringLiteral("Calculator"));
    ASSERT_GE(idx, 0);
    QModelIndex midx = model.index(idx);
    EXPECT_EQ(model.data(midx, ActionModel::ActionTypeRole).toString(),
              QStringLiteral("app-launch"));
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
