#include <gtest/gtest.h>
#include <QSignalSpy>

#include "DeviceModel.h"
#include "DeviceRegistry.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "hidpp/HidrawDevice.h"
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

TEST_F(DeviceModelTest, AppIndicatorInstallCommandNonEmpty) {
    const QString cmd = model.appIndicatorInstallCommand();
    EXPECT_FALSE(cmd.isEmpty());
    EXPECT_TRUE(cmd.contains(QStringLiteral("gnome-shell-extension-appindicator")))
        << "command: " << cmd.toStdString();
}

// ---------------------------------------------------------------------------
// Fixture with a fake-connected PhysicalDevice, so addPhysicalDevice's
// per-property relay hooks fire. The session is never driven through HID++
// enumeration; we just flip setConnectedForTest(true) so PhysicalDevice's
// isConnected() returns true and DeviceModel inserts the row immediately.
// ---------------------------------------------------------------------------

class DeviceModelWithDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();

        auto hidraw = std::make_unique<logitune::hidpp::HidrawDevice>("/dev/null");
        m_session = std::make_unique<logitune::DeviceSession>(
            std::move(hidraw), 0xFF, QStringLiteral("Bluetooth"), &m_registry);
        m_session->setConnectedForTest(true);
        m_session->setDeviceNameForTest(QStringLiteral("Test Device"));

        m_pd = std::make_unique<logitune::PhysicalDevice>(QStringLiteral("test-serial"));
        m_pd->attachTransport(m_session.get());

        model.addPhysicalDevice(m_pd.get());
        model.setSelectedIndex(0);
    }

    logitune::DeviceRegistry m_registry;
    std::unique_ptr<logitune::DeviceSession> m_session;
    std::unique_ptr<logitune::PhysicalDevice> m_pd;
    DeviceModel model;
};

TEST_F(DeviceModelWithDeviceTest, HardwareSmartShiftChangeInvalidatesDisplayCache) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);
    EXPECT_TRUE(model.smartShiftEnabled()) << "precondition: cache is armed";

    emit m_pd->smartShiftChanged(false, 128);

    // Falsifiable: after invalidation the getter falls through to the
    // unenumerated session, which reports smartShiftEnabled() == false.
    // If the handler failed to clear m_hasDisplayValues, the cached true
    // would still be returned and this assertion would fail.
    EXPECT_FALSE(model.smartShiftEnabled()) << "cache invalidated, falls through to live session";

    // Re-arm with distinct values and verify the fresh cache is honored.
    model.setDisplayValues(2000, false, 64, false, true,
                           QStringLiteral("zoom"), true);
    EXPECT_EQ(model.currentDPI(), 2000);
    EXPECT_FALSE(model.smartShiftEnabled());
    EXPECT_EQ(model.smartShiftThreshold(), 64);
    EXPECT_FALSE(model.scrollHiRes());
    EXPECT_TRUE(model.scrollInvert());
    EXPECT_EQ(model.thumbWheelMode(), QStringLiteral("zoom"));
    EXPECT_TRUE(model.thumbWheelInvert());
}

TEST_F(DeviceModelWithDeviceTest, ScrollConfigHardwareChangeInvalidatesCache) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);
    EXPECT_TRUE(model.scrollHiRes()) << "precondition: scrollHiRes cache is armed true";

    emit m_pd->scrollConfigChanged();

    // Falsifiable: cache was true, session default is false.
    EXPECT_FALSE(model.scrollHiRes()) << "cache invalidated, falls through to live session";

    model.setDisplayValues(2500, true, 200, false, true,
                           QStringLiteral("scroll"), false);
    EXPECT_FALSE(model.scrollHiRes());
    EXPECT_TRUE(model.scrollInvert());
}

TEST_F(DeviceModelWithDeviceTest, DPIHardwareChangeInvalidatesCache) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);
    EXPECT_EQ(model.currentDPI(), 1000) << "precondition: cache is armed";

    emit m_pd->currentDPIChanged();

    // Falsifiable: cache was 1000, session default is 0.
    EXPECT_EQ(model.currentDPI(), 0) << "cache invalidated, falls through to live session";

    model.setDisplayValues(3000, true, 128, true, false,
                           QStringLiteral("scroll"), false);
    EXPECT_EQ(model.currentDPI(), 3000);
}

TEST_F(DeviceModelWithDeviceTest, ThumbWheelHardwareChangeInvalidatesCache) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("zoom"), true);
    EXPECT_EQ(model.thumbWheelMode(), QStringLiteral("zoom"))
        << "precondition: cache is armed";

    emit m_pd->thumbWheelModeChanged();

    // Falsifiable: cache was "zoom", session default is "scroll".
    EXPECT_EQ(model.thumbWheelMode(), QStringLiteral("scroll"))
        << "cache invalidated, falls through to live session";

    // Re-arm and verify.
    model.setDisplayValues(1500, true, 128, true, false,
                           QStringLiteral("volume"), false);
    EXPECT_EQ(model.thumbWheelMode(), QStringLiteral("volume"));
    EXPECT_FALSE(model.thumbWheelInvert());
}

TEST_F(DeviceModelWithDeviceTest, SmartShiftHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);

    QSignalSpy enabledSpy(&model, &DeviceModel::smartShiftEnabledChanged);
    QSignalSpy thresholdSpy(&model, &DeviceModel::smartShiftThresholdChanged);

    emit m_pd->smartShiftChanged(false, 192);

    EXPECT_EQ(enabledSpy.count(), 1);
    EXPECT_EQ(thresholdSpy.count(), 1);
}

TEST_F(DeviceModelWithDeviceTest, ScrollConfigHardwareChangeEmitsOnce) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);

    QSignalSpy spy(&model, &DeviceModel::scrollConfigChanged);
    emit m_pd->scrollConfigChanged();

    // scrollHiRes and scrollInvert both NOTIFY on this signal; expect
    // a single emission, not two.
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelWithDeviceTest, ThumbWheelHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);

    QSignalSpy spy(&model, &DeviceModel::thumbWheelModeChanged);
    emit m_pd->thumbWheelModeChanged();

    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelWithDeviceTest, DPIHardwareChangeEmitsProperty) {
    model.setDisplayValues(1000, true, 128, true, false,
                           QStringLiteral("scroll"), false);

    QSignalSpy spy(&model, &DeviceModel::currentDPIChanged);
    emit m_pd->currentDPIChanged();

    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelWithDeviceTest, SetDisplayValuesEmitsPerPropertySignals) {
    QSignalSpy dpiSpy(&model, &DeviceModel::currentDPIChanged);
    QSignalSpy smartSpy(&model, &DeviceModel::smartShiftEnabledChanged);
    QSignalSpy threshSpy(&model, &DeviceModel::smartShiftThresholdChanged);
    QSignalSpy scrollSpy(&model, &DeviceModel::scrollConfigChanged);
    QSignalSpy thumbSpy(&model, &DeviceModel::thumbWheelModeChanged);

    model.setDisplayValues(1500, false, 200, false, true,
                           QStringLiteral("zoom"), true);

    EXPECT_EQ(dpiSpy.count(), 1);
    EXPECT_EQ(smartSpy.count(), 1);
    EXPECT_EQ(threshSpy.count(), 1);
    EXPECT_EQ(scrollSpy.count(), 1);
    EXPECT_EQ(thumbSpy.count(), 1);
}
