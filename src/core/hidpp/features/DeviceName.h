#pragma once
#include "hidpp/HidppTypes.h"
#include <QString>
#include <cstdint>

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

    static constexpr uint8_t kFnGetNameLength = 0x00;
    static constexpr uint8_t kFnGetName       = 0x01;
    static constexpr uint8_t kFnGetDeviceType = 0x02;
};

} // namespace logitune::hidpp::features
