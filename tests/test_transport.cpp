#include <gtest/gtest.h>
#include "hidpp/HidppTypes.h"

using namespace logitune::hidpp;

TEST(Report, SerializeShort) {
    Report r;
    r.reportId = kShortReportId;
    r.deviceIndex = 0xFF;
    r.featureIndex = 0x00;
    r.functionId = 0x0;
    r.softwareId = 0x01;
    r.paramLength = 3;
    r.params[0] = 0x00; r.params[1] = 0x01; r.params[2] = 0x00;
    auto bytes = r.serialize();
    ASSERT_EQ(bytes.size(), static_cast<size_t>(kShortReportSize));
    EXPECT_EQ(bytes[0], kShortReportId);
    EXPECT_EQ(bytes[1], 0xFF);
}

TEST(Report, SerializeShortFunctionAndSoftwareId) {
    Report r;
    r.reportId = kShortReportId;
    r.deviceIndex = 0x01;
    r.featureIndex = 0x02;
    r.functionId = 0x03;
    r.softwareId = 0x05;
    r.paramLength = 3;
    auto bytes = r.serialize();
    ASSERT_EQ(bytes.size(), static_cast<size_t>(kShortReportSize));
    // byte[3] = (0x03 << 4) | 0x05 = 0x35
    EXPECT_EQ(bytes[3], 0x35);
}

TEST(Report, SerializeLong) {
    Report r;
    r.reportId = kLongReportId;
    r.deviceIndex = 0x01;
    r.featureIndex = 0x03;
    r.functionId = 0x02;
    r.softwareId = 0x01;
    r.paramLength = 16;
    for (int i = 0; i < 16; ++i)
        r.params[i] = static_cast<uint8_t>(i);
    auto bytes = r.serialize();
    ASSERT_EQ(bytes.size(), static_cast<size_t>(kLongReportSize));
    EXPECT_EQ(bytes[0], kLongReportId);
    EXPECT_EQ(bytes[4], 0x00);
    EXPECT_EQ(bytes[19], 0x0F);
}

TEST(Report, ParseShort) {
    std::array<uint8_t, 7> raw = {0x10, 0xFF, 0x00, 0x01, 0xAA, 0xBB, 0xCC};
    auto r = Report::parse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->reportId, kShortReportId);
    EXPECT_EQ(r->deviceIndex, 0xFF);
    EXPECT_EQ(r->featureIndex, 0x00);
    EXPECT_EQ(r->functionId, 0x00);
    EXPECT_EQ(r->softwareId, 0x01);
}

TEST(Report, ParseShortParams) {
    std::array<uint8_t, 7> raw = {0x10, 0x01, 0x02, 0x35, 0xAA, 0xBB, 0xCC};
    auto r = Report::parse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->functionId, 0x03);
    EXPECT_EQ(r->softwareId, 0x05);
    EXPECT_EQ(r->params[0], 0xAA);
    EXPECT_EQ(r->params[1], 0xBB);
    EXPECT_EQ(r->params[2], 0xCC);
    EXPECT_EQ(r->paramLength, 3);
}

TEST(Report, ParseErrorReport) {
    std::array<uint8_t, 20> raw{};
    raw[0] = kLongReportId;
    raw[1] = 0xFF;
    raw[2] = 0xFF;  // error marker
    raw[3] = 0x00;
    raw[4] = 0x00;
    raw[5] = 0x05;  // ErrorCode::Busy
    auto r = Report::parse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->isError());
    EXPECT_EQ(r->errorCode(), ErrorCode::Busy);
}

TEST(Report, ParseInvalidTooShort) {
    std::array<uint8_t, 3> raw = {0x10, 0xFF, 0x00};
    auto r = Report::parse(raw);
    EXPECT_FALSE(r.has_value());
}

TEST(Report, IsErrorFalseForNormalReport) {
    std::array<uint8_t, 7> raw = {0x10, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00};
    auto r = Report::parse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->isError());
}

TEST(Report, SerializeRoundTrip) {
    Report original;
    original.reportId    = kShortReportId;
    original.deviceIndex  = 0x02;
    original.featureIndex = 0x05;
    original.functionId   = 0x04;
    original.softwareId   = 0x03;
    original.paramLength  = 3;
    original.params[0]    = 0x11;
    original.params[1]    = 0x22;
    original.params[2]    = 0x33;

    auto bytes = original.serialize();
    auto parsed = Report::parse(bytes);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->reportId,     original.reportId);
    EXPECT_EQ(parsed->deviceIndex,  original.deviceIndex);
    EXPECT_EQ(parsed->featureIndex, original.featureIndex);
    EXPECT_EQ(parsed->functionId,   original.functionId);
    EXPECT_EQ(parsed->softwareId,   original.softwareId);
    EXPECT_EQ(parsed->params[0],    original.params[0]);
    EXPECT_EQ(parsed->params[1],    original.params[1]);
    EXPECT_EQ(parsed->params[2],    original.params[2]);
}
