#include "hidpp/ReceiverInfo.h"

namespace logitune::hidpp {

namespace {

std::optional<uint8_t> pairingSubRegister(uint16_t receiverPid, uint8_t slot)
{
    if (slot < 1 || slot > 6)
        return std::nullopt;
    if (receiverPid == kPidBoltReceiver)
        return static_cast<uint8_t>(ReceiverInfo::kBoltPairingInfo + slot);
    if (receiverPid == kPidUnifyReceiver)
        return static_cast<uint8_t>(ReceiverInfo::kUnifyingPairingInfo + slot - 1);
    return std::nullopt;
}

} // namespace

bool ReceiverInfo::isReceiver(uint16_t pid)
{
    return pid == kPidBoltReceiver || pid == kPidUnifyReceiver;
}

std::optional<Report> ReceiverInfo::buildPairingInfoRequest(uint16_t receiverPid,
                                                            uint8_t slot)
{
    const auto sub = pairingSubRegister(receiverPid, slot);
    if (!sub)
        return std::nullopt;

    // HID++ 1.0 frame: [0x10, 0xFF (receiver), sub ID, register, params..].
    // Report's featureIndex / function / software ID slots carry the sub ID
    // and register address byte.
    Report r;
    r.reportId     = kShortReportId;
    r.deviceIndex  = kDeviceIndexDirect;
    r.featureIndex = kSubIdGetLongRegister;
    r.functionId   = (kRegReceiverInfo >> 4) & 0x0F;
    r.softwareId   = kRegReceiverInfo & 0x0F;
    r.params[0]    = *sub;
    r.paramLength  = 3;
    return r;
}

std::optional<uint16_t> ReceiverInfo::parseWpid(uint16_t receiverPid, uint8_t slot,
                                                const Report &reply)
{
    const auto sub = pairingSubRegister(receiverPid, slot);
    if (!sub)
        return std::nullopt;

    const uint8_t address = static_cast<uint8_t>((reply.functionId << 4) | reply.softwareId);
    if (reply.featureIndex != kSubIdGetLongRegister || address != kRegReceiverInfo
        || reply.params[0] != *sub)
        return std::nullopt;

    uint16_t wpid = 0;
    if (receiverPid == kPidBoltReceiver)
        wpid = static_cast<uint16_t>((reply.params[3] << 8) | reply.params[2]);
    else
        wpid = static_cast<uint16_t>((reply.params[3] << 8) | reply.params[4]);

    if (wpid == 0)
        return std::nullopt;
    return wpid;
}

} // namespace logitune::hidpp
