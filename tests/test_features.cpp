#include <gtest/gtest.h>
#include "hidpp/features/Battery.h"
#include "hidpp/features/AdjustableDPI.h"
#include "hidpp/features/DeviceName.h"

using namespace logitune::hidpp;
using namespace logitune::hidpp::features;

// ---------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------

TEST(Battery, ParseStatusReport) {
    Report r;
    r.params[0] = 78;
    r.params[1] = 0x00; // discharging
    r.paramLength = 2;
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 78);
    EXPECT_FALSE(status.charging);
}

TEST(Battery, ParseChargingReport) {
    Report r;
    r.params[0] = 50;
    r.params[1] = 0x03; // charging
    r.paramLength = 2;
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 50);
    EXPECT_TRUE(status.charging);
}

TEST(Battery, FullBattery) {
    Report r;
    r.params[0] = 100;
    r.params[1] = 0x00;
    r.paramLength = 2;
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 100);
    EXPECT_FALSE(status.charging);
}

TEST(Battery, ZeroBatteryCharging) {
    Report r;
    r.params[0] = 0;
    r.params[1] = 0x03;
    r.paramLength = 2;
    auto status = Battery::parseStatus(r);
    EXPECT_EQ(status.level, 0);
    EXPECT_TRUE(status.charging);
}

// ---------------------------------------------------------------------------
// AdjustableDPI
// ---------------------------------------------------------------------------

TEST(AdjustableDPI, ParseSensorInfo) {
    Report r;
    r.params[0] = 0x03; r.params[1] = 0xE8; // currentDPI = 1000
    r.params[2] = 0x01;                      // sensor count
    r.params[3] = 0x00; r.params[4] = 0xC8; // minDPI = 200
    r.params[5] = 0x0F; r.params[6] = 0xA0; // maxDPI = 4000
    r.params[7] = 0x00; r.params[8] = 0x32; // stepDPI = 50
    r.paramLength = 9;
    auto info = AdjustableDPI::parseSensorInfo(r);
    EXPECT_EQ(info.currentDPI, 1000);
    EXPECT_EQ(info.minDPI, 200);
    EXPECT_EQ(info.maxDPI, 4000);
    EXPECT_EQ(info.stepDPI, 50);
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

TEST(AdjustableDPI, ParseCurrentDPI) {
    Report r;
    r.params[0] = 0x07; r.params[1] = 0xD0; // 2000
    r.paramLength = 2;
    EXPECT_EQ(AdjustableDPI::parseCurrentDPI(r), 2000);
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

TEST(DeviceName, ParseSerialReturnsASCII) {
    Report r;
    r.params[0] = 'A'; r.params[1] = 'B'; r.params[2] = 'C';
    r.params[3] = '1'; r.params[4] = '2'; r.params[5] = '3';
    r.paramLength = 6;
    auto serial = DeviceName::parseSerial(r);
    EXPECT_EQ(serial, "ABC123");
}
