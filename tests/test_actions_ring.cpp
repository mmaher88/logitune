#include <gtest/gtest.h>
#include "ActionsRing.h"
#include "ProfileEngine.h"
#include "hidpp/capabilities/HapticsCapability.h"
#include <QTemporaryDir>
#include "services/ProfileOrchestratorFixture.h"
#include "services/ActionsRingService.h"
#include "DeviceManager.h"
#include <QTest>
#include "input/UinputInjector.h"
#include <QFileInfo>

using namespace logitune;

TEST(ActionsRing, TapAndHoldBoundary) {
    ActionsRingTrigger trigger;
    EXPECT_EQ(trigger.press(100), ActionsRingTrigger::Open);
    EXPECT_EQ(trigger.release(600), ActionsRingTrigger::None);
    EXPECT_EQ(trigger.press(700), ActionsRingTrigger::Close);
    trigger.closed();
    EXPECT_EQ(trigger.press(1000), ActionsRingTrigger::Open);
    EXPECT_EQ(trigger.release(1501), ActionsRingTrigger::Select);
    EXPECT_EQ(trigger.release(1600), ActionsRingTrigger::None);
}

TEST(ActionsRing, DefaultsUseExecutableKeystrokes) {
    for (const auto &entry : defaultRingSlots()) {
        auto check = [](const RingSlot &slot) {
            if (slot.action.type == ButtonAction::Keystroke || slot.action.type == ButtonAction::Media)
                EXPECT_FALSE(ActionExecutor::parseKeystroke(slot.action.payload).empty())
                    << slot.action.payload.toStdString();
        };
        check(entry);
        for (const auto &child : entry.children) check(child);
    }
}

TEST(ActionsRing, AppLaunchPassesArgumentsWithoutShellExpansion) {
    QTemporaryDir dir;
    const auto path = dir.filePath("literal $HOME argument");
    UinputInjector injector;
    injector.launchApp("/usr/bin/touch \"" + path + "\"");
    QElapsedTimer timeout;
    timeout.start();
    while (!QFileInfo::exists(path) && timeout.elapsed() < 1000) QTest::qWait(10);
    EXPECT_TRUE(QFileInfo::exists(path));
}

TEST(ActionsRing, ProfileRoundTrip) {
    QTemporaryDir dir;
    Profile profile;
    ASSERT_EQ(profile.ringSlots.size(), 8);
    profile.ringSlots[0].name = "Custom";
    profile.ringSlots[0].action = {ButtonAction::Keystroke, "Ctrl+Shift+T"};
    profile.ringSize = 150;
    const auto path = dir.filePath("ring.conf");
    ProfileEngine::saveProfile(path, profile);
    const auto loaded = ProfileEngine::loadProfile(path);
    ASSERT_EQ(loaded.ringSlots.size(), 8);
    EXPECT_EQ(loaded.ringSlots[0].name, "Custom");
    EXPECT_EQ(loaded.ringSlots[0].action, profile.ringSlots[0].action);
    EXPECT_EQ(loaded.ringSlots[4].children.size(), 3);
    EXPECT_EQ(loaded.ringSize, 150);
}

TEST(ActionsRing, LimitsAndNoNestedSubmenus) {
    RingSlot slot;
    slot.children.resize(12);
    slot.children[0].children.resize(2);
    QList<RingSlot> entries(12, slot);
    const auto loaded = ringSlotsFromJson(ringSlotsToJson(entries));
    ASSERT_EQ(loaded.size(), 8);
    ASSERT_EQ(loaded[0].children.size(), 8);
    EXPECT_TRUE(loaded[0].children[0].children.isEmpty());
}

TEST(Haptics, WaveformBytes) {
    const auto &variant = hidpp::capabilities::kHapticsVariants[0];
    EXPECT_EQ(static_cast<uint16_t>(variant.feature), 0x19b0);
    EXPECT_EQ(variant.playFn, 4);
    EXPECT_EQ(variant.buildPlay(4), (std::vector<uint8_t>{4}));
    EXPECT_EQ(variant.buildPlay(1), (std::vector<uint8_t>{1}));
}

using logitune::test::ProfileOrchestratorFixture;

TEST_F(ProfileOrchestratorFixture, RingEditorPersistsDisplayedProfileOnly) {
    attachMockSession();
    DeviceManager devices(nullptr);
    ActionsRingService service(m_profileEngine.get(), &devices, m_executor.get(),
        m_selection.get(), m_desktop.get(), m_orchestrator.get());
    m_profileEngine->createProfileForApp("mock-serial", "editor", "Editor");
    m_profileEngine->setDisplayProfile("mock-serial", "Editor");
    auto entries = service.entries();
    auto first = entries[0].toMap();
    first["name"] = "Profile specific";
    entries[0] = first;
    service.setEntries(entries);
    service.setSize(80);
    EXPECT_EQ(service.entries()[0].toMap()["name"].toString(), "Profile specific");
    EXPECT_EQ(service.size(), 80);
    const auto &hardware = m_profileEngine->cachedProfile("mock-serial", "default");
    EXPECT_NE(hardware.ringSlots[0].name, "Profile specific");
    EXPECT_EQ(hardware.ringSize, 100);
}

TEST_F(ProfileOrchestratorFixture, RingExecutesOnlyAfterOverlayCloses) {
    attachMockSession();
    DeviceManager devices(nullptr);
    ActionsRingService service(m_profileEngine.get(), &devices, m_executor.get(),
        m_selection.get(), m_desktop.get(), m_orchestrator.get());
    service.run("keystroke:Ctrl+C");
    EXPECT_TRUE(m_injector->lastArg("injectKeystroke").isEmpty());
    service.finished();
    QTest::qWait(80);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+C");
    service.run("actions-ring");
    service.finished();
    QTest::qWait(80);
    EXPECT_EQ(m_injector->lastArg("injectKeystroke"), "Ctrl+C");
}

TEST_F(ProfileOrchestratorFixture, RingButtonMigratesOnceAndPreservesExplicitDefault) {
    attachMockSession();
    ControlDescriptor control;
    control.controlId = 0x01a0;
    control.buttonIndex = 8;
    control.defaultName = "Haptic button";
    control.defaultActionType = "actions-ring";
    control.configurable = true;
    m_device.m_controls.append(control);
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + "/devices/mock-serial/profiles";
    QDir().mkpath(directory);
    Profile original;
    original.ringSlots[0].name = "Existing ring edit";
    ProfileEngine::saveProfile(directory + "/default.conf", original);
    m_orchestrator->setupProfileForDevice(m_physical);
    auto &migrated = m_profileEngine->cachedProfile("mock-serial", "default");
    EXPECT_EQ(migrated.buttons[8].type, ButtonAction::ActionsRing);
    EXPECT_TRUE(migrated.ringButtonMigrated);
    EXPECT_EQ(migrated.ringSlots[0].name, "Existing ring edit");
    m_orchestrator->onDisplayProfileChanged("mock-serial", migrated);
    EXPECT_EQ(m_buttonModel->actionTypeForButton(8), "actions-ring");
    EXPECT_EQ(m_buttonModel->actionNameForButton(8), "Actions Ring");
    migrated.buttons[8] = {};
    m_profileEngine->saveProfileToDisk("mock-serial", "default");
    m_orchestrator->setupProfileForDevice(m_physical);
    EXPECT_EQ(m_profileEngine->cachedProfile("mock-serial", "default").buttons[8].type,
              ButtonAction::Default);
    EXPECT_EQ(m_buttonModel->rowCount(), 9);
}
