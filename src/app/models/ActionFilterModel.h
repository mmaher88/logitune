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
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
public:
    explicit ActionFilterModel(DeviceModel *deviceModel,
                               IDesktopIntegration *desktop = nullptr,
                               const ActionPresetRegistry *registry = nullptr,
                               QObject *parent = nullptr);

    QVariantList entries() const;
    void setGestureMode(bool mode);
    void setRadialMode(bool mode);

signals:
    void entriesChanged();

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
    IDesktopIntegration *m_desktop;
    const ActionPresetRegistry *m_registry;
    bool m_gestureMode = false;
    bool m_radialMode = false;
};

} // namespace logitune
