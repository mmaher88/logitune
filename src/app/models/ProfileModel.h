#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <qqmlintegration.h>

namespace logitune {

class ProfileModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)

public:
    enum Roles {
        NameRole    = Qt::UserRole + 1,
        IconRole,
        WmClassRole,
        IsActiveRole
    };

    explicit ProfileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeIndex() const;

    Q_INVOKABLE void addProfile(const QString &wmClass, const QString &name);
    Q_INVOKABLE void removeProfile(int index);
    Q_INVOKABLE void setActiveByWmClass(const QString &wmClass);
    Q_INVOKABLE void setActiveByIndex(int index);

signals:
    void activeIndexChanged();
    void profileSwitched(const QString &profileName);

private:
    struct ProfileEntry {
        QString name;
        QString icon;
        QString wmClass;
    };

    QVector<ProfileEntry> m_profiles;
    int m_activeIndex = 0; // 0 = "Defaults"
};

} // namespace logitune
