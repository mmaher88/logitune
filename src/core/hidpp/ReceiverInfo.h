#pragma once
#include "hidpp/HidppTypes.h"
#include <cstdint>
#include <optional>

namespace logitune::hidpp {

// HID++ 1.0 receiver register 0x2B5 (RECEIVER_INFO). Its per-slot pairing
// sub-registers hold the paired device's wireless PID (WPID), which is the
// device's own product ID. The hidraw node of a receiver-attached device
// reports the receiver's PID instead, so this is how a device on a receiver
// is identified. Layout follows Solaar
// (logitech_receiver/receiver.py, device_pairing_information):
//
//   Bolt     sub 0x50 + slot      reply: [sub, kind, wpid lo, wpid hi, serial x4]
//   Unifying sub 0x20 + slot - 1  reply: [sub, dest, interval, wpid hi, wpid lo, ..]
class ReceiverInfo {
public:
    static constexpr uint8_t kSubIdGetLongRegister = 0x83;
    static constexpr uint8_t kRegReceiverInfo      = 0xB5;  // register 0x2B5
    static constexpr uint8_t kBoltPairingInfo      = 0x50;
    static constexpr uint8_t kUnifyingPairingInfo  = 0x20;

    static bool isReceiver(uint16_t pid);

    // Read request for the pairing info of `slot` (1-6) on a receiver with
    // USB PID `receiverPid`. nullopt if `receiverPid` is not a receiver.
    static std::optional<Report> buildPairingInfoRequest(uint16_t receiverPid,
                                                         uint8_t slot);

    // WPID from the reply to buildPairingInfoRequest(receiverPid, slot).
    // nullopt if the reply is not that register read or the slot is empty.
    static std::optional<uint16_t> parseWpid(uint16_t receiverPid, uint8_t slot,
                                             const Report &reply);
};

} // namespace logitune::hidpp
