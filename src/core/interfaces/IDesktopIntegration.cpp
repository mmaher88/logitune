// MOC requires a .cpp for abstract QObject interfaces with signals.
#include "interfaces/IDesktopIntegration.h"

namespace logitune {

void IDesktopIntegration::requestActionRingPlacement(int requestId, const QSize &,
                                                      const QPoint &)
{
    emit actionRingPlacementFailed(requestId);
}

void IDesktopIntegration::cancelActionRingPlacement(int)
{
}

} // namespace logitune
