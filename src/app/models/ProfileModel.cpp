#include "ProfileModel.h"

namespace logitune {

ProfileModel::ProfileModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Seed with the always-present default entry
    // Name matches the profile file basename ("default.conf")
    // No theme icon: AppProfileBar draws its own "all applications" mark for
    // this entry, since no icon-theme name covers it on every desktop.
    m_profiles.append(ProfileEntry{
        QStringLiteral("Defaults"),
        QString(),
        QStringLiteral("default")  // wmClass doubles as profile key for default
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
    case NameRole:       return entry.name;
    case IconRole:       return entry.icon;
    case WmClassRole:    return entry.wmClass;
    case IsActiveRole:   return (index.row() == m_displayIndex);
    case IsHwActiveRole: return (index.row() == m_hwActiveIndex);
    default: return {};
    }
}

QHash<int, QByteArray> ProfileModel::roleNames() const
{
    return {
        { NameRole,       "name"       },
        { IconRole,       "icon"       },
        { WmClassRole,    "wmClass"    },
        { IsActiveRole,   "isActive"   },
        { IsHwActiveRole, "isHwActive" },
    };
}

int ProfileModel::displayIndex() const
{
    return m_displayIndex;
}

void ProfileModel::addProfile(const QString &wmClass, const QString &name, const QString &icon)
{
    // Prevent duplicate wmClass bindings
    for (const auto &entry : m_profiles) {
        if (entry.wmClass.compare(wmClass, Qt::CaseInsensitive) == 0)
            return;
    }

    const int row = m_profiles.size();
    beginInsertRows({}, row, row);
    m_profiles.append(ProfileEntry{ name, icon, wmClass });
    endInsertRows();

    emit profileAdded(wmClass, name);

    // Auto-switch to the newly added profile
    selectTab(row);
}

void ProfileModel::restoreProfile(const QString &wmClass, const QString &name, const QString &icon)
{
    for (const auto &entry : m_profiles) {
        if (entry.wmClass.compare(wmClass, Qt::CaseInsensitive) == 0)
            return;
    }

    const int row = m_profiles.size();
    beginInsertRows({}, row, row);
    m_profiles.append(ProfileEntry{ name, icon, wmClass });
    endInsertRows();
    // No profileAdded, no selectTab — profile already exists on disk
}

void ProfileModel::removeProfile(int index)
{
    if (index <= 0 || index >= m_profiles.size()) // never remove "Defaults" (index 0)
        return;

    const QString wmClass = m_profiles.at(index).wmClass;

    beginRemoveRows({}, index, index);
    m_profiles.removeAt(index);
    endRemoveRows();

    // Handle display index
    if (m_displayIndex == index) {
        // Display tab was removed — fall back to Defaults
        const int oldDisplay = m_displayIndex;
        m_displayIndex = 0;
        emit dataChanged(createIndex(0, 0), createIndex(0, 0), { IsActiveRole });
        if (oldDisplay < m_profiles.size())
            emit dataChanged(createIndex(oldDisplay, 0), createIndex(oldDisplay, 0), { IsActiveRole });
        emit displayIndexChanged();
        emit profileSwitched(QStringLiteral("default"));
    } else if (m_displayIndex >= m_profiles.size()) {
        m_displayIndex = m_profiles.size() - 1;
        emit displayIndexChanged();
    } else if (m_displayIndex > index) {
        // Shift down since a row before displayIndex was removed
        m_displayIndex--;
        emit displayIndexChanged();
    }

    // Handle hw active index
    if (m_hwActiveIndex == index) {
        const int oldHw = m_hwActiveIndex;
        m_hwActiveIndex = 0;
        emit dataChanged(createIndex(0, 0), createIndex(0, 0), { IsHwActiveRole });
        if (oldHw < m_profiles.size())
            emit dataChanged(createIndex(oldHw, 0), createIndex(oldHw, 0), { IsHwActiveRole });
    } else if (m_hwActiveIndex >= m_profiles.size()) {
        m_hwActiveIndex = m_profiles.size() - 1;
    } else if (m_hwActiveIndex > index) {
        m_hwActiveIndex--;
    }

    emit profileRemoved(wmClass);
}

void ProfileModel::selectTab(int index)
{
    if (index < 0 || index >= m_profiles.size())
        return;
    if (m_displayIndex == index)
        return;

    const int prev = m_displayIndex;
    m_displayIndex = index;

    emit dataChanged(createIndex(prev, 0),  createIndex(prev, 0),  { IsActiveRole });
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), { IsActiveRole });
    emit displayIndexChanged();

    // For the default entry (index 0), emit "default" to match the profile file name
    emit profileSwitched(index == 0 ? QStringLiteral("default") : m_profiles.at(index).name);
}

void ProfileModel::setHwActiveIndex(int index)
{
    if (index < 0 || index >= m_profiles.size())
        return;
    if (m_hwActiveIndex == index)
        return;

    const int prev = m_hwActiveIndex;
    m_hwActiveIndex = index;

    emit dataChanged(createIndex(prev, 0),  createIndex(prev, 0),  { IsHwActiveRole });
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), { IsHwActiveRole });
    // NOTE: intentionally does NOT emit profileSwitched — that's only for user tab clicks
}

int ProfileModel::indexForProfileName(const QString &profileName) const
{
    // Match by profile name — avoids wmClass mismatch (e.g. "dolphin" vs "org.kde.dolphin").
    // Default is always index 0.
    if (profileName.isEmpty() || profileName == QStringLiteral("default"))
        return 0;
    for (int i = 1; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).name.compare(profileName, Qt::CaseInsensitive) == 0)
            return i;
    }
    return 0;
}

void ProfileModel::setHwActiveByProfileName(const QString &profileName)
{
    setHwActiveIndex(indexForProfileName(profileName));
}

void ProfileModel::resetProfiles(const QList<AppTab> &appTabs,
                                 const QString &displayProfile,
                                 const QString &hwProfile)
{
    beginResetModel();
    m_profiles.resize(1);  // keep "Defaults"
    for (const auto &tab : appTabs) {
        bool duplicate = false;
        for (const auto &entry : m_profiles) {
            if (entry.wmClass.compare(tab.wmClass, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            m_profiles.append(ProfileEntry{ tab.name, tab.icon, tab.wmClass });
    }
    m_displayIndex  = indexForProfileName(displayProfile);
    m_hwActiveIndex = indexForProfileName(hwProfile);
    endResetModel();

    emit displayIndexChanged();
}

} // namespace logitune
