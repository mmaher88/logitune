#pragma once
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::capabilities {

struct HapticsVariant {
    FeatureId feature;
    uint8_t playFn;
    std::vector<uint8_t> (*buildPlay)(uint8_t waveform);
};

extern const HapticsVariant kHapticsVariants[1];

} // namespace logitune::hidpp::capabilities
