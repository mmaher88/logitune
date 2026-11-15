#pragma once

#include "ButtonAction.h"

#include <QList>
#include <QSettings>
#include <QString>

namespace logitune {

struct RadialMenuItem {
    QString label;
    ButtonAction action;

    bool operator==(const RadialMenuItem &other) const = default;
};

struct RadialMenuConfig {
    QList<RadialMenuItem> items = defaultItems();

    bool isValid() const;
    static bool validAction(const ButtonAction &action);

    static QList<RadialMenuItem> defaultItems();
    static RadialMenuConfig defaults();
    static RadialMenuConfig load(QSettings &settings);
    void save(QSettings &settings) const;
};

} // namespace logitune
