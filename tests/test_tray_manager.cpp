#include <gtest/gtest.h>
#include <memory>
#include <QApplication>
#include <QSignalSpy>
#include <QAction>
#include <QMenu>

#include "TrayManager.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "hidpp/HidrawDevice.h"
#include "mocks/MockDevice.h"
#include "models/DeviceModel.h"

using namespace logitune;
using namespace logitune::test;

namespace {

// Count actions in the menu excluding separators.
int nonSeparatorCount(QMenu *menu) {
    int n = 0;
    for (auto *a : menu->actions())
        if (!a->isSeparator()) ++n;
    return n;
}

// Find the first action matching a predicate.
template <typename Pred>
QAction* findAction(QMenu *menu, Pred pred) {
    for (auto *a : menu->actions())
        if (pred(a)) return a;
    return nullptr;
}

// Build a PhysicalDevice + DeviceSession pair backed by a MockDevice
// with a staged battery level / charging state, attach to the DeviceModel.
// Returns the PhysicalDevice for later removal.
PhysicalDevice* attachMockDevice(DeviceModel &model,
                                 MockDevice &mock,
                                 const QString &name,
                                 int batteryLevel,
                                 bool charging,
                                 const QString &serial)
{
    auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
    auto *session = new DeviceSession(std::move(mockHidraw), 0xFF,
                                      QStringLiteral("Bluetooth"),
                                      nullptr, &model);
    // applySimulation wires up the descriptor and flips m_connected to true,
    // but it forces m_batteryLevel to 85. Override afterwards via the test
    // hooks so each test can stage its own battery state and device name.
    session->applySimulation(&mock, serial);
    session->setDeviceNameForTest(name);
    session->setBatteryForTest(batteryLevel, charging);

    auto *device = new PhysicalDevice(serial, &model);
    device->attachTransport(session);
    model.addPhysicalDevice(device);
    return device;
}

} // namespace

TEST(TrayManager, ZeroDevicesOnlyShowsShowAndQuit) {
    DeviceModel dm;
    TrayManager tray(&dm);

    EXPECT_EQ(nonSeparatorCount(tray.menu()), 2);
    EXPECT_EQ(tray.showAction()->text(), QStringLiteral("Show Logitune"));
    EXPECT_EQ(tray.quitAction()->text(), QStringLiteral("Quit"));
}

TEST(TrayManager, ShowActionEmitsShowWindowRequested) {
    DeviceModel dm;
    TrayManager tray(&dm);

    QSignalSpy spy(&tray, &TrayManager::showWindowRequested);
    tray.showAction()->trigger();
    EXPECT_EQ(spy.count(), 1);
}

TEST(TrayManager, QuitActionExists) {
    DeviceModel dm;
    TrayManager tray(&dm);
    EXPECT_EQ(tray.quitAction()->text(), QStringLiteral("Quit"));
}

TEST(TrayManager, OneDeviceAddsHeaderAndBatterySection) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mock;
    mock.setupMxControls();
    attachMockDevice(dm, mock, QStringLiteral("Mock Master"),
                     80, false, QStringLiteral("mock-A"));

    // Show + Mock Master + Battery: 80% + Quit = 4 non-separator actions
    EXPECT_EQ(nonSeparatorCount(tray.menu()), 4);

    auto *header = findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Mock Master");
    });
    ASSERT_NE(header, nullptr);
    EXPECT_FALSE(header->isEnabled());

    auto *battery = findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 80%");
    });
    ASSERT_NE(battery, nullptr);
    EXPECT_FALSE(battery->isEnabled());
}

TEST(TrayManager, SecondDeviceAppendsSection) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                     80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    // Show + Device A + 80% + Device B + 45% + Quit = 6
    EXPECT_EQ(nonSeparatorCount(tray.menu()), 6);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 45%"); }), nullptr);
}

TEST(TrayManager, DeviceRemovedStripsSection) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                                  80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));
    ASSERT_EQ(nonSeparatorCount(tray.menu()), 6);

    // Simulate disconnect by flipping the session's connected flag and
    // emitting stateChanged; DeviceModel's handler calls removeRow which
    // fires countChanged which triggers rebuildEntries.
    auto *sessionA = qobject_cast<DeviceSession *>(devA->primary());
    ASSERT_NE(sessionA, nullptr);
    sessionA->setConnectedForTest(false);
    emit devA->stateChanged();

    EXPECT_EQ(nonSeparatorCount(tray.menu()), 4);
    EXPECT_EQ(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 45%"); }), nullptr);
}

TEST(TrayManager, BatteryChangeUpdatesMatchingEntryOnly) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                                  80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    // Mutate device A's battery, fire its stateChanged
    auto *sessionA = qobject_cast<DeviceSession *>(devA->primary());
    ASSERT_NE(sessionA, nullptr);
    sessionA->setBatteryForTest(12, false);
    emit devA->stateChanged();

    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 12%"); }), nullptr);
    EXPECT_EQ(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("    Battery: 45%"); }), nullptr);
}

TEST(TrayManager, ChargingSuffixAppearsWhenCharging) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mock;
    mock.setupMxControls();
    attachMockDevice(dm, mock, QStringLiteral("Charging Mouse"),
                     60, true, QStringLiteral("mock-C"));

    auto *battery = findAction(tray.menu(), [](QAction *a) {
        return a->text().startsWith(QStringLiteral("    Battery: 60%"));
    });
    ASSERT_NE(battery, nullptr);
    EXPECT_TRUE(battery->text().contains(QStringLiteral("\u26A1")));
}

TEST(TrayManager, TooltipReflectsAllDevices) {
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                     80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    const QString expected = QStringLiteral(
        "Logitune\nDevice A: 80% \u2022 Device B: 45%");
    EXPECT_EQ(tray.trayIcon()->toolTip(), expected);
}
