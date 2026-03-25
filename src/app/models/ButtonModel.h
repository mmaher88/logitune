#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <qqmlintegration.h>

namespace logitune {

struct ButtonEntry {
    int buttonId;
    QString buttonName;
    QString actionName;
    QString actionType;
};

class ButtonModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        ButtonIdRole = Qt::UserRole + 1,
        ButtonNameRole,
        ActionNameRole,
        ActionTypeRole,
    };

    explicit ButtonModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setAction(int buttonId, const QString &actionName, const QString &actionType);
    Q_INVOKABLE QString actionNameForButton(int buttonId) const;
    Q_INVOKABLE QString actionTypeForButton(int buttonId) const;

private:
    QList<ButtonEntry> m_buttons;
};

} // namespace logitune
