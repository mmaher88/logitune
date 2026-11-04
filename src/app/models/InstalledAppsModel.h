#pragma once
#include <QAbstractListModel>
#include <QVariantMap>

namespace logitune {

class InstalledAppsModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit InstalledAppsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int row) const;
    void reload(const QStringList &directories, const QString &locale = {});
private:
    QList<QVariantMap> m_apps;
};

} // namespace logitune
