#pragma once
#include <cstdint>
#include <vector>
#include "hidpp/HidppTypes.h"

namespace logitune::hidpp::features {

// SmartShift V1 (0x2110) — from logid/Solaar source
// Mode byte: 0=don't change, 1=freespin, 2=ratchet/SmartShift active
// AutoDisengage: 0=don't change, 1-254=threshold, 255=always ratcheted

struct SmartShiftConfig {
    uint8_t mode;            // 1=freespin, 2=ratchet
    uint8_t autoDisengage;   // threshold 1-255
    uint8_t defaultAutoDisengage;

    bool isRatchet() const { return mode == 2; }
    bool isFreespin() const { return mode == 1; }
};

class SmartShift {
public:
    // Function 0: GetStatus → [mode, autoDisengage, defaultAutoDisengage]
    static SmartShiftConfig parseConfig(const Report &r);

    // Function 1: SetStatus → [mode, autoDisengage]
    // mode: 0=don't change, 1=freespin, 2=ratchet
    // autoDisengage: 0=don't change, 1-255=threshold
    static std::vector<uint8_t> buildSetConfig(uint8_t mode, uint8_t autoDisengage);

    static constexpr uint8_t kFnGetStatus = 0x00;
    static constexpr uint8_t kFnSetStatus = 0x01;
};

} // namespace logitune::hidpp::features
