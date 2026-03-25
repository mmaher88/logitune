#include "hidpp/features/HiResWheel.h"

namespace logitune::hidpp::features {

ScrollConfig HiResWheel::parseConfig(const Report &r)
{
    ScrollConfig cfg{};
    cfg.hiRes   = (r.params[0] & 0x01) != 0;
    cfg.invert  = (r.params[0] & 0x02) != 0;
    // params[1]: 0x01 = ratchet, 0x02 = freespin
    cfg.ratchet = (r.params[1] == 0x01);
    return cfg;
}

std::vector<uint8_t> HiResWheel::buildSetConfig(bool hiRes, bool invert)
{
    uint8_t flags = 0;
    if (hiRes)  flags |= 0x01;
    if (invert) flags |= 0x02;
    return {flags};
}

} // namespace logitune::hidpp::features
