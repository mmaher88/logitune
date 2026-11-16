// Every device-originated action must target the mouse it came from, never
// "whichever mouse is selected in the carousel". The selection is UI state:
// it is cleared while the only mouse is offline and points at another mouse
// when more than one is connected, so routing hardware writes or input
// events through it misconfigures (or skips) the real device.

#include <gtest/gtest.h>
#include <QDir>
#include <QStandardPaths>

#include "helpers/AppRootFixture.h"

using namespace logitune;
using namespace logitune::test;

namespace {

const QString kSerialA = QStringLiteral("mock-serial");
const QString kSerialB = QStringLiteral("mock-serial-B");

constexpr uint16_t kBackCid    = 0x0053;
constexpr uint16_t kGestureCid = 0x00C3;
constexpr uint16_t kShiftCid   = 0x00C4;

void simulateDisconnect(DeviceSession *session)
{
    session->setConnectedForTest(false);
    emit session->disconnected();
}

// Mirrors the end of DeviceSession::enumerateAndSetup on reconnect: the
// session is marked connected, then setupComplete fires.
void simulateReconnect(DeviceSession *session)
{
    session->setConnectedForTest(true);
    emit session->setupComplete();
}

} // namespace

class DeviceRoutingTest : public AppRootFixture {
protected:
    // Adds mouse B and re-selects the fixture's mouse A, so every test
    // starts from "A is on screen, B is the other mouse".
    PhysicalDevice *addSecondMouseAndSelectFirst()
    {
        PhysicalDevice *b = addMockDevice(QStringLiteral("B"));
        const int idxA = deviceModel().devices().indexOf(m_physicalDevice);
        deviceModel().setSelectedIndex(idxA);
        return b;
    }

    Profile &hwProfile(const QString &serial)
    {
        return profileEngine().cachedProfile(serial,
                                             profileEngine().hardwareProfile(serial));
    }
};

// --- Reconnect / wake ------------------------------------------------------

TEST_F(DeviceRoutingTest, ReconnectReappliesProfileWhenItIsTheOnlyMouse) {
    hwProfile(kSerialA).thumbWheelMode = QStringLiteral("zoom");

    // Going offline hides the only mouse and clears the carousel selection.
    simulateDisconnect(m_session);
    ASSERT_EQ(deviceModel().selectedIndex(), -1);

    // The firmware comes back in its native state after sleep / KVM switch.
    m_session->setThumbWheelMode(QStringLiteral("scroll"));
    simulateReconnect(m_session);

    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("zoom"));
    EXPECT_EQ(deviceModel().selectedIndex(), 0);
}

TEST_F(DeviceRoutingTest, ReconnectOfOtherMouseConfiguresThatMouseOnly) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    DeviceSession *bSession = b->primary();

    hwProfile(kSerialA).thumbWheelMode = QStringLiteral("volume");
    hwProfile(kSerialB).thumbWheelMode = QStringLiteral("zoom");
    m_session->setThumbWheelMode(QStringLiteral("volume"));

    simulateDisconnect(bSession);
    bSession->setThumbWheelMode(QStringLiteral("scroll"));
    simulateReconnect(bSession);

    EXPECT_EQ(bSession->thumbWheelMode(), QStringLiteral("zoom"));
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("volume"));
}

TEST_F(DeviceRoutingTest, AddingSecondMouseAppliesItsProfileToItNotToSelected) {
    // setupProfileForDevice reads B's profiles from AppConfigLocation; give
    // B a default profile there with a distinctive thumb wheel mode.
    const QString bDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/devices/") + kSerialB + QStringLiteral("/profiles");
    QDir().mkpath(bDir);
    Profile bDefault;
    bDefault.name = QStringLiteral("Default");
    bDefault.thumbWheelMode = QStringLiteral("volume");
    ProfileEngine::saveProfile(bDir + QStringLiteral("/default.conf"), bDefault);

    m_session->setThumbWheelMode(QStringLiteral("zoom"));

    PhysicalDevice *b = addMockDevice(QStringLiteral("B"));

    EXPECT_EQ(b->primary()->thumbWheelMode(), QStringLiteral("volume"));
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("zoom"));
}

// --- Input events ----------------------------------------------------------

TEST_F(DeviceRoutingTest, ButtonPressRunsTheMappingOfTheMouseItCameFrom) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    hwProfile(kSerialA).buttons[3] = {ButtonAction::Keystroke, QStringLiteral("Alt+Left")};
    hwProfile(kSerialB).buttons[3] = {ButtonAction::Keystroke, QStringLiteral("Ctrl+Z")};
    m_injector->clear();

    emit b->divertedButtonPressed(kBackCid, true);

    EXPECT_EQ(m_injector->lastArg(QStringLiteral("injectKeystroke")), QStringLiteral("Ctrl+Z"));
}

TEST_F(DeviceRoutingTest, ThumbWheelUsesTheModeOfTheMouseItCameFrom) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    m_session->setThumbWheelMode(QStringLiteral("volume"));
    b->primary()->setThumbWheelMode(QStringLiteral("zoom"));
    m_injector->clear();

    emit b->thumbWheelRotation(15);

    EXPECT_TRUE(m_injector->hasCalled(QStringLiteral("injectCtrlScroll")));
    EXPECT_FALSE(m_injector->hasCalled(QStringLiteral("injectKeystroke")));
}

TEST_F(DeviceRoutingTest, GestureUsesTheGestureMapOfTheMouseItCameFrom) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    hwProfile(kSerialA).buttons[5] = {ButtonAction::GestureTrigger, {}};
    hwProfile(kSerialA).gestures[QStringLiteral("right")] =
        {ButtonAction::Keystroke, QStringLiteral("Ctrl+Super+Right")};
    hwProfile(kSerialB).buttons[5] = {ButtonAction::GestureTrigger, {}};
    hwProfile(kSerialB).gestures[QStringLiteral("right")] =
        {ButtonAction::Keystroke, QStringLiteral("Alt+Tab")};
    m_injector->clear();

    emit b->divertedButtonPressed(kGestureCid, true);
    emit b->gestureRawXY(80, 0);
    emit b->divertedButtonPressed(kGestureCid, false);

    EXPECT_EQ(m_injector->lastArg(QStringLiteral("injectKeystroke")), QStringLiteral("Alt+Tab"));
}

TEST_F(DeviceRoutingTest, GestureOnOneMouseDoesNotLeakIntoTheOther) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    hwProfile(kSerialA).buttons[5] = {ButtonAction::GestureTrigger, {}};
    hwProfile(kSerialB).buttons[5] = {ButtonAction::GestureTrigger, {}};

    emit b->divertedButtonPressed(kGestureCid, true);
    emit b->gestureRawXY(80, 0);

    // Mouse A never pressed its gesture button, so it has no gesture
    // in progress and nothing accumulated.
    EXPECT_FALSE(gestureActive());
    EXPECT_EQ(gestureTotalDx(), 0);
}

TEST_F(DeviceRoutingTest, ButtonSettingChangePersistsToTheMouseItCameFrom) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    hwProfile(kSerialA).buttons[6] = {ButtonAction::SmartShiftToggle, {}};
    hwProfile(kSerialA).smartShiftEnabled = true;
    hwProfile(kSerialB).buttons[6] = {ButtonAction::SmartShiftToggle, {}};
    hwProfile(kSerialB).smartShiftEnabled = false;

    // B's session reports SmartShift off, so the toggle turns it on.
    emit b->divertedButtonPressed(kShiftCid, true);

    EXPECT_TRUE(hwProfile(kSerialB).smartShiftEnabled);
    EXPECT_TRUE(hwProfile(kSerialA).smartShiftEnabled);
}

// --- Window focus ----------------------------------------------------------

TEST_F(DeviceRoutingTest, FocusChangeSwitchesEveryConnectedMouse) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();

    // The long overload writes the values into the engine's cache as well.
    createAppProfile(QStringLiteral("google-chrome"), QStringLiteral("Google Chrome"),
                     1000, QStringLiteral("zoom"), /*thumbInvert=*/false);
    profileEngine().createProfileForApp(kSerialB, QStringLiteral("google-chrome"),
                                        QStringLiteral("Chrome on B"));
    profileEngine().cachedProfile(kSerialB, QStringLiteral("Chrome on B")).thumbWheelMode =
        QStringLiteral("volume");

    m_desktop->simulateFocus(QStringLiteral("google-chrome"), QStringLiteral("Chrome"));

    EXPECT_EQ(profileEngine().hardwareProfile(kSerialA), QStringLiteral("Google Chrome"));
    EXPECT_EQ(profileEngine().hardwareProfile(kSerialB), QStringLiteral("Chrome on B"));
    EXPECT_EQ(m_session->thumbWheelMode(), QStringLiteral("zoom"));
    EXPECT_EQ(b->primary()->thumbWheelMode(), QStringLiteral("volume"));
}

// --- Profile tabs ------------------------------------------------------------
// The tab bar shows the app profiles of the mouse selected in the carousel.
// Each mouse has its own; they must never be merged.

namespace {

QStringList tabNames(ProfileModel &model)
{
    QStringList names;
    for (int i = 0; i < model.rowCount(); ++i)
        names << model.data(model.index(i), ProfileModel::NameRole).toString();
    return names;
}

} // namespace

TEST_F(DeviceRoutingTest, TabsShowOnlyTheSelectedMousesAppProfiles) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    createAppProfile(QStringLiteral("google-chrome"), QStringLiteral("Google Chrome"));
    profileEngine().createProfileForApp(kSerialB, QStringLiteral("firefox"),
                                        QStringLiteral("Firefox"));

    deviceModel().setSelectedIndex(deviceModel().devices().indexOf(b));
    EXPECT_EQ(tabNames(profileModel()),
              (QStringList{QStringLiteral("Defaults"), QStringLiteral("Firefox")}));

    deviceModel().setSelectedIndex(deviceModel().devices().indexOf(m_physicalDevice));
    EXPECT_EQ(tabNames(profileModel()),
              (QStringList{QStringLiteral("Defaults"), QStringLiteral("Google Chrome")}));
}

TEST_F(DeviceRoutingTest, ConnectingSecondMouseDoesNotAddItsTabs) {
    // B already has a Firefox profile on disk when it connects.
    const QString bDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/devices/") + kSerialB + QStringLiteral("/profiles");
    QDir().mkpath(bDir);
    Profile bDefault;
    bDefault.name = QStringLiteral("Default");
    ProfileEngine::saveProfile(bDir + QStringLiteral("/default.conf"), bDefault);
    Profile bFirefox = bDefault;
    bFirefox.name = QStringLiteral("Firefox");
    ProfileEngine::saveProfile(bDir + QStringLiteral("/Firefox.conf"), bFirefox);
    ProfileEngine::saveAppBindings(bDir + QStringLiteral("/app-bindings.conf"),
                                   {{QStringLiteral("firefox"), QStringLiteral("Firefox")}});

    addMockDevice(QStringLiteral("B"));

    // A is still the mouse on screen, and A has no app profiles.
    ASSERT_EQ(deviceModel().selectedIndex(),
              deviceModel().devices().indexOf(m_physicalDevice));
    EXPECT_EQ(tabNames(profileModel()), (QStringList{QStringLiteral("Defaults")}));
}

TEST_F(DeviceRoutingTest, TabAddedOnOneMouseIsNotShownOnTheOther) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    const int idxA = deviceModel().devices().indexOf(m_physicalDevice);
    const int idxB = deviceModel().devices().indexOf(b);

    deviceModel().setSelectedIndex(idxB);
    profileModel().addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox"));
    EXPECT_EQ(profileEngine().profileForApp(kSerialB, QStringLiteral("firefox")),
              QStringLiteral("Firefox"));

    deviceModel().setSelectedIndex(idxA);
    EXPECT_EQ(tabNames(profileModel()), (QStringList{QStringLiteral("Defaults")}));

    // A can have a Firefox profile of its own.
    profileModel().addProfile(QStringLiteral("firefox"), QStringLiteral("Firefox on A"));
    EXPECT_EQ(profileEngine().profileForApp(kSerialA, QStringLiteral("firefox")),
              QStringLiteral("Firefox on A"));
    EXPECT_EQ(profileEngine().profileForApp(kSerialB, QStringLiteral("firefox")),
              QStringLiteral("Firefox"));
}

TEST_F(DeviceRoutingTest, TabHighlightsFollowTheSelectedMouse) {
    PhysicalDevice *b = addSecondMouseAndSelectFirst();
    const int idxA = deviceModel().devices().indexOf(m_physicalDevice);
    const int idxB = deviceModel().devices().indexOf(b);
    createAppProfile(QStringLiteral("google-chrome"), QStringLiteral("Google Chrome"));
    profileEngine().createProfileForApp(kSerialB, QStringLiteral("firefox"),
                                        QStringLiteral("Firefox"));

    // Viewing and running Google Chrome on A; B stays on its defaults.
    deviceModel().setSelectedIndex(idxB);
    deviceModel().setSelectedIndex(idxA);
    profileModel().selectTab(1);
    m_desktop->simulateFocus(QStringLiteral("google-chrome"), QStringLiteral("Chrome"));
    ASSERT_EQ(profileEngine().displayProfile(kSerialA), QStringLiteral("Google Chrome"));
    ASSERT_EQ(profileEngine().hardwareProfile(kSerialB), QStringLiteral("default"));

    deviceModel().setSelectedIndex(idxB);
    EXPECT_EQ(profileModel().displayIndex(), 0);
    EXPECT_TRUE(profileModel().data(profileModel().index(0), ProfileModel::IsHwActiveRole).toBool());

    deviceModel().setSelectedIndex(idxA);
    EXPECT_EQ(profileModel().displayIndex(), 1);
    EXPECT_TRUE(profileModel().data(profileModel().index(1), ProfileModel::IsHwActiveRole).toBool());
}
