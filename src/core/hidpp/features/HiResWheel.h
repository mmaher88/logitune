#pragma once
#include <cstdint>
#include <vector>
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::features {

struct ScrollConfig {
    bool hiRes;
    bool invert;
    bool ratchet;  // true = ratchet, false = freespin
};

class HiResWheel {
public:
    static ScrollConfig          parseConfig(const Report &r);
    static std::vector<uint8_t>  buildSetConfig(bool hiRes, bool invert);

    static constexpr uint8_t kFnGetWheelMode    = 0x00;
    static constexpr uint8_t kFnSetWheelMode    = 0x02;
    static constexpr uint8_t kFnGetRatchetSwitch = 0x03;
};

} // namespace logitune::hidpp::features
