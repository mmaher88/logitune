#include <gtest/gtest.h>
#include "DeviceRegistry.h"
using namespace logitune;

struct DeviceSpec {
    uint16_t pid;
    const char* name;
    int minDpi, maxDpi, dpiStep;
    size_t buttonHotspots, scrollHotspots;
    size_t minControls;
    uint16_t control0Cid, control5Cid;
    const char* control5ActionType, *control6ActionType;
    bool battery, adjustableDpi, smartShift, reprogControls, gestureV2;
    // gestures
    ButtonAction::Type gestureDownType;
    const char* gestureDownPayload;
    ButtonAction::Type gestureUpType;
};

static const DeviceSpec kDevices[] = {
    {
        .pid = 0xb019,
        .name = "MX Master 2S",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 6, .scrollHotspots = 3,
        .minControls = 7,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "gesture-trigger",
        .control6ActionType = "smartshift-toggle",
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Keystroke,
        .gestureDownPayload = "Super+D",
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb034,
        .name = "MX Master 3S",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 6, .scrollHotspots = 3,
        .minControls = 7,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "gesture-trigger",
        .control6ActionType = "smartshift-toggle",
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Keystroke,
        .gestureDownPayload = "Super+D",
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb042,
        .name = "MX Master 4",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 6, .scrollHotspots = 3,
        .minControls = 7,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "gesture-trigger",
        .control6ActionType = "smartshift-toggle",
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Keystroke,
        .gestureDownPayload = "Super+D",
        .gestureUpType = ButtonAction::Default,
    },
};

class DeviceRegistryTest : public testing::TestWithParam<DeviceSpec> {
protected:
    DeviceRegistry reg;
};

TEST_P(DeviceRegistryTest, FindsByPid) {
    auto& s = GetParam();
    auto* dev = reg.findByPid(s.pid);
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), s.name);
}

TEST_P(DeviceRegistryTest, ControlsHaveExpectedCids) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto controls = dev->controls();
    ASSERT_GE(controls.size(), s.minControls);
    EXPECT_EQ(controls[0].controlId, s.control0Cid);
    EXPECT_EQ(controls[5].controlId, s.control5Cid);
    EXPECT_EQ(controls[5].defaultActionType, s.control5ActionType);
    EXPECT_EQ(controls[6].defaultActionType, s.control6ActionType);
}

TEST_P(DeviceRegistryTest, DefaultGesturesPresent) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto gestures = dev->defaultGestures();
    EXPECT_TRUE(gestures.contains("down"));
    EXPECT_EQ(gestures["down"].type, s.gestureDownType);
    EXPECT_EQ(gestures["down"].payload, s.gestureDownPayload);
    EXPECT_EQ(gestures["up"].type, s.gestureUpType);
}

TEST_P(DeviceRegistryTest, FeatureSupport) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto f = dev->features();
    EXPECT_EQ(f.battery, s.battery);
    EXPECT_EQ(f.adjustableDpi, s.adjustableDpi);
    EXPECT_EQ(f.smartShift, s.smartShift);
    EXPECT_EQ(f.reprogControls, s.reprogControls);
    EXPECT_EQ(f.gestureV2, s.gestureV2);
}

TEST_P(DeviceRegistryTest, DpiRange) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    EXPECT_EQ(dev->minDpi(), s.minDpi);
    EXPECT_EQ(dev->maxDpi(), s.maxDpi);
    EXPECT_EQ(dev->dpiStep(), s.dpiStep);
}

TEST_P(DeviceRegistryTest, Hotspots) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    EXPECT_EQ(dev->buttonHotspots().size(), s.buttonHotspots);
    EXPECT_EQ(dev->scrollHotspots().size(), s.scrollHotspots);
}

INSTANTIATE_TEST_SUITE_P(
    AllDevices,
    DeviceRegistryTest,
    testing::ValuesIn(kDevices),
    [](const auto& info) {
        // Makes test names like "AllDevices/DeviceRegistryTest/MX_Master_3S"
        std::string name = info.param.name;
        std::replace(name.begin(), name.end(), ' ', '_');
        return name;
    }
);

TEST(DeviceRegistry, ReturnsNullForUnknownPid) {
    DeviceRegistry reg;
    EXPECT_EQ(reg.findByPid(0x0000), nullptr);
    EXPECT_EQ(reg.findByPid(0xFFFF), nullptr);
}
