#include <gtest/gtest.h>

#include "actions/ActionPresetRegistry.h"

using logitune::ActionPresetRegistry;

namespace {
const QByteArray kMiniCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "category": "workspace",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    },
    {
        "id": "task-switcher",
        "label": "Task Switcher",
        "category": "workspace",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Expose" } }
        }
    }
])";
} // namespace

TEST(ActionPresetRegistry, LoadsTwoPresets) {
    ActionPresetRegistry r;
    EXPECT_EQ(r.loadFromJson(kMiniCatalog), 2);
    EXPECT_EQ(r.all().size(), 2u);
}

TEST(ActionPresetRegistry, PresetByIdReturnsNullForUnknown) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_EQ(r.preset("nonexistent"), nullptr);
}

TEST(ActionPresetRegistry, PresetByIdReturnsMatch) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    const auto *p = r.preset("show-desktop");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->label, "Show Desktop");
}

TEST(ActionPresetRegistry, SupportedByReturnsTrueForPresentVariant) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_TRUE(r.supportedBy("show-desktop", "kde"));
    EXPECT_TRUE(r.supportedBy("show-desktop", "gnome"));
}

TEST(ActionPresetRegistry, SupportedByReturnsFalseForAbsentVariant) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_FALSE(r.supportedBy("show-desktop", "hyprland"));
    EXPECT_FALSE(r.supportedBy("task-switcher", "gnome"));
    EXPECT_FALSE(r.supportedBy("nonexistent", "kde"));
}

TEST(ActionPresetRegistry, VariantDataReturnsNestedObject) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    QJsonObject data = r.variantData("show-desktop", "kde");
    ASSERT_TRUE(data.contains("kglobalaccel"));
    EXPECT_EQ(data.value("kglobalaccel").toObject().value("name").toString(),
              "Show Desktop");
}

TEST(ActionPresetRegistry, VariantDataReturnsEmptyForMissing) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_TRUE(r.variantData("nonexistent", "kde").isEmpty());
    EXPECT_TRUE(r.variantData("show-desktop", "hyprland").isEmpty());
}

TEST(ActionPresetRegistry, SkipsMalformedEntries) {
    ActionPresetRegistry r;
    const QByteArray mixed = R"([
        { "id": "valid", "label": "Valid", "variants": {} },
        { "label": "missing-id" },
        { "id": "valid2", "label": "Valid 2", "variants": {} }
    ])";
    EXPECT_EQ(r.loadFromJson(mixed), 2);
    EXPECT_NE(r.preset("valid"), nullptr);
    EXPECT_NE(r.preset("valid2"), nullptr);
}

TEST(ActionPresetRegistry, LoadFromJsonReplacesPreviousContents) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_EQ(r.all().size(), 2u);

    const QByteArray replacement = R"([
        { "id": "only-one", "label": "Only", "variants": {} }
    ])";
    EXPECT_EQ(r.loadFromJson(replacement), 1);
    EXPECT_EQ(r.all().size(), 1u);
    EXPECT_EQ(r.preset("show-desktop"), nullptr);
    EXPECT_NE(r.preset("only-one"), nullptr);
}

TEST(ActionPresetRegistry, NonArrayRootReturnsZero) {
    ActionPresetRegistry r;
    const QByteArray obj = R"({"not": "an array"})";
    EXPECT_EQ(r.loadFromJson(obj), 0);
    EXPECT_TRUE(r.all().empty());
}

TEST(ActionPresetRegistry, UnparsableInputReturnsZero) {
    ActionPresetRegistry r;
    const QByteArray garbage = "not json at all";
    EXPECT_EQ(r.loadFromJson(garbage), 0);
    EXPECT_TRUE(r.all().empty());
}

TEST(ActionPresetRegistry, EmptyArrayReturnsZero) {
    ActionPresetRegistry r;
    EXPECT_EQ(r.loadFromJson("[]"), 0);
    EXPECT_TRUE(r.all().empty());
}

TEST(ActionPresetRegistry, LoadsFromBundledResource) {
    ActionPresetRegistry r;
    int n = r.loadFromResource();
    EXPECT_GE(n, 7);
    EXPECT_NE(r.preset("show-desktop"), nullptr);
    EXPECT_NE(r.preset("task-switcher"), nullptr);
    EXPECT_NE(r.preset("switch-desktop-left"), nullptr);
    EXPECT_NE(r.preset("switch-desktop-right"), nullptr);
    EXPECT_NE(r.preset("screenshot"), nullptr);
    EXPECT_NE(r.preset("close-window"), nullptr);
    EXPECT_NE(r.preset("calculator"), nullptr);
}

TEST(ActionPresetRegistry, ShippedShowDesktopHasKdeAndGnomeVariants) {
    ActionPresetRegistry r;
    r.loadFromResource();
    EXPECT_TRUE(r.supportedBy("show-desktop", "kde"));
    EXPECT_TRUE(r.supportedBy("show-desktop", "gnome"));
    EXPECT_FALSE(r.supportedBy("show-desktop", "hyprland"));
}

TEST(ActionPresetRegistry, ShippedHyprlandVariantsUseLiveBinds) {
    ActionPresetRegistry r;
    r.loadFromResource();

    EXPECT_TRUE(r.supportedBy("close-window", "hyprland"));
    EXPECT_TRUE(r.supportedBy("switch-desktop-left", "hyprland"));
    EXPECT_TRUE(r.supportedBy("switch-desktop-right", "hyprland"));
    EXPECT_FALSE(r.supportedBy("task-switcher", "hyprland"));
    EXPECT_FALSE(r.supportedBy("screenshot", "hyprland"));

    QJsonObject close = r.variantData("close-window", "hyprland")
                            .value("hyprland-bind").toObject();
    EXPECT_EQ(close.value("dispatcher").toString(), "killactive");

    QJsonObject left = r.variantData("switch-desktop-left", "hyprland")
                           .value("hyprland-bind").toObject();
    EXPECT_EQ(left.value("dispatcher").toString(), "workspace");
    EXPECT_EQ(left.value("arg").toString(), "r-1");
}

TEST(ActionPresetRegistry, ShippedCalculatorUsesAppLaunch) {
    ActionPresetRegistry r;
    r.loadFromResource();
    QJsonObject gnome = r.variantData("calculator", "gnome");
    ASSERT_TRUE(gnome.contains("app-launch"));
    EXPECT_EQ(gnome.value("app-launch").toObject().value("binary").toString(),
              "gnome-calculator");
}
