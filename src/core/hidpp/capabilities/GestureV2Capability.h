#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/features/GestureV2.h"

namespace logitune::hidpp::capabilities {

// One Gesture 2 variant: feature ID + setDiverted function ID + decoder/builder
// pointers. Used by devices that route the thumb wheel through 0x6501 gesture
// diversion instead of the dedicated ThumbWheel feature 0x2150 (e.g. the
// 1st-generation MX Master).
struct GestureV2Variant {
    FeatureId feature;
    uint8_t   setEnabledFn;
    uint8_t   setDivertedFn;
    logitune::hidpp::features::DivertedGestureEvent (*parseDivertedEvent)(
        const logitune::hidpp::Report&);
    // Both setEnabled and setDiverted use the same {offset,count,mask,value}
    // payload layout, just different function IDs — so one builder serves both.
    std::vector<uint8_t> (*buildOffsetMaskRequest)(uint8_t index, bool enable);
};

// Only one variant today; kept as a table for symmetry with the other
// capabilities and so additional 0x6501 dialects can be slotted in later.
extern const GestureV2Variant kGestureV2Variants[1];

} // namespace logitune::hidpp::capabilities
