#pragma once
#include "actions/ActionPreset.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <vector>

namespace logitune {

/// Static data registry for semantic action presets. Parses JSON, indexes
/// presets by id, answers per-DE "is this supported" queries.
///
/// Load from raw JSON via loadFromJson (tests) or from the bundled Qt
/// resource via loadFromResource (production).
class ActionPresetRegistry {
public:
    ActionPresetRegistry() = default;

    /// Parse JSON and replace the registry contents. Returns the count
    /// of successfully-loaded presets. Malformed entries are skipped.
    int loadFromJson(const QByteArray &json);

    /// Load the bundled resource at :/logitune/actions.json.
    /// Returns the count of successfully-loaded presets.
    int loadFromResource();

    /// Lookup by id. Returns nullptr for unknown ids.
    ///
    /// The returned pointer is valid until the next load(FromJson|FromResource)
    /// call. Do not cache across loads.
    const ActionPreset *preset(const QString &id) const;

    /// True if the preset exists AND has a variant entry for variantKey.
    bool supportedBy(const QString &id, const QString &variantKey) const;

    /// Returns the variant hint object for id/variantKey, or empty
    /// object if either is absent.
    QJsonObject variantData(const QString &id, const QString &variantKey) const;

    /// All presets, order of insertion. For UI listing.
    const std::vector<ActionPreset> &all() const { return m_presets; }

private:
    std::vector<ActionPreset> m_presets;
    QHash<QString, size_t>    m_index;  // id -> position in m_presets
};

} // namespace logitune
