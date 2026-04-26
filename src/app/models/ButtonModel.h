#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <cstdint>
#include <qqmlintegration.h>

namespace logitune {

struct ButtonEntry {
    int buttonId;
    QString buttonName;
    QString actionName;
    QString actionType;
    uint16_t controlId;  // HID++ CID; 0x0000 = virtual thumb wheel
};

struct ButtonAssignment {
    QString actionName;
    QString actionType;
    uint16_t controlId;
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
    Q_INVOKABLE bool isThumbWheel(int buttonId) const;

    /// Programmatic bulk update -- does NOT emit per-row dataChanged.
    /// Emits a single modelReset so QML rebinds all at once.
    void loadFromProfile(const QList<ButtonAssignment> &assignments);

signals:
    void userActionChanged(int buttonId, const QString &actionName, const QString &actionType);

private:
    QList<ButtonEntry> m_buttons;
};

} // namespace logitune
