#pragma once
#include <cstdint>
#include <vector>
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::features {

struct ThumbWheelConfig {
    bool invert;
    int  resolution;  // 0–100 range
};

class ThumbWheel {
public:
    static ThumbWheelConfig      parseConfig(const Report &r);
    static std::vector<uint8_t>  buildSetConfig(bool invert);

    static constexpr uint8_t kFnGetConfig = 0x00;
    static constexpr uint8_t kFnSetConfig = 0x01;
};

} // namespace logitune::hidpp::features
