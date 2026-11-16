#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include "ProfileEngine.h"
#include "desktop/KDeDesktop.h"
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

// ---------------------------------------------------------------------------
// Picker → binding → KWin focus round trip.
//
// The "add profile" picker records each app under its .desktop file ID
// (LinuxDesktopBase::runningApplications). KDeDesktop::focusChanged must
// report the same ID for that app's windows, or the binding never activates.
// KWin's desktopFileName is the Wayland app_id, which is not always a .desktop
// file ID: flatpak Chrome reports "google-chrome" while its .desktop file is
// com.google.Chrome.desktop (StartupWMClass=google-chrome).
// ---------------------------------------------------------------------------

namespace {

struct DesktopEntry {
    const char *id;              // .desktop file completeBaseName
    const char *name;
    const char *startupWmClass;  // empty → no StartupWMClass key
};

struct FocusCase {
    const char *label;
    const char *pickerTitle;      // picker entry the user chose
    const char *resourceClass;    // what KWin reports for the window
    const char *desktopFileName;
    const char *expectedAppId;    // .desktop ID both sides must agree on
};

// Mirrors the user's machine: native apps in one dir, flatpak exports in another.
const DesktopEntry kSystemApps[] = {
    {"org.gnome.Evince",               "Document Viewer", ""},
    {"com.github.xournalpp.xournalpp", "Xournal++",       "com.github.xournalpp.xournalpp"},
    {"org.kde.dolphin",                "Dolphin",         ""},
};
const DesktopEntry kFlatpakApps[] = {
    {"com.google.Chrome",    "Google Chrome", "google-chrome"},
    {"org.kde.okular",       "Okular",        "okular"},
    {"org.telegram.desktop", "Telegram",      "TelegramDesktop"},
};

void writeDesktopFile(const QString &dir, const DesktopEntry &e)
{
    QDir().mkpath(dir);
    QFile f(dir + QLatin1Char('/') + QString::fromLatin1(e.id) + QStringLiteral(".desktop"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray body = "[Desktop Entry]\nType=Application\nName=";
    body += e.name;
    body += "\nExec=true\n";
    if (*e.startupWmClass) {
        body += "StartupWMClass=";
        body += e.startupWmClass;
        body += '\n';
    }
    f.write(body);
}

void PrintTo(const FocusCase &c, std::ostream *os) { *os << c.label; }

} // namespace

class KdeFocusWmClassTest : public ProfileFixture {
protected:
    void SetUp() override {
        ProfileFixture::SetUp();
        m_systemDir  = tmpPath() + QStringLiteral("/system-apps");
        m_flatpakDir = tmpPath() + QStringLiteral("/flatpak-apps");
        for (const auto &e : kSystemApps)  writeDesktopFile(m_systemDir, e);
        for (const auto &e : kFlatpakApps) writeDesktopFile(m_flatpakDir, e);

        const QString profilesDir = tmpPath() + QStringLiteral("/profiles");
        QDir().mkpath(profilesDir);
        ProfileEngine::saveProfile(profilesDir + "/default.conf", makeDefaultProfile());
        m_engine.registerDevice(kSerial, profilesDir);
    }

    QStringList dirs() const { return {m_systemDir, m_flatpakDir}; }

    // wmClass the picker stores for the entry titled `title`.
    QString pickerWmClass(const QString &title) const {
        KDeDesktop kde;
        kde.setDesktopDirsForTest(dirs());
        for (const QVariant &v : kde.runningApplications()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("title")).toString() == title)
                return m.value(QStringLiteral("wmClass")).toString();
        }
        return {};
    }

    // App ID KDeDesktop emits when KWin reports a focused window.
    QString focusedAppId(const QString &resourceClass, const QString &desktopFileName,
                         const QStringList &desktopDirs) const {
        KDeDesktop kde;
        kde.setDesktopDirsForTest(desktopDirs);
        QSignalSpy spy(&kde, &IDesktopIntegration::activeWindowChanged);
        kde.focusChanged(resourceClass, QStringLiteral("window title"), desktopFileName);
        return spy.isEmpty() ? QString() : spy.takeFirst().at(0).toString();
    }

    ProfileEngine m_engine;
    QString m_systemDir;
    QString m_flatpakDir;
    const QString kSerial = QStringLiteral("test-serial");
};

// Regression: flatpak Chrome picked from the "add profile" list must activate.
// KWin reports resourceClass = desktopFileName = "google-chrome" (the Wayland
// app_id); the picker stored "com.google.Chrome".
TEST_F(KdeFocusWmClassTest, FlatpakChromePickerBindingActivatesOnFocus) {
    const QString picked = pickerWmClass(QStringLiteral("Google Chrome"));
    ASSERT_EQ(picked.toStdString(), "com.google.Chrome");
    m_engine.createProfileForApp(kSerial, picked, QStringLiteral("Google Chrome"));

    const QString focused = focusedAppId(QStringLiteral("google-chrome"),
                                         QStringLiteral("google-chrome"), dirs());
    EXPECT_EQ(focused.toStdString(), picked.toStdString());
    EXPECT_EQ(m_engine.profileForApp(kSerial, focused).toStdString(), "Google Chrome");
}

// A real .desktop ID from KWin wins over a StartupWMClass match in another
// file: native Chrome ships google-chrome.desktop, so bindings made for it
// keep working even when flatpak Chrome (StartupWMClass=google-chrome) is
// installed too and its directory is scanned first.
TEST_F(KdeFocusWmClassTest, ExistingDesktopFileNameIsTrustedVerbatim) {
    writeDesktopFile(m_systemDir, {"google-chrome", "Google Chrome (native)", "Google-chrome"});
    const QString focused = focusedAppId(QStringLiteral("google-chrome"),
                                         QStringLiteral("google-chrome"),
                                         {m_flatpakDir, m_systemDir});
    EXPECT_EQ(focused.toStdString(), "google-chrome");
}

// Apps with no .desktop file at all keep KWin's desktopFileName.
TEST_F(KdeFocusWmClassTest, UnknownAppFallsBackToDesktopFileName) {
    EXPECT_EQ(focusedAppId(QStringLiteral("some-tool"), QStringLiteral("some-tool"), dirs())
                  .toStdString(), "some-tool");
    EXPECT_EQ(focusedAppId(QStringLiteral("some-tool"), QString(), dirs()).toStdString(),
              "some-tool");
}

class KdeFocusRoundTrip : public KdeFocusWmClassTest,
                          public ::testing::WithParamInterface<FocusCase> {};

// Reverse-DNS apps whose KWin desktopFileName already is their .desktop ID
// (or that only report a StartupWMClass) must keep round-tripping.
TEST_P(KdeFocusRoundTrip, PickerBindingActivatesOnFocus) {
    const FocusCase &c = GetParam();
    const QString picked = pickerWmClass(QString::fromLatin1(c.pickerTitle));
    ASSERT_EQ(picked.toStdString(), c.expectedAppId);
    m_engine.createProfileForApp(kSerial, picked, QString::fromLatin1(c.pickerTitle));

    const QString focused = focusedAppId(QString::fromLatin1(c.resourceClass),
                                         QString::fromLatin1(c.desktopFileName), dirs());
    EXPECT_EQ(focused.toStdString(), picked.toStdString());
    EXPECT_EQ(m_engine.profileForApp(kSerial, focused).toStdString(), c.pickerTitle);
}

INSTANTIATE_TEST_SUITE_P(
    ReverseDnsApps, KdeFocusRoundTrip,
    ::testing::Values(
        // Evince and Dolphin values were read from a live KDE Plasma 6 Wayland
        // session (resourceClass / desktopFileName of each window).
        FocusCase{"Evince",          "Document Viewer", "org.gnome.Evince", "org.gnome.Evince", "org.gnome.Evince"},
        FocusCase{"Dolphin",         "Dolphin",         "org.kde.dolphin", "org.kde.dolphin", "org.kde.dolphin"},
        FocusCase{"FlatpakOkular",   "Okular",          "org.kde.okular",  "org.kde.okular",  "org.kde.okular"},
        FocusCase{"Xournalpp",       "Xournal++",       "com.github.xournalpp.xournalpp",
                                                        "com.github.xournalpp.xournalpp",
                                                        "com.github.xournalpp.xournalpp"},
        FocusCase{"FlatpakTelegram", "Telegram",        "org.telegram.desktop", "org.telegram.desktop",
                                                        "org.telegram.desktop"},
        // X11 windows can arrive with no desktopFileName; StartupWMClass maps them.
        FocusCase{"OkularNoDesktopFileName", "Okular",  "okular",          "",                "org.kde.okular"},
        FocusCase{"TelegramX11",     "Telegram",        "TelegramDesktop", "",                "org.telegram.desktop"}),
    [](const ::testing::TestParamInfo<FocusCase> &info) { return std::string(info.param.label); });
