#pragma once
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::capabilities {

// Range a force-sensing button accepts, in the device's own units (MX Master 4 panel: 3842..6388).
struct ForceButtonInfo {
    bool changeable = false;
    int defaultForce = 0;
    int maxForce = 0;
    int minForce = 0;
};

// Feature 0x19C0: buttons that trigger at a configurable press force, like the MX Master 4's
// haptic thumb panel. Function numbers follow Solaar's ForceSensingButton.
struct ForceSensingVariant {
    FeatureId feature;
    uint8_t countFn;
    uint8_t infoFn;
    uint8_t currentFn;
    uint8_t setFn;
    ForceButtonInfo (*parseInfo)(const Report &reply);
    int (*parseCurrent)(const Report &reply);
    std::vector<uint8_t> (*buildSet)(uint8_t button, int force);
};

extern const ForceSensingVariant kForceSensingVariants[1];

} // namespace logitune::hidpp::capabilities
