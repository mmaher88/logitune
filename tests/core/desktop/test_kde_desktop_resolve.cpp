#include <gtest/gtest.h>

#include "desktop/KDeDesktop.h"
#include "actions/ActionPresetRegistry.h"

using logitune::KDeDesktop;
using logitune::ActionPresetRegistry;
using logitune::ButtonAction;

namespace {
const QByteArray kCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    },
    {
        "id": "task-switcher",
        "label": "Task Switcher",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "ExposeAll" } }
        }
    },
    {
        "id": "calculator",
        "label": "Calculator",
        "variants": {
            "kde": { "app-launch": { "binary": "kcalc" } }
        }
    },
    {
        "id": "gnome-only",
        "label": "Gnome Only",
        "variants": {
            "gnome": { "gsettings": { "schema": "x", "key": "y" } }
        }
    }
])";
} // namespace

TEST(KdeDesktopResolve, VariantKeyIsKde) {
    KDeDesktop d;
    EXPECT_EQ(d.variantKey(), "kde");
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptWithoutRegistry) {
    KDeDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}

TEST(KdeDesktopResolve, ResolveKglobalaccelReturnsDBusPayload) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("show-desktop");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::DBus);
    EXPECT_EQ(result->payload,
              "org.kde.kglobalaccel,/component/kwin,"
              "org.kde.kglobalaccel.Component,invokeShortcut,Show Desktop");
}

TEST(KdeDesktopResolve, ResolveKglobalaccelSecondPreset) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("task-switcher");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::DBus);
    EXPECT_EQ(result->payload,
              "org.kde.kglobalaccel,/component/kwin,"
              "org.kde.kglobalaccel.Component,invokeShortcut,ExposeAll");
}

TEST(KdeDesktopResolve, ResolveAppLaunchReturnsAppLaunchPayload) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("calculator");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::AppLaunch);
    EXPECT_EQ(result->payload, "kcalc");
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptForGnomeOnlyPreset) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("gnome-only").has_value());
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptForUnknownId) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("not-a-real-preset").has_value());
}

TEST(KdeDesktopResolve, ResolveKglobalaccelMissingComponentReturnsNullopt) {
    const QByteArray cat = R"([
        { "id": "x", "label": "X",
          "variants": { "kde": { "kglobalaccel": { "name": "Foo" } } } }
    ])";
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(cat);
    d.setPresetRegistry(&reg);
    EXPECT_FALSE(d.resolveNamedAction("x").has_value());
}

TEST(KdeDesktopResolve, ResolveKglobalaccelMissingNameReturnsNullopt) {
    const QByteArray cat = R"([
        { "id": "x", "label": "X",
          "variants": { "kde": { "kglobalaccel": { "component": "kwin" } } } }
    ])";
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(cat);
    d.setPresetRegistry(&reg);
    EXPECT_FALSE(d.resolveNamedAction("x").has_value());
}

TEST(KdeDesktopResolve, ResolveAppLaunchMissingBinaryReturnsNullopt) {
    const QByteArray cat = R"([
        { "id": "x", "label": "X",
          "variants": { "kde": { "app-launch": { } } } }
    ])";
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(cat);
    d.setPresetRegistry(&reg);
    EXPECT_FALSE(d.resolveNamedAction("x").has_value());
}

TEST(KdeDesktopResolve, ResolveUnknownVariantKindReturnsNullopt) {
    const QByteArray cat = R"([
        { "id": "x", "label": "X",
          "variants": { "kde": { "future-kind": { "whatever": "value" } } } }
    ])";
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(cat);
    d.setPresetRegistry(&reg);
    EXPECT_FALSE(d.resolveNamedAction("x").has_value());
}
