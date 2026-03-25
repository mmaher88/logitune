#pragma once
#include <cstdint>
#include <vector>
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::features {

struct SmartShiftConfig {
    bool enabled;
    int  threshold;  // 1–255
};

class SmartShift {
public:
    static SmartShiftConfig      parseConfig(const Report &r);
    static std::vector<uint8_t>  buildSetConfig(bool enabled, int threshold);

    static constexpr uint8_t kFnGetConfig = 0x00;
    static constexpr uint8_t kFnSetConfig = 0x01;
};

} // namespace logitune::hidpp::features
