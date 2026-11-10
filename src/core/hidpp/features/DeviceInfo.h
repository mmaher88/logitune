#pragma once
#include "hidpp/HidppTypes.h"
#include <array>
#include <cstdint>
#include <optional>

namespace logitune::hidpp::features {

// Reply to DeviceInformation (0x0003) getDeviceInfo.
struct DeviceInfoIds {
    uint8_t entityCount = 0;
    // Bitfield: 0x01 Bluetooth, 0x02 Bluetooth LE, 0x04 eQuad (receiver),
    // 0x08 USB. modelIds holds one PID per set bit, in that order.
    uint16_t transports = 0;
    std::array<uint16_t, 3> modelIds{};
    // Tells apart models that share every PID, e.g. MX Master 3 and
    // MX Master 3 for Mac. Absent when the reply is too short to carry it.
    std::optional<uint8_t> extendedModelId;
};

class DeviceInfo {
public:
    // Layout as in Solaar's hidpp20.get_ids(): [0] entity count,
    // [1..4] unit ID, [5..6] transports, [7..12] model IDs, then
    // [13] extended model ID.
    static DeviceInfoIds parseDeviceInfo(const Report &r);

    static constexpr uint8_t kFnGetDeviceInfo = 0x00;
};

} // namespace logitune::hidpp::features
