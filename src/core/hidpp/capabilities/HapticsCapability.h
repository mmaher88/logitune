#pragma once
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::capabilities {

struct HapticConfig {
    bool enabled = false;
    int level = 0;               // 0..100
    bool fourLevelsOnly = false; // device only accepts 25/50/75/100
};

struct HapticsVariant {
    FeatureId feature;
    uint8_t playFn;
    std::vector<uint8_t> (*buildPlay)(uint8_t waveform);
    uint8_t getConfigFn;
    uint8_t setConfigFn;
    HapticConfig (*parseConfig)(const Report &reply);
    std::vector<uint8_t> (*buildSetConfig)(bool enabled, int level);
};

extern const HapticsVariant kHapticsVariants[1];

} // namespace logitune::hidpp::capabilities
