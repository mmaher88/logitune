#include "actions/ActionPreset.h"

#include <QJsonObject>
#include <QJsonValue>

namespace logitune {

ActionPreset ActionPreset::fromJson(const QJsonObject &obj)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return {};  // invalid

    ActionPreset p;
    p.id       = id;
    p.label    = obj.value(QStringLiteral("label")).toString();
    p.icon     = obj.value(QStringLiteral("icon")).toString();
    p.category = obj.value(QStringLiteral("category")).toString();

    const QJsonValue variantsVal = obj.value(QStringLiteral("variants"));
    if (variantsVal.isObject()) {
        const QJsonObject variants = variantsVal.toObject();
        for (auto it = variants.constBegin(); it != variants.constEnd(); ++it) {
            if (it.value().isObject())
                p.variants.insert(it.key(), it.value().toObject());
        }
    }

    return p;
}

} // namespace logitune
