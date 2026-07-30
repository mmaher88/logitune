#pragma once
#include "ButtonAction.h"
#include <QAbstractListModel>
#include <QObject>
#include <qqmlintegration.h>

namespace logitune {

struct ActionEntry {
    QString name;
    QString description;
    QString actionType;
    QString payload;  // e.g. "Ctrl+C" for keystroke, "" for default
    QString category;
};

class ActionModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        ActionTypeRole,
        PayloadRole,
        CategoryRole,
    };

    explicit ActionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedIndex() const;
    void setSelectedIndex(int index);

    Q_INVOKABLE int indexForName(const QString &name) const;
    Q_INVOKABLE QString payloadForName(const QString &name) const;

    /// The UI's type token for a ButtonAction. Together with
    /// buttonActionToName this is the (actionType, actionName) pair the UI
    /// speaks in, and the inverse of buttonEntryToAction.
    QString buttonActionToType(const ButtonAction &ba) const;

    /// The UI's display name for a ButtonAction, resolved by looking up the
    /// row whose (actionType, payload) the action carries.
    QString buttonActionToName(const ButtonAction &ba) const;

    /// Translate the UI's (typeName, displayName) pair back to a ButtonAction.
    /// Inverse of buttonActionToName.
    ButtonAction buttonEntryToAction(const QString &actionType, const QString &actionName) const;

signals:
    void selectedIndexChanged();

private:
    QList<ActionEntry> m_actions;
    int m_selectedIndex = -1;
};

} // namespace logitune
