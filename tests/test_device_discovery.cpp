#include <gtest/gtest.h>
#include "DeviceManager.h"
#include "DeviceRegistry.h"

using namespace logitune;

TEST(DeviceDiscovery, IdentifyBoltReceiver) {
    EXPECT_TRUE(DeviceManager::isReceiver(0xc548));
    EXPECT_TRUE(DeviceManager::isReceiver(0xc52b));
    EXPECT_FALSE(DeviceManager::isReceiver(0xb034));
    EXPECT_FALSE(DeviceManager::isReceiver(0x0000));
}

TEST(DeviceDiscovery, IdentifyDirectDevice) {
    DeviceRegistry registry;
    EXPECT_NE(registry.findByPid(0xb034), nullptr);
    EXPECT_EQ(registry.findByPid(0xc548), nullptr);
    EXPECT_EQ(registry.findByPid(0x0000), nullptr);
}

TEST(DeviceDiscovery, DeviceIndexForTransport) {
    EXPECT_EQ(DeviceManager::deviceIndexForDirect(), 0xFF);
    EXPECT_EQ(DeviceManager::deviceIndexForReceiver(1), 1);
    EXPECT_EQ(DeviceManager::deviceIndexForReceiver(6), 6);
}
