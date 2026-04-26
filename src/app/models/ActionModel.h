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
    };

    explicit ActionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedIndex() const;
    void setSelectedIndex(int index);

    Q_INVOKABLE int indexForName(const QString &name) const;
    Q_INVOKABLE QString payloadForName(const QString &name) const;

    /// Translate a ButtonAction (domain type) to the (typeName, displayName)
    /// pair the UI uses. Inverse of buttonEntryToAction.
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
