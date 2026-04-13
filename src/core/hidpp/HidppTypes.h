#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <vector>
#include <optional>

namespace logitune::hidpp {

constexpr uint8_t kShortReportId = 0x10;   // 7 bytes total
constexpr uint8_t kLongReportId  = 0x11;   // 20 bytes total
constexpr int kShortReportSize = 7;
constexpr int kLongReportSize  = 20;

constexpr uint8_t kDeviceIndexDirect   = 0xFF;

constexpr uint16_t kVendorLogitech    = 0x046d;
constexpr uint16_t kPidBoltReceiver   = 0xc548;
constexpr uint16_t kPidUnifyReceiver  = 0xc52b;
constexpr uint16_t kPidMxMaster3s    = 0xb034;

enum class FeatureId : uint16_t {
    Root            = 0x0000,
    FeatureSet      = 0x0001,
    DeviceInfo      = 0x0003,
    DeviceName      = 0x0005,
    BatteryStatus   = 0x1000,
    BatteryUnified  = 0x1004,
    ChangeHost      = 0x1814,
    ReprogControls    = 0x1b00,
    ReprogControlsV2  = 0x1b01,
    ReprogControlsV2_2= 0x1b02,
    ReprogControlsV3  = 0x1b03,
    ReprogControlsV4  = 0x1b04,
    SmartShift         = 0x2110,
    SmartShiftEnhanced = 0x2111,
    HiResWheel      = 0x2121,
    ThumbWheel      = 0x2150,
    AdjustableDPI   = 0x2201,
    GestureV2       = 0x6501,
};

enum class ErrorCode : uint8_t {
    NoError          = 0x00,
    Unknown          = 0x01,
    InvalidArgument  = 0x02,
    OutOfRange       = 0x03,
    HwError          = 0x04,
    Busy             = 0x05,
    Unsupported      = 0x09,
    InvalidAddress   = 0x0B,
};

struct Report {
    uint8_t reportId{};
    uint8_t deviceIndex{};
    uint8_t featureIndex{};
    uint8_t functionId{};     // upper 4 bits of byte[3]
    uint8_t softwareId{};     // lower 4 bits of byte[3]
    std::array<uint8_t, 16> params{};
    int paramLength{};

    std::vector<uint8_t> serialize() const;
    static std::optional<Report> parse(std::span<const uint8_t> data);
    bool isError() const;
    ErrorCode errorCode() const;
};

} // namespace logitune::hidpp
