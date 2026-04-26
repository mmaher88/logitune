#pragma once
#include <QSortFilterProxyModel>

namespace logitune {

class DeviceModel;

/// Hides actions whose required device capability is absent on the
/// currently-selected device. Source model must be an ActionModel.
/// Reinvalidates its filter whenever DeviceModel::selectedChanged fires.
class ActionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ActionFilterModel(DeviceModel *deviceModel,
                               QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
};

} // namespace logitune
