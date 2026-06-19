#include "hidpp/features/GestureV2.h"

namespace logitune::hidpp::features {

GestureEvent GestureV2::parseGestureEvent(const Report &r)
{
    GestureEvent evt;
    evt.dx       = static_cast<int>(static_cast<int16_t>((r.params[0] << 8) | r.params[1]));
    evt.dy       = static_cast<int>(static_cast<int16_t>((r.params[2] << 8) | r.params[3]));
    evt.released = (r.params[4] == 0x01);
    return evt;
}

std::vector<uint8_t> GestureV2::buildSetGestureEnable(bool enable)
{
    return { static_cast<uint8_t>(enable ? 0x01 : 0x00) };
}

DivertedGestureEvent GestureV2::parseDivertedGestureEvent(const Report &r)
{
    return DivertedGestureEvent{
        r.params[0],
        r.params[2],
        static_cast<int8_t>(r.params[3]),
    };
}

// Constant byte-2 marker that identifies a thumb-wheel rotation event in the
// Gesture 2 stream. Same value across MX Master generations.
const uint8_t GestureV2::kThumbWheelEventMarker = 0x03;

std::vector<uint8_t> GestureV2::buildSetDiverted(uint8_t diversionIndex, bool enable)
{
    const uint8_t offset = static_cast<uint8_t>(diversionIndex >> 3);
    const uint8_t mask   = static_cast<uint8_t>(1u << (diversionIndex & 0x07));
    return { offset, 0x01, mask, enable ? mask : uint8_t{0x00} };
}

} // namespace logitune::hidpp::features
