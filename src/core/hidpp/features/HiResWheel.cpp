#include "hidpp/features/HiResWheel.h"

namespace logitune::hidpp::features {

ScrollConfig HiResWheel::parseWheelMode(const Report &r)
{
    // Mode byte: bit0=diversion, bit1=hiRes, bit2=invert
    uint8_t mode = r.params[0];
    ScrollConfig cfg;
    cfg.hiRes   = (mode & 0x02) != 0;
    cfg.invert  = (mode & 0x04) != 0;
    cfg.ratchet = false; // filled separately via getRatchetSwitch
    return cfg;
}

bool HiResWheel::parseRatchetSwitch(const Report &r)
{
    return (r.params[0] & 0x01) != 0;
}

std::vector<uint8_t> HiResWheel::buildSetWheelMode(uint8_t currentMode, bool hiRes, bool invert)
{
    // Force bit 0 (diversion target) to 0 = hardware/native. logitune never
    // consumes software-diverted wheel movement, so if the wheel comes back
    // diverted-to-software (e.g. a KVM round-trip through a host whose driver
    // left it that way), preserving that bit would silently kill scrolling.
    // currentMode is intentionally ignored for the target bit.
    (void)currentMode;
    uint8_t newMode = 0;
    if (hiRes)  newMode |= 0x02;
    if (invert) newMode |= 0x04;
    return { newMode };
}

} // namespace logitune::hidpp::features
