#pragma once
#include "hidpp/HidppTypes.h"
#include <QString>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace logitune::hidpp::features {

struct DeviceNameInfo {
    QString name;
    QString serial;
    uint8_t deviceType;
};

class DeviceName {
public:
    static int     parseNameLength(const Report &r);
    static QString parseNameChunk(const Report &r);
    static QString parseSerial(const Report &r);

    // Sends one getName (0x01) request with the given parameter bytes and
    // returns the reply, or nullopt on failure.
    using NameFragmentFetcher =
        std::function<std::optional<Report>(std::span<const uint8_t> params)>;

    // Reads a name of nameLen characters (from getNameLength) by calling
    // getName until the whole name has been read. Returns the part read so
    // far if a fragment request fails.
    static QString readName(int nameLen, const NameFragmentFetcher &fetch);

    static constexpr uint8_t kFnGetNameLength = 0x00;
    static constexpr uint8_t kFnGetName       = 0x01;
    static constexpr uint8_t kFnGetDeviceType = 0x02;
};

} // namespace logitune::hidpp::features
