#pragma once
#include <QHash>
#include <QJsonObject>
#include <QString>

namespace logitune {

/// A semantic action (e.g. "show-desktop") whose concrete implementation
/// differs per desktop environment. Static data; variants is a DE-native hint
/// the DE impl interprets at resolve-time, not a raw keystroke.
struct ActionPreset {
    QString id;                             // "show-desktop"
    QString label;                          // "Show Desktop"
    QString icon;                           // "desktop"
    QString category;                       // "workspace"
    QHash<QString, QJsonObject> variants;   // variantKey ("kde", "gnome") -> hint object

    /// Parse one preset from a JSON object. Returns an empty preset
    /// (id.isEmpty()) on malformed input.
    static ActionPreset fromJson(const QJsonObject &obj);

    bool isValid() const { return !id.isEmpty(); }
};

} // namespace logitune
