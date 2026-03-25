#include "hidpp/features/SmartShift.h"

namespace logitune::hidpp::features {

SmartShiftConfig SmartShift::parseConfig(const Report &r)
{
    SmartShiftConfig cfg{};
    cfg.enabled   = (r.params[0] == 0x01);
    cfg.threshold = static_cast<int>(r.params[1]);
    // params[2..3] = auto-disengage speed (ignored)
    return cfg;
}

std::vector<uint8_t> SmartShift::buildSetConfig(bool enabled, int threshold)
{
    return {
        0x02,                                          // set both flags
        enabled ? uint8_t{0x01} : uint8_t{0x00},
        static_cast<uint8_t>(threshold),
    };
}

} // namespace logitune::hidpp::features
