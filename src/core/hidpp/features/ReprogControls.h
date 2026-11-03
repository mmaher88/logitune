#pragma once
#include "hidpp/HidppTypes.h"
#include <cstdint>
#include <vector>
#include <utility>

namespace logitune::hidpp::features {

struct ControlInfo {
    uint16_t controlId;
    uint16_t taskId;
    bool divertable;
    bool persist;
    bool rawXY;        // supports raw XY diversion (for gestures)
};

struct RawXYEvent {
    int16_t dx;
    int16_t dy;
};

class DivertedButtons {
public:
    std::vector<std::pair<uint16_t, bool>> update(const Report &report);
    void clear() { m_held.clear(); }
private:
    std::vector<uint16_t> m_held;
};

class ReprogControls {
public:
    static int parseControlCount(const Report &r);
    static ControlInfo parseControlInfo(const Report &r);
    // divert: basic button diversion. rawXY: also divert raw mouse movement (for gestures)
    static std::vector<uint8_t> buildSetDivert(uint16_t controlId, bool divert, bool rawXY = false);
    static uint16_t parseDivertedButtonEvent(const Report &r);
    static RawXYEvent parseDivertedRawXYEvent(const Report &r);

    static constexpr uint8_t kFnGetControlCount     = 0x00;
    static constexpr uint8_t kFnGetControlInfo       = 0x01;
    static constexpr uint8_t kFnSetControlReporting  = 0x03;
};

} // namespace logitune::hidpp::features
