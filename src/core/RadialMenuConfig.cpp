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

namespace {
bool validItem(const QVariantMap &entry, bool allowFolder)
{
    const QString kind = entry.value("kind", "action").toString();
    static const QStringList icons{"", "action", "back", "brightness", "calculator", "close",
        "copy", "cut", "desktop", "dpi", "folder", "forward", "keyboard", "launch", "mute",
        "next", "none", "paste", "play", "previous", "redo", "screenshot", "stop", "undo",
        "volume", "volume-down", "wheel", "windows",
        "search", "settings", "more", "plus", "grid", "trash", "edit", "help", "chevron", "browser", "terminal", "brush", "crop", "eyedropper", "star", "heart", "mail", "chat", "camera", "video", "microphone", "file", "save", "link"};
    if (!icons.contains(entry.value("icon").toString())) return false;
    if (kind == "empty") return !entry.contains("children");
    const auto label = entry.value("label").toString().trimmed();
    if (label.isEmpty() || label.size() > 80) return false;
    if (kind == "folder") {
        if (!allowFolder || entry.value("children").metaType() != QMetaType::fromType<QVariantList>())
            return false;
        const auto children = entry.value("children").toList();
        if (children.size() > 9) return false;
        for (const auto &child : children)
            if (!validItem(child.toMap(), false)) return false;
        return true;
    }
    if (entry.contains("children")) return false;
    if (kind == "adjustment")
        return QStringList{"volume", "brightness"}.contains(entry.value("parameter").toString());
    return kind == "action" && RadialMenuConfig::validEntry(entry);
}
}

QVariantMap RadialMenuConfig::emptyEntry()
{
    return {{"kind", "empty"}, {"label", ""}, {"action", "default"}};
}

QVariantList RadialMenuConfig::padded(const QVariantList &items, int count)
{
    auto result = items;
    while (result.size() < count) result.append(emptyEntry());
    return result;
}

bool RadialMenuConfig::isValid() const
{
    // Accept the old 2–8 item format; presentation and new saves use fixed slots.
    if (items.size() < 2 || items.size() > 8 || !validEntry(click)) return false;
    if (click.value("action").toString() != "default"
        && click.value("label").toString().trimmed().isEmpty()) return false;
    for (const auto &item : items)
        if (!validItem(item.toMap(), true)) return false;
    return true;
}

QByteArray RadialMenuConfig::toJson() const
{
    return QJsonDocument::fromVariant(QVariantMap{{"version", 2}, {"items", padded(items)}, {"click", click}})
        .toJson(QJsonDocument::Compact);
}

RadialMenuConfig RadialMenuConfig::fromJson(const QByteArray &json)
{
    const auto map = QJsonDocument::fromJson(json).toVariant().toMap();
    if (map.value("version", 1).toInt() < 1 || map.value("version", 1).toInt() > 2)
        return {};
    RadialMenuConfig result{map.value("items").toList(), map.value("click").toMap()};
    if (!result.isValid()) return {};
    result.items = padded(result.items);
    return result;
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
