#include "ButtonModel.h"

namespace logitune {

ButtonModel::ButtonModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Default button assignments — hardcoded until ProfileEngine integration.
    // controlIds line up with Jelco's canonical MX-family descriptor ordering
    // so ButtonModel::isThumbWheel returns the right answer before a real
    // device descriptor has loaded.
    m_buttons = {
        { 0, "Left click",   "Left click",        "default",           0x0050 },
        { 1, "Right click",  "Right click",       "default",           0x0051 },
        { 2, "Middle click", "Middle click",      "default",           0x0052 },
        { 3, "Back",         "Back",              "default",           0x0053 },
        { 4, "Forward",      "Forward",           "default",           0x0056 },
        { 5, "Gesture",      "Gestures",          "gesture-trigger",   0x00C3 },
        { 6, "Shift wheel",  "Shift wheel mode",  "smartshift-toggle", 0x00C4 },
        { 7, "Thumb wheel",  "Horizontal scroll", "default",           0x0000 },
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
            emit userActionChanged(buttonId, actionName, actionType);
            return;
        }
    }
}

void ButtonModel::loadFromProfile(const QList<ButtonAssignment> &assignments)
{
    beginResetModel();
    for (int i = 0; i < assignments.size() && i < m_buttons.size(); ++i) {
        m_buttons[i].actionName = assignments[i].actionName;
        m_buttons[i].actionType = assignments[i].actionType;
        m_buttons[i].controlId  = assignments[i].controlId;
    }
    endResetModel();

    if (!m_buttons.isEmpty())
        emit dataChanged(index(0), index(m_buttons.size() - 1));
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

bool ButtonModel::isThumbWheel(int buttonId) const
{
    for (const auto &entry : m_buttons) {
        if (entry.buttonId == buttonId)
            return entry.controlId == 0x0000;
    }
    return false;
}

} // namespace logitune
