#include <gtest/gtest.h>
#include <QSignalSpy>

#include "ProfileModel.h"
#include "helpers/TestFixtures.h"

using logitune::ProfileModel;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ProfileModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();
    }

    ProfileModel model;
};

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, InitialRowCount) {
    EXPECT_EQ(model.rowCount(), 1);
}

TEST_F(ProfileModelTest, DefaultEntry) {
    QModelIndex idx = model.index(0);
    EXPECT_EQ(model.data(idx, ProfileModel::NameRole).toString(), QStringLiteral("Defaults"));
    EXPECT_TRUE(model.data(idx, ProfileModel::IsActiveRole).toBool());
}

// ---------------------------------------------------------------------------
// addProfile
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, AddProfile) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    EXPECT_EQ(model.rowCount(), 2);
}

TEST_F(ProfileModelTest, AddProfileEmitsProfileAdded) {
    QSignalSpy spy(&model, &ProfileModel::profileAdded);
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), QStringLiteral("firefox"));
    EXPECT_EQ(args.at(1).toString(), QStringLiteral("Firefox"));
}

TEST_F(ProfileModelTest, AddProfileAutoSelectsTab) {
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toString(), QStringLiteral("Firefox"));
}

TEST_F(ProfileModelTest, AddDuplicateWmClassIgnored) {
    model.addProfile(QStringLiteral("Firefox"), QStringLiteral("Firefox"));
    // Same wmClass, different case — should be ignored
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox2"));
    EXPECT_EQ(model.rowCount(), 2);
}

// ---------------------------------------------------------------------------
// restoreProfile
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, RestoreProfileSilent) {
    QSignalSpy spyAdded(&model, &ProfileModel::profileAdded);
    QSignalSpy spySwitched(&model, &ProfileModel::profileSwitched);
    model.restoreProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    EXPECT_EQ(spyAdded.count(), 0);
    EXPECT_EQ(spySwitched.count(), 0);
    EXPECT_EQ(model.rowCount(), 2);
}

// ---------------------------------------------------------------------------
// removeProfile
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, RemoveProfileCantRemoveDefault) {
    model.removeProfile(0);
    EXPECT_EQ(model.rowCount(), 1);
}

TEST_F(ProfileModelTest, RemoveProfile) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    ASSERT_EQ(model.rowCount(), 2);
    model.removeProfile(1);
    EXPECT_EQ(model.rowCount(), 1);
}

TEST_F(ProfileModelTest, RemoveDisplayedTabFallsBack) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    // After addProfile, displayIndex is 1 (auto-selected)
    ASSERT_EQ(model.displayIndex(), 1);
    model.removeProfile(1);
    EXPECT_EQ(model.displayIndex(), 0);
}

// ---------------------------------------------------------------------------
// selectTab
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, SelectTabEmitsProfileSwitched) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    // Go back to index 0 first so we can switch to 1 again
    model.selectTab(0);
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(1);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toString(), QStringLiteral("Firefox"));
}

TEST_F(ProfileModelTest, SelectTabIndex0EmitsDefault) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    // displayIndex is 1 after addProfile
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toString(), QStringLiteral("default"));
}

TEST_F(ProfileModelTest, SelectTabNoOpWhenAlreadySelected) {
    // displayIndex starts at 0
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(0);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ProfileModelTest, SelectTabOutOfBoundsIgnored) {
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(99);
    EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// setHwActiveByProfileName
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, SetHwActiveByProfileName) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    // hwActiveIndex starts at 0; switch to Firefox
    model.setHwActiveByProfileName(QStringLiteral("Firefox"));
    QModelIndex idx = model.index(1);
    EXPECT_TRUE(model.data(idx, ProfileModel::IsHwActiveRole).toBool());
    // index 0 should no longer be hw-active
    QModelIndex idx0 = model.index(0);
    EXPECT_FALSE(model.data(idx0, ProfileModel::IsHwActiveRole).toBool());
}

TEST_F(ProfileModelTest, SetHwActiveDoesNotEmitProfileSwitched) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.setHwActiveByProfileName(QStringLiteral("Firefox"));
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ProfileModelTest, SetHwActiveUnknownDefaultsToZero) {
    model.addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    // Move hw to index 1 first
    model.setHwActiveByProfileName(QStringLiteral("Firefox"));
    ASSERT_TRUE(model.data(model.index(1), ProfileModel::IsHwActiveRole).toBool());

    // Set unknown name — should fall back to index 0
    model.setHwActiveByProfileName(QStringLiteral("UnknownApp"));
    EXPECT_TRUE(model.data(model.index(0), ProfileModel::IsHwActiveRole).toBool());
}

// ---------------------------------------------------------------------------
// Index shift after removal
// ---------------------------------------------------------------------------

TEST_F(ProfileModelTest, RemoveShiftsDisplayIndex) {
    // Add 3 profiles: indices 1, 2, 3
    model.addProfile(QStringLiteral("app1"), QStringLiteral("App1"));
    model.addProfile(QStringLiteral("app2"), QStringLiteral("App2"));
    model.addProfile(QStringLiteral("app3"), QStringLiteral("App3"));
    ASSERT_EQ(model.rowCount(), 4);

    // Select index 3 (App3)
    model.selectTab(3);
    ASSERT_EQ(model.displayIndex(), 3);

    // Remove index 1 (App1); displayIndex should shift down to 2
    model.removeProfile(1);
    EXPECT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.displayIndex(), 2);
}
