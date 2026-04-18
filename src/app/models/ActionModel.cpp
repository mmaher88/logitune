#include "ActionModel.h"

namespace logitune {

ActionModel::ActionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    //                name                   description                                  actionType         payload
    m_actions = {
        { "Back",                 "Navigate backward in browser/file manager",  "keystroke",       "Alt+Left"    },
        { "Brightness down",      "Decrease display brightness",                "keystroke",       "BrightnessDown" },
        { "Brightness up",        "Increase display brightness",                "keystroke",       "BrightnessUp" },
        { "Calculator",           "Open the system calculator",                 "app-launch",      "kcalc"       },
        { "Close window",         "Close the active window",                    "keystroke",       "Alt+F4"      },
        { "Copy",                 "Copy selected content to clipboard",         "keystroke",       "Ctrl+C"      },
        { "Cut",                  "Cut selected content to clipboard",          "keystroke",       "Ctrl+X"      },
        { "DPI cycle",            "Step through the device's DPI preset list",  "dpi-cycle",       ""            },
        { "Do nothing",           "Button is disabled",                         "none",            ""            },
        { "Forward",              "Navigate forward in browser/file manager",   "keystroke",       "Alt+Right"   },
        { "Gestures",             "Trigger gesture recognition",                "gesture-trigger", ""            },
        { "Keyboard shortcut",    "Send a custom key combination",              "keystroke",       ""            },
        { "Media controls",       "Control media playback",                     "media-controls",  ""            },
        { "Middle click",         "Emulate middle mouse button click",          "default",         ""            },
        { "Mute",                 "Toggle audio mute",                          "keystroke",       "Mute"        },
        { "Paste",                "Paste clipboard content",                    "keystroke",       "Ctrl+V"      },
        { "Play/Pause",           "Play or pause media",                        "keystroke",       "Play"        },
        { "Redo",                 "Redo last undone action",                    "keystroke",       "Ctrl+Shift+Z"},
        { "Screenshot",           "Capture a screenshot",                       "keystroke",       "Print"       },
        { "Shift wheel mode",     "Toggle scroll wheel ratchet/freespin",       "smartshift-toggle", ""          },
        { "Show desktop",         "Minimize all windows to show desktop",       "keystroke",       "Super+D"     },
        { "Switch desktop left",  "Switch to the virtual desktop on the left",  "keystroke",       "Ctrl+Super+Left" },
        { "Switch desktop right", "Switch to the virtual desktop on the right", "keystroke",       "Ctrl+Super+Right" },
        { "Task switcher",        "Open the window/task switcher",              "keystroke",       "Super+W"     },
        { "Undo",                 "Undo the last action",                       "keystroke",       "Ctrl+Z"      },
        { "Volume down",          "Decrease system volume",                     "keystroke",       "VolumeDown"  },
        { "Volume up",            "Increase system volume",                     "keystroke",       "VolumeUp"    },
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

} // namespace logitune
