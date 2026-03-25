#pragma once
#include "hidpp/HidppTypes.h"
#include <cstdint>

namespace logitune::hidpp::features {

struct BatteryStatus {
    int level;     // 0-100 percentage
    bool charging;
};

class Battery {
public:
    static BatteryStatus parseStatus(const Report &r);

    static constexpr uint8_t kFnGetCapabilities = 0x00;
    static constexpr uint8_t kFnGetStatus = 0x01;
};

} // namespace logitune::hidpp::features
