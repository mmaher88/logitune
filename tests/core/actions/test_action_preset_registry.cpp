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
