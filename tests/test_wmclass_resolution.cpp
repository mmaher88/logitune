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

        // Write default profile to disk so setDeviceConfigDir picks it up
        Profile def = makeDefaultProfile();
        ProfileEngine::saveProfile(m_profilesDir + "/default.conf", def);

        m_engine.setDeviceConfigDir(m_profilesDir);

        // Set up two app bindings for most tests
        m_engine.createProfileForApp("google-chrome", "Google Chrome");
        m_engine.createProfileForApp("dolphin", "Dolphin");
    }

    ProfileEngine m_engine;
    QString m_profilesDir;
};

// ExactMatch — binding stored under exact key resolves correctly
TEST_F(WmClassTest, ExactMatch) {
    EXPECT_EQ(m_engine.profileForApp("google-chrome"), "Google Chrome");
}

// CaseInsensitive — lookup is case-insensitive on the wmClass key
TEST_F(WmClassTest, CaseInsensitive) {
    EXPECT_EQ(m_engine.profileForApp("Google-Chrome"), "Google Chrome");
}

// ShortClassFallback — "org.kde.dolphin" -> last component "dolphin" matches binding
TEST_F(WmClassTest, ShortClassFallback) {
    EXPECT_EQ(m_engine.profileForApp("org.kde.dolphin"), "Dolphin");
}

// NoMatchReturnsDefault — unknown wmClass returns "default"
TEST_F(WmClassTest, NoMatchReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp("firefox"), "default");
}

// EmptyReturnsDefault — empty wmClass returns "default"
TEST_F(WmClassTest, EmptyReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp(""), "default");
}

// ShortClassNoFalseMatch — "org.kde.something" does not match "dolphin" binding
TEST_F(WmClassTest, ShortClassNoFalseMatch) {
    EXPECT_EQ(m_engine.profileForApp("org.kde.something"), "default");
}

// MultipleBindingsCorrectOne — both chrome and dolphin resolve to their own profiles
TEST_F(WmClassTest, MultipleBindingsCorrectOne) {
    EXPECT_EQ(m_engine.profileForApp("google-chrome"), "Google Chrome");
    EXPECT_EQ(m_engine.profileForApp("dolphin"), "Dolphin");
}

// ShortClassCaseInsensitive — short-class fallback is also case-insensitive
TEST_F(WmClassTest, ShortClassCaseInsensitive) {
    EXPECT_EQ(m_engine.profileForApp("org.KDE.Dolphin"), "Dolphin");
}

// CreateProfileForAppGuard — modifying a cached profile's DPI and re-calling
// createProfileForApp must NOT reset the cached profile (guard prevents overwrite)
TEST_F(WmClassTest, CreateProfileForAppGuard) {
    // Mutate the cached "Google Chrome" profile
    m_engine.cachedProfile("Google Chrome").dpi = 4000;

    // Call createProfileForApp again — should be a no-op for the profile itself
    m_engine.createProfileForApp("google-chrome", "Google Chrome");

    EXPECT_EQ(m_engine.cachedProfile("Google Chrome").dpi, 4000);
}

// DisplayHardwareProfileSeparation — display and hardware profiles track independently
TEST_F(WmClassTest, DisplayHardwareProfileSeparation) {
    m_engine.setDisplayProfile("Google Chrome");
    m_engine.setHardwareProfile("Dolphin");

    EXPECT_EQ(m_engine.displayProfile(), "Google Chrome");
    EXPECT_EQ(m_engine.hardwareProfile(), "Dolphin");
}
