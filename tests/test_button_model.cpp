#include <gtest/gtest.h>
#include <QSignalSpy>

#include "ButtonModel.h"
#include "helpers/TestFixtures.h"

using logitune::ButtonModel;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ButtonModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();
    }

    ButtonModel model;
};

// ---------------------------------------------------------------------------
// Row count
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, InitialRowCount) {
    EXPECT_EQ(model.rowCount(), 8);
}

// ---------------------------------------------------------------------------
// setAction — signals
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, SetActionEmitsDataChanged) {
    QSignalSpy spy(&model, &ButtonModel::dataChanged);
    model.setAction(3, "Copy", "keystroke");
    EXPECT_GE(spy.count(), 1);
}

TEST_F(ButtonModelTest, SetActionEmitsUserActionChanged) {
    QSignalSpy spy(&model, &ButtonModel::userActionChanged);
    model.setAction(3, "Copy", "keystroke");
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 3);
    EXPECT_EQ(args.at(1).toString(), QStringLiteral("Copy"));
    EXPECT_EQ(args.at(2).toString(), QStringLiteral("keystroke"));
}

// ---------------------------------------------------------------------------
// setAction — data update
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, SetActionUpdatesData) {
    model.setAction(3, "Copy", "keystroke");
    EXPECT_EQ(model.actionNameForButton(3), QStringLiteral("Copy"));
}

TEST_F(ButtonModelTest, SetActionUpdatesActionType) {
    model.setAction(3, "Copy", "keystroke");
    EXPECT_EQ(model.actionTypeForButton(3), QStringLiteral("keystroke"));
}

// ---------------------------------------------------------------------------
// loadFromProfile — signals
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, LoadFromProfileEmitsDataChanged) {
    QSignalSpy spy(&model, &ButtonModel::dataChanged);
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({ QStringLiteral("Action%1").arg(i), QStringLiteral("keystroke") });
    model.loadFromProfile(buttons);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(ButtonModelTest, LoadFromProfileDoesNotEmitUserActionChanged) {
    QSignalSpy spy(&model, &ButtonModel::userActionChanged);
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({ QStringLiteral("Action%1").arg(i), QStringLiteral("keystroke") });
    model.loadFromProfile(buttons);
    EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// loadFromProfile — data update
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, LoadFromProfileUpdatesData) {
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({ QStringLiteral("NewAction%1").arg(i), QStringLiteral("type%1").arg(i) });
    model.loadFromProfile(buttons);
    // loadFromProfile maps by index position (button at index 0 has buttonId 0)
    EXPECT_EQ(model.actionNameForButton(0), QStringLiteral("NewAction0"));
    EXPECT_EQ(model.actionNameForButton(7), QStringLiteral("NewAction7"));
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, LookupNonexistentButton) {
    EXPECT_TRUE(model.actionNameForButton(99).isEmpty());
}

TEST_F(ButtonModelTest, LookupNonexistentButtonType) {
    EXPECT_TRUE(model.actionTypeForButton(99).isEmpty());
}

// ---------------------------------------------------------------------------
// data() roles
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, DataRoleButtonId) {
    QModelIndex idx = model.index(0);
    EXPECT_EQ(model.data(idx, ButtonModel::ButtonIdRole).toInt(), 0);
}

TEST_F(ButtonModelTest, DataRoleButtonName) {
    QModelIndex idx = model.index(0);
    // Button 0 is "Left click" per the default constructor
    EXPECT_FALSE(model.data(idx, ButtonModel::ButtonNameRole).toString().isEmpty());
}

TEST_F(ButtonModelTest, DataRoleActionName) {
    QModelIndex idx = model.index(0);
    EXPECT_FALSE(model.data(idx, ButtonModel::ActionNameRole).toString().isEmpty());
}

TEST_F(ButtonModelTest, DataRoleActionType) {
    QModelIndex idx = model.index(0);
    EXPECT_FALSE(model.data(idx, ButtonModel::ActionTypeRole).toString().isEmpty());
}

// ---------------------------------------------------------------------------
// Invalid index
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, InvalidIndexReturnsInvalidVariant) {
    QModelIndex badIdx = model.index(99);
    EXPECT_FALSE(model.data(badIdx, ButtonModel::ButtonIdRole).isValid());
    EXPECT_FALSE(model.data(badIdx, ButtonModel::ActionNameRole).isValid());
}

// ---------------------------------------------------------------------------
// loadFromProfile with fewer entries than model size
// ---------------------------------------------------------------------------

TEST_F(ButtonModelTest, LoadFewerThanModelSizeButton0Changes) {
    // Record original value for button 1 before loading
    QString original1 = model.actionNameForButton(1);

    QList<QPair<QString, QString>> buttons;
    buttons.append({ QStringLiteral("OnlyOne"), QStringLiteral("custom") });
    model.loadFromProfile(buttons);

    EXPECT_EQ(model.actionNameForButton(0), QStringLiteral("OnlyOne"));
    // Button 1 should be unchanged
    EXPECT_EQ(model.actionNameForButton(1), original1);
}
