#include "ActionModel.h"

namespace logitune {

ActionModel::ActionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    //                name                   description                                  actionType         payload                 category
    m_actions = {
        // Device
        { "DPI cycle",             "Step through the device's DPI preset list",  "dpi-cycle",         "",                  "Device" },
        { "Gestures",              "Trigger gesture recognition",                "gesture-trigger",   "",                  "Device" },
        { "Shift wheel mode",      "Toggle scroll wheel ratchet/freespin",       "smartshift-toggle", "",                  "Device" },

        // Edit
        { "Copy",                  "Copy selected content to clipboard",         "keystroke",         "Ctrl+C",            "Edit" },
        { "Cut",                   "Cut selected content to clipboard",          "keystroke",         "Ctrl+X",            "Edit" },
        { "Paste",                 "Paste clipboard content",                    "keystroke",         "Ctrl+V",            "Edit" },
        { "Redo",                  "Redo last undone action",                    "keystroke",         "Ctrl+Shift+Z",      "Edit" },
        { "Undo",                  "Undo the last action",                       "keystroke",         "Ctrl+Z",            "Edit" },

        // Media
        { "Mute",                  "Toggle audio mute",                          "keystroke",         "Mute",              "Media" },
        { "Next track",            "Skip to next media track",                   "keystroke",         "Next",              "Media" },
        { "Play/Pause",            "Play or pause media",                        "keystroke",         "Play",              "Media" },
        { "Previous track",        "Go to previous media track",                 "keystroke",         "Previous",          "Media" },
        { "Stop",                  "Stop media playback",                        "keystroke",         "Stop",              "Media" },
        { "Volume down",           "Decrease system volume",                     "keystroke",         "VolumeDown",        "Media" },
        { "Volume up",             "Increase system volume",                     "keystroke",         "VolumeUp",          "Media" },

        // Modifiers — held down until the button fires again, so a drag
        // started afterwards carries the modifier.
        { "Sticky Alt",            "Hold Alt until pressed again",               "sticky",            "Alt",               "Modifiers" },
        { "Sticky Ctrl",           "Hold Ctrl until pressed again",              "sticky",            "Ctrl",              "Modifiers" },
        { "Sticky Meta",           "Hold Meta until pressed again",              "sticky",            "Meta",              "Modifiers" },
        { "Sticky Shift",          "Hold Shift until pressed again",             "sticky",            "Shift",             "Modifiers" },

        // Navigation
        { "Back",                  "Navigate backward in browser/file manager",  "keystroke",         "Alt+Left",          "Navigation" },
        { "Forward",               "Navigate forward in browser/file manager",   "keystroke",         "Alt+Right",         "Navigation" },

        // Other
        { "Do nothing",            "Button is disabled",                         "none",              "",                  "Other" },
        { "Keyboard shortcut",     "Send a custom key combination",              "keystroke",         "",                  "Other" },
        { "Middle click",          "Emulate middle mouse button click",          "default",           "",                  "Other" },

        // System
        { "Brightness down",       "Decrease display brightness",                "keystroke",         "BrightnessDown",    "System" },
        { "Brightness up",         "Increase display brightness",                "keystroke",         "BrightnessUp",      "System" },
        { "Calculator",            "Open the system calculator",                 "preset",            "calculator",        "System" },
        { "Screenshot",            "Capture a screenshot",                       "preset",            "screenshot",        "System" },

        // Window
        { "Close window",          "Close the active window",                    "preset",            "close-window",      "Window" },

        // Workspace
        { "Show desktop",          "Minimize all windows to show desktop",       "preset",            "show-desktop",         "Workspace" },
        { "Switch desktop left",   "Switch to the virtual desktop on the left",  "preset",            "switch-desktop-left",  "Workspace" },
        { "Switch desktop right",  "Switch to the virtual desktop on the right", "preset",            "switch-desktop-right", "Workspace" },
        { "Task switcher",         "Open the window/task switcher",              "preset",            "task-switcher",        "Workspace" },
    };
}

int ActionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_actions.size();
}

QVariant ActionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_actions.size())
        return {};

    const ActionEntry &entry = m_actions[index.row()];
    switch (role) {
    case NameRole:        return entry.name;
    case DescriptionRole: return entry.description;
    case ActionTypeRole:  return entry.actionType;
    case PayloadRole:     return entry.payload;
    case CategoryRole:    return entry.category;
    default:              return {};
    }
}

QHash<int, QByteArray> ActionModel::roleNames() const
{
    return {
        { NameRole,        "name"        },
        { DescriptionRole, "description" },
        { ActionTypeRole,  "actionType"  },
        { PayloadRole,     "payload"     },
        { CategoryRole,    "category"    },
    };
}

int ActionModel::selectedIndex() const
{
    return m_selectedIndex;
}

void ActionModel::setSelectedIndex(int idx)
{
    if (m_selectedIndex == idx)
        return;
    m_selectedIndex = idx;
    emit selectedIndexChanged();
}

int ActionModel::indexForName(const QString &name) const
{
    for (int i = 0; i < m_actions.size(); ++i) {
        if (m_actions[i].name == name)
            return i;
    }
    return -1;
}

QString ActionModel::payloadForName(const QString &name) const
{
    for (const auto &a : m_actions) {
        if (a.name == name)
            return a.payload;
    }
    return {};
}

QString ActionModel::buttonActionToType(const ButtonAction &ba) const
{
    switch (ba.type) {
    case ButtonAction::GestureTrigger:   return QStringLiteral("gesture-trigger");
    case ButtonAction::SmartShiftToggle: return QStringLiteral("smartshift-toggle");
    case ButtonAction::DpiCycle:         return QStringLiteral("dpi-cycle");
    case ButtonAction::AppLaunch:        return QStringLiteral("app-launch");
    case ButtonAction::PresetRef:        return QStringLiteral("preset");
    case ButtonAction::DBus:             return QStringLiteral("dbus");
    case ButtonAction::Keystroke:        return QStringLiteral("keystroke");
    case ButtonAction::StickyModifier:   return QStringLiteral("sticky");

    // Profiles written by older releases store media keys as "media:...".
    // The picker has no separate row for them, so they surface — and are
    // injected — as ordinary keystrokes.
    case ButtonAction::Media:            return QStringLiteral("keystroke");

    case ButtonAction::Default:          return QStringLiteral("default");
    }
    return QStringLiteral("default");
}

QString ActionModel::buttonActionToName(const ButtonAction &ba) const
{
    if (ba.type == ButtonAction::Default)
        return QString();

    // Every row is identified by its (actionType, payload) pair, and the
    // payload-less action types each own exactly one row, so one uniform
    // lookup names all of them.
    const QString type = buttonActionToType(ba);
    for (const auto &a : m_actions) {
        if (a.actionType == type && a.payload == ba.payload)
            return a.name;
    }

    // A custom binding the picker has no row for — show the payload itself.
    return ba.payload;
}

ButtonAction ActionModel::buttonEntryToAction(const QString &actionType, const QString &actionName) const
{
    if (actionType == QStringLiteral("default"))
        return {ButtonAction::Default, {}};
    if (actionType == QStringLiteral("gesture-trigger"))
        return {ButtonAction::GestureTrigger, {}};
    if (actionType == QStringLiteral("smartshift-toggle"))
        return {ButtonAction::SmartShiftToggle, {}};
    if (actionType == QStringLiteral("dpi-cycle"))
        return {ButtonAction::DpiCycle, {}};
    if (actionType == QStringLiteral("keystroke")) {
        QString payload = payloadForName(actionName);
        if (payload.isEmpty() && actionName != QStringLiteral("Keyboard shortcut"))
            payload = actionName;
        return {ButtonAction::Keystroke, payload};
    }
    if (actionType == QStringLiteral("sticky")) {
        QString payload = payloadForName(actionName);
        if (payload.isEmpty()) payload = actionName;
        return {ButtonAction::StickyModifier, payload};
    }
    if (actionType == QStringLiteral("app-launch")) {
        QString payload = payloadForName(actionName);
        if (payload.isEmpty()) payload = actionName;
        return {ButtonAction::AppLaunch, payload};
    }
    // No picker row spells a D-Bus call — only a hand-edited profile does —
    // but the type still has to survive a UI save, or editing any other
    // button would quietly reset this one.
    if (actionType == QStringLiteral("dbus"))
        return {ButtonAction::DBus, actionName};
    if (actionType == QStringLiteral("preset")) {
        QString payload = payloadForName(actionName);
        if (payload.isEmpty()) payload = actionName;
        return {ButtonAction::PresetRef, payload};
    }
    return {ButtonAction::Default, {}};
}

} // namespace logitune
