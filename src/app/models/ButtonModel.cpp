#include "ButtonModel.h"

namespace logitune {

ButtonModel::ButtonModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Default button assignments — hardcoded until ProfileEngine integration
    m_buttons = {
        { 0, "Left click",   "Left click",        "default"         },
        { 1, "Right click",  "Right click",       "default"         },
        { 2, "Middle click", "Middle click",      "default"         },
        { 3, "Back",         "Back",              "default"         },
        { 4, "Forward",      "Forward",           "default"         },
        { 5, "Thumb",        "Gesture button",    "default"         },
        { 6, "Top",          "Shift wheel mode",  "default"         },
        { 7, "Thumb wheel",  "Horizontal scroll", "default"         },
    };
}

int ButtonModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_buttons.size();
}

QVariant ButtonModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_buttons.size())
        return {};

    const ButtonEntry &entry = m_buttons[index.row()];
    switch (role) {
    case ButtonIdRole:   return entry.buttonId;
    case ButtonNameRole: return entry.buttonName;
    case ActionNameRole: return entry.actionName;
    case ActionTypeRole: return entry.actionType;
    default:             return {};
    }
}

QHash<int, QByteArray> ButtonModel::roleNames() const
{
    return {
        { ButtonIdRole,   "buttonId"   },
        { ButtonNameRole, "buttonName" },
        { ActionNameRole, "actionName" },
        { ActionTypeRole, "actionType" },
    };
}

void ButtonModel::setAction(int buttonId, const QString &actionName, const QString &actionType)
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i].buttonId == buttonId) {
            m_buttons[i].actionName = actionName;
            m_buttons[i].actionType = actionType;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, { ActionNameRole, ActionTypeRole });
            return;
        }
    }
}

QString ButtonModel::actionNameForButton(int buttonId) const
{
    for (const auto &entry : m_buttons) {
        if (entry.buttonId == buttonId)
            return entry.actionName;
    }
    return {};
}

QString ButtonModel::actionTypeForButton(int buttonId) const
{
    for (const auto &entry : m_buttons) {
        if (entry.buttonId == buttonId)
            return entry.actionType;
    }
    return {};
}

} // namespace logitune
