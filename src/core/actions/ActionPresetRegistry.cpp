#include "actions/ActionPresetRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace logitune {

int ActionPresetRegistry::loadFromJson(const QByteArray &json)
{
    m_presets.clear();
    m_index.clear();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return 0;

    const QJsonArray arr = doc.array();
    m_presets.reserve(static_cast<size_t>(arr.size()));

    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        ActionPreset p = ActionPreset::fromJson(v.toObject());
        if (!p.isValid()) continue;
        m_index.insert(p.id, m_presets.size());
        m_presets.push_back(std::move(p));
    }

    return static_cast<int>(m_presets.size());
}

int ActionPresetRegistry::loadFromResource()
{
    QFile f(QStringLiteral(":/logitune/actions.json"));
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return loadFromJson(f.readAll());
}

const ActionPreset *ActionPresetRegistry::preset(const QString &id) const
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return nullptr;
    return &m_presets.at(*it);
}

bool ActionPresetRegistry::supportedBy(const QString &id,
                                       const QString &variantKey) const
{
    const ActionPreset *p = preset(id);
    if (!p) return false;
    return p->variants.contains(variantKey);
}

QJsonObject ActionPresetRegistry::variantData(const QString &id,
                                              const QString &variantKey) const
{
    const ActionPreset *p = preset(id);
    if (!p) return {};
    return p->variants.value(variantKey);
}

} // namespace logitune
