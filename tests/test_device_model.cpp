#include <gtest/gtest.h>
#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>

#include "ButtonAction.h"
#include "DeviceModel.h"
#include "DeviceRegistry.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "hidpp/HidrawDevice.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"

using logitune::ButtonAction;
using logitune::DeviceModel;
using logitune::GestureEntry;
using logitune::RadialMenuConfig;

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
    model.setGestureAction(QStringLiteral("left"),
                           QStringLiteral("Undo"),
                           QStringLiteral("keystroke"),
                           QStringLiteral("Ctrl+Z"));
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), QStringLiteral("left"));
    EXPECT_EQ(args.at(1).toString(), QStringLiteral("Undo"));
    EXPECT_EQ(args.at(2).toString(), QStringLiteral("keystroke"));
    EXPECT_EQ(args.at(3).toString(), QStringLiteral("Ctrl+Z"));
}

TEST_F(DeviceModelTest, SetGestureActionEmitsGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::gestureChanged);
    model.setGestureAction(QStringLiteral("right"),
                           QString(),
                           QStringLiteral("none"),
                           QString());
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, SetGestureActionPresetRoundTrips) {
    model.setGestureAction(QStringLiteral("down"),
                           QStringLiteral("Show desktop"),
                           QStringLiteral("preset"),
                           QStringLiteral("show-desktop"));

    EXPECT_EQ(model.gestureActionName(QStringLiteral("down")),
              QStringLiteral("Show desktop"));
    EXPECT_EQ(model.gestureActionType(QStringLiteral("down")),
              QStringLiteral("preset"));
    EXPECT_EQ(model.gestureActionPayload(QStringLiteral("down")),
              QStringLiteral("show-desktop"));

    const auto ba = model.gestureAction(QStringLiteral("down"));
    EXPECT_EQ(ba.type, ButtonAction::PresetRef);
    EXPECT_EQ(ba.payload, QStringLiteral("show-desktop"));
}

TEST_F(DeviceModelTest, SetGestureActionNoneClearsBinding) {
    // Seed a binding, then clear it via the "none" sentinel and verify the
    // accessors all report it as cleared.
    model.setGestureAction(QStringLiteral("up"),
                           QStringLiteral("Undo"),
                           QStringLiteral("keystroke"),
                           QStringLiteral("Ctrl+Z"));
    model.setGestureAction(QStringLiteral("up"),
                           QString(),
                           QStringLiteral("none"),
                           QString());
    EXPECT_TRUE(model.gestureActionName(QStringLiteral("up")).isEmpty());
    EXPECT_TRUE(model.gestureActionType(QStringLiteral("up")).isEmpty());
    EXPECT_TRUE(model.gestureActionPayload(QStringLiteral("up")).isEmpty());
    EXPECT_EQ(model.gestureAction(QStringLiteral("up")).type, ButtonAction::Default);
}

// ---------------------------------------------------------------------------
// loadGesturesFromProfile — emits gestureChanged but NOT userGestureChanged
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, LoadGesturesEmitsGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::gestureChanged);
    QMap<QString, GestureEntry> gestures;
    gestures[QStringLiteral("up")]   = {QStringLiteral("Copy"),
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Ctrl+C")}};
    gestures[QStringLiteral("down")] = {QString(),
        ButtonAction{ButtonAction::Default, {}}};
    model.loadGesturesFromProfile(gestures);
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, LoadGesturesDoesNotEmitUserGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::userGestureChanged);
    QMap<QString, GestureEntry> gestures;
    gestures[QStringLiteral("up")] = {QStringLiteral("Paste"),
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Ctrl+V")}};
    model.loadGesturesFromProfile(gestures);
    EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// gestureActionName / gestureActionPayload lookups
// ---------------------------------------------------------------------------

TEST_F(DeviceModelTest, GestureLookup) {
    QMap<QString, GestureEntry> gestures;
    gestures[QStringLiteral("left")] = {QStringLiteral("Back"),
        ButtonAction{ButtonAction::Keystroke, QStringLiteral("Alt+Left")}};
    model.loadGesturesFromProfile(gestures);

    EXPECT_EQ(model.gestureActionName(QStringLiteral("left")), QStringLiteral("Back"));
    EXPECT_EQ(model.gestureActionType(QStringLiteral("left")), QStringLiteral("keystroke"));
    EXPECT_EQ(model.gestureActionPayload(QStringLiteral("left")), QStringLiteral("Alt+Left"));
}

TEST_F(DeviceModelTest, GestureLookupMissReturnsEmpty) {
    EXPECT_TRUE(model.gestureActionName(QStringLiteral("unknown")).isEmpty());
    EXPECT_TRUE(model.gestureActionPayload(QStringLiteral("unknown")).isEmpty());
    EXPECT_TRUE(model.gestureActionType(QStringLiteral("unknown")).isEmpty());
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

TEST_F(DeviceModelTest, LoadRadialMenuUpdatesEditorWithoutUserSignal) {
    QSignalSpy changedSpy(&model, &DeviceModel::radialMenuChanged);
    QSignalSpy userSpy(&model, &DeviceModel::userRadialMenuChanged);
    RadialMenuConfig config;
    config.items = {
        {QStringLiteral("Back"), {ButtonAction::Keystroke, QStringLiteral("Alt+Left")}},
        {QStringLiteral("Mute"), {ButtonAction::Keystroke, QStringLiteral("Mute")}},
    };

    model.loadRadialMenu(config);

    EXPECT_EQ(changedSpy.count(), 1);
    EXPECT_EQ(userSpy.count(), 0);
    const QVariantList items = model.radialItems();
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items.at(0).toMap().value(QStringLiteral("action")).toString(),
              QStringLiteral("keystroke:Alt+Left"));
}

TEST_F(DeviceModelTest, SaveRadialMenuAcceptsValidEntriesAndEmitsUserSignal) {
    QSignalSpy userSpy(&model, &DeviceModel::userRadialMenuChanged);
    const QVariantList items{
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Copy")},
                    {QStringLiteral("action"), QStringLiteral("keystroke:Ctrl+C")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Terminal")},
                    {QStringLiteral("action"), QStringLiteral("app-launch:konsole")}},
    };

    EXPECT_TRUE(model.saveRadialMenu(items));
    EXPECT_EQ(userSpy.count(), 1);
    EXPECT_EQ(model.radialMenuConfig().items.size(), 2);
}

TEST_F(DeviceModelTest, SaveRadialMenuRejectsRecursiveEntryWithoutMutation) {
    const RadialMenuConfig before = model.radialMenuConfig();
    const QVariantList items{
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Copy")},
                    {QStringLiteral("action"), QStringLiteral("keystroke:Ctrl+C")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Nested")},
                    {QStringLiteral("action"), QStringLiteral("radial-menu")}},
    };

    EXPECT_FALSE(model.saveRadialMenu(items));
    EXPECT_EQ(model.radialMenuConfig().items, before.items);
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

// The card shows the variant the mouse reports (issue #106): its name, and
// its images where it has them, with the descriptor filling in the rest.
TEST_F(DeviceModelWithDeviceTest, NameAndImagesFollowTheDeviceVariant) {
    logitune::test::MockDevice dev;
    dev.m_deviceName = QStringLiteral("Mock Mouse");
    dev.m_frontImagePath = QStringLiteral("/base/front.png");
    dev.m_sideImagePath = QStringLiteral("/base/side.png");
    dev.m_backImagePath = QStringLiteral("/base/back.png");
    logitune::DeviceVariant graphite;
    graphite.extendedModelId = 7;
    graphite.name = QStringLiteral("Mock Mouse Graphite");
    graphite.frontImagePath = QStringLiteral("/graphite/front.png");
    graphite.backImagePath = QStringLiteral("/graphite/back.png");
    dev.m_variants = { graphite };

    m_session->applyDescriptor(&dev, 7, QString());

    EXPECT_EQ(model.deviceName(), QStringLiteral("Mock Mouse Graphite"));
    EXPECT_EQ(model.frontImage(), QStringLiteral("file:///graphite/front.png"));
    EXPECT_EQ(model.sideImage(), QStringLiteral("file:///base/side.png"));
    EXPECT_EQ(model.backImage(), QStringLiteral("file:///graphite/back.png"));
    EXPECT_EQ(model.data(model.index(0), DeviceModel::FrontImageRole).toString(),
              QStringLiteral("file:///graphite/front.png"));

    m_session->applyDescriptor(nullptr, std::nullopt, QString());
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

// ---------------------------------------------------------------------------
// Selection must follow the selected device, not its row number. Rows come
// and go as mice disconnect and reconnect; an index that is not adjusted
// silently retargets every UI edit at a different mouse.
// ---------------------------------------------------------------------------

class DeviceModelSelectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        logitune::test::ensureApp();
        logitune::test::clearTestAppConfig();  // no saved device order
        // saveDeviceOrder() writes into AppConfigLocation without creating
        // it; in the app the profile folders always create it first.
        QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));

        for (const char *serial : {"serial-A", "serial-B", "serial-C"}) {
            auto hidraw = std::make_unique<logitune::hidpp::HidrawDevice>("/dev/null");
            auto session = std::make_unique<logitune::DeviceSession>(
                std::move(hidraw), 0xFF, QStringLiteral("Bluetooth"), &m_registry);
            session->setConnectedForTest(true);
            auto pd = std::make_unique<logitune::PhysicalDevice>(QString::fromLatin1(serial));
            pd->attachTransport(session.get());
            model.addPhysicalDevice(pd.get());
            m_sessions.push_back(std::move(session));
            m_devices.push_back(std::move(pd));
        }
    }

    logitune::PhysicalDevice *dev(int i) { return m_devices[static_cast<size_t>(i)].get(); }

    logitune::PhysicalDevice *selected() const {
        const int idx = model.selectedIndex();
        const auto &rows = model.devices();
        return (idx >= 0 && idx < rows.size()) ? rows[idx] : nullptr;
    }

    void disconnect(int i) {
        auto *s = m_sessions[static_cast<size_t>(i)].get();
        s->setConnectedForTest(false);
        emit s->disconnected();
    }

    void reconnect(int i) {
        auto *s = m_sessions[static_cast<size_t>(i)].get();
        s->setConnectedForTest(true);
        emit s->setupComplete();
    }

    logitune::DeviceRegistry m_registry;
    // Sessions must outlive the PhysicalDevices that point at them.
    std::vector<std::unique_ptr<logitune::DeviceSession>> m_sessions;
    std::vector<std::unique_ptr<logitune::PhysicalDevice>> m_devices;
    DeviceModel model;
};

TEST_F(DeviceModelSelectionTest, RemovingRowBeforeSelectionKeepsSameDeviceSelected) {
    model.setSelectedIndex(1);  // B
    ASSERT_EQ(selected(), dev(1));

    QSignalSpy spy(&model, &DeviceModel::selectedChanged);
    disconnect(0);  // A goes offline: rows are now [B, C]

    EXPECT_EQ(selected(), dev(1));
    EXPECT_EQ(model.selectedIndex(), 0);
    EXPECT_GE(spy.count(), 1);  // selectedIndex changed, QML must hear it
}

TEST_F(DeviceModelSelectionTest, InsertingRowBeforeSelectionKeepsSameDeviceSelected) {
    // Persist the order [A, B, C] so a reconnecting A goes back to the front.
    model.moveDevice(0, 1);
    model.moveDevice(1, 0);

    model.setSelectedIndex(1);  // B
    disconnect(0);              // [B, C]
    ASSERT_EQ(selected(), dev(1));

    QSignalSpy spy(&model, &DeviceModel::selectedChanged);
    reconnect(0);               // A re-inserted at the front: [A, B, C]

    EXPECT_EQ(selected(), dev(1));
    EXPECT_EQ(model.selectedIndex(), 1);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(DeviceModelSelectionTest, RemovingRowAfterSelectionLeavesSelectionAlone) {
    model.setSelectedIndex(0);  // A
    QSignalSpy spy(&model, &DeviceModel::selectedChanged);

    disconnect(2);  // C goes offline

    EXPECT_EQ(selected(), dev(0));
    EXPECT_EQ(model.selectedIndex(), 0);
    EXPECT_EQ(spy.count(), 0);
}
