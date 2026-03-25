#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <qqmlintegration.h>

namespace logitune {

struct ActionEntry {
    QString name;
    QString description;
    QString actionType;
};

class ActionModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        ActionTypeRole,
    };

    explicit ActionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedIndex() const;
    void setSelectedIndex(int index);

    Q_INVOKABLE int indexForName(const QString &name) const;

signals:
    void selectedIndexChanged();

private:
    QList<ActionEntry> m_actions;
    int m_selectedIndex = -1;
};

} // namespace logitune
