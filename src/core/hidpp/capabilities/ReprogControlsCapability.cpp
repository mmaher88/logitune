#include "hidpp/capabilities/ReprogControlsCapability.h"

namespace logitune::hidpp::capabilities {

// Preference order: pick the highest version the device advertises.
// Only V4 actually supports SetControlReporting (button diversion); the
// older versions can still enumerate buttons via fn 0/1 but button
// remapping will be a no-op on them.
const ReprogControlsVariant kReprogControlsVariants[5] = {
    { FeatureId::ReprogControlsV4,   true  },
    { FeatureId::ReprogControlsV3,   false },
    { FeatureId::ReprogControlsV2_2, false },
    { FeatureId::ReprogControlsV2,   false },
    { FeatureId::ReprogControls,     false },
};

} // namespace logitune::hidpp::capabilities
