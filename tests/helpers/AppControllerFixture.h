#pragma once
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <memory>

#include "AppController.h"
#include "DeviceSession.h"
#include "PhysicalDevice.h"
#include "ProfileEngine.h"
#include "ButtonAction.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDesktop.h"
#include "mocks/MockInjector.h"
#include "mocks/MockDevice.h"
#include "hidpp/HidrawDevice.h"

namespace logitune::test {

class AppControllerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ensureApp();
        clearTestAppConfig();
        ASSERT_TRUE(m_tmpDir.isValid());

        m_desktop  = new MockDesktop();
        m_injector = new MockInjector();

        m_ctrl = std::make_unique<AppController>(m_desktop, m_injector);
        m_ctrl->init();

        m_profilesDir = m_tmpDir.path() + QStringLiteral("/profiles");
        QDir().mkpath(m_profilesDir);

        Profile defaultProfile;
        defaultProfile.name                = QStringLiteral("Default");
        defaultProfile.dpi                 = 1000;
        defaultProfile.smartShiftEnabled   = true;
        defaultProfile.smartShiftThreshold = 128;
        defaultProfile.smoothScrolling     = false;
        defaultProfile.scrollDirection     = QStringLiteral("standard");
        defaultProfile.hiResScroll         = true;
        defaultProfile.thumbWheelMode      = QStringLiteral("scroll");
        ProfileEngine::saveProfile(m_profilesDir + QStringLiteral("/default.conf"), defaultProfile);

        const QString kSerial = QStringLiteral("mock-serial");

        m_device.setupMxControls();

        auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        m_session = new DeviceSession(std::move(mockHidraw), 0xFF, "Bluetooth",
                                       nullptr, m_ctrl.get());
        m_session->m_connected = true;
        m_session->m_deviceName = QStringLiteral("Mock Device");
        // PhysicalDevice::descriptor() forwards to its primary session's
        // descriptor(). onPhysicalDeviceAdded writes that into AppController's
        // m_currentDevice, so the mock session must already know which
        // IDevice it's backing before we drive the added flow.
        m_session->m_activeDevice = &m_device;

        m_physicalDevice = new PhysicalDevice(kSerial, m_ctrl.get());
        m_physicalDevice->attachTransport(m_session);

        // Drive through the normal device-added flow.
        m_ctrl->onPhysicalDeviceAdded(m_physicalDevice);

        // setupProfileForDevice points the engine at AppConfigLocation
        // (the real config dir, scoped to test mode). Tests need their
        // temp dir instead, so re-register here and force a display
        // profile refresh. setDisplayProfile short-circuits when the
        // name matches its current value, so bounce through "" to make
        // the engine re-emit with the freshly loaded cache values.
        m_ctrl->m_profileEngine.registerDevice(kSerial, m_profilesDir);
        m_ctrl->m_profileEngine.setDisplayProfile(kSerial, QString());
        m_ctrl->m_profileEngine.setHardwareProfile(kSerial, QString());
        m_ctrl->m_profileEngine.setDisplayProfile(kSerial, QStringLiteral("default"));
        m_ctrl->m_profileEngine.setHardwareProfile(kSerial, QStringLiteral("default"));
    }

    void TearDown() override {
        m_ctrl.reset();
        m_session = nullptr; // owned by m_ctrl via QObject parent
        delete m_desktop;
        m_desktop = nullptr;
        delete m_injector;
        m_injector = nullptr;
    }

    // -----------------------------------------------------------------------
    // Helper methods
    // -----------------------------------------------------------------------

    void createAppProfile(const QString &wmClass,
                          const QString &profileName,
                          int dpi = 1000,
                          const QString &thumbMode = QStringLiteral("scroll"))
    {
        const QString kSerial = QStringLiteral("mock-serial");
        Profile p = m_ctrl->m_profileEngine.cachedProfile(kSerial, QStringLiteral("default"));
        p.name           = profileName;
        p.dpi            = dpi;
        p.thumbWheelMode = thumbMode;

        ProfileEngine::saveProfile(m_profilesDir + "/" + profileName + ".conf", p);
        m_ctrl->m_profileEngine.createProfileForApp(kSerial, wmClass, profileName);
        m_ctrl->m_profileModel.restoreProfile(wmClass, profileName);
    }

    void createAppProfile(const QString &wmClass,
                          const QString &profileName,
                          int dpi,
                          const QString &thumbMode,
                          bool thumbInvert,
                          bool smartShiftEnabled = true,
                          int smartShiftThreshold = 128,
                          const QString &scrollDirection = "standard",
                          bool hiResScroll = true)
    {
        const QString kSerial = QStringLiteral("mock-serial");
        Profile p = m_ctrl->m_profileEngine.cachedProfile(kSerial, QStringLiteral("default"));
        p.name                = profileName;
        p.dpi                 = dpi;
        p.thumbWheelMode      = thumbMode;
        p.thumbWheelInvert    = thumbInvert;
        p.smartShiftEnabled   = smartShiftEnabled;
        p.smartShiftThreshold = smartShiftThreshold;
        p.scrollDirection     = scrollDirection;
        p.hiResScroll         = hiResScroll;

        m_ctrl->m_profileEngine.createProfileForApp(kSerial, wmClass, profileName);
        Profile &cached = m_ctrl->m_profileEngine.cachedProfile(kSerial, profileName);
        cached = p;
        ProfileEngine::saveProfile(m_profilesDir + "/" + profileName + ".conf", p);
        m_ctrl->m_profileModel.restoreProfile(wmClass, profileName);
    }

    void setProfileButton(const QString &profileName, int buttonIdx, const ButtonAction &action) {
        const QString kSerial = QStringLiteral("mock-serial");
        Profile &p = m_ctrl->m_profileEngine.cachedProfile(kSerial, profileName);
        if (buttonIdx >= 0 && buttonIdx < static_cast<int>(p.buttons.size()))
            p.buttons[static_cast<std::size_t>(buttonIdx)] = action;
        m_ctrl->m_profileEngine.saveProfileToDisk(kSerial, profileName);
    }

    void setProfileGesture(const QString &profileName,
                           const QString &direction,
                           const QString &keystroke)
    {
        const QString kSerial = QStringLiteral("mock-serial");
        Profile &p = m_ctrl->m_profileEngine.cachedProfile(kSerial, profileName);
        p.gestures[direction] = ButtonAction{ButtonAction::Keystroke, keystroke};
        m_ctrl->m_profileEngine.saveProfileToDisk(kSerial, profileName);
    }

    void focusApp(const QString &wmClass) {
        m_desktop->simulateFocus(wmClass, wmClass);
        const QString hwProfile = m_ctrl->m_profileEngine.hardwareProfile(QStringLiteral("mock-serial"));
        int hwIndex = 0;
        const int count = m_ctrl->m_profileModel.rowCount();
        for (int i = 0; i < count; ++i) {
            QModelIndex mi = m_ctrl->m_profileModel.index(i);
            if (m_ctrl->m_profileModel.data(mi, ProfileModel::WmClassRole).toString()
                    == hwProfile
                || m_ctrl->m_profileModel.data(mi, ProfileModel::NameRole).toString()
                    == hwProfile) {
                hwIndex = i;
                break;
            }
        }
        m_ctrl->m_profileModel.selectTab(hwIndex);
    }

    void pressButton(uint16_t controlId) {
        m_ctrl->onDivertedButtonPressed(controlId, true);
    }

    void releaseButton(uint16_t controlId) {
        m_ctrl->onDivertedButtonPressed(controlId, false);
    }

    void gestureXY(int16_t dx, int16_t dy) {
        // Feed directly into per-device state (onGestureRawXY is handled per-session now)
        if (m_session) {
            auto &state = m_ctrl->m_perDeviceState[m_session->deviceId()];
            if (state.gestureActive) {
                state.gestureAccumX += dx;
                state.gestureAccumY += dy;
            }
        }
    }

    void thumbWheel(int delta) {
        m_ctrl->onThumbWheelRotation(delta);
    }

    // Adds a second mock device and registers it through the normal flow.
    // Returns the new PhysicalDevice for test-level manipulation.
    PhysicalDevice* addMockDevice(const QString &serialSuffix,
                                  int seedDpi = 1000) {
        const QString serial = QStringLiteral("mock-serial-") + serialSuffix;
        const QString devProfilesDir = m_tmpDir.path()
            + "/" + serial + "/profiles";
        QDir().mkpath(devProfilesDir);

        Profile seed;
        seed.name = QStringLiteral("Default");
        seed.dpi  = seedDpi;
        ProfileEngine::saveProfile(devProfilesDir + "/default.conf", seed);

        m_ctrl->m_profileEngine.registerDevice(serial, devProfilesDir);

        auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
        auto *session = new DeviceSession(std::move(mockHidraw), 0xFF,
                                          "Bluetooth", nullptr, m_ctrl.get());
        session->m_connected = true;
        session->m_deviceName = QStringLiteral("Mock Device ") + serialSuffix;
        session->m_activeDevice = &m_device;  // reuse the fixture's MockDevice descriptor

        auto *device = new PhysicalDevice(serial, m_ctrl.get());
        device->attachTransport(session);

        m_ctrl->onPhysicalDeviceAdded(device);

        // Same bounce as SetUp: setupProfileForDevice wrote a stale
        // default.conf into AppConfigLocation using the mock's zero DPI;
        // re-register at our temp dir and force a display-profile emit
        // with the properly seeded cache.
        m_ctrl->m_profileEngine.registerDevice(serial, devProfilesDir);
        m_ctrl->m_profileEngine.setDisplayProfile(serial, QString());
        m_ctrl->m_profileEngine.setHardwareProfile(serial, QString());
        m_ctrl->m_profileEngine.setDisplayProfile(serial, QStringLiteral("default"));
        m_ctrl->m_profileEngine.setHardwareProfile(serial, QStringLiteral("default"));

        return device;
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    ProfileEngine &profileEngine() { return m_ctrl->m_profileEngine; }
    ProfileModel  &profileModel()  { return m_ctrl->m_profileModel; }
    ButtonModel   &buttonModel()   { return m_ctrl->m_buttonModel; }
    DeviceModel   &deviceModel()   { return m_ctrl->m_deviceModel; }
    ActionModel   &actionModel()   { return m_ctrl->m_actionModel; }
    ActionExecutor &actionExecutor() { return m_ctrl->m_actionExecutor; }

    const IDevice *currentDevice() const { return m_ctrl->m_currentDevice; }

    int gestureTotalDx() const {
        if (!m_session) return 0;
        auto it = m_ctrl->m_perDeviceState.find(m_session->deviceId());
        return it != m_ctrl->m_perDeviceState.end() ? it->gestureAccumX : 0;
    }
    int gestureTotalDy() const {
        if (!m_session) return 0;
        auto it = m_ctrl->m_perDeviceState.find(m_session->deviceId());
        return it != m_ctrl->m_perDeviceState.end() ? it->gestureAccumY : 0;
    }
    bool gestureActive() const {
        if (!m_session) return false;
        auto it = m_ctrl->m_perDeviceState.find(m_session->deviceId());
        return it != m_ctrl->m_perDeviceState.end() ? it->gestureActive : false;
    }

    int thumbAccum() const {
        if (!m_session) return 0;
        auto it = m_ctrl->m_perDeviceState.find(m_session->deviceId());
        return it != m_ctrl->m_perDeviceState.end() ? it->thumbAccum : 0;
    }

    void setThumbWheelMode(const QString &mode) {
        if (m_session) m_session->setThumbWheelMode(mode);
    }

    void setThumbWheelInvert(bool invert) {
        if (m_session) m_session->setThumbWheelMode(m_session->thumbWheelMode(), invert);
    }

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    MockDesktop  *m_desktop  = nullptr;
    MockInjector *m_injector = nullptr;
    MockDevice    m_device;
    DeviceSession *m_session = nullptr;
    PhysicalDevice *m_physicalDevice = nullptr;
    std::unique_ptr<AppController> m_ctrl;
    QString       m_profilesDir;
    QTemporaryDir m_tmpDir;
};

} // namespace logitune::test
