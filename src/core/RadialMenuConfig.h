#pragma once
#include "ButtonAction.h"
#include <QVariantList>
#include <QVariantMap>

namespace logitune {
// Action ids/payloads are independent of the label, allowing new catalog
// entries to be used without changing the menu renderer or profile schema.
struct RadialMenuConfig {
    static QVariantMap emptyEntry();
    static QVariantList padded(const QVariantList &items, int count = 8);
    QVariantList items = padded({
        QVariantMap{{"label", "Copy"}, {"action", "keystroke:Ctrl+C"}},
        QVariantMap{{"label", "Paste"}, {"action", "keystroke:Ctrl+V"}}
    });
    QVariantMap click{{"label", ""}, {"action", "default"}};
    bool isValid() const;
    QByteArray toJson() const;
    static RadialMenuConfig fromJson(const QByteArray &json);
    static bool validEntry(const QVariantMap &entry);
    static int selection(double x, double y, int count);
};
}
