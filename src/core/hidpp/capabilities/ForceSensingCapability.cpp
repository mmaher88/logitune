#include "ForceSensingCapability.h"

namespace logitune::hidpp::capabilities {

static int bigEndian16(const Report &reply, int offset)
{
    return (reply.params[offset] << 8) | reply.params[offset + 1];
}

// Info reply: big-endian u16s flags (bit 0 = changeable), default, max, min.
static ForceButtonInfo parseInfo(const Report &reply)
{
    ForceButtonInfo info;
    info.changeable = bigEndian16(reply, 0) & 0x01;
    info.defaultForce = bigEndian16(reply, 2);
    info.maxForce = bigEndian16(reply, 4);
    info.minForce = bigEndian16(reply, 6);
    return info;
}

static int parseCurrent(const Report &reply)
{
    return bigEndian16(reply, 0);
}

static std::vector<uint8_t> buildSet(uint8_t button, int force)
{
    const auto high = static_cast<uint8_t>((force >> 8) & 0xFF);
    const auto low = static_cast<uint8_t>(force & 0xFF);
    return {button, high, low};
}

const ForceSensingVariant kForceSensingVariants[1] = {
    {FeatureId::ForceSensingButton, 0, 1, 2, 3, &parseInfo, &parseCurrent, &buildSet},
};

} // namespace logitune::hidpp::capabilities
