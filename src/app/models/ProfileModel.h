#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <qqmlintegration.h>

namespace logitune {

class ProfileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int displayIndex READ displayIndex NOTIFY displayIndexChanged)

public:
    enum Roles {
        NameRole       = Qt::UserRole + 1,
        IconRole,
        WmClassRole,
        IsActiveRole,      // true if this is the user's selected tab
        IsHwActiveRole     // true if this is the hardware-active profile
    };

    explicit ProfileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int displayIndex() const;

    Q_INVOKABLE void addProfile(const QString &wmClass, const QString &name, const QString &icon = QString());
    Q_INVOKABLE void removeProfile(int index);
    Q_INVOKABLE void selectTab(int index);                      // user click -> emits profileSwitched

    /// Restore an existing profile entry at startup — no selectTab, no profileAdded signal.
    void restoreProfile(const QString &wmClass, const QString &name, const QString &icon = QString());

    void setHwActiveByProfileName(const QString &profileName);   // window focus -> hw indicator only
    void setHwActiveIndex(int index);                           // hw indicator only, no profileSwitched

signals:
    void displayIndexChanged();
    void profileSwitched(const QString &profileName);
    void profileAdded(const QString &wmClass, const QString &name);
    void profileRemoved(const QString &wmClass);

private:
    struct ProfileEntry {
        QString name;
        QString icon;
        QString wmClass;
    };

    QVector<ProfileEntry> m_profiles;
    int m_displayIndex   = 0;   // which tab the user is viewing
    int m_hwActiveIndex  = 0;   // which profile is active on hardware
};

} // namespace logitune
