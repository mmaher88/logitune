#include "ActionFilterModel.h"
#include "ActionModel.h"
#include "DeviceModel.h"
#include "actions/ActionPresetRegistry.h"
#include "interfaces/IDesktopIntegration.h"

namespace logitune {

ActionFilterModel::ActionFilterModel(DeviceModel *deviceModel,
                                     IDesktopIntegration *desktop,
                                     const ActionPresetRegistry *registry,
                                     QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_deviceModel(deviceModel)
    , m_desktop(desktop)
    , m_registry(registry)
{
    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::selectedChanged,
                this, [this]() { invalidateFilter(); });
    }
}

void ActionFilterModel::setGestureMode(bool mode)
{
    if (m_gestureMode == mode) return;
    m_gestureMode = mode;
    invalidateFilter();
}

bool ActionFilterModel::filterAcceptsRow(int sourceRow,
                                         const QModelIndex &sourceParent) const
{
    const QString type = sourceModel()->data(
        sourceModel()->index(sourceRow, 0, sourceParent),
        ActionModel::ActionTypeRole).toString();

    // Gesture-mode filter: hide "Gestures" itself (recursive: a gesture
    // direction can't trigger gestures) and "Do nothing" (already
    // represented by the None sentinel at the top of the picker). Other
    // device-only actions (DPI cycle, Shift wheel mode, Middle click)
    // stay in the list; the dispatcher currently no-ops on them and
    // can be extended later to handle them as gesture actions.
    if (m_gestureMode) {
        if (type == QLatin1String("gesture-trigger")) return false;
        if (type == QLatin1String("none")) return false;
    }

    // Preset filtering: hide if not supported on the current DE variant.
    if (type == QLatin1String("preset")) {
        if (!m_desktop || !m_registry)
            return true;   // startup race: show; dispatcher will no-op if unresolvable
        const QString id = sourceModel()->data(
            sourceModel()->index(sourceRow, 0, sourceParent),
            ActionModel::PayloadRole).toString();
        if (!m_registry->supportedBy(id, m_desktop->variantKey()))
            return false;
        // Second gate: live resolution (catches empty user bindings on GNOME etc.)
        return m_desktop->resolveNamedAction(id).has_value();
    }

    // Capability filtering (unchanged): hide actions whose required device
    // capability is absent on the current device.
    if (!m_deviceModel || m_deviceModel->selectedIndex() < 0)
        return true;

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
