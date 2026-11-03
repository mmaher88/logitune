#include "HapticsCapability.h"

namespace logitune::hidpp::capabilities {

static std::vector<uint8_t> buildPlay(uint8_t waveform) { return {waveform}; }

const HapticsVariant kHapticsVariants[1] = {{FeatureId::Haptics, 4, &buildPlay}};

} // namespace logitune::hidpp::capabilities
