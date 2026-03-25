#pragma once
#include "hidpp/HidppTypes.h"
#include <array>
#include <cstdint>

namespace logitune::hidpp::features {

struct DPISensorInfo {
    int currentDPI;
    int minDPI;
    int maxDPI;
    int stepDPI;
};

class AdjustableDPI {
public:
    static DPISensorInfo parseSensorInfo(const Report &r);
    static int parseCurrentDPI(const Report &r);
    static std::array<uint8_t, 3> buildSetDPI(int dpi, uint8_t sensorIndex = 0);

    static constexpr uint8_t kFnGetSensorCount = 0x00;
    static constexpr uint8_t kFnGetSensorDPI   = 0x01;
    static constexpr uint8_t kFnSetSensorDPI   = 0x02;
};

} // namespace logitune::hidpp::features
