#include <gtest/gtest.h>
#include <QSignalSpy>
#include "DeviceSession.h"
#include "DeviceRegistry.h"
#include "hidpp/HidppTypes.h"
#include "hidpp/HidrawDevice.h"

using namespace logitune;
using namespace logitune::hidpp;

class NotificationFilterTest : public ::testing::Test {
protected:
    DeviceRegistry registry;
};

TEST_F(NotificationFilterTest, ResponseWithSoftwareId1IsDiscarded) {
    auto device = std::make_unique<HidrawDevice>("/dev/null");
    DeviceSession session(std::move(device), 0xFF, "Bluetooth", &registry);

    Report response;
    response.reportId     = 0x11;
    response.deviceIndex  = 0x01;
    response.featureIndex = 0x10;
    response.functionId   = 0x02;
    response.softwareId   = 0x01;
    response.params[0]    = 0x01;
    response.params[1]    = 0x00;
    response.paramLength  = 2;

    QSignalSpy thumbSpy(&session, &DeviceSession::thumbWheelRotation);
    QSignalSpy batterySpy(&session, &DeviceSession::batteryChanged);

    session.handleNotification(response);

    EXPECT_EQ(thumbSpy.count(), 0) << "Response must not trigger thumbWheelRotation";
    EXPECT_EQ(batterySpy.count(), 0) << "Response must not trigger batteryChanged";
}

TEST_F(NotificationFilterTest, NotificationWithSoftwareId0IsNotDiscarded) {
    auto device = std::make_unique<HidrawDevice>("/dev/null");
    DeviceSession session(std::move(device), 0xFF, "Bluetooth", &registry);

    Report notification;
    notification.reportId     = 0x11;
    notification.deviceIndex  = 0x01;
    notification.featureIndex = 0xFF;
    notification.functionId   = 0x00;
    notification.softwareId   = 0x00;
    notification.paramLength  = 2;

    session.handleNotification(notification);
}
