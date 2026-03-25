#include "hidpp/features/Battery.h"

namespace logitune::hidpp::features {

BatteryStatus Battery::parseStatus(const Report &r)
{
    BatteryStatus status;
    status.level = static_cast<int>(r.params[0]);
    // params[1] = battery status flags:
    //   bit 0 (0x01) = discharging
    //   bit 1 (0x02) = recharging
    //   bit 2 (0x04) = charge almost done
    //   bit 3 (0x08) = charge complete
    uint8_t flags = r.params[1];
    status.charging = (flags & 0x06) != 0; // recharging or almost done
    return status;
}

} // namespace logitune::hidpp::features
