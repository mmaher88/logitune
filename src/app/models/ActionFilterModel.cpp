#include "ActionFilterModel.h"
#include "ActionModel.h"
#include "DeviceModel.h"

namespace logitune {

ActionFilterModel::ActionFilterModel(DeviceModel *deviceModel, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_deviceModel(deviceModel)
{
    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::selectedChanged,
                this, [this]() { invalidateFilter(); });
    }
}

bool ActionFilterModel::filterAcceptsRow(int sourceRow,
                                         const QModelIndex &sourceParent) const
{
    // Before any device is selected (e.g. startup before udev scan completes)
    // the picker should show every action. ActionsPanel only opens from a
    // hotspot click on a selected device, so this branch is mostly defensive.
    if (!m_deviceModel || m_deviceModel->selectedIndex() < 0)
        return true;

    const QString type = sourceModel()->data(
        sourceModel()->index(sourceRow, 0, sourceParent),
        ActionModel::ActionTypeRole).toString();

    if (type == QLatin1String("dpi-cycle"))
        return m_deviceModel->adjustableDpiSupported();
    if (type == QLatin1String("smartshift-toggle"))
        return m_deviceModel->smartShiftSupported();
    if (type == QLatin1String("gesture-trigger"))
        return m_deviceModel->reprogControlsSupported();
    if (type == QLatin1String("wheel-mode"))
        return m_deviceModel->thumbWheelSupported();
    return true;
}

} // namespace logitune
