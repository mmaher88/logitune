#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <functional>
#include <map>
#include <unistd.h>

#include "DeviceManager.h"
#include "hidpp/HidrawDevice.h"
#include "helpers/TestFixtures.h"

namespace logitune::test {

// A receiver whose slot pings all fail (no mouse linked yet, e.g. right
// after a KVM switch-back) is kept open until a mouse links up on it.
// Pipes stand in for the receivers' hidraw nodes: writing a report is a
// mouse linking up, closing the write end is the receiver going away.
class IdleReceiverFixture : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }

    void TearDown() override
    {
        for (auto &[node, fd] : m_writeEnds)
            ::close(fd);
    }

    void addIdleReceiver(const QString &node)
    {
        int fds[2];
        ASSERT_EQ(::pipe(fds), 0);
        auto device = std::make_unique<hidpp::HidrawDevice>(node);
        device->m_fd = fds[0];
        m_dm.watchIdleReceiver(node, std::move(device));
        m_writeEnds[node] = fds[1];
    }

    void sendReport(const QString &node, const std::vector<uint8_t> &bytes)
    {
        ASSERT_EQ(::write(m_writeEnds.at(node), bytes.data(), bytes.size()),
                  static_cast<ssize_t>(bytes.size()));
    }

    void unplug(const QString &node)
    {
        ::close(m_writeEnds.at(node));
        m_writeEnds.erase(node);
    }

    void udevRemove(const QString &node)
    {
        m_dm.onUdevEvent(QStringLiteral("remove"), node);
    }

    bool watching(const QString &node) const { return m_dm.m_idleReceivers.count(node) > 0; }
    size_t watchedCount() const { return m_dm.m_idleReceivers.size(); }

    // Runs the event loop until pred holds, so socket notifiers fire.
    bool waitUntil(const std::function<bool()> &pred, int timeoutMs = 1000)
    {
        QElapsedTimer t;
        t.start();
        while (!pred() && t.elapsed() < timeoutMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return pred();
    }

    DeviceManager m_dm{nullptr};
    std::map<QString, int> m_writeEnds;
};

static const QString kReceiverA = QStringLiteral("/dev/fake-hidraw15");
static const QString kReceiverB = QStringLiteral("/dev/fake-hidraw12");

// HID++ 1.0 device-connection notification: slot 2, link established,
// WPID b035 (MX Master 3S For Business).
static const std::vector<uint8_t> kLinkUpSlot2 = {0x10, 0x02, 0x41, 0x10, 0x02, 0x35, 0xb0};

TEST_F(IdleReceiverFixture, EveryIdleReceiverIsWatched)
{
    // Both receivers come back with no mouse linked yet: neither watch may
    // replace the other.
    addIdleReceiver(kReceiverA);
    addIdleReceiver(kReceiverB);

    EXPECT_TRUE(watching(kReceiverA));
    EXPECT_TRUE(watching(kReceiverB));
    EXPECT_EQ(watchedCount(), 2u);
}

TEST_F(IdleReceiverFixture, MouseLinkingUpReprobesOnlyItsReceiver)
{
    addIdleReceiver(kReceiverA);
    addIdleReceiver(kReceiverB);
    ASSERT_TRUE(watching(kReceiverA) && watching(kReceiverB));

    sendReport(kReceiverA, kLinkUpSlot2);

    // A hands over to a re-probe; B keeps waiting for its own mouse.
    EXPECT_TRUE(waitUntil([&] { return !watching(kReceiverA); }));
    EXPECT_TRUE(watching(kReceiverB));
}

TEST_F(IdleReceiverFixture, ReceiverThatGoesAwayStopsBeingWatched)
{
    addIdleReceiver(kReceiverA);
    addIdleReceiver(kReceiverB);
    ASSERT_TRUE(watching(kReceiverA) && watching(kReceiverB));

    unplug(kReceiverA);

    EXPECT_TRUE(waitUntil([&] { return !watching(kReceiverA); }));
    EXPECT_TRUE(watching(kReceiverB));
}

TEST_F(IdleReceiverFixture, UdevRemoveStopsWatchingThatReceiver)
{
    addIdleReceiver(kReceiverA);
    addIdleReceiver(kReceiverB);
    ASSERT_TRUE(watching(kReceiverA) && watching(kReceiverB));

    udevRemove(kReceiverA);

    EXPECT_FALSE(watching(kReceiverA));
    EXPECT_TRUE(watching(kReceiverB));
}

TEST_F(IdleReceiverFixture, RewatchingANodeReplacesItsOldWatch)
{
    addIdleReceiver(kReceiverA);
    addIdleReceiver(kReceiverA);

    EXPECT_EQ(watchedCount(), 1u);
}

} // namespace logitune::test
