#include <gtest/gtest.h>
#include <QSignalSpy>
#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "hidpp/HidrawDevice.h"
#include "hidpp/HidppTypes.h"

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

TEST_F(DeviceSessionTest, FlushCommandQueueWhenEmpty) {
    auto session = makeSession();
    session->flushCommandQueue();
    // No crash — safe on null command queue
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
