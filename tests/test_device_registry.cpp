#include <gtest/gtest.h>
#include "DeviceRegistry.h"
#include "devices/JsonDevice.h"
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
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
    {
        .pid = 0xb037,
        .name = "MX Anywhere 3S",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb038,
        .name = "MX Anywhere 3S for Business",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb025,
        .name = "MX Anywhere 3",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb02d,
        .name = "MX Anywhere 3 for Business",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C4,
        .control5ActionType = "smartshift-toggle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = true,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb020,
        .name = "MX Vertical",
        .minDpi = 400, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00C3,
        .control5ActionType = "dpi-cycle",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
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
    if (s.minControls >= 7 && s.control6ActionType) {
        EXPECT_EQ(controls[6].defaultActionType, s.control6ActionType);
    }
}

TEST_P(DeviceRegistryTest, DefaultGesturesPresent) {
    auto* dev = reg.findByPid(GetParam().pid);
    ASSERT_NE(dev, nullptr);
    auto& s = GetParam();
    auto gestures = dev->defaultGestures();
    if (s.gestureDownPayload) {
        EXPECT_TRUE(gestures.contains("down"));
        EXPECT_EQ(gestures["down"].type, s.gestureDownType);
        EXPECT_EQ(gestures["down"].payload, s.gestureDownPayload);
        EXPECT_EQ(gestures["up"].type, s.gestureUpType);
    } else {
        EXPECT_TRUE(gestures.isEmpty());
    }
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

TEST(DeviceRegistry, ReloadByPathRefreshesSingleDevice) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    QDir().mkpath(tmp.path() + QStringLiteral("/logitune/devices/test"));
    const QString descPath = tmp.path() + QStringLiteral("/logitune/devices/test/descriptor.json");

    auto write = [&](const QString &name) {
        QFile f(descPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        f.write(QStringLiteral(R"({"name":"%1","status":"beta","productIds":["0xffff"],"features":{},"controls":[],"hotspots":{"buttons":[],"scroll":[]},"images":{},"easySwitchSlots":[]})").arg(name).toUtf8());
        f.close();
        return true;
    };
    ASSERT_TRUE(write(QStringLiteral("Original")));

    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("Original"));
    ASSERT_NE(dev, nullptr);
    const auto *jdev = dynamic_cast<const logitune::JsonDevice*>(dev);
    ASSERT_NE(jdev, nullptr);
    const QString srcPath = jdev->sourcePath();

    ASSERT_TRUE(write(QStringLiteral("Mutated")));
    ASSERT_TRUE(reg.reload(srcPath));

    EXPECT_EQ(jdev->deviceName(), QStringLiteral("Mutated"));
    EXPECT_EQ(reg.findBySourcePath(srcPath), dev);

    qunsetenv("XDG_DATA_HOME");
}

TEST(DeviceRegistry, ReloadUnknownPathReturnsFalse) {
    logitune::DeviceRegistry reg;
    EXPECT_FALSE(reg.reload(QStringLiteral("/nonexistent/path/that/does/not/exist")));
}

TEST(DeviceRegistry, MxVerticalForBusinessRegistered) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("MX Vertical for Business"));
    ASSERT_NE(dev, nullptr);
    const auto ids = dev->productIds();
    EXPECT_NE(std::find(ids.begin(), ids.end(), 0xb020), ids.end());
    EXPECT_EQ(dev->maxDpi(), 4000);
    EXPECT_EQ(dev->minDpi(), 400);
    EXPECT_EQ(dev->controls().size(), 6);
    EXPECT_EQ(dev->controls()[5].controlId, 0x00C3);
    EXPECT_EQ(dev->controls()[5].defaultActionType, QStringLiteral("dpi-cycle"));
    EXPECT_FALSE(dev->features().smartShift);
    EXPECT_TRUE(dev->features().pointerSpeed);
    const auto ring = dev->dpiCycleRing();
    ASSERT_EQ(ring.size(), 4u);
    EXPECT_EQ(ring[0], 400);
    EXPECT_EQ(ring[1], 1000);
    EXPECT_EQ(ring[2], 1750);
    EXPECT_EQ(ring[3], 4000);
}

TEST(DeviceRegistry, MxMaster3sHasNoDpiCycleRing) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("MX Master 3S"));
    ASSERT_NE(dev, nullptr);
    EXPECT_TRUE(dev->dpiCycleRing().empty());
}
