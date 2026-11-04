#include "HapticsCapability.h"

#include <algorithm>

namespace logitune::hidpp::capabilities {

static std::vector<uint8_t> buildPlay(uint8_t waveform) { return {waveform}; }

// Reply: byte 0 bit 0 = feedback on, byte 1 = level (0..100), byte 2 bit 0 = only four levels.
static HapticConfig parseConfig(const Report &reply)
{
    HapticConfig config;
    config.enabled = reply.params[0] & 0x01;
    config.level = reply.params[1];
    config.fourLevelsOnly = reply.params[2] & 0x01;
    return config;
}

static std::vector<uint8_t> buildSetConfig(bool enabled, int level)
{
    const auto clamped = static_cast<uint8_t>(std::clamp(level, 0, 100));
    return {static_cast<uint8_t>(enabled ? 0x01 : 0x00), clamped};
}

const HapticsVariant kHapticsVariants[1] = {
    {FeatureId::Haptics, 4, &buildPlay, 1, 2, &parseConfig, &buildSetConfig},
};

} // namespace logitune::hidpp::capabilities
