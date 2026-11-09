#include "RadialMenuConfig.h"

#include "ActionExecutor.h"

#include <algorithm>

namespace logitune {
namespace {

RadialMenuItem fallbackItem(int index)
{
    const auto defaults = RadialMenuConfig::defaultItems();
    if (index >= 0 && index < defaults.size())
        return defaults.at(index);
    return {QStringLiteral("Action %1").arg(index + 1), {ButtonAction::Default, {}}};
}

} // namespace

QList<RadialMenuItem> RadialMenuConfig::defaultItems()
{
    return {
        {QStringLiteral("Copy"), {ButtonAction::Keystroke, QStringLiteral("Ctrl+C")}},
        {QStringLiteral("Paste"), {ButtonAction::Keystroke, QStringLiteral("Ctrl+V")}},
        {QStringLiteral("Undo"), {ButtonAction::Keystroke, QStringLiteral("Ctrl+Z")}},
        {QStringLiteral("Redo"), {ButtonAction::Keystroke, QStringLiteral("Ctrl+Shift+Z")}},
    };
}

RadialMenuConfig RadialMenuConfig::defaults()
{
    return {};
}

bool RadialMenuConfig::validAction(const ButtonAction &action)
{
    switch (action.type) {
    case ButtonAction::Default:
    case ButtonAction::SmartShiftToggle:
    case ButtonAction::DpiCycle:
        return true;
    case ButtonAction::Keystroke:
    case ButtonAction::Media:
        return !action.payload.trimmed().isEmpty()
            && !ActionExecutor::parseKeystroke(action.payload).empty();
    case ButtonAction::AppLaunch:
    case ButtonAction::DBus:
    case ButtonAction::PresetRef:
        return !action.payload.trimmed().isEmpty();
    case ButtonAction::GestureTrigger:
    case ButtonAction::RadialMenu:
        return false;
    }
    return false;
}

bool RadialMenuConfig::isValid() const
{
    if (items.size() < 2 || items.size() > 8)
        return false;
    return std::all_of(items.cbegin(), items.cend(), [](const RadialMenuItem &item) {
        return !item.label.trimmed().isEmpty() && validAction(item.action);
    });
}

RadialMenuConfig RadialMenuConfig::load(QSettings &settings)
{
    const RadialMenuConfig fallback = defaults();
    if (!settings.contains(QStringLiteral("ActionRing/count")))
        return fallback;

    bool countOk = false;
    const int storedCount = settings.value(QStringLiteral("ActionRing/count")).toInt(&countOk);
    const int count = countOk && storedCount >= 2 && storedCount <= 8
        ? storedCount : fallback.items.size();

    RadialMenuConfig result;
    result.items.clear();
    result.items.reserve(count);
    for (int i = 0; i < count; ++i) {
        const RadialMenuItem fallbackEntry = fallbackItem(i);
        const QString prefix = QStringLiteral("ActionRing/Items/%1/").arg(i);

        QString label = settings.value(prefix + QStringLiteral("label"),
                                       fallbackEntry.label).toString().trimmed();
        if (label.isEmpty())
            label = fallbackEntry.label;

        const QString serialized = settings.value(
            prefix + QStringLiteral("action"), fallbackEntry.action.serialize()).toString();
        ButtonAction action = ButtonAction::parse(serialized);
        if (action.serialize() != serialized || !validAction(action))
            action = fallbackEntry.action;

        result.items.append({label, action});
    }
    return result;
}

void RadialMenuConfig::save(QSettings &settings) const
{
    RadialMenuConfig clean = *this;
    if (clean.items.size() < 2 || clean.items.size() > 8)
        clean = defaults();
    else {
        for (int i = 0; i < clean.items.size(); ++i) {
            const RadialMenuItem fallback = fallbackItem(i);
            clean.items[i].label = clean.items[i].label.trimmed();
            if (clean.items[i].label.isEmpty())
                clean.items[i].label = fallback.label;
            if (!validAction(clean.items[i].action))
                clean.items[i].action = fallback.action;
        }
    }
    settings.remove(QStringLiteral("ActionRing"));
    settings.setValue(QStringLiteral("ActionRing/count"), clean.items.size());
    for (int i = 0; i < clean.items.size(); ++i) {
        const QString prefix = QStringLiteral("ActionRing/Items/%1/").arg(i);
        settings.setValue(prefix + QStringLiteral("label"), clean.items.at(i).label);
        settings.setValue(prefix + QStringLiteral("action"),
                          clean.items.at(i).action.serialize());
    }
}

} // namespace logitune
