#include <gtest/gtest.h>
#include <QSignalSpy>
#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "hidpp/HidrawDevice.h"
#include "hidpp/HidppTypes.h"
#include "interfaces/IDevice.h"
#include "mocks/MockDevice.h"

using namespace logitune;
using namespace logitune::hidpp;

class DeviceSessionTest : public ::testing::Test {
protected:
    DeviceRegistry registry;

    std::unique_ptr<DeviceSession> makeSession(
        const QString &connType = "Bluetooth",
        uint8_t deviceIndex = 0xFF)
    {
        auto device = std::make_unique<HidrawDevice>("/dev/null");
        return std::make_unique<DeviceSession>(
            std::move(device), deviceIndex, connType, &registry);
    }
};

TEST_F(DeviceSessionTest, DeviceIdFormat) {
    auto session = makeSession();
    QString id = session->deviceId();
    // Before enumeration: vid=0, pid=0, serial=""
    // Format: "VVVV-PPPP-SERIAL"
    EXPECT_TRUE(id.startsWith("0000-0000")) << id.toStdString();
}

TEST_F(DeviceSessionTest, InitialStateDefaults) {
    auto session = makeSession("Bolt");
    EXPECT_FALSE(session->isConnected());
    EXPECT_EQ(session->connectionType(), "Bolt");
    EXPECT_EQ(session->batteryLevel(), 0);
    EXPECT_FALSE(session->batteryCharging());
    EXPECT_EQ(session->currentDPI(), 0);
    EXPECT_EQ(session->minDPI(), 200);
    EXPECT_EQ(session->maxDPI(), 8000);
    EXPECT_EQ(session->dpiStep(), 50);
    EXPECT_TRUE(session->smartShiftEnabled() == false);
    EXPECT_EQ(session->smartShiftThreshold(), 0);
    EXPECT_FALSE(session->scrollHiRes());
    EXPECT_FALSE(session->scrollInvert());
    EXPECT_TRUE(session->scrollRatchet());
    EXPECT_EQ(session->thumbWheelMode(), "scroll");
    EXPECT_FALSE(session->thumbWheelInvert());
    EXPECT_EQ(session->currentHost(), -1);
    EXPECT_EQ(session->hostCount(), 0);
    EXPECT_EQ(session->deviceIndex(), 0xFF);
}

TEST_F(DeviceSessionTest, ConnectionTypePreserved) {
    auto bolt = makeSession("Bolt");
    EXPECT_EQ(bolt->connectionType(), "Bolt");

    auto bt = makeSession("Bluetooth");
    EXPECT_EQ(bt->connectionType(), "Bluetooth");
}

TEST_F(DeviceSessionTest, DeviceIndexPreserved) {
    auto session = makeSession("Bolt", 0x03);
    EXPECT_EQ(session->deviceIndex(), 0x03);
}

TEST_F(DeviceSessionTest, ResponseWithSoftwareIdIsRouted) {
    auto session = makeSession();

    Report response;
    response.reportId     = 0x11;
    response.deviceIndex  = 0xFF;
    response.featureIndex = 0x05;
    response.functionId   = 0x00;
    response.softwareId   = 0x0A;
    response.paramLength  = 2;

    QSignalSpy thumbSpy(session.get(), &DeviceSession::thumbWheelRotation);
    session->handleNotification(response);

    EXPECT_EQ(thumbSpy.count(), 0)
        << "Response (softwareId != 0) must not trigger thumbWheelRotation";
}

TEST_F(DeviceSessionTest, NotificationWithSoftwareId0Passes) {
    auto session = makeSession();

    Report notification;
    notification.reportId     = 0x11;
    notification.deviceIndex  = 0xFF;
    notification.featureIndex = 0xFE;
    notification.functionId   = 0x00;
    notification.softwareId   = 0x00;
    notification.paramLength  = 2;

    // No crash — notification passes the softwareId filter
    session->handleNotification(notification);
}

TEST_F(DeviceSessionTest, DisconnectCleanupResetsState) {
    auto session = makeSession("Bluetooth");

    QSignalSpy spy(session.get(), &DeviceSession::disconnected);
    session->disconnectCleanup();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(session->isConnected());
    EXPECT_EQ(session->batteryLevel(), 0);
    EXPECT_EQ(session->deviceName(), QString());
    EXPECT_EQ(session->deviceSerial(), QString());
}

TEST_F(DeviceSessionTest, SetDPISkipsWhenNotConnected) {
    auto session = makeSession();
    session->setDPI(2000);
    EXPECT_EQ(session->currentDPI(), 0);
}

TEST_F(DeviceSessionTest, SetDPIEmitsCurrentDPIChangedWhenConnected) {
    auto session = makeSession();
    session->setConnectedForTest(true);
    QSignalSpy spy(session.get(), &DeviceSession::currentDPIChanged);
    session->setDPI(2000);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceSessionTest, SetDPISkipsEmitWhenNotConnected) {
    auto session = makeSession();
    // Default: m_connected = false
    QSignalSpy spy(session.get(), &DeviceSession::currentDPIChanged);
    session->setDPI(2000);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceSessionTest, CycleDpiSkipsWhenNotConnected) {
    auto session = makeSession();
    session->cycleDpi();
    EXPECT_EQ(session->currentDPI(), 0);
}

TEST_F(DeviceSessionTest, SetSmartShiftSkipsWhenNotConnected) {
    auto session = makeSession();
    QSignalSpy spy(session.get(), &DeviceSession::smartShiftChanged);
    session->setSmartShift(true, 100);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceSessionTest, SetScrollConfigSkipsWhenNotConnected) {
    auto session = makeSession();
    QSignalSpy spy(session.get(), &DeviceSession::scrollConfigChanged);
    session->setScrollConfig(true, true);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceSessionTest, DivertButtonSkipsWhenNotConnected) {
    auto session = makeSession();
    session->divertButton(0x0050, true);
    // No crash, no assertion — just a no-op
}

TEST_F(DeviceSessionTest, FlushCommandProcessorWhenEmpty) {
    auto session = makeSession();
    session->flushCommandProcessor();
    // No crash — safe on null command processor
}

TEST_F(DeviceSessionTest, TouchResponseTime) {
    auto session = makeSession();
    session->touchResponseTime();
    // No crash — updates internal timestamp
}

TEST_F(DeviceSessionTest, ThumbWheelModeDefault) {
    auto session = makeSession();
    EXPECT_EQ(session->thumbWheelMode(), "scroll");
    EXPECT_FALSE(session->thumbWheelInvert());
    EXPECT_EQ(session->thumbWheelDefaultDirection(), 1);
}

TEST_F(DeviceSessionTest, SetThumbWheelModeUpdatesState) {
    auto session = makeSession();
    QSignalSpy spy(session.get(), &DeviceSession::thumbWheelModeChanged);

    session->setThumbWheelMode("volume", true);

    EXPECT_EQ(session->thumbWheelMode(), "volume");
    EXPECT_TRUE(session->thumbWheelInvert());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceSessionTest, HostPairingOutOfRange) {
    auto session = makeSession();
    EXPECT_FALSE(session->isHostPaired(-1));
    EXPECT_FALSE(session->isHostPaired(0));
    EXPECT_FALSE(session->isHostPaired(10));
}

TEST_F(DeviceSessionTest, DescriptorNullBeforeEnumerate) {
    auto session = makeSession();
    EXPECT_EQ(session->descriptor(), nullptr);
}

TEST_F(DeviceSessionTest, AccessorsReturnNullables) {
    auto session = makeSession();
    EXPECT_EQ(session->features(), nullptr);
    // transport is created in constructor
    EXPECT_NE(session->transport(), nullptr);
    EXPECT_NE(session->device(), nullptr);
}

// ---------------------------------------------------------------------------
// Descriptor resolution
// ---------------------------------------------------------------------------

// On a receiver the hidraw node reports the receiver's PID (Bolt 0xC548),
// so the device is identified by the WPID read from the receiver's pairing
// register. The WPID alone is enough, even with no usable name.
TEST_F(DeviceSessionTest, ReceiverDeviceResolvesByWpid) {
    const auto *dev = DeviceSession::resolveDescriptor(
        &registry, kPidBoltReceiver, 0xB038, QString());
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Anywhere 3S for Business"));
}

TEST_F(DeviceSessionTest, MxMaster3sForBusinessOnBoltResolvesByWpid) {
    const auto *dev = DeviceSession::resolveDescriptor(
        &registry, kPidBoltReceiver, 0xB035, QStringLiteral("MX Master 3S For Business"));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3S For Business"));
}

TEST_F(DeviceSessionTest, MxMaster3OnUnifyingResolvesByWpid) {
    const auto *dev = DeviceSession::resolveDescriptor(
        &registry, kPidUnifyReceiver, 0x4082, QString());
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3"));
}

// The receiver's own PID says nothing about which device is paired to it.
TEST_F(DeviceSessionTest, ReceiverPidNeverSelectsADescriptor) {
    EXPECT_EQ(DeviceSession::resolveDescriptor(
                  &registry, kPidBoltReceiver, std::nullopt, QString()), nullptr);
    EXPECT_EQ(DeviceSession::resolveDescriptor(
                  &registry, kPidUnifyReceiver, std::nullopt, QString()), nullptr);
}

// If the pairing register can't be read, the reported name is all we have.
TEST_F(DeviceSessionTest, ReceiverWithoutWpidFallsBackToName) {
    const auto *dev = DeviceSession::resolveDescriptor(
        &registry, kPidBoltReceiver, std::nullopt, QStringLiteral("MX Master 3S"));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3S"));
}

// Over Bluetooth the hidraw PID is the device's own.
TEST_F(DeviceSessionTest, BluetoothDeviceResolvesByHidrawPid) {
    const auto *dev = DeviceSession::resolveDescriptor(
        &registry, 0xB034, std::nullopt, QString());
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3S"));
}

// MX Vertical and MX Vertical for Business share PID 0xB020; the reported
// name picks between them, on Bluetooth and on a receiver alike.
TEST_F(DeviceSessionTest, SharedPidResolvedByName) {
    const auto *business = DeviceSession::resolveDescriptor(
        &registry, 0xB020, std::nullopt, QStringLiteral("MX Vertical for Business"));
    ASSERT_NE(business, nullptr);
    EXPECT_EQ(business->deviceName(), QStringLiteral("MX Vertical for Business"));

    const auto *retail = DeviceSession::resolveDescriptor(
        &registry, 0xB020, std::nullopt, QStringLiteral("MX Vertical"));
    ASSERT_NE(retail, nullptr);
    EXPECT_EQ(retail->deviceName(), QStringLiteral("MX Vertical"));

    const auto *onBolt = DeviceSession::resolveDescriptor(
        &registry, kPidBoltReceiver, 0xB020, QStringLiteral("MX Vertical for Business"));
    ASSERT_NE(onBolt, nullptr);
    EXPECT_EQ(onBolt->deviceName(), QStringLiteral("MX Vertical for Business"));
}

// ---------------------------------------------------------------------------
// Variants: MX Master 3 and MX Master 3 for Mac share every PID (issue #106);
// the extended model ID from DeviceInfo (0x0003) tells them apart.
// ---------------------------------------------------------------------------

TEST_F(DeviceSessionTest, MacVariantRenamesMxMaster3) {
    auto session = makeSession();
    session->applyDescriptor(registry.findByPid(0xb023), 3, QStringLiteral("MX Master 3"));
    EXPECT_EQ(session->deviceName(), QStringLiteral("MX Master 3 for Mac"));
    ASSERT_TRUE(session->extendedModelId().has_value());
    EXPECT_EQ(*session->extendedModelId(), 3);
}

TEST_F(DeviceSessionTest, RetailExtendedModelKeepsDescriptorName) {
    auto session = makeSession();
    session->applyDescriptor(registry.findByPid(0xb023), 1, QStringLiteral("MX Master 3"));
    EXPECT_EQ(session->deviceName(), QStringLiteral("MX Master 3"));
}

TEST_F(DeviceSessionTest, NoExtendedModelKeepsDescriptorName) {
    auto session = makeSession();
    session->applyDescriptor(registry.findByPid(0xb023), std::nullopt, QStringLiteral("whatever"));
    EXPECT_EQ(session->deviceName(), QStringLiteral("MX Master 3"));
    EXPECT_FALSE(session->extendedModelId().has_value());
}

TEST_F(DeviceSessionTest, UnknownDeviceKeepsReportedName) {
    auto session = makeSession();
    session->applyDescriptor(nullptr, 3, QStringLiteral("Some Logitech Mouse"));
    EXPECT_EQ(session->deviceName(), QStringLiteral("Some Logitech Mouse"));
    EXPECT_EQ(session->descriptor(), nullptr);
}

// A variant's images replace the descriptor's view by view; views it leaves
// out, and its name when it has none, come from the descriptor.
TEST_F(DeviceSessionTest, VariantImagesOverrideDescriptorPerView) {
    logitune::test::MockDevice dev;
    dev.m_deviceName = QStringLiteral("Mock Mouse");
    dev.m_frontImagePath = QStringLiteral("/base/front.png");
    dev.m_sideImagePath = QStringLiteral("/base/side.png");
    dev.m_backImagePath = QStringLiteral("/base/back.png");
    DeviceVariant grey;
    grey.extendedModelId = 7;
    grey.frontImagePath = QStringLiteral("/grey/front.png");
    dev.m_variants = { grey };

    auto session = makeSession();
    session->applyDescriptor(&dev, 7, QString());
    EXPECT_EQ(session->deviceName(), QStringLiteral("Mock Mouse"));
    EXPECT_EQ(session->frontImagePath(), QStringLiteral("/grey/front.png"));
    EXPECT_EQ(session->sideImagePath(), QStringLiteral("/base/side.png"));
    EXPECT_EQ(session->backImagePath(), QStringLiteral("/base/back.png"));

    session->applyDescriptor(&dev, 1, QString());
    EXPECT_EQ(session->frontImagePath(), QStringLiteral("/base/front.png"));
}
