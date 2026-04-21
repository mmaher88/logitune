#include <gtest/gtest.h>
#include <memory>

#include "mocks/MockDevice.h"
#include "models/ActionModel.h"
#include "models/ActionFilterModel.h"
#include "models/DeviceModel.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "hidpp/HidrawDevice.h"

using namespace logitune;
using namespace logitune::test;

// These tests deliberately avoid ensureApp() so they don't enable
// QStandardPaths test mode before DeviceRegistry.ReloadByPathRefreshesSingleDevice
// runs; that test fiddles with XDG_DATA_HOME, which test mode overrides.
// The proxy + model constructors don't need a running event loop.

namespace {

// Count how many rows in the proxy have a given Name role. 0 = hidden.
int proxyCountByName(ActionFilterModel &proxy, const QString &name) {
    int count = 0;
    for (int i = 0; i < proxy.rowCount(); ++i) {
        const QString rowName = proxy.data(
            proxy.index(i, 0), ActionModel::NameRole).toString();
        if (rowName == name) count++;
    }
    return count;
}

// Build a PhysicalDevice + DeviceSession pair backed by a MockDevice
// with the given FeatureSupport flags. Added to the DeviceModel and
// selected, but NOT driven through AppController::onPhysicalDeviceAdded
// because this test does not exercise the profile engine.
//
// DeviceSession's m_connected / m_deviceName / m_activeDevice are
// private; applySimulation() is the public shim that sets all three in
// one call (same path --simulate-all uses) so the test doesn't need
// friend access.
PhysicalDevice* attachMockDevice(DeviceModel &model,
                                 MockDevice &mock,
                                 const QString &serial = QStringLiteral("mock"))
{
    auto *owner = &model;  // parent the heap objects to the model for cleanup
    auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
    auto *session = new DeviceSession(std::move(mockHidraw), 0xFF, "Bluetooth",
                                       nullptr, owner);
    session->applySimulation(&mock, serial);

    auto *device = new PhysicalDevice(serial, owner);
    device->attachTransport(session);
    model.addPhysicalDevice(device);
    return device;
}

} // namespace

TEST(ActionFilterModel, EmptyDeviceModelShowsFullList) {
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    EXPECT_EQ(proxy.rowCount(), source.rowCount());
}

TEST(ActionFilterModel, FilterHidesUnsupportedActions) {
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mock;
    mock.setupMxControls();
    // setupMxControls() seeds a full-featured MX-family profile; overwrite
    // m_features to model a ratcheted-wheel device without a thumb wheel.
    mock.m_features = FeatureSupport{};
    mock.m_features.adjustableDpi   = true;
    mock.m_features.smartShift      = false;
    mock.m_features.thumbWheel      = false;
    mock.m_features.reprogControls  = true;

    attachMockDevice(dm, mock);
    dm.setSelectedIndex(0);

    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Shift wheel mode")), 0);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Gestures")), 1);
    EXPECT_LT(proxy.rowCount(), source.rowCount());
}

TEST(ActionFilterModel, SelectionChangeInvalidates) {
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mockA;
    mockA.setupMxControls();
    FeatureSupport fa;
    fa.adjustableDpi = true;
    fa.smartShift    = true;
    fa.thumbWheel    = true;
    fa.reprogControls = true;
    mockA.m_features = fa;

    MockDevice mockB;
    mockB.setupMxControls();
    FeatureSupport fb;  // all flags default false
    mockB.m_features = fb;

    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("mock-A"));
    auto *devB = attachMockDevice(dm, mockB, QStringLiteral("mock-B"));

    const int idxA = dm.devices().indexOf(devA);
    const int idxB = dm.devices().indexOf(devB);
    ASSERT_GE(idxA, 0);
    ASSERT_GE(idxB, 0);

    dm.setSelectedIndex(idxA);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 1);

    dm.setSelectedIndex(idxB);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("DPI cycle")), 0);
}

TEST(ActionFilterModel, UnrestrictedActionsAlwaysVisible) {
    ActionModel source;
    DeviceModel dm;
    ActionFilterModel proxy(&dm);
    proxy.setSourceModel(&source);

    MockDevice mock;
    mock.setupMxControls();
    FeatureSupport f;  // every capability flag false
    mock.m_features = f;

    attachMockDevice(dm, mock);
    dm.setSelectedIndex(0);

    // Even on a device with zero capabilities, keystroke/app-launch/media
    // /default/none actions remain visible.
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Keyboard shortcut")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Copy")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Paste")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Do nothing")), 1);
    EXPECT_EQ(proxyCountByName(proxy, QStringLiteral("Media controls")), 1);
}
