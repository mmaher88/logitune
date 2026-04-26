#include <gtest/gtest.h>
#include "ProfileEngine.h"
#include "helpers/TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

// ---------------------------------------------------------------------------
// WmClass resolution tests — uses ProfileFixture for temp dir setup
// ---------------------------------------------------------------------------

class WmClassTest : public ProfileFixture {
protected:
    void SetUp() override {
        ProfileFixture::SetUp();
        m_profilesDir = tmpPath();

        // Write default profile to disk so registerDevice picks it up
        Profile def = makeDefaultProfile();
        ProfileEngine::saveProfile(m_profilesDir + "/default.conf", def);

        m_engine.registerDevice(kSerial, m_profilesDir);

        // Set up two app bindings for most tests
        m_engine.createProfileForApp(kSerial, "google-chrome", "Google Chrome");
        m_engine.createProfileForApp(kSerial, "dolphin", "Dolphin");
    }

    ProfileEngine m_engine;
    QString m_profilesDir;
    const QString kSerial = QStringLiteral("test-serial");
};

// ExactMatch — binding stored under exact key resolves correctly
TEST_F(WmClassTest, ExactMatch) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "google-chrome"), "Google Chrome");
}

// CaseInsensitive — lookup is case-insensitive on the wmClass key
TEST_F(WmClassTest, CaseInsensitive) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "Google-Chrome"), "Google Chrome");
}

// No fallback — "org.kde.dolphin" does NOT match binding "dolphin".
// Identity resolution (short-class matching) is handled by KDeDesktop, not ProfileEngine.
TEST_F(WmClassTest, NoShortClassFallbackInProfileEngine) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "org.kde.dolphin"), "default");
}

// NoMatchReturnsDefault — unknown wmClass returns "default"
TEST_F(WmClassTest, NoMatchReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "firefox"), "default");
}

// EmptyReturnsDefault — empty wmClass returns "default"
TEST_F(WmClassTest, EmptyReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, ""), "default");
}

// ShortClassNoFalseMatch — "org.kde.something" does not match "dolphin" binding
TEST_F(WmClassTest, ShortClassNoFalseMatch) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "org.kde.something"), "default");
}

// MultipleBindingsCorrectOne — both chrome and dolphin resolve to their own profiles
TEST_F(WmClassTest, MultipleBindingsCorrectOne) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "google-chrome"), "Google Chrome");
    EXPECT_EQ(m_engine.profileForApp(kSerial, "dolphin"), "Dolphin");
}

// No short-class fallback — identity resolution moved to KDeDesktop
TEST_F(WmClassTest, NoShortClassCaseInsensitiveFallback) {
    EXPECT_EQ(m_engine.profileForApp(kSerial, "org.KDE.Dolphin"), "default");
}

// CreateProfileForAppGuard — modifying a cached profile's DPI and re-calling
// createProfileForApp must NOT reset the cached profile (guard prevents overwrite)
TEST_F(WmClassTest, CreateProfileForAppGuard) {
    // Mutate the cached "Google Chrome" profile
    m_engine.cachedProfile(kSerial, "Google Chrome").dpi = 4000;

    // Call createProfileForApp again — should be a no-op for the profile itself
    m_engine.createProfileForApp(kSerial, "google-chrome", "Google Chrome");

    EXPECT_EQ(m_engine.cachedProfile(kSerial, "Google Chrome").dpi, 4000);
}

// DisplayHardwareProfileSeparation — display and hardware profiles track independently
TEST_F(WmClassTest, DisplayHardwareProfileSeparation) {
    m_engine.setDisplayProfile(kSerial, "Google Chrome");
    m_engine.setHardwareProfile(kSerial, "Dolphin");

    EXPECT_EQ(m_engine.displayProfile(kSerial), "Google Chrome");
    EXPECT_EQ(m_engine.hardwareProfile(kSerial), "Dolphin");
}
