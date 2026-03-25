#include "ProfileModel.h"

namespace logitune {

ProfileModel::ProfileModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Seed with the always-present "Defaults" entry
    m_profiles.append(ProfileEntry{
        QStringLiteral("Defaults"),
        QStringLiteral("\u229E"), // ⊞ squared plus
        QString{}
    });
}

int ProfileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_profiles.size();
}

QVariant ProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profiles.size())
        return {};

    const ProfileEntry &entry = m_profiles.at(index.row());
    switch (role) {
    case NameRole:    return entry.name;
    case IconRole:    return entry.icon;
    case WmClassRole: return entry.wmClass;
    case IsActiveRole: return (index.row() == m_activeIndex);
    default: return {};
    }
}

QHash<int, QByteArray> ProfileModel::roleNames() const
{
    return {
        { NameRole,     "name"     },
        { IconRole,     "icon"     },
        { WmClassRole,  "wmClass"  },
        { IsActiveRole, "isActive" },
    };
}

int ProfileModel::activeIndex() const
{
    return m_activeIndex;
}

void ProfileModel::addProfile(const QString &wmClass, const QString &name)
{
    const int row = m_profiles.size();
    beginInsertRows({}, row, row);
    m_profiles.append(ProfileEntry{ name, QString{}, wmClass });
    endInsertRows();
}

void ProfileModel::removeProfile(int index)
{
    if (index <= 0 || index >= m_profiles.size()) // never remove "Defaults" (index 0)
        return;

    beginRemoveRows({}, index, index);
    m_profiles.removeAt(index);
    endRemoveRows();

    if (m_activeIndex >= m_profiles.size()) {
        m_activeIndex = m_profiles.size() - 1;
        emit activeIndexChanged();
    }
}

void ProfileModel::setActiveByWmClass(const QString &wmClass)
{
    // Empty wmClass means no match — stay on Defaults (index 0)
    if (wmClass.isEmpty()) {
        setActiveByIndex(0);
        return;
    }

    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).wmClass.compare(wmClass, Qt::CaseInsensitive) == 0) {
            setActiveByIndex(i);
            return;
        }
    }

    // No matching profile — fall back to Defaults
    setActiveByIndex(0);
}

void ProfileModel::setActiveByIndex(int index)
{
    if (index < 0 || index >= m_profiles.size())
        return;
    if (m_activeIndex == index)
        return;

    const int prev = m_activeIndex;
    m_activeIndex = index;

    // Notify the two rows whose isActive state changed
    emit dataChanged(createIndex(prev, 0),  createIndex(prev, 0),  { IsActiveRole });
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), { IsActiveRole });

    emit activeIndexChanged();
    emit profileSwitched(m_profiles.at(index).name);
}

} // namespace logitune
