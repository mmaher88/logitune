#include "services/DeviceCommandHandlerFixture.h"

using namespace logitune;
using namespace logitune::test;

TEST_F(DeviceCommandHandlerFixture, NullSessionNoOpDoesNotEmit) {
    QSignalSpy spy(m_commands.get(), &DeviceCommandHandler::userChangedSomething);
    m_commands->requestDpi(1600);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceCommandHandlerFixture, RequestDpiCallsSetDPI) {
    attachMockSession();
    m_commands->requestDpi(1600);
    EXPECT_EQ(m_session->currentDPI(), 1600);
}

TEST_F(DeviceCommandHandlerFixture, RequestDpiEmitsUserChangedExactlyOnce) {
    attachMockSession();
    QSignalSpy spy(m_commands.get(), &DeviceCommandHandler::userChangedSomething);
    m_commands->requestDpi(1600);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceCommandHandlerFixture, RequestSmartShiftEmitsUserChanged) {
    // DeviceSession::setSmartShift bails early without a SmartShift dispatcher
    // (populated by enumerateAndSetup, not available in unit tests), so we
    // can't inspect the session state here. We still verify the service
    // reached the session and emitted userChangedSomething.
    attachMockSession();
    QSignalSpy spy(m_commands.get(), &DeviceCommandHandler::userChangedSomething);
    m_commands->requestSmartShift(true, 200);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceCommandHandlerFixture, RequestScrollConfigEmitsUserChanged) {
    // Same story as SmartShift — setScrollConfig needs the HiResWheel feature
    // which requires enumeration. Verify the service plumbing only.
    attachMockSession();
    QSignalSpy spy(m_commands.get(), &DeviceCommandHandler::userChangedSomething);
    m_commands->requestScrollConfig(true, false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceCommandHandlerFixture, RequestThumbWheelModeForwardsString) {
    attachMockSession();
    m_commands->requestThumbWheelMode(QStringLiteral("zoom"));
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("zoom"));
}

TEST_F(DeviceCommandHandlerFixture, RequestThumbWheelInvertForwardsBool) {
    attachMockSession();
    m_commands->requestThumbWheelInvert(true);
    EXPECT_TRUE(m_session->thumbWheelInvert());
}

TEST_F(DeviceCommandHandlerFixture, RequestThumbWheelModePreservesInvert) {
    // The service reads the current invert from the session and passes it
    // alongside the new mode, so toggling mode shouldn't clobber invert.
    attachMockSession();
    m_commands->requestThumbWheelInvert(true);
    m_commands->requestThumbWheelMode(QStringLiteral("zoom"));
    EXPECT_TRUE(m_session->thumbWheelInvert());
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("zoom"));
}
