#include <gtest/gtest.h>

#include "desktop/HyprlandDesktop.h"
#include "actions/ActionPresetRegistry.h"
#include "interfaces/IDesktopIntegration.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using logitune::ActionPresetRegistry;
using logitune::ButtonAction;
using logitune::HyprlandDesktop;
using logitune::IDesktopIntegration;

namespace {
const QByteArray kCatalog = R"([
    {
        "id": "close-window",
        "label": "Close Window",
        "variants": {
            "hyprland": { "hyprland-bind": { "dispatcher": "killactive" } }
        }
    },
    {
        "id": "switch-desktop-left",
        "label": "Switch Desktop Left",
        "variants": {
            "hyprland": { "hyprland-bind": { "dispatcher": "workspace", "arg": "r-1" } }
        }
    },
    {
        "id": "switch-desktop-right",
        "label": "Switch Desktop Right",
        "variants": {
            "hyprland": { "hyprland-bind": { "dispatcher": "workspace", "arg": "r+1" } }
        }
    },
    {
        "id": "calculator",
        "label": "Calculator",
        "variants": {
            "hyprland": { "app-launch": { "binary": "qalculate-gtk" } }
        }
    },
    {
        "id": "kde-only",
        "label": "Kde Only",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Something" } }
        }
    }
])";

void attachRegistry(HyprlandDesktop &desktop, ActionPresetRegistry &registry)
{
    registry.loadFromJson(kCatalog);
    desktop.setPresetRegistry(&registry);
}

QByteArray oneBind(int modmask, const char *key, const char *dispatcher,
                   const char *arg = "")
{
    return QByteArray("[{\"mouse\":false,\"submap\":\"\",\"modmask\":")
        + QByteArray::number(modmask)
        + ",\"key\":\"" + key
        + "\",\"dispatcher\":\"" + dispatcher
        + "\",\"arg\":\"" + arg + "\"}]";
}
} // namespace

TEST(HyprlandDesktopResolve, VariantKeyIsHyprland) {
    HyprlandDesktop d;
    EXPECT_EQ(d.variantKey(), "hyprland");
}

TEST(HyprlandDesktopResolve, DesktopNameIsHyprland) {
    HyprlandDesktop d;
    EXPECT_EQ(d.desktopName(), "Hyprland");
}

TEST(HyprlandDesktopResolve, ResolveReturnsNulloptWithoutRegistry) {
    HyprlandDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("close-window").has_value());
}

TEST(HyprlandDesktopResolve, ResolveKillactiveBindReturnsKeystroke) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] { return oneBind(64, "Q", "killactive"); });

    auto result = d.resolveNamedAction("close-window");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::Keystroke);
    EXPECT_EQ(result->payload, "Super+Q");
}

TEST(HyprlandDesktopResolve, ResolveWorkspaceBindMatchesArg) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] {
        return QByteArray(R"([
            {"mouse":false,"submap":"","modmask":64,"key":"Right","dispatcher":"workspace","arg":"r+1"},
            {"mouse":false,"submap":"","modmask":68,"key":"Left","dispatcher":"workspace","arg":"r-1"}
        ])");
    });

    auto result = d.resolveNamedAction("switch-desktop-left");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::Keystroke);
    EXPECT_EQ(result->payload, "Ctrl+Super+Left");
}

TEST(HyprlandDesktopResolve, ResolveReturnsNulloptForAbsentBind) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] { return oneBind(64, "Q", "killactive"); });

    EXPECT_FALSE(d.resolveNamedAction("switch-desktop-left").has_value());
}

TEST(HyprlandDesktopResolve, ResolveIgnoresMouseBinds) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] {
        return QByteArray(R"([
            {"mouse":true,"submap":"","modmask":64,"key":"mouse:272","dispatcher":"killactive","arg":""}
        ])");
    });

    EXPECT_FALSE(d.resolveNamedAction("close-window").has_value());
}

TEST(HyprlandDesktopResolve, ResolveReturnsNulloptForUnsupportedModifierMask) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] { return oneBind(66, "Q", "killactive"); });

    EXPECT_FALSE(d.resolveNamedAction("close-window").has_value());
}

TEST(HyprlandDesktopResolve, ResolveReturnsNulloptForInvalidJson) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);
    d.setBindReader([] { return QByteArray("not json"); });

    EXPECT_FALSE(d.resolveNamedAction("close-window").has_value());
}

TEST(HyprlandDesktopResolve, ResolveAppLaunchReturnsPayload) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);

    auto result = d.resolveNamedAction("calculator");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::AppLaunch);
    EXPECT_EQ(result->payload, "qalculate-gtk");
}

TEST(HyprlandDesktopResolve, ResolveReturnsNulloptForOtherDesktopVariant) {
    HyprlandDesktop d;
    ActionPresetRegistry reg;
    attachRegistry(d, reg);

    EXPECT_FALSE(d.resolveNamedAction("kde-only").has_value());
}

TEST(HyprlandDesktopFocus, ActiveWindowEventEmitsFocusSignal) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString socketDir = tmp.path() + QStringLiteral("/hypr/testsig");
    ASSERT_TRUE(QDir().mkpath(socketDir));
    const QString socketPath = socketDir + QStringLiteral("/.socket2.sock");
    QLocalServer::removeServer(socketPath);

    QLocalServer server;
    ASSERT_TRUE(server.listen(socketPath));

    HyprlandDesktop d;
    d.setSocketDirForTest(socketDir);
    QSignalSpy spy(&d, &IDesktopIntegration::activeWindowChanged);
    d.start();

    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket *client = server.nextPendingConnection();
    ASSERT_NE(client, nullptr);

    client->write("activewindow>>firefox,Mozilla Firefox\n");
    ASSERT_TRUE(client->waitForBytesWritten(1000));
    ASSERT_TRUE(spy.wait(1000));

    ASSERT_EQ(spy.size(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "firefox");
    EXPECT_EQ(spy.at(0).at(1).toString(), "Mozilla Firefox");
}

TEST(HyprlandDesktopFocus, DuplicateActiveWindowClassIsSuppressed) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString socketDir = tmp.path() + QStringLiteral("/hypr/testsig");
    ASSERT_TRUE(QDir().mkpath(socketDir));
    const QString socketPath = socketDir + QStringLiteral("/.socket2.sock");
    QLocalServer::removeServer(socketPath);

    QLocalServer server;
    ASSERT_TRUE(server.listen(socketPath));

    HyprlandDesktop d;
    d.setSocketDirForTest(socketDir);
    QSignalSpy spy(&d, &IDesktopIntegration::activeWindowChanged);
    d.start();

    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket *client = server.nextPendingConnection();
    ASSERT_NE(client, nullptr);

    client->write("activewindow>>firefox,One\n");
    ASSERT_TRUE(client->waitForBytesWritten(1000));
    ASSERT_TRUE(spy.wait(1000));

    client->write("activewindow>>firefox,Two\n");
    ASSERT_TRUE(client->waitForBytesWritten(1000));
    QTest::qWait(50);

    EXPECT_EQ(spy.size(), 1);
}
