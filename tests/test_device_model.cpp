#include <gtest/gtest.h>
#include <QSignalSpy>

#include "DeviceModel.h"
#include "helpers/TestFixtures.h"

using logitune::DeviceModel;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class DeviceModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();
    }

    DeviceModel model;  // no DeviceManager set
};

// ---------------------------------------------------------------------------
// Default state (no DeviceManager)
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, BatteryStatusTextDefault) {
    EXPECT_EQ(model.batteryStatusText(), QStringLiteral("Battery: 0%"));
}

// ---------------------------------------------------------------------------
// setDisplayValues
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, SetDisplayValuesEmitsSettingsReloaded) {
    QSignalSpy spy(&model, &DeviceModel::settingsReloaded);
    model.setDisplayValues(2000, true, 30, true, false, QStringLiteral("scroll"));
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, DisplayValuesOverrideDefaults) {
    model.setDisplayValues(2000, false, 45, true, true, QStringLiteral("zoom"));
    EXPECT_EQ(model.currentDPI(), 2000);
    EXPECT_EQ(model.smartShiftEnabled(), false);
    EXPECT_EQ(model.smartShiftThreshold(), 45);
    EXPECT_EQ(model.scrollHiRes(), true);
    EXPECT_EQ(model.scrollInvert(), true);
    EXPECT_EQ(model.thumbWheelMode(), QStringLiteral("zoom"));
}

// ---------------------------------------------------------------------------
// Request-only setters
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, SetDPIEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::dpiChangeRequested);
    model.setDPI(3200);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toInt(), 3200);
}

TEST_F(DeviceModelTest, SetSmartShiftEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::smartShiftChangeRequested);
    model.setSmartShift(true, 64);
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toBool(), true);
    EXPECT_EQ(args.at(1).toInt(), 64);
}

TEST_F(DeviceModelTest, SetScrollConfigEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::scrollConfigChangeRequested);
    model.setScrollConfig(true, false);
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toBool(), true);
    EXPECT_EQ(args.at(1).toBool(), false);
}

TEST_F(DeviceModelTest, SetThumbWheelModeEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::thumbWheelModeChangeRequested);
    model.setThumbWheelMode(QStringLiteral("zoom"));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toString(), QStringLiteral("zoom"));
}

// ---------------------------------------------------------------------------
// setGestureAction — emits both signals
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, SetGestureActionEmitsUserGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::userGestureChanged);
    model.setGestureAction(QStringLiteral("left"), QStringLiteral("keystroke"), QStringLiteral("Ctrl+Z"));
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), QStringLiteral("left"));
    EXPECT_EQ(args.at(1).toString(), QStringLiteral("keystroke"));
    EXPECT_EQ(args.at(2).toString(), QStringLiteral("Ctrl+Z"));
}

TEST_F(DeviceModelTest, SetGestureActionEmitsGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::gestureChanged);
    model.setGestureAction(QStringLiteral("right"), QStringLiteral("none"), QString());
    ASSERT_EQ(spy.count(), 1);
}

// ---------------------------------------------------------------------------
// loadGesturesFromProfile — emits gestureChanged but NOT userGestureChanged
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, LoadGesturesEmitsGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::gestureChanged);
    QMap<QString, QPair<QString, QString>> gestures;
    gestures[QStringLiteral("up")]   = qMakePair(QStringLiteral("keystroke"), QStringLiteral("Ctrl+C"));
    gestures[QStringLiteral("down")] = qMakePair(QStringLiteral("none"), QString());
    model.loadGesturesFromProfile(gestures);
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, LoadGesturesDoesNotEmitUserGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::userGestureChanged);
    QMap<QString, QPair<QString, QString>> gestures;
    gestures[QStringLiteral("up")] = qMakePair(QStringLiteral("keystroke"), QStringLiteral("Ctrl+V"));
    model.loadGesturesFromProfile(gestures);
    EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// gestureActionName / gestureKeystroke lookups
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, GestureLookup) {
    QMap<QString, QPair<QString, QString>> gestures;
    gestures[QStringLiteral("left")] = qMakePair(QStringLiteral("keystroke"), QStringLiteral("Alt+Left"));
    model.loadGesturesFromProfile(gestures);

    EXPECT_EQ(model.gestureActionName(QStringLiteral("left")), QStringLiteral("keystroke"));
    EXPECT_EQ(model.gestureKeystroke(QStringLiteral("left")), QStringLiteral("Alt+Left"));
}

TEST_F(DeviceModelTest, GestureLookupMissReturnsEmpty) {
    EXPECT_TRUE(model.gestureActionName(QStringLiteral("unknown")).isEmpty());
    EXPECT_TRUE(model.gestureKeystroke(QStringLiteral("unknown")).isEmpty());
}

// ---------------------------------------------------------------------------
// setActiveProfileName
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, SetActiveProfileNameEmitsSignal) {
    QSignalSpy spy(&model, &DeviceModel::activeProfileNameChanged);
    model.setActiveProfileName(QStringLiteral("Firefox"));
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, SetActiveProfileNameNoOpOnSame) {
    model.setActiveProfileName(QStringLiteral("Firefox"));
    QSignalSpy spy(&model, &DeviceModel::activeProfileNameChanged);
    model.setActiveProfileName(QStringLiteral("Firefox"));
    EXPECT_EQ(spy.count(), 0);
}
