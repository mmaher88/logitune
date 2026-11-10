#include <gtest/gtest.h>
#include "hidpp/features/Battery.h"
#include "hidpp/features/AdjustableDPI.h"
#include "hidpp/features/DeviceName.h"
#include "hidpp/features/DeviceInfo.h"

#include <QByteArray>
#include <vector>

using namespace logitune::hidpp;
using namespace logitune::hidpp::features;

// ---------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------

// UnifiedBattery format: [percentage, levelBitmask, BatteryState, reserved]
TEST(Battery, ParseDischarging) {
    Report r;
    r.params[0] = 78;   // 78%
    r.params[1] = 0x08; // level bitmask: full
    r.params[2] = 0x00; // BatteryState::Discharging
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 78);
    EXPECT_EQ(status.state, BatteryState::Discharging);
    EXPECT_FALSE(status.charging);
}

TEST(Battery, ParseRecharging) {
    Report r;
    r.params[0] = 50;
    r.params[1] = 0x04; // level bitmask: good
    r.params[2] = 0x01; // BatteryState::Recharging
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 50);
    EXPECT_EQ(status.state, BatteryState::Recharging);
    EXPECT_TRUE(status.charging);
}

TEST(Battery, ParseFull) {
    Report r;
    r.params[0] = 100;
    r.params[1] = 0x08;
    r.params[2] = 0x03; // BatteryState::Full
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 100);
    EXPECT_EQ(status.state, BatteryState::Full);
    EXPECT_TRUE(status.charging); // Full still means plugged in
}

TEST(Battery, ParseSlowRecharge) {
    Report r;
    r.params[0] = 95;
    r.params[1] = 0x08;
    r.params[2] = 0x04; // BatteryState::SlowRecharge
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 95);
    EXPECT_EQ(status.state, BatteryState::SlowRecharge);
    EXPECT_TRUE(status.charging);
}

TEST(Battery, ParseZeroPercentUsesLevelBitmask) {
    Report r;
    r.params[0] = 0;    // percentage 0 = use bitmask
    r.params[1] = 0x02; // level bitmask: low
    r.params[2] = 0x00; // discharging
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 20); // low = 20%
    EXPECT_FALSE(status.charging);
}

TEST(Battery, ParseZeroPercentCritical) {
    Report r;
    r.params[0] = 0;
    r.params[1] = 0x01; // critical
    r.params[2] = 0x01; // recharging
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 5); // critical = 5%
    EXPECT_TRUE(status.charging);
}

// ---------------------------------------------------------------------------
// Battery (BATTERY_STATUS 0x1000 legacy format)
// ---------------------------------------------------------------------------
// Legacy format:
//   params[0] = current discharge level (percentage)
//   params[1] = next discharge threshold (ignored)
//   params[2] = status byte (same enum as UnifiedBattery)

TEST(BatteryLegacy, ParseDischarging) {
    Report r;
    r.params[0] = 78;    // 78%
    r.params[1] = 50;    // next threshold (ignored)
    r.params[2] = 0x00;  // BatteryState::Discharging
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 78);
    EXPECT_EQ(status.state, BatteryState::Discharging);
    EXPECT_FALSE(status.charging);
}

TEST(BatteryLegacy, ParseRecharging) {
    Report r;
    r.params[0] = 45;
    r.params[1] = 30;
    r.params[2] = 0x01;  // BatteryState::Recharging
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 45);
    EXPECT_EQ(status.state, BatteryState::Recharging);
    EXPECT_TRUE(status.charging);
}

TEST(BatteryLegacy, ParseFull) {
    Report r;
    r.params[0] = 100;
    r.params[1] = 90;
    r.params[2] = 0x03;  // BatteryState::Full
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 100);
    EXPECT_EQ(status.state, BatteryState::Full);
    EXPECT_TRUE(status.charging);
}

TEST(BatteryLegacy, IgnoresMiddleByte) {
    // Legacy format's params[1] must NOT be interpreted as a level bitmask.
    // If parseStatusLegacy accidentally fell through to bitmask logic, it
    // would mis-report percentage. Setting params[0]=0 guards the fallback path.
    Report r;
    r.params[0] = 0;    // would trigger bitmask fallback in parseStatus()
    r.params[1] = 0x08; // looks like "full" bitmask but is next-threshold %
    r.params[2] = 0x00;
    auto status = Battery::parseStatusLegacy(r);
    EXPECT_EQ(status.level, 0);  // must stay 0, not become 90
    EXPECT_FALSE(status.charging);
}

// ---------------------------------------------------------------------------
// AdjustableDPI
// ---------------------------------------------------------------------------

TEST(AdjustableDPI, ParseSensorDpiList) {
    // Function 1 response: [sensorIdx, minDPI_hi, minDPI_lo, ?, step, maxDPI_hi, maxDPI_lo]
    Report r;
    r.params[0] = 0x00;                      // sensor index
    r.params[1] = 0x00; r.params[2] = 0xC8; // minDPI = 200
    r.params[3] = 0xE0;                      // unknown
    r.params[4] = 0x32;                      // step = 50
    r.params[5] = 0x1F; r.params[6] = 0x40; // maxDPI = 8000
    auto info = AdjustableDPI::parseSensorDpiList(r);
    EXPECT_EQ(info.minDPI, 200);
    EXPECT_EQ(info.maxDPI, 8000);
    EXPECT_EQ(info.stepDPI, 50);
}

TEST(AdjustableDPI, ParseCurrentDPI) {
    // Function 2 response: [sensorIdx, dpi_hi, dpi_lo, ...]
    Report r;
    r.params[0] = 0x00;
    r.params[1] = 0x03; r.params[2] = 0xE8; // 1000
    EXPECT_EQ(AdjustableDPI::parseCurrentDPI(r), 1000);
}

TEST(AdjustableDPI, BuildSetDPIRequest) {
    // 1600 = 0x0640
    auto params = AdjustableDPI::buildSetDPI(1600, 0);
    EXPECT_EQ(params[0], 0x00); // sensorIndex
    EXPECT_EQ(params[1], 0x06); // high byte
    EXPECT_EQ(params[2], 0x40); // low byte
}

TEST(AdjustableDPI, BuildSetDPIHighSensor) {
    // 800 = 0x0320
    auto params = AdjustableDPI::buildSetDPI(800, 1);
    EXPECT_EQ(params[0], 0x01);
    EXPECT_EQ(params[1], 0x03);
    EXPECT_EQ(params[2], 0x20);
}

TEST(AdjustableDPI, BuildSetDPIRoundTrip) {
    int dpi = 3200;
    auto params = AdjustableDPI::buildSetDPI(dpi, 0);
    int decoded = (static_cast<int>(params[1]) << 8) | static_cast<int>(params[2]);
    EXPECT_EQ(decoded, dpi);
}

// ---------------------------------------------------------------------------
// DeviceName
// ---------------------------------------------------------------------------

TEST(DeviceName, ParseNameLength) {
    Report r;
    r.params[0] = 14;
    r.paramLength = 1;
    auto len = DeviceName::parseNameLength(r);
    EXPECT_EQ(len, 14);
}

TEST(DeviceName, ParseNameChunk) {
    Report r;
    r.params[0] = 'M'; r.params[1] = 'X'; r.params[2] = ' ';
    r.params[3] = 'M'; r.params[4] = 'a'; r.params[5] = 's';
    r.paramLength = 6;
    auto name = DeviceName::parseNameChunk(r);
    EXPECT_EQ(name, "MX Mas");
}

TEST(DeviceName, ParseNameChunkEmpty) {
    Report r;
    r.paramLength = 0;
    auto name = DeviceName::parseNameChunk(r);
    EXPECT_TRUE(name.isEmpty());
}

TEST(DeviceName, ParseNameLengthZero) {
    Report r;
    r.params[0] = 0;
    r.paramLength = 1;
    EXPECT_EQ(DeviceName::parseNameLength(r), 0);
}

namespace {

// Stands in for a device answering getName (0x0005 fn 0x01): the single
// parameter byte is the index of the first character to return, and each
// long-report reply carries up to 16 characters from there (zero padded).
struct FakeNameDevice {
    QByteArray name;
    std::vector<std::vector<uint8_t>> requests;

    std::optional<Report> operator()(std::span<const uint8_t> params) {
        requests.emplace_back(params.begin(), params.end());
        Report r;
        r.reportId = kLongReportId;
        r.paramLength = 16;
        const int index = params.empty() ? 0 : params[0];
        const QByteArray fragment = name.mid(index, 16);
        for (int i = 0; i < fragment.size(); ++i)
            r.params[i] = static_cast<uint8_t>(fragment[i]);
        return r;
    }
};

} // namespace

TEST(DeviceName, ReadNameLongerThanOneFragment) {
    // 25 characters: needs two getName calls. Each call sends the index of
    // the next unread character as a single byte (Solaar get_name()).
    FakeNameDevice dev{QByteArrayLiteral("MX Master 3S For Business"), {}};
    const QString name = DeviceName::readName(
        25, [&](std::span<const uint8_t> p) { return dev(p); });

    EXPECT_EQ(name, QStringLiteral("MX Master 3S For Business"));
    const std::vector<std::vector<uint8_t>> expected = {{0x00}, {0x10}};
    EXPECT_EQ(dev.requests, expected);
}

TEST(DeviceName, ReadNameFitsInOneFragment) {
    FakeNameDevice dev{QByteArrayLiteral("MX Master 3S"), {}};
    const QString name = DeviceName::readName(
        12, [&](std::span<const uint8_t> p) { return dev(p); });

    EXPECT_EQ(name, QStringLiteral("MX Master 3S"));
    EXPECT_EQ(dev.requests.size(), 1u);
}

TEST(DeviceName, ReadNameStopsWhenAFragmentFails) {
    int calls = 0;
    const QString name = DeviceName::readName(
        25, [&](std::span<const uint8_t>) -> std::optional<Report> {
            ++calls;
            return std::nullopt;
        });

    EXPECT_TRUE(name.isEmpty());
    EXPECT_EQ(calls, 1);
}

TEST(DeviceName, ParseSerialReturnsASCII) {
    Report r;
    r.params[0] = 'A'; r.params[1] = 'B'; r.params[2] = 'C';
    r.params[3] = '1'; r.params[4] = '2'; r.params[5] = '3';
    r.paramLength = 6;
    auto serial = DeviceName::parseSerial(r);
    EXPECT_EQ(serial, "ABC123");
}

// ---------------------------------------------------------------------------
// DeviceInfo (0x0003) getDeviceInfo
// ---------------------------------------------------------------------------

// A getDeviceInfo reply off the wire. Layout as Solaar's hidpp20.get_ids()
// reads it: [0] entity count, [1..4] unit ID, [5..6] transport bits,
// [7..12] model IDs (one 16-bit PID per transport bit, Bluetooth first),
// then [13] the extended model ID.
static Report getDeviceInfoReply(uint8_t extendedModelId)
{
    const std::array<uint8_t, 20> raw = {
        0x11, 0xFF, 0x02, 0x00,              // long report, feature index 2, fn 0
        0x03,                                // 3 firmware entities
        0x12, 0x34, 0x56, 0x78,              // unit ID
        0x00, 0x06,                          // Bluetooth LE | eQuad
        0xB0, 0x23, 0x40, 0x82, 0x00, 0x00,  // BLE PID B023, WPID 4082
        extendedModelId,
        0x00, 0x00,
    };
    return *Report::parse(raw);
}

TEST(DeviceInfo, ParsesGetDeviceInfoReply) {
    const auto info = DeviceInfo::parseDeviceInfo(getDeviceInfoReply(0x01));
    EXPECT_EQ(info.entityCount, 3);
    EXPECT_EQ(info.transports, 0x0006);
    EXPECT_EQ(info.modelIds, (std::array<uint16_t, 3>{0xB023, 0x4082, 0x0000}));
    ASSERT_TRUE(info.extendedModelId.has_value());
    EXPECT_EQ(*info.extendedModelId, 0x01);
}

// MX Master 3 and MX Master 3 for Mac share every PID; only this byte
// tells them apart (issue #106).
TEST(DeviceInfo, ReadsExtendedModelIdAfterModelIds) {
    const auto info = DeviceInfo::parseDeviceInfo(getDeviceInfoReply(0x03));
    ASSERT_TRUE(info.extendedModelId.has_value());
    EXPECT_EQ(*info.extendedModelId, 0x03);
}

TEST(DeviceInfo, ShortReplyHasNoExtendedModelId) {
    const std::array<uint8_t, 7> raw = { 0x10, 0xFF, 0x02, 0x00, 0x03, 0x12, 0x34 };
    const auto info = DeviceInfo::parseDeviceInfo(*Report::parse(raw));
    EXPECT_EQ(info.entityCount, 3);
    EXPECT_FALSE(info.extendedModelId.has_value());
}
