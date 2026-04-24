#pragma once
#include <QSortFilterProxyModel>

namespace logitune {

class DeviceModel;
class IDesktopIntegration;
class ActionPresetRegistry;

/// Hides actions whose required device capability is absent on the
/// currently-selected device, and preset rows whose current DE doesn't
/// support them. Source model must be an ActionModel.
class ActionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ActionFilterModel(DeviceModel *deviceModel,
                               IDesktopIntegration *desktop = nullptr,
                               const ActionPresetRegistry *registry = nullptr,
                               QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
    IDesktopIntegration *m_desktop;
    const ActionPresetRegistry *m_registry;
};

} // namespace logitune
