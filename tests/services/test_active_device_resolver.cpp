#include "services/ActiveDeviceResolverFixture.h"
#include <QSignalSpy>

using namespace logitune;
using namespace logitune::test;

TEST_F(ActiveDeviceResolverFixture, NoDevicesReturnsNulls) {
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
    EXPECT_EQ(m_selection->activeSession(), nullptr);
    EXPECT_TRUE(m_selection->activeSerial().isEmpty());
}

TEST_F(ActiveDeviceResolverFixture, OutOfRangeIndexReturnsNulls) {
    m_deviceModel->setSelectedIndex(99);
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
}

TEST_F(ActiveDeviceResolverFixture, SelectionChangedEmitsOnSlot) {
    QSignalSpy spy(m_selection.get(), &ActiveDeviceResolver::selectionChanged);
    m_selection->onSelectionIndexChanged();
    EXPECT_EQ(spy.count(), 1);
}
