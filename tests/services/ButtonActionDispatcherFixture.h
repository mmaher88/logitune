#pragma once
#include <gtest/gtest.h>
#include <memory>

#include "ActionExecutor.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "ProfileEngine.h"
#include "models/DeviceModel.h"
#include "services/ButtonActionDispatcher.h"
#include "services/ActiveDeviceResolver.h"

#include "helpers/TestFixtures.h"
#include "hidpp/HidrawDevice.h"
#include "mocks/MockDesktop.h"
#include "mocks/MockDevice.h"
#include "mocks/MockInjector.h"

namespace logitune::test {

/// Fixture for ButtonActionDispatcher tests. Constructs the full dependency
/// graph (ProfileEngine, DeviceModel, ActiveDeviceResolver, MockInjector,
/// ActionExecutor) and registers a MockDevice + DeviceSession via
/// applySimulation so the dispatcher has an active session+descriptor to
/// dispatch against.
class ButtonActionDispatcherFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();

        m_deviceModel   = std::make_unique<DeviceModel>();
        m_selection     = std::make_unique<ActiveDeviceResolver>(m_deviceModel.get());
        m_profileEngine = std::make_unique<ProfileEngine>();
        m_injector      = std::make_unique<MockInjector>();
        m_executor      = std::make_unique<ActionExecutor>(m_injector.get());
        m_desktop       = std::make_unique<MockDesktop>();
        m_dispatcher    = std::make_unique<ButtonActionDispatcher>(
            m_profileEngine.get(), m_executor.get(), m_selection.get(),
            m_desktop.get());
    }

    void TearDown() override {
        m_dispatcher.reset();
        m_desktop.reset();
        m_executor.reset();
        m_injector.reset();
        delete m_physical;
        m_physical = nullptr;
        m_session  = nullptr;
        m_profileEngine.reset();
        m_selection.reset();
        m_deviceModel.reset();
    }

    /// Attach a MockDevice-backed PhysicalDevice/DeviceSession so the
    /// dispatcher sees an active session. Uses the same applySimulation
    /// entry point as --simulate-all so we avoid touching private session
    /// internals. Also points the dispatcher's m_currentDevice at the
    /// MockDevice descriptor.
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

        m_dispatcher->onCurrentDeviceChanged(&m_device);

        // Register a profile dir so the engine returns a real cached profile
        // when the dispatcher looks up the active hardware profile.
        ASSERT_TRUE(m_tmpDir.isValid());
        const QString serial = QStringLiteral("mock-serial");
        m_profileEngine->registerDevice(serial, m_tmpDir.path());
        m_profileEngine->setHardwareProfile(serial, QStringLiteral("default"));
        m_profileEngine->setDisplayProfile(serial, QStringLiteral("default"));
        // Force the cached profile to exist so the dispatcher's
        // cachedProfile lookup returns a stable reference.
        (void)m_profileEngine->cachedProfile(serial, QStringLiteral("default"));
    }

    Profile &hwProfile() {
        return m_profileEngine->cachedProfile(QStringLiteral("mock-serial"),
                                              QStringLiteral("default"));
    }

    // --- Introspection helpers (access dispatcher internals via friend) ---

    bool gestureActive() const {
        const auto &state = m_dispatcher->m_state;
        auto it = state.find(QStringLiteral("mock-serial"));
        return it != state.end() ? it->gestureActive : false;
    }
    int gestureAccumX() const {
        const auto &state = m_dispatcher->m_state;
        auto it = state.find(QStringLiteral("mock-serial"));
        return it != state.end() ? it->gestureAccumX : 0;
    }
    int gestureAccumY() const {
        const auto &state = m_dispatcher->m_state;
        auto it = state.find(QStringLiteral("mock-serial"));
        return it != state.end() ? it->gestureAccumY : 0;
    }
    int thumbAccum() const {
        const auto &state = m_dispatcher->m_state;
        auto it = state.find(QStringLiteral("mock-serial"));
        return it != state.end() ? it->thumbAccum : 0;
    }
    bool hasStateFor(const QString &serial) const {
        return m_dispatcher->m_state.contains(serial);
    }

    std::unique_ptr<DeviceModel>             m_deviceModel;
    std::unique_ptr<ActiveDeviceResolver>         m_selection;
    std::unique_ptr<ProfileEngine>           m_profileEngine;
    std::unique_ptr<MockInjector>            m_injector;
    std::unique_ptr<ActionExecutor>          m_executor;
    std::unique_ptr<MockDesktop>             m_desktop;
    std::unique_ptr<ButtonActionDispatcher>  m_dispatcher;

    MockDevice       m_device;
    DeviceSession   *m_session  = nullptr;
    PhysicalDevice  *m_physical = nullptr;
    QTemporaryDir    m_tmpDir;
};

} // namespace logitune::test
