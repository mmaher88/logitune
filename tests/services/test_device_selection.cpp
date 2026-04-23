#include "services/DeviceSelectionFixture.h"
#include <QSignalSpy>

using namespace logitune;
using namespace logitune::test;

TEST_F(DeviceSelectionFixture, NoDevicesReturnsNulls) {
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
    EXPECT_EQ(m_selection->activeSession(), nullptr);
    EXPECT_TRUE(m_selection->activeSerial().isEmpty());
}

TEST_F(DeviceSelectionFixture, OutOfRangeIndexReturnsNulls) {
    m_deviceModel->setSelectedIndex(99);
    EXPECT_EQ(m_selection->activeDevice(), nullptr);
}

TEST_F(DeviceSelectionFixture, SelectionChangedEmitsOnSlot) {
    QSignalSpy spy(m_selection.get(), &DeviceSelection::selectionChanged);
    m_selection->onSelectionIndexChanged();
    EXPECT_EQ(spy.count(), 1);
}
