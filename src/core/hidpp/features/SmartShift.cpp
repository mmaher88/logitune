#include "hidpp/features/SmartShift.h"

namespace logitune::hidpp::features {

// Parse GetStatus (function 0) response
// From logid: response[0]=mode (1=freespin, 2=ratchet), response[1]=autoDisengage, response[2]=default
SmartShiftConfig SmartShift::parseConfig(const Report &r)
{
    SmartShiftConfig cfg{};
    cfg.mode = r.params[0];
    cfg.autoDisengage = r.params[1];
    cfg.defaultAutoDisengage = r.params[2];
    return cfg;
}

// Build SetStatus (function 1) request
// From logid: [0]=mode (0=don't change, 1=freespin, 2=ratchet), [1]=autoDisengage (0=don't change)
std::vector<uint8_t> SmartShift::buildSetConfig(uint8_t mode, uint8_t autoDisengage)
{
    return { mode, autoDisengage };
}

} // namespace logitune::hidpp::features
