#pragma once
#include <cstdint>
#include <vector>
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::features {

struct ThumbWheelConfig {
    bool invert;
    int  resolution;  // 0–100 range
};

/// Life cycle of one thumb-wheel roll (event byte 4).
enum class ThumbWheelRotationStatus : uint8_t {
    Inactive = 0,
    Start    = 1,
    Active   = 2,
    Stop     = 3,
};

/// Diverted `thumbwheelEvent` / ThumbwheelStatusUpdate (feature 0x2150).
struct ThumbWheelEvent {
    int16_t rotation = 0;
    uint16_t timeElapsed = 0;
    ThumbWheelRotationStatus rotationStatus = ThumbWheelRotationStatus::Inactive;
    bool touch = false;
    bool proxy = false;
    bool singleTap = false;
};

class ThumbWheel {
public:
    static ThumbWheelConfig      parseConfig(const Report &r);
    static std::vector<uint8_t>  buildSetConfig(bool invert);
    static ThumbWheelEvent       parseEvent(const Report &r);

    static constexpr uint8_t kFnGetConfig = 0x00;
    static constexpr uint8_t kFnSetConfig = 0x01;

    static constexpr uint8_t kEvTouch     = 0x02;
    static constexpr uint8_t kEvProxy     = 0x04;
    static constexpr uint8_t kEvSingleTap = 0x08;
};

} // namespace logitune::hidpp::features
