#include "ActionsRing.h"
#include <QJsonObject>

namespace logitune {

QList<RingSlot> defaultRingSlots()
{
    return {
        {"", "Terminal", {ButtonAction::AppLaunch, "kitty"}, {}},
        {"", "Files", {ButtonAction::AppLaunch, "xdg-open ."}, {}},
        {"", "Copy", {ButtonAction::Keystroke, "Ctrl+C"}, {}},
        {"", "Paste", {ButtonAction::Keystroke, "Ctrl+V"}, {}},
        {"", "Media", {}, {
            {"", "Previous", {ButtonAction::Media, "Previous"}, {}},
            {"", "Play / pause", {ButtonAction::Media, "Play"}, {}},
            {"", "Next", {ButtonAction::Media, "Next"}, {}}}},
        {"", "Mute", {ButtonAction::Media, "Mute"}, {}},
        {"", "Undo", {ButtonAction::Keystroke, "Ctrl+Z"}, {}},
        {"", "Redo", {ButtonAction::Keystroke, "Ctrl+Shift+Z"}, {}}
    };
}

static QJsonArray encodeSlots(const QList<RingSlot> &entries, bool children)
{
    QJsonArray array;
    for (const auto &slot : entries.mid(0, 8)) {
        QJsonObject object{{"icon", slot.icon}, {"name", slot.name},
                           {"action", slot.action.serialize()}};
        if (children && !slot.children.isEmpty())
            object.insert("children", encodeSlots(slot.children, false));
        if (!slot.adjust.isEmpty())
            object.insert("adjust", slot.adjust);
        array.append(object);
    }
    return array;
}

QJsonArray ringSlotsToJson(const QList<RingSlot> &entries) { return encodeSlots(entries, true); }

static QList<RingSlot> decodeSlots(const QJsonArray &array, bool children)
{
    QList<RingSlot> entries;
    for (const auto &value : array) {
        if (entries.size() == 8) break;
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        RingSlot slot{object.value("icon").toString(), object.value("name").toString(),
                      ButtonAction::parse(object.value("action").toString()), {},
                      object.value("adjust").toObject()};
        if (children) slot.children = decodeSlots(object.value("children").toArray(), false);
        entries.append(slot);
    }
    return entries;
}

QList<RingSlot> ringSlotsFromJson(const QJsonArray &array) { return decodeSlots(array, true); }

} // namespace logitune
