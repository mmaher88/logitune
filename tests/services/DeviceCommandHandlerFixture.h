#pragma once
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <memory>
#include "services/DeviceCommandHandler.h"
#include "services/ActiveDeviceResolver.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "models/DeviceModel.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"
#include "hidpp/HidrawDevice.h"

namespace logitune::test {

class DeviceCommandHandlerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        m_deviceModel = std::make_unique<DeviceModel>();
        m_selection   = std::make_unique<ActiveDeviceResolver>(m_deviceModel.get());
        m_commands    = std::make_unique<DeviceCommandHandler>(m_selection.get());
    }

    void TearDown() override {
        m_commands.reset();
        m_selection.reset();
        // PhysicalDevice owns the DeviceSession via Qt parent.
        delete m_physical;
        m_physical = nullptr;
        m_session  = nullptr;
        m_deviceModel.reset();
    }

    /// Attach a mock session so the commands have a target. Uses the public
    /// applySimulation() entry point (same mechanism the --simulate-all CLI
    /// flag drives) to avoid needing private access to m_connected /
    /// m_activeDevice.
    void attachMockSession() {
        m_device.setupMxControls();
        auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_physical = new PhysicalDevice(QStringLiteral("mock-serial"));
        m_session = new DeviceSession(std::move(hidraw), 0xFF, "Bluetooth",
                                      nullptr, m_physical);
        m_session->applySimulation(&m_device, QStringLiteral("mock-serial"));
        m_physical->attachTransport(m_session);
        m_deviceModel->addPhysicalDevice(m_physical);
        m_deviceModel->setSelectedIndex(0);
    }

    std::unique_ptr<DeviceModel>     m_deviceModel;
    std::unique_ptr<ActiveDeviceResolver> m_selection;
    std::unique_ptr<DeviceCommandHandler>  m_commands;
    MockDevice       m_device;
    DeviceSession   *m_session  = nullptr;
    PhysicalDevice  *m_physical = nullptr;
};

} // namespace logitune::test
