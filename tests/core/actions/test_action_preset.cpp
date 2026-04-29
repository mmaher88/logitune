#include <gtest/gtest.h>

#include "actions/ActionPreset.h"
#include <QJsonDocument>
#include <QJsonObject>

using logitune::ActionPreset;

namespace {
QJsonObject parseObj(const char *json) {
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    return doc.object();
}
} // namespace

TEST(ActionPreset, ParsesFullyFormedEntry) {
    auto obj = parseObj(R"({
        "id": "show-desktop",
        "label": "Show Desktop",
        "icon": "desktop",
        "category": "workspace",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    })");

    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_TRUE(p.isValid());
    EXPECT_EQ(p.id, "show-desktop");
    EXPECT_EQ(p.label, "Show Desktop");
    EXPECT_EQ(p.icon, "desktop");
    EXPECT_EQ(p.category, "workspace");
    EXPECT_EQ(p.variants.size(), 2);
    EXPECT_TRUE(p.variants.contains("kde"));
    EXPECT_TRUE(p.variants.contains("gnome"));
}

TEST(ActionPreset, VariantDataPreservesNestedStructure) {
    auto obj = parseObj(R"({
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } }
        }
    })");

    ActionPreset p = ActionPreset::fromJson(obj);
    ASSERT_TRUE(p.variants.contains("kde"));
    QJsonObject kde = p.variants.value("kde");
    ASSERT_TRUE(kde.contains("kglobalaccel"));
    QJsonObject spec = kde.value("kglobalaccel").toObject();
    EXPECT_EQ(spec.value("component").toString(), "kwin");
    EXPECT_EQ(spec.value("name").toString(), "Show Desktop");
}

TEST(ActionPreset, MissingIdReturnsInvalid) {
    auto obj = parseObj(R"({"label": "x"})");
    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_FALSE(p.isValid());
}

TEST(ActionPreset, EmptyObjectReturnsInvalid) {
    ActionPreset p = ActionPreset::fromJson(QJsonObject{});
    EXPECT_FALSE(p.isValid());
}

TEST(ActionPreset, OptionalFieldsDefaultEmpty) {
    auto obj = parseObj(R"({"id": "x", "label": "X"})");
    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_TRUE(p.isValid());
    EXPECT_EQ(p.id, "x");
    EXPECT_TRUE(p.icon.isEmpty());
    EXPECT_TRUE(p.category.isEmpty());
    EXPECT_TRUE(p.variants.isEmpty());
}
