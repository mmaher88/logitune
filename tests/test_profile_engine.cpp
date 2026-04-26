#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QSettings>
#include <QCoreApplication>
#include "ProfileEngine.h"

using namespace logitune;

// ---------------------------------------------------------------------------
// ButtonAction
// ---------------------------------------------------------------------------

TEST(ButtonAction, ParseKeystroke) {
    auto a = ButtonAction::parse("keystroke:Ctrl+C");
    EXPECT_EQ(a.type, ButtonAction::Keystroke);
    EXPECT_EQ(a.payload, "Ctrl+C");
}

TEST(ButtonAction, ParseDefault) {
    auto a = ButtonAction::parse("default");
    EXPECT_EQ(a.type, ButtonAction::Default);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseEmptyIsDefault) {
    auto a = ButtonAction::parse("");
    EXPECT_EQ(a.type, ButtonAction::Default);
}

TEST(ButtonAction, ParseGestureTrigger) {
    auto a = ButtonAction::parse("gesture-trigger");
    EXPECT_EQ(a.type, ButtonAction::GestureTrigger);
    EXPECT_TRUE(a.payload.isEmpty());
}

TEST(ButtonAction, ParseMedia) {
    auto a = ButtonAction::parse("media:VolumeUp");
    EXPECT_EQ(a.type, ButtonAction::Media);
    EXPECT_EQ(a.payload, "VolumeUp");
}

TEST(ButtonAction, ParseDBus) {
    auto a = ButtonAction::parse("dbus:org.kde.KWin,/VirtualDesktopManager,org.kde.KWin.VirtualDesktopManager,nextDesktop");
    EXPECT_EQ(a.type, ButtonAction::DBus);
    EXPECT_EQ(a.payload, "org.kde.KWin,/VirtualDesktopManager,org.kde.KWin.VirtualDesktopManager,nextDesktop");
}

TEST(ButtonAction, ParseAppLaunch) {
    auto a = ButtonAction::parse("app-launch:/usr/bin/foo");
    EXPECT_EQ(a.type, ButtonAction::AppLaunch);
    EXPECT_EQ(a.payload, "/usr/bin/foo");
}

TEST(ButtonAction, SerializeRoundTrip) {
    ButtonAction a{ButtonAction::DBus, "org.kde.KWin,/foo,bar,baz"};
    EXPECT_EQ(ButtonAction::parse(a.serialize()).payload, a.payload);
}

TEST(ButtonAction, SerializeKeystroke) {
    ButtonAction a{ButtonAction::Keystroke, "Ctrl+Z"};
    EXPECT_EQ(a.serialize(), "keystroke:Ctrl+Z");
}

TEST(ButtonAction, SerializeDefault) {
    ButtonAction a;
    EXPECT_EQ(a.serialize(), "default");
}

TEST(ButtonAction, SerializeGestureTrigger) {
    ButtonAction a{ButtonAction::GestureTrigger, {}};
    EXPECT_EQ(a.serialize(), "gesture-trigger");
}

// ---------------------------------------------------------------------------
// LoadProfile
// ---------------------------------------------------------------------------

TEST(ProfileEngine, LoadProfile) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/test.conf";
    QSettings s(path, QSettings::IniFormat);
    s.setValue("General/version", 1);
    s.setValue("General/name", "Test");
    s.setValue("DPI/value", 1600);
    s.setValue("SmartShift/enabled", true);
    s.setValue("SmartShift/threshold", 50);
    s.setValue("Buttons/3", "keystroke:Back");
    s.setValue("Gestures/up", "keystroke:Ctrl+Up");
    s.sync();

    Profile p = ProfileEngine::loadProfile(path);
    EXPECT_EQ(p.name, "Test");
    EXPECT_EQ(p.dpi, 1600);
    EXPECT_TRUE(p.smartShiftEnabled);
    EXPECT_EQ(p.smartShiftThreshold, 50);
    EXPECT_EQ(p.buttons[3].type, ButtonAction::Keystroke);
    EXPECT_EQ(p.buttons[3].payload, "Back");
    EXPECT_EQ(p.gestures["up"].type, ButtonAction::Keystroke);
}

TEST(ProfileEngine, LoadProfileDefaults) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/empty.conf";
    QSettings s(path, QSettings::IniFormat);
    s.setValue("General/name", "Empty");
    s.sync();

    Profile p = ProfileEngine::loadProfile(path);
    EXPECT_EQ(p.dpi, 1000);
    EXPECT_TRUE(p.smartShiftEnabled);
    EXPECT_EQ(p.smartShiftThreshold, 128);
    EXPECT_TRUE(p.hiResScroll);
    EXPECT_EQ(p.scrollDirection, "standard");
}

// ---------------------------------------------------------------------------
// SaveProfile
// ---------------------------------------------------------------------------

TEST(ProfileEngine, SaveProfile) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/out.conf";
    Profile p;
    p.name = "Saved";
    p.dpi = 2400;
    p.smartShiftEnabled = false;
    ProfileEngine::saveProfile(path, p);
    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(loaded.name, "Saved");
    EXPECT_EQ(loaded.dpi, 2400);
    EXPECT_FALSE(loaded.smartShiftEnabled);
}

TEST(ProfileEngine, SaveAndLoadButtons) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/btns.conf";
    Profile p;
    p.buttons[0] = ButtonAction{ButtonAction::Keystroke, "Ctrl+C"};
    p.buttons[5] = ButtonAction{ButtonAction::GestureTrigger, {}};
    ProfileEngine::saveProfile(path, p);
    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(loaded.buttons[0].type, ButtonAction::Keystroke);
    EXPECT_EQ(loaded.buttons[0].payload, "Ctrl+C");
    EXPECT_EQ(loaded.buttons[5].type, ButtonAction::GestureTrigger);
}

TEST(ProfileEngine, SaveAndLoadGestures) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/gestures.conf";
    Profile p;
    p.gestures["up"]    = ButtonAction{ButtonAction::Keystroke, "Ctrl+Alt+Up"};
    p.gestures["click"] = ButtonAction{ButtonAction::DBus, "org.foo,/bar,baz,qux"};
    ProfileEngine::saveProfile(path, p);
    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_EQ(loaded.gestures["up"].type, ButtonAction::Keystroke);
    EXPECT_EQ(loaded.gestures["up"].payload, "Ctrl+Alt+Up");
    EXPECT_EQ(loaded.gestures["click"].type, ButtonAction::DBus);
}

TEST(ProfileEngine, SaveAndLoadScrollSmooth) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/scroll.conf";
    Profile p;
    p.smoothScrolling = true;
    p.scrollDirection = "natural";
    p.hiResScroll     = false;
    ProfileEngine::saveProfile(path, p);
    Profile loaded = ProfileEngine::loadProfile(path);
    EXPECT_TRUE(loaded.smoothScrolling);
    EXPECT_EQ(loaded.scrollDirection, "natural");
    EXPECT_FALSE(loaded.hiResScroll);
}

// ---------------------------------------------------------------------------
// AppBindings
// ---------------------------------------------------------------------------

TEST(ProfileEngine, ResolveAppBinding) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/app-bindings.conf";
    QSettings s(path, QSettings::IniFormat);
    s.setValue("Bindings/firefox", "browsing");
    s.setValue("Bindings/org.kde.dolphin", "default");
    s.sync();

    auto bindings = ProfileEngine::loadAppBindings(path);
    EXPECT_EQ(bindings["firefox"], "browsing");
    EXPECT_EQ(bindings["org.kde.dolphin"], "default");
    EXPECT_EQ(bindings.value("unknown", "default"), "default");
}

TEST(ProfileEngine, SaveAndLoadAppBindings) {
    QTemporaryDir tmpDir;
    QString path = tmpDir.path() + "/bindings.conf";
    QMap<QString, QString> bindings;
    bindings["code"] = "coding";
    bindings["vlc"]  = "media";
    ProfileEngine::saveAppBindings(path, bindings);
    auto loaded = ProfileEngine::loadAppBindings(path);
    EXPECT_EQ(loaded["code"], "coding");
    EXPECT_EQ(loaded["vlc"], "media");
    EXPECT_EQ(loaded.size(), 2);
}

// ---------------------------------------------------------------------------
// Delta
// ---------------------------------------------------------------------------

TEST(ProfileEngine, DeltaBetweenProfiles) {
    Profile a; a.dpi = 1000; a.smartShiftEnabled = true; a.smartShiftThreshold = 50;
    Profile b; b.dpi = 1600; b.smartShiftEnabled = true; b.smartShiftThreshold = 50;
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_TRUE(delta.dpiChanged);
    EXPECT_FALSE(delta.smartShiftChanged);
}

TEST(ProfileEngine, DeltaSmartShift) {
    Profile a; a.smartShiftEnabled = true;  a.smartShiftThreshold = 30;
    Profile b; b.smartShiftEnabled = false; b.smartShiftThreshold = 30;
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_FALSE(delta.dpiChanged);
    EXPECT_TRUE(delta.smartShiftChanged);
}

TEST(ProfileEngine, DeltaScroll) {
    Profile a; a.smoothScrolling = false; a.scrollDirection = "standard";
    Profile b; b.smoothScrolling = true;  b.scrollDirection = "natural";
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_TRUE(delta.scrollChanged);
    EXPECT_FALSE(delta.dpiChanged);
}

TEST(ProfileEngine, DeltaButtons) {
    Profile a, b;
    b.buttons[2] = ButtonAction{ButtonAction::Keystroke, "Ctrl+W"};
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_TRUE(delta.buttonsChanged);
    EXPECT_FALSE(delta.gesturesChanged);
}

TEST(ProfileEngine, DeltaGestures) {
    Profile a, b;
    b.gestures["up"] = ButtonAction{ButtonAction::Keystroke, "Ctrl+Up"};
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_TRUE(delta.gesturesChanged);
    EXPECT_FALSE(delta.buttonsChanged);
}

TEST(ProfileEngine, DeltaNoDiff) {
    Profile a;
    a.dpi = 800;
    a.buttons[0] = ButtonAction{ButtonAction::Media, "VolumeUp"};
    a.gestures["left"] = ButtonAction{ButtonAction::Keystroke, "Alt+Left"};
    Profile b = a;
    auto delta = ProfileEngine::diff(a, b);
    EXPECT_FALSE(delta.dpiChanged);
    EXPECT_FALSE(delta.smartShiftChanged);
    EXPECT_FALSE(delta.scrollChanged);
    EXPECT_FALSE(delta.buttonsChanged);
    EXPECT_FALSE(delta.gesturesChanged);
}

// --- Per-device contexts ---

TEST(ProfileEngine, MultipleDevicesKeepSeparateCaches) {
    QTemporaryDir tmpA, tmpB;
    ASSERT_TRUE(tmpA.isValid());
    ASSERT_TRUE(tmpB.isValid());

    logitune::ProfileEngine eng;
    eng.registerDevice(QStringLiteral("A"), tmpA.path());
    eng.registerDevice(QStringLiteral("B"), tmpB.path());

    eng.cachedProfile(QStringLiteral("A"), QStringLiteral("default")).dpi = 1234;
    eng.cachedProfile(QStringLiteral("B"), QStringLiteral("default")).dpi = 5678;

    EXPECT_EQ(eng.cachedProfile(QStringLiteral("A"),
                                QStringLiteral("default")).dpi, 1234);
    EXPECT_EQ(eng.cachedProfile(QStringLiteral("B"),
                                QStringLiteral("default")).dpi, 5678);
}

TEST(ProfileEngine, UnknownDeviceLazyRegisters) {
    logitune::ProfileEngine eng;
    EXPECT_FALSE(eng.hasDevice(QStringLiteral("ghost")));

    auto &p = eng.cachedProfile(QStringLiteral("ghost"),
                                QStringLiteral("default"));
    EXPECT_EQ(p.dpi, 1000);
    EXPECT_EQ(p.name, QStringLiteral("default"));
    EXPECT_TRUE(eng.hasDevice(QStringLiteral("ghost")));
}

TEST(ProfileEngine, SetDisplayProfileScopedToDevice) {
    QTemporaryDir tmpA, tmpB;
    ASSERT_TRUE(tmpA.isValid());
    ASSERT_TRUE(tmpB.isValid());

    logitune::ProfileEngine eng;
    eng.registerDevice(QStringLiteral("A"), tmpA.path());
    eng.registerDevice(QStringLiteral("B"), tmpB.path());

    eng.setDisplayProfile(QStringLiteral("A"), QStringLiteral("chrome"));

    EXPECT_EQ(eng.displayProfile(QStringLiteral("A")),
              QStringLiteral("chrome"));
    EXPECT_EQ(eng.displayProfile(QStringLiteral("B")), QString());
}
