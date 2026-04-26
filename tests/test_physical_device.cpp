#include <gtest/gtest.h>
#include <QSignalSpy>
#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "PhysicalDevice.h"
#include "hidpp/HidrawDevice.h"

using namespace logitune;

class PhysicalDeviceTest : public ::testing::Test {
protected:
    DeviceRegistry registry;

    std::unique_ptr<DeviceSession> makeSession(const QString &connType,
                                                bool connected = true,
                                                const QString &name = {})
    {
        auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        auto s = std::make_unique<DeviceSession>(std::move(hidraw), 0xFF,
                                                 connType, &registry);
        s->setConnectedForTest(connected);
        if (!name.isEmpty())
            s->setDeviceNameForTest(name);
        return s;
    }
};

TEST_F(PhysicalDeviceTest, AttachFirstTransportBecomesPrimary) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto s = makeSession("Bolt", true, "Test");

    QSignalSpy spy(&pd, &PhysicalDevice::stateChanged);
    pd.attachTransport(s.get());

    EXPECT_EQ(pd.primary(), s.get());
    EXPECT_EQ(pd.transportCount(), 1);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(pd.deviceName(), "Test");
    EXPECT_EQ(pd.connectionType(), "Bolt");
    EXPECT_TRUE(pd.isConnected());
}

TEST_F(PhysicalDeviceTest, SecondConnectedTransportBecomesAlternate) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto boltSession = makeSession("Bolt", true);
    auto btSession   = makeSession("Bluetooth", true);

    pd.attachTransport(boltSession.get());
    pd.attachTransport(btSession.get());

    EXPECT_EQ(pd.primary(), boltSession.get());
    EXPECT_EQ(pd.transportCount(), 2);
    auto all = pd.transports();
    EXPECT_TRUE(all.contains(boltSession.get()));
    EXPECT_TRUE(all.contains(btSession.get()));
}

TEST_F(PhysicalDeviceTest, NewTransportPromotedIfPrimaryOffline) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto offline = makeSession("Bolt", false);
    auto fresh   = makeSession("Bluetooth", true);

    pd.attachTransport(offline.get());
    EXPECT_FALSE(pd.isConnected());

    pd.attachTransport(fresh.get());
    EXPECT_EQ(pd.primary(), fresh.get());
    EXPECT_TRUE(pd.isConnected());
    EXPECT_EQ(pd.connectionType(), "Bluetooth");
}

TEST_F(PhysicalDeviceTest, DetachPrimaryPromotesAlternate) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto a = makeSession("Bolt", true, "A");
    auto b = makeSession("Bluetooth", true, "B");

    pd.attachTransport(a.get());
    pd.attachTransport(b.get());
    EXPECT_EQ(pd.primary(), a.get());

    const bool empty = pd.detachTransport(a.get());
    EXPECT_FALSE(empty);
    EXPECT_EQ(pd.primary(), b.get());
    EXPECT_EQ(pd.transportCount(), 1);
    EXPECT_EQ(pd.deviceName(), "B");
}

TEST_F(PhysicalDeviceTest, DetachAlternateKeepsPrimary) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto a = makeSession("Bolt", true, "A");
    auto b = makeSession("Bluetooth", true, "B");

    pd.attachTransport(a.get());
    pd.attachTransport(b.get());

    const bool empty = pd.detachTransport(b.get());
    EXPECT_FALSE(empty);
    EXPECT_EQ(pd.primary(), a.get());
    EXPECT_EQ(pd.transportCount(), 1);
}

TEST_F(PhysicalDeviceTest, DetachLastTransportReturnsTrue) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto s = makeSession("Bolt", true);
    pd.attachTransport(s.get());

    const bool empty = pd.detachTransport(s.get());
    EXPECT_TRUE(empty);
    EXPECT_EQ(pd.primary(), nullptr);
    EXPECT_EQ(pd.transportCount(), 0);
}

TEST_F(PhysicalDeviceTest, AttachingSameTransportTwiceIsNoOp) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto s = makeSession("Bolt", true);

    pd.attachTransport(s.get());
    pd.attachTransport(s.get());  // no-op

    EXPECT_EQ(pd.transportCount(), 1);
}

TEST_F(PhysicalDeviceTest, DetachNullOrUnknownDoesNotCrash) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto attached = makeSession("Bolt", true);
    auto other = makeSession("Bluetooth", true);

    pd.attachTransport(attached.get());
    pd.detachTransport(other.get());   // not attached — no-op
    pd.detachTransport(nullptr);       // null — no-op

    EXPECT_EQ(pd.primary(), attached.get());
    EXPECT_EQ(pd.transportCount(), 1);
}

TEST_F(PhysicalDeviceTest, ForwardsGestureRawXYFromActiveTransport) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto s = makeSession("Bolt", true);
    pd.attachTransport(s.get());

    QSignalSpy spy(&pd, &PhysicalDevice::gestureRawXY);
    emit s->gestureRawXY(3, 7);

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 3);
    EXPECT_EQ(spy.at(0).at(1).toInt(), 7);
}

TEST_F(PhysicalDeviceTest, ForwardsDivertedButtonPressedFromAlternate) {
    // Events from any transport (primary or alternate) should be forwarded.
    PhysicalDevice pd(QStringLiteral("serial-1"));
    auto a = makeSession("Bolt", true);
    auto b = makeSession("Bluetooth", true);
    pd.attachTransport(a.get());
    pd.attachTransport(b.get());

    QSignalSpy spy(&pd, &PhysicalDevice::divertedButtonPressed);
    emit b->divertedButtonPressed(0x00C3, true);  // b is alternate

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toUInt(), 0x00C3u);
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(PhysicalDeviceTest, SerialAccessor) {
    PhysicalDevice pd(QStringLiteral("ABC123"));
    EXPECT_EQ(pd.serial(), "ABC123");
    EXPECT_EQ(pd.deviceSerial(), "ABC123");
}

TEST_F(PhysicalDeviceTest, DisconnectedDelegatorsReturnDefaults) {
    PhysicalDevice pd(QStringLiteral("serial-1"));
    // No transports attached.
    EXPECT_FALSE(pd.isConnected());
    EXPECT_EQ(pd.deviceName(), QString());
    EXPECT_EQ(pd.batteryLevel(), 0);
    EXPECT_FALSE(pd.batteryCharging());
    EXPECT_EQ(pd.descriptor(), nullptr);
}
