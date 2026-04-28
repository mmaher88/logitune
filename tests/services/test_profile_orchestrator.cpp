#include "services/ProfileOrchestratorFixture.h"

using namespace logitune;
using namespace logitune::test;

// --- saveCurrentProfile ----------------------------------------------------

TEST_F(ProfileOrchestratorFixture, SaveCurrentProfilePersistsButtonAssignments) {
    attachMockSession();

    // Set button 3 (Back Button, CID 0x0053) to a DPI cycle via the
    // ButtonModel, then invoke saveCurrentProfile and re-load from disk.
    QList<ButtonAssignment> assignments;
    for (const auto &ctrl : m_device.m_controls) {
        assignments.append({ctrl.defaultName, QStringLiteral("default"),
                            ctrl.controlId});
    }
    m_buttonModel->loadFromProfile(assignments);
    m_buttonModel->setAction(3, QStringLiteral("DPI cycle"),
                              QStringLiteral("dpi-cycle"));

    m_orchestrator->saveCurrentProfile();

    const QString path = m_profilesDir + QStringLiteral("/default.conf");
    Profile reloaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(reloaded.buttons[3].type, ButtonAction::DpiCycle);
}

// --- applyProfileToHardware -----------------------------------------------

TEST_F(ProfileOrchestratorFixture, ApplyProfileToHardwareEmitsProfileApplied) {
    attachMockSession();
    QSignalSpy spy(m_orchestrator.get(), &ProfileOrchestrator::profileApplied);

    Profile p = cached();
    p.dpi = 2400;
    m_orchestrator->applyProfileToHardware(p);

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().at(0).toString(), QStringLiteral("mock-serial"));
    EXPECT_EQ(m_session->currentDPI(), 2400);
}

// --- Window focus ---------------------------------------------------------

TEST_F(ProfileOrchestratorFixture, WindowFocusSwitchesHardwareProfile) {
    attachMockSession();
    const QString kSerial = QStringLiteral("mock-serial");

    m_profileEngine->createProfileForApp(kSerial, QStringLiteral("firefox"),
                                         QStringLiteral("firefox"));
    cached(QStringLiteral("firefox")).dpi = 1800;
    m_profileEngine->saveProfileToDisk(kSerial, QStringLiteral("firefox"));
    m_profileModel->restoreProfile(QStringLiteral("firefox"),
                                    QStringLiteral("firefox"));

    m_desktop->simulateFocus(QStringLiteral("firefox"), QStringLiteral("Mozilla"));

    EXPECT_EQ(m_profileEngine->hardwareProfile(kSerial),
              QStringLiteral("firefox"));
    EXPECT_EQ(m_session->currentDPI(), 1800);
}

TEST_F(ProfileOrchestratorFixture, WindowFocusNoProfileFallsBackToDefault) {
    attachMockSession();
    const QString kSerial = QStringLiteral("mock-serial");

    m_desktop->simulateFocus(QStringLiteral("unknown.app"),
                              QStringLiteral("Unknown"));

    // No app-specific profile => profileForApp returns "default", which
    // equals hardwareProfile so onWindowFocusChanged short-circuits without
    // setting a new hardware profile.
    EXPECT_EQ(m_profileEngine->hardwareProfile(kSerial),
              QStringLiteral("default"));
}

TEST_F(ProfileOrchestratorFixture, WindowFocusSameAppTwiceNoDoubleApply) {
    attachMockSession();
    const QString kSerial = QStringLiteral("mock-serial");

    m_profileEngine->createProfileForApp(kSerial, QStringLiteral("firefox"),
                                         QStringLiteral("firefox"));
    cached(QStringLiteral("firefox")).dpi = 1800;
    m_profileModel->restoreProfile(QStringLiteral("firefox"),
                                    QStringLiteral("firefox"));

    QSignalSpy spy(m_orchestrator.get(), &ProfileOrchestrator::profileApplied);

    m_desktop->simulateFocus(QStringLiteral("firefox"), QStringLiteral("Mozilla"));
    const int firstCount = spy.count();
    m_desktop->simulateFocus(QStringLiteral("firefox"), QStringLiteral("Mozilla 2"));

    // Second focus should short-circuit (profileForApp == hardwareProfile).
    EXPECT_EQ(spy.count(), firstCount);
}

// --- Tab switching --------------------------------------------------------

TEST_F(ProfileOrchestratorFixture, TabSwitchChangesDisplayNotHardware) {
    attachMockSession();
    const QString kSerial = QStringLiteral("mock-serial");

    m_profileEngine->createProfileForApp(kSerial, QStringLiteral("firefox"),
                                         QStringLiteral("firefox"));
    cached(QStringLiteral("firefox")).dpi = 1800;

    m_orchestrator->onTabSwitched(QStringLiteral("firefox"));

    EXPECT_EQ(m_profileEngine->displayProfile(kSerial),
              QStringLiteral("firefox"));
    EXPECT_EQ(m_profileEngine->hardwareProfile(kSerial),
              QStringLiteral("default"));  // unchanged
}

// --- Display profile changed ---------------------------------------------

TEST_F(ProfileOrchestratorFixture, DisplayProfileChangedPushesToModels) {
    attachMockSession();

    Profile p = cached();
    p.name = QStringLiteral("test");
    p.dpi  = 2400;
    p.thumbWheelMode = QStringLiteral("volume");

    m_orchestrator->onDisplayProfileChanged(QStringLiteral("mock-serial"), p);

    EXPECT_EQ(m_deviceModel->currentDPI(), 2400);
    EXPECT_EQ(m_deviceModel->thumbWheelMode(), QStringLiteral("volume"));
    EXPECT_EQ(m_deviceModel->activeProfileName(), QStringLiteral("test"));
}

// --- Setup for new device -------------------------------------------------

TEST_F(ProfileOrchestratorFixture, SetupProfileForDeviceSeedsCache) {
    m_device.setupMxControls();
    auto hidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
    auto *physical = new PhysicalDevice(QStringLiteral("seeded-serial"));
    auto *session = new DeviceSession(std::move(hidraw), 0xFF, "Bluetooth",
                                      nullptr, physical);
    session->applySimulation(&m_device, QStringLiteral("seeded-serial"));
    physical->attachTransport(session);
    m_deviceModel->addPhysicalDevice(physical);
    m_deviceModel->setSelectedIndex(0);

    m_orchestrator->setupProfileForDevice(physical);

    // setupProfileForDevice writes default.conf under AppConfigLocation and
    // sets the hardware/display profile to "default".
    EXPECT_EQ(m_profileEngine->hardwareProfile(QStringLiteral("seeded-serial")),
              QStringLiteral("default"));
    EXPECT_EQ(m_profileEngine->displayProfile(QStringLiteral("seeded-serial")),
              QStringLiteral("default"));

    delete physical;
}

// --- Current device signal ------------------------------------------------

TEST_F(ProfileOrchestratorFixture, OnCurrentDeviceChangedEmitsSignal) {
    QSignalSpy spy(m_orchestrator.get(),
                   &ProfileOrchestrator::currentDeviceChanged);

    m_device.setupMxControls();
    m_orchestrator->onCurrentDeviceChanged(&m_device);

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().at(0).value<const IDevice *>(),
              static_cast<const IDevice *>(&m_device));
}

// --- applyDisplayedChange template ---------------------------------------

TEST_F(ProfileOrchestratorFixture, ApplyDisplayedChangeMutatesCacheAndPushesToUI) {
    attachMockSession();

    bool forwarded = false;
    m_orchestrator->applyDisplayedChange(
        [](Profile &p) { p.dpi = 2000; },
        [&]() { forwarded = true; });

    EXPECT_EQ(cached().dpi, 2000);
    // UI model was refreshed.
    EXPECT_EQ(m_deviceModel->currentDPI(), 2000);
    // displayed == hardware ("default" == "default"), so hardwareForward fired.
    EXPECT_TRUE(forwarded);
}

TEST_F(ProfileOrchestratorFixture, ApplyDisplayedChangeOnlyForwardsToHardwareWhenActive) {
    attachMockSession();
    const QString kSerial = QStringLiteral("mock-serial");

    // Create a second profile and point the display there; hardware stays
    // on "default". The displayed-profile mutation must persist but must
    // NOT fire the hardwareForward callback.
    m_profileEngine->createProfileForApp(kSerial, QStringLiteral("other"),
                                         QStringLiteral("other"));
    m_profileEngine->setDisplayProfile(kSerial, QStringLiteral("other"));

    bool forwarded = false;
    m_orchestrator->applyDisplayedChange(
        [](Profile &p) { p.dpi = 3200; },
        [&]() { forwarded = true; });

    EXPECT_EQ(cached(QStringLiteral("other")).dpi, 3200);
    EXPECT_FALSE(forwarded);
}

// --- PresetRef round-trip --------------------------------------------------

TEST_F(ProfileOrchestratorFixture, PresetRefRoundTripRestoresTypeAndName) {
    attachMockSession();

    // Assign a PresetRef action to button 3 (Back Button) via ButtonModel,
    // then save. The orchestrator's saveCurrentProfile reads actionType/Name
    // from ButtonModel and converts via buttonEntryToAction.
    QList<ButtonAssignment> assignments;
    for (const auto &ctrl : m_device.m_controls) {
        assignments.append({ctrl.defaultName, QStringLiteral("default"),
                            ctrl.controlId});
    }
    m_buttonModel->loadFromProfile(assignments);
    m_buttonModel->setAction(3, QStringLiteral("Show desktop"),
                              QStringLiteral("preset"));

    m_orchestrator->saveCurrentProfile();

    // Verify the profile on disk has the correct ButtonAction.
    const QString path = m_profilesDir + QStringLiteral("/default.conf");
    Profile reloaded = ProfileEngine::loadProfile(path);
    ASSERT_EQ(reloaded.buttons[3].type, ButtonAction::PresetRef);
    EXPECT_EQ(reloaded.buttons[3].payload, QStringLiteral("show-desktop"));

    // Now simulate a display-profile change (the restore path). Before the
    // fix this fell through to the default branch and reported "Back Button".
    m_orchestrator->onDisplayProfileChanged(QStringLiteral("mock-serial"), reloaded);

    EXPECT_EQ(m_buttonModel->actionTypeForButton(3), QStringLiteral("preset"));
    EXPECT_EQ(m_buttonModel->actionNameForButton(3),  QStringLiteral("Show desktop"));
}
