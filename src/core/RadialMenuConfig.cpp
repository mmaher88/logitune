#include "RadialMenuConfig.h"
#include "ActionExecutor.h"
#include <QJsonDocument>
#include <cmath>
#include <numbers>

namespace logitune {
bool RadialMenuConfig::validEntry(const QVariantMap &entry)
{
    const auto text = entry.value("action").toString();
    const auto action = ButtonAction::parse(text);
    if (action.type == ButtonAction::GestureTrigger || action.type == ButtonAction::RadialMenu)
        return false;
    if (action.type == ButtonAction::Default) return text == "default";
    if (action.type == ButtonAction::Keystroke || action.type == ButtonAction::Media) {
        if (action.payload != "+") {
            for (const auto &token : action.payload.split('+'))
                if (ActionExecutor::parseKeystroke(token.trimmed()).empty()) return false;
        }
    }
    if (action.type == ButtonAction::SmartShiftToggle || action.type == ButtonAction::DpiCycle)
        return true;
    return !action.payload.trimmed().isEmpty() && action.serialize() == text;
}

bool RadialMenuConfig::isValid() const
{
    if (items.size() < 2 || items.size() > 8 || !validEntry(click)) return false;
    if (click.value("action").toString() != "default"
        && click.value("label").toString().trimmed().isEmpty()) return false;
    for (const auto &item : items) {
        const auto entry = item.toMap();
        if (entry.value("label").toString().trimmed().isEmpty() || !validEntry(entry)) return false;
    }
    return true;
}

QByteArray RadialMenuConfig::toJson() const
{
    return QJsonDocument::fromVariant(QVariantMap{{"items", items}, {"click", click}})
        .toJson(QJsonDocument::Compact);
}

RadialMenuConfig RadialMenuConfig::fromJson(const QByteArray &json)
{
    const auto map = QJsonDocument::fromJson(json).toVariant().toMap();
    RadialMenuConfig result{map.value("items").toList(), map.value("click").toMap()};
    return result.isValid() ? result : RadialMenuConfig{};
}

int RadialMenuConfig::selection(double x, double y, int count)
{
    if (count < 2 || count > 8 || std::hypot(x, y) < 50.0) return -1;
    const double circle = 2 * std::numbers::pi;
    double angle = std::atan2(y, x) + std::numbers::pi / 2;
    if (angle < 0) angle += circle;
    return int(std::floor(angle / circle * count + 0.5)) % count;
}
}
