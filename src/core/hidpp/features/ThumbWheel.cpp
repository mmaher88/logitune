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

ThumbWheelEvent ThumbWheel::parseEvent(const Report &r)
{
    ThumbWheelEvent evt{};
    evt.rotation = static_cast<int16_t>(
        (static_cast<uint16_t>(r.params[0]) << 8) | r.params[1]);
    evt.timeElapsed = static_cast<uint16_t>(
        (static_cast<uint16_t>(r.params[2]) << 8) | r.params[3]);

    switch (r.params[4]) {
    case 1:
        evt.rotationStatus = ThumbWheelRotationStatus::Start;
        break;
    case 2:
        evt.rotationStatus = ThumbWheelRotationStatus::Active;
        break;
    case 3:
        evt.rotationStatus = ThumbWheelRotationStatus::Stop;
        break;
    default:
        evt.rotationStatus = ThumbWheelRotationStatus::Inactive;
        break;
    }

    const uint8_t flags = r.params[5];
    evt.touch     = (flags & kEvTouch) != 0;
    evt.proxy     = (flags & kEvProxy) != 0;
    evt.singleTap = (flags & kEvSingleTap) != 0;
    return evt;
}

} // namespace logitune::hidpp::features
