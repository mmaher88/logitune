#include "hidpp/features/Battery.h"

namespace logitune::hidpp::features {

BatteryStatus Battery::parseStatus(const Report &r)
{
    BatteryStatus status;
    status.level    = static_cast<int>(r.params[0]);
    status.charging = (r.params[1] == 0x03);
    return status;
}

} // namespace logitune::hidpp::features
