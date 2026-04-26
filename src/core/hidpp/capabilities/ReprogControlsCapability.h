#pragma once
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::capabilities {

// ReprogControls comes in five versions with the same enumeration API
// (GetControlCount fn 0, GetControlInfo fn 1) but only V4 adds the
// reporting/diversion API (GetControlReporting fn 2, SetControlReporting
// fn 3). V1-V3 devices can enumerate their buttons but cannot have them
// diverted — button remapping is unavailable on those devices.
struct ReprogControlsVariant {
    FeatureId feature;
    bool      supportsDiversion;  // only V4
};

// Variants in preference order: V4 first (so modern mice pick it), then
// V3 → V1 as fallbacks for older hardware where we can still read button
// metadata even if we can't divert.
extern const ReprogControlsVariant kReprogControlsVariants[5];

} // namespace logitune::hidpp::capabilities
