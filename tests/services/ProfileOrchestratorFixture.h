#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>

#include "ActionExecutor.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "ProfileEngine.h"
#include "models/ActionModel.h"
#include "models/ButtonModel.h"
#include "models/DeviceModel.h"
#include "models/ProfileModel.h"
#include "services/DeviceSelection.h"
#include "services/ProfileOrchestrator.h"

#include "helpers/TestFixtures.h"
#include "hidpp/HidrawDevice.h"
#include "mocks/MockDesktop.h"
#include "mocks/MockDevice.h"
#include "mocks/MockInjector.h"

namespace logitune::test {

/// Fixture for ProfileOrchestrator tests. Clones the AppRootFixture
/// shape (mock desktop/injector/session, seeded default profile in a temp
/// dir, real ProfileEngine + models) but constructs a ProfileOrchestrator
/// directly instead of an AppRoot. Wires only the signals the
/// orchestrator cares about (from the engine and desktop); the cross-
/// service bridges to ButtonActionDispatcher / DeviceCommandHandler are not
/// wired here because the orchestrator is the subject under test.
class ProfileOrchestratorFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        clearTestAppConfig();
        ASSERT_TRUE(m_tmpDir.isValid());

        m_profilesDir = m_tmpDir.path() + QStringLiteral("/profiles");
        QDir().mkpath(m_profilesDir);

        Profile seed;
        seed.name                = QStringLiteral("Default");
        seed.dpi                 = 1000;
        seed.smartShiftEnabled   = true;
        seed.smartShiftThreshold = 128;
        seed.smoothScrolling     = false;
        seed.scrollDirection     = QStringLiteral("standard");
        seed.hiResScroll         = true;
        seed.thumbWheelMode      = QStringLiteral("scroll");
        ProfileEngine::saveProfile(m_profilesDir + QStringLiteral("/default.conf"), seed);

        m_deviceModel   = std::make_unique<DeviceModel>();
        m_buttonModel   = std::make_unique<ButtonModel>();
        m_actionModel   = std::make_unique<ActionModel>();
        m_profileModel  = std::make_unique<ProfileModel>();
        m_profileEngine = std::make_unique<ProfileEngine>();
        m_selection     = std::make_unique<DeviceSelection>(m_deviceModel.get());
        m_injector      = std::make_unique<MockInjector>();
        m_executor      = std::make_unique<ActionExecutor>(m_injector.get());
        m_desktop       = std::make_unique<MockDesktop>();

        m_orchestrator = std::make_unique<ProfileOrchestrator>(
            m_profileEngine.get(), m_executor.get(), m_selection.get(),
            m_deviceModel.get(), m_buttonModel.get(), m_actionModel.get(),
            m_profileModel.get(), m_desktop.get());

        // Wire the DeviceModel -> DeviceSelection -> selectionChanged chain
        // and the engine's display-profile signal. These are normally wired
        // by AppRoot::wireSignals(); tests need the same plumbing to
        // exercise the orchestrator's slots the way production code does.
        QObject::connect(m_deviceModel.get(), &DeviceModel::selectedChanged,
                         m_selection.get(), &DeviceSelection::onSelectionIndexChanged);
        QObject::connect(m_profileEngine.get(), &ProfileEngine::deviceDisplayProfileChanged,
                         m_orchestrator.get(), &ProfileOrchestrator::onDisplayProfileChanged);
        QObject::connect(m_desktop.get(), &IDesktopIntegration::activeWindowChanged,
                         m_orchestrator.get(), &ProfileOrchestrator::onWindowFocusChanged);
    }

    void TearDown() override {
        m_orchestrator.reset();
        m_executor.reset();
        m_injector.reset();
        m_desktop.reset();
        delete m_physical;
        m_physical = nullptr;
        m_session  = nullptr;
        m_selection.reset();
        m_profileEngine.reset();
        m_profileModel.reset();
        m_actionModel.reset();
        m_buttonModel.reset();
        m_deviceModel.reset();
    }

    /// Attach a MockDevice-backed PhysicalDevice/DeviceSession so the
    /// orchestrator has an active session + current IDevice. Points the
    /// orchestrator's m_currentDevice at the mock and registers a profile
    /// dir for the serial so cachedProfile lookups resolve.
    void attachMockSession() {
        const QString kSerial = QStringLiteral("mock-serial");

        m_device.setupMxControls();
        auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_physical = new PhysicalDevice(kSerial);
        m_session = new DeviceSession(std::move(hidraw), 0xFF, "Bluetooth",
                                      nullptr, m_physical);
        m_session->applySimulation(&m_device, kSerial);
        m_physical->attachTransport(m_session);
        m_deviceModel->addPhysicalDevice(m_physical);
        m_deviceModel->setSelectedIndex(0);

        m_orchestrator->onCurrentDeviceChanged(&m_device);

        m_profileEngine->registerDevice(kSerial, m_profilesDir);
        m_profileEngine->setHardwareProfile(kSerial, QStringLiteral("default"));
        m_profileEngine->setDisplayProfile(kSerial, QStringLiteral("default"));
        // Materialize the cache entry so tests can mutate it in place.
        (void)m_profileEngine->cachedProfile(kSerial, QStringLiteral("default"));
    }

    Profile &cached(const QString &name = QStringLiteral("default")) {
        return m_profileEngine->cachedProfile(QStringLiteral("mock-serial"), name);
    }

    std::unique_ptr<DeviceModel>         m_deviceModel;
    std::unique_ptr<ButtonModel>         m_buttonModel;
    std::unique_ptr<ActionModel>         m_actionModel;
    std::unique_ptr<ProfileModel>        m_profileModel;
    std::unique_ptr<ProfileEngine>       m_profileEngine;
    std::unique_ptr<DeviceSelection>     m_selection;
    std::unique_ptr<MockInjector>        m_injector;
    std::unique_ptr<ActionExecutor>      m_executor;
    std::unique_ptr<MockDesktop>         m_desktop;
    std::unique_ptr<ProfileOrchestrator> m_orchestrator;

    MockDevice       m_device;
    DeviceSession   *m_session  = nullptr;
    PhysicalDevice  *m_physical = nullptr;
    QString          m_profilesDir;
    QTemporaryDir    m_tmpDir;
};

} // namespace logitune::test
