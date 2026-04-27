#pragma once
#include "hidpp/HidppTypes.h"
#include <cstdint>
#include <vector>

namespace logitune::hidpp::features {

struct GestureEvent {
    int dx;         // horizontal displacement delta
    int dy;         // vertical displacement delta
    bool released;  // true if thumb button was released (end of gesture)
};

// Notification emitted by feature 0x6501 when a gesture is diverted to the host.
// Payload layout (params[0..3]):
//   byte 0 — status: 0x11 touch start, 0x23 movement, 0x31 touch end
//   byte 1 — reserved (always 0x00 in observed traffic)
//   byte 2 — gesture-class marker. Constant 0x03 for thumb-wheel events on
//            every MX Master generation observed; treated as a signature
//            here, not a dynamic per-device index.
//   byte 3 — signed int8 movement delta (only meaningful when status == 0x23)
struct DivertedGestureEvent {
    uint8_t status;
    uint8_t gestureMarker;
    int8_t  delta;
};

class GestureV2 {
public:
    static GestureEvent parseGestureEvent(const Report &r);
    static std::vector<uint8_t> buildSetGestureEnable(bool enable);

    static DivertedGestureEvent parseDivertedGestureEvent(const Report &r);

    // Encodes a setDiverted request for the gesture at the given diversion index.
    // Matches Solaar's feature_request(GESTURE_2, 0x40, offset, 0x01, mask, value)
    // pattern: byte offset into the divert bitmap, count, modify-mask, write-value.
    static std::vector<uint8_t> buildSetDiverted(uint8_t diversionIndex, bool enable);

    static constexpr uint8_t kFnSetGestureEnable = 0x05;
    static constexpr uint8_t kFnSetEnabled       = 0x02;
    static constexpr uint8_t kFnSetDiverted      = 0x04;

    // Marker byte that distinguishes thumb-wheel rotation events from other
    // diverted gestures (e.g. a future swipe path) within the Gesture 2 stream.
    static const uint8_t kThumbWheelEventMarker;
};

} // namespace logitune::hidpp::features
