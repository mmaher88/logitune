#include <gtest/gtest.h>

#include "desktop/GnomeDesktop.h"
#include "actions/ActionPresetRegistry.h"

using logitune::GnomeDesktop;
using logitune::ActionPresetRegistry;
using logitune::ButtonAction;

namespace {
const QByteArray kCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "gnome": { "gsettings": { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } },
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } }
        }
    },
    {
        "id": "calculator",
        "label": "Calculator",
        "variants": {
            "gnome": { "app-launch": { "binary": "gnome-calculator" } }
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
} // namespace

// ---------------------------------------------------------------------------
// Static transform: gsettings output to keystroke
// ---------------------------------------------------------------------------

TEST(GnomeDesktopResolve, TransformSingleModifier) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['<Super>d']"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformCtrlAlt) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['<Primary><Alt>Left']"),
              "Ctrl+Alt+Left");
}

TEST(GnomeDesktopResolve, TransformPicksFirstNonEmpty) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['', '<Super>d']"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformReturnsEmptyForEmptyList) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("[]"), "");
}

TEST(GnomeDesktopResolve, TransformReturnsEmptyForAllEmpty) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['']"), "");
}

TEST(GnomeDesktopResolve, TransformHandlesWhitespace) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("[ '<Super>d' ]"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformBareKeyNoModifiers) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['Print']"), "Print");
}

// ---------------------------------------------------------------------------
// End-to-end resolve
// ---------------------------------------------------------------------------

TEST(GnomeDesktopResolve, VariantKeyIsGnome) {
    GnomeDesktop d;
    EXPECT_EQ(d.variantKey(), "gnome");
}

TEST(GnomeDesktopResolve, ResolveGsettingsReturnsKeystroke) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);
    d.setGsettingsReader([](const QString &schema, const QString &key) {
        if (schema == "org.gnome.desktop.wm.keybindings" && key == "show-desktop")
            return QStringLiteral("['<Super>d']");
        return QString();
    });

    auto result = d.resolveNamedAction("show-desktop");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::Keystroke);
    EXPECT_EQ(result->payload, "Super+d");
}

TEST(GnomeDesktopResolve, ResolveAppLaunchReturnsAppLaunchPayload) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("calculator");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::AppLaunch);
    EXPECT_EQ(result->payload, "gnome-calculator");
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptWhenBindingEmpty) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);
    d.setGsettingsReader([](const QString &, const QString &) {
        return QStringLiteral("['']");
    });

    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptForKdeOnlyPreset) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("kde-only").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptForUnknownId) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("not-real").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptWithoutRegistry) {
    GnomeDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}
