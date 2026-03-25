#include "hidpp/features/AdjustableDPI.h"

namespace logitune::hidpp::features {

static inline int readBE16(const std::array<uint8_t, 16> &params, int offset)
{
    return (static_cast<int>(params[offset]) << 8) | static_cast<int>(params[offset + 1]);
}

DPISensorInfo AdjustableDPI::parseSensorInfo(const Report &r)
{
    DPISensorInfo info;
    info.currentDPI = readBE16(r.params, 0);
    // params[2] = sensor count (ignored in struct)
    info.minDPI  = readBE16(r.params, 3);
    info.maxDPI  = readBE16(r.params, 5);
    info.stepDPI = readBE16(r.params, 7);
    return info;
}

int AdjustableDPI::parseCurrentDPI(const Report &r)
{
    return readBE16(r.params, 0);
}

std::array<uint8_t, 3> AdjustableDPI::buildSetDPI(int dpi, uint8_t sensorIndex)
{
    return {
        sensorIndex,
        static_cast<uint8_t>((dpi >> 8) & 0xFF),
        static_cast<uint8_t>(dpi & 0xFF)
    };
}

} // namespace logitune::hidpp::features
