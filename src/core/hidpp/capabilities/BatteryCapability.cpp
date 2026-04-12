#include "hidpp/capabilities/BatteryCapability.h"

namespace logitune::hidpp::capabilities {

const BatteryVariant kBatteryVariants[2] = {
    {
        FeatureId::BatteryUnified,
        features::Battery::kFnGetStatus,     // 0x01
        &features::Battery::parseStatus,
    },
    {
        FeatureId::BatteryStatus,
        0x00,                                 // fn0 for legacy
        &features::Battery::parseStatusLegacy,
    },
};

} // namespace logitune::hidpp::capabilities
