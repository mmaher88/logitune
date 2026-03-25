#include "hidpp/features/ThumbWheel.h"

namespace logitune::hidpp::features {

ThumbWheelConfig ThumbWheel::parseConfig(const Report &r)
{
    ThumbWheelConfig cfg{};
    cfg.invert     = (r.params[0] & 0x01) != 0;
    cfg.resolution = static_cast<int>(r.params[1]);
    return cfg;
}

std::vector<uint8_t> ThumbWheel::buildSetConfig(bool invert)
{
    return {invert ? uint8_t{0x01} : uint8_t{0x00}};
}

} // namespace logitune::hidpp::features
