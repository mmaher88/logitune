#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/features/Battery.h"

namespace logitune::hidpp::capabilities {

// One Battery variant: a feature ID + its get function + a parser.
struct BatteryVariant {
    FeatureId feature;
    uint8_t   getFn;
    logitune::hidpp::features::BatteryStatus (*parse)(const logitune::hidpp::Report&);
};

// Known battery variants in preference order.
// UnifiedBattery (0x1004) is preferred when present because it exposes the
// level bitmask fallback. Legacy BatteryStatus (0x1000) is used otherwise.
extern const BatteryVariant kBatteryVariants[2];

} // namespace logitune::hidpp::capabilities
