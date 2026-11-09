#include <gtest/gtest.h>
#include "hidpp/ReceiverInfo.h"
#include "hidpp/HidppTypes.h"

#include <vector>

using namespace logitune::hidpp;

namespace {

Report parseBytes(std::vector<uint8_t> bytes)
{
    bytes.resize(kLongReportSize, 0);
    auto r = Report::parse(bytes);
    EXPECT_TRUE(r.has_value());
    return r.value_or(Report{});
}

} // namespace

// Register 0x2B5 is read with HID++ 1.0 "get long register" (sub ID 0x83)
// addressed to the receiver itself (device index 0xFF). Bolt keeps each
// slot's pairing info at sub-register 0x50 + slot.
TEST(ReceiverInfo, BoltPairingInfoRequestBytes) {
    auto req = ReceiverInfo::buildPairingInfoRequest(kPidBoltReceiver, 2);
    ASSERT_TRUE(req.has_value());
    const std::vector<uint8_t> expected = {0x10, 0xFF, 0x83, 0xB5, 0x52, 0x00, 0x00};
    EXPECT_EQ(req->serialize(), expected);
}

// Unifying numbers its pairing sub-registers from 0x20 for slot 1.
TEST(ReceiverInfo, UnifyingPairingInfoRequestBytes) {
    auto req = ReceiverInfo::buildPairingInfoRequest(kPidUnifyReceiver, 1);
    ASSERT_TRUE(req.has_value());
    const std::vector<uint8_t> expected = {0x10, 0xFF, 0x83, 0xB5, 0x20, 0x00, 0x00};
    EXPECT_EQ(req->serialize(), expected);
}

TEST(ReceiverInfo, NoRequestForDirectDevice) {
    EXPECT_FALSE(ReceiverInfo::buildPairingInfoRequest(0xB034, 0xFF).has_value());
}

// Bolt reply: [sub, kind, wpid lo, wpid hi, serial x4]. This is the
// MX Master 3S For Business on Bolt slot 2; its device-connection
// notification reads "10 02 41 10 02 35 b0" (WPID 0xB035, little-endian).
TEST(ReceiverInfo, ParsesBoltWpidLittleEndian) {
    Report reply = parseBytes({0x11, 0xFF, 0x83, 0xB5,
                               0x52, 0x02, 0x35, 0xB0, 0x1A, 0x2B, 0x3C, 0x4D});
    auto wpid = ReceiverInfo::parseWpid(kPidBoltReceiver, 2, reply);
    ASSERT_TRUE(wpid.has_value());
    EXPECT_EQ(*wpid, 0xB035);
}

// Unifying reply: [sub, dest id, report interval, wpid hi, wpid lo, ...].
TEST(ReceiverInfo, ParsesUnifyingWpidBigEndian) {
    Report reply = parseBytes({0x11, 0xFF, 0x83, 0xB5,
                               0x20, 0x51, 0x08, 0x40, 0x82, 0x00, 0x00, 0x02});
    auto wpid = ReceiverInfo::parseWpid(kPidUnifyReceiver, 1, reply);
    ASSERT_TRUE(wpid.has_value());
    EXPECT_EQ(*wpid, 0x4082);
}

TEST(ReceiverInfo, RejectsReplyForAnotherSlot) {
    Report reply = parseBytes({0x11, 0xFF, 0x83, 0xB5,
                               0x51, 0x02, 0x34, 0xB0});
    EXPECT_FALSE(ReceiverInfo::parseWpid(kPidBoltReceiver, 2, reply).has_value());
}

TEST(ReceiverInfo, RejectsOtherRegister) {
    Report reply = parseBytes({0x11, 0xFF, 0x83, 0xB3,
                               0x52, 0x02, 0x35, 0xB0});
    EXPECT_FALSE(ReceiverInfo::parseWpid(kPidBoltReceiver, 2, reply).has_value());
}

TEST(ReceiverInfo, EmptySlotHasNoWpid) {
    Report reply = parseBytes({0x11, 0xFF, 0x83, 0xB5, 0x53});
    EXPECT_FALSE(ReceiverInfo::parseWpid(kPidBoltReceiver, 3, reply).has_value());
}
