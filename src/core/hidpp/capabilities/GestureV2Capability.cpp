#include "hidpp/capabilities/GestureV2Capability.h"

namespace logitune::hidpp::capabilities {

const GestureV2Variant kGestureV2Variants[1] = {
    {
        FeatureId::GestureV2,
        features::GestureV2::kFnSetEnabled,
        features::GestureV2::kFnSetDiverted,
        &features::GestureV2::parseDivertedGestureEvent,
        &features::GestureV2::buildSetDiverted,
    },
};

} // namespace logitune::hidpp::capabilities
