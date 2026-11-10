#include <gtest/gtest.h>
#include "DeviceRegistry.h"
#include "DeviceSession.h"
#include "devices/JsonDevice.h"
#include "hidpp/HidppTypes.h"
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <algorithm>
#include <cstdio>
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
    // Unifying WPID of the MX Master 2S.
    {
        .pid = 0x4069,
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
    // Second MX Master 3S hardware revision (Options+ model 2b043).
    {
        .pid = 0xb043,
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
        .pid = 0xb035,
        .name = "MX Master 3S For Business",
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
    // Second MX Master 3S For Business revision (Options+ model 2b044).
    {
        .pid = 0xb044,
        .name = "MX Master 3S For Business",
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
        .pid = 0x4082,
        .name = "MX Master 3",
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
        .pid = 0xb042,
        .name = "MX Master 4",
        .minDpi = 200, .maxDpi = 8000, .dpiStep = 50,
        .buttonHotspots = 7, .scrollHotspots = 3,
        .minControls = 9,
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
    // Beta descriptors built from the Options+ device catalogue, with
    // Logitech product shots for images. DPI is the Options+ importer
    // fallback (200-4000); on hardware the range comes from
    // AdjustableDPI (0x2201) at connect.
    {
        .pid = 0xb01d,
        .name = "MX Ergo",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 6, .scrollHotspots = 2,
        .minControls = 8,
        .control0Cid = 0x0050, .control5Cid = 0x005B,
        .control5ActionType = "default",
        .control6ActionType = "default",
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb01a,
        .name = "MX Anywhere 2S",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 5, .scrollHotspots = 2,
        .minControls = 7,
        .control0Cid = 0x0050, .control5Cid = 0x005B,
        .control5ActionType = "default",
        .control6ActionType = "default",
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb031,
        .name = "LIFT",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00FD,
        .control5ActionType = "default",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    {
        .pid = 0xb033,
        .name = "LIFT for Business",
        .minDpi = 200, .maxDpi = 4000, .dpiStep = 50,
        .buttonHotspots = 4, .scrollHotspots = 2,
        .minControls = 6,
        .control0Cid = 0x0050, .control5Cid = 0x00FD,
        .control5ActionType = "default",
        .control6ActionType = nullptr,
        .battery = true, .adjustableDpi = true, .smartShift = false,
        .reprogControls = true, .gestureV2 = false,
        .gestureDownType = ButtonAction::Default,
        .gestureDownPayload = nullptr,
        .gestureUpType = ButtonAction::Default,
    },
    // MX Vertical over its USB cable.
    {
        .pid = 0xc08a,
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
        // Makes test names like "AllDevices/DeviceRegistryTest/MX_Master_3S_b034".
        // The PID suffix keeps names unique when one descriptor has several PIDs.
        std::string name = info.param.name;
        std::replace(name.begin(), name.end(), ' ', '_');
        char pid[8];
        std::snprintf(pid, sizeof(pid), "_%04x", info.param.pid);
        return name + pid;
    }
);

// ---------------------------------------------------------------------------
// Variants: models that share a PID, told apart by the extended model ID from
// DeviceInfo (0x0003). Mappings come from Logitech Options+'s device catalogue
// ("derivatives" keyed by "extendedModel"). variantName == nullptr means the
// extended model ID is a plain retail unit with no variant entry.
// ---------------------------------------------------------------------------

struct VariantSpec {
    uint16_t pid;
    uint8_t extendedModelId;
    const char *variantName;
};

static const VariantSpec kVariants[] = {
    { 0xb023,  1, nullptr },
    { 0xb023,  2, nullptr },
    { 0xb023,  3, "MX Master 3 for Mac" },
    { 0x4082,  3, "MX Master 3 for Mac" },   // same descriptor over Unifying
    { 0xb034,  1, nullptr },
    { 0xb034,  2, nullptr },
    { 0xb034,  3, "MX Master 3S for Mac" },
    { 0xb034,  4, "MX Master 3S for Mac" },
    { 0xb034, 10, nullptr },
    { 0xb034, 11, "MX Master 3S for Mac" },
    { 0xb034, 12, nullptr },
    { 0xb034, 13, nullptr },
    { 0xb025,  1, nullptr },
    { 0xb025,  3, nullptr },
    { 0xb025,  4, "MX Anywhere 3 for Mac" },
    { 0x4090,  4, "MX Anywhere 3 for Mac" },
};

class DeviceVariantTest : public testing::TestWithParam<VariantSpec> {
protected:
    DeviceRegistry reg;
};

TEST_P(DeviceVariantTest, ExtendedModelIdSelectsVariant) {
    const auto &s = GetParam();
    const IDevice *dev = reg.findByPid(s.pid);
    ASSERT_NE(dev, nullptr);
    const auto variant = DeviceSession::resolveVariant(dev, s.extendedModelId);
    if (!s.variantName) {
        EXPECT_FALSE(variant.has_value());
        return;
    }
    ASSERT_TRUE(variant.has_value());
    EXPECT_EQ(variant->name, QString::fromUtf8(s.variantName));
}

INSTANTIATE_TEST_SUITE_P(
    ShippedDescriptors,
    DeviceVariantTest,
    testing::ValuesIn(kVariants),
    [](const auto &info) {
        // e.g. "ShippedDescriptors/DeviceVariantTest/pid_b023_ext_3"
        char buf[32];
        std::snprintf(buf, sizeof(buf), "pid_%04x_ext_%d",
                      info.param.pid, info.param.extendedModelId);
        return std::string(buf);
    }
);

TEST(DeviceVariant, NoExtendedModelIdMeansNoVariant) {
    DeviceRegistry reg;
    EXPECT_FALSE(DeviceSession::resolveVariant(reg.findByPid(0xb023), std::nullopt).has_value());
}

TEST(DeviceVariant, NoDescriptorMeansNoVariant) {
    EXPECT_FALSE(DeviceSession::resolveVariant(nullptr, 3).has_value());
}

TEST(DeviceRegistry, ReturnsNullForUnknownPid) {
    DeviceRegistry reg;
    EXPECT_EQ(reg.findByPid(0x0000), nullptr);
    EXPECT_EQ(reg.findByPid(0xFFFF), nullptr);
}

TEST(DeviceRegistry, ReloadByPathRefreshesSingleDevice) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    // XDG_DATA_HOME only takes effect with QStandardPaths test mode off,
    // and ensureApp() in any earlier test leaves it on for the whole run.
    const bool wasTestMode = QStandardPaths::isTestModeEnabled();
    QStandardPaths::setTestModeEnabled(false);
    const auto restore = qScopeGuard([wasTestMode] {
        qunsetenv("XDG_DATA_HOME");
        QStandardPaths::setTestModeEnabled(wasTestMode);
    });
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

// MX Vertical's DPI button (CID 0x00C3) is firmware-locked — SetControlReporting
// is accepted by the device but the firmware still fires the native DPI cycle
// without emitting a divert event. Mark the descriptor entry as non-configurable
// so the UI shows the button as informational rather than offering a remap that
// silently fails. Loaded via JsonDevice::load against the source tree so the
// assertion tracks the descriptor regardless of whatever is installed at the
// system path.
TEST(DeviceRegistry, MxVerticalDpiButtonIsNotConfigurable) {
    for (const char *slug : {"mx-vertical", "mx-vertical-for-business"}) {
        const QString dir = QStringLiteral(SOURCE_ROOT "/devices/") + slug;
        auto dev = logitune::JsonDevice::load(dir);
        ASSERT_NE(dev, nullptr) << slug;
        ASSERT_EQ(dev->controls().size(), 6u) << slug;
        EXPECT_EQ(dev->controls()[5].controlId, 0x00C3) << slug;
        EXPECT_FALSE(dev->controls()[5].configurable) << slug;
    }
}

// Descriptors load in folder order (mx-master-3 before mx-master-3s), so a
// first-hit contains() match resolved every 3S variant to "MX Master 3".
TEST(DeviceRegistry, FindByNamePrefersLongestContainedName) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("Wireless Mouse MX Master 3S"));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3S"));

    dev = reg.findByName(QStringLiteral("MX Anywhere 3S Wireless Mouse"));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Anywhere 3S"));
}

TEST(DeviceRegistry, FindByNamePrefersExactMatch) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("mx master 3s for business"));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("MX Master 3S For Business"));
}

// MX Master 3S and 3S For Business each ship under two PIDs (b034/b043 and
// b035/b044). Every one of those PIDs belongs to exactly one descriptor, so the
// PID decides, even when the reported name would match the sibling descriptor.
TEST(DeviceRegistry, FindByPidAndNameResolvesMxMaster3sRevisions) {
    logitune::DeviceRegistry reg;
    struct Case { uint16_t pid; const char *reportedName; const char *expected; };
    const Case cases[] = {
        {0xb034, "Wireless Mouse MX Master 3S", "MX Master 3S"},
        {0xb043, "Wireless Mouse MX Master 3S", "MX Master 3S"},
        {0xb035, "MX Master 3S For Business",   "MX Master 3S For Business"},
        {0xb044, "MX Master 3S For Business",   "MX Master 3S For Business"},
        {0xb043, "MX Master 3S For Business",   "MX Master 3S"},
        {0xb044, "Wireless Mouse MX Master 3S", "MX Master 3S For Business"},
    };
    for (const auto &c : cases) {
        const auto *dev = reg.findByPidAndName(c.pid, QString::fromUtf8(c.reportedName));
        ASSERT_NE(dev, nullptr) << std::hex << c.pid;
        EXPECT_EQ(dev->deviceName(), QString::fromUtf8(c.expected))
            << std::hex << c.pid << " reported as " << c.reportedName;
    }
}

// A receiver's USB PID identifies the receiver, not the device paired to it,
// so no shipped descriptor may list one. Receiver-attached devices are
// matched by their WPID instead.
TEST(DeviceRegistry, NoDescriptorClaimsAReceiverPid) {
    const QDir devicesDir(QStringLiteral(SOURCE_ROOT "/devices"));
    const auto slugs = devicesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    ASSERT_FALSE(slugs.isEmpty());
    for (const QString &slug : slugs) {
        auto dev = logitune::JsonDevice::load(devicesDir.filePath(slug));
        ASSERT_NE(dev, nullptr) << slug.toStdString();
        EXPECT_FALSE(dev->matchesPid(logitune::hidpp::kPidBoltReceiver)) << slug.toStdString();
        EXPECT_FALSE(dev->matchesPid(logitune::hidpp::kPidUnifyReceiver)) << slug.toStdString();
    }
}

// FindsByPid only covers the first PID of each spec. The Unifying-receiver
// WPIDs come from the Options+ catalogue and must resolve to the same
// descriptor as the Bluetooth PID.
TEST(DeviceRegistry, UnifyingWpidsResolveToTheirDescriptor) {
    logitune::DeviceRegistry reg;
    const struct { uint16_t wpid; const char *name; } kCases[] = {
        { 0x406f, "MX Ergo" },
        { 0x406a, "MX Anywhere 2S" },
    };
    for (const auto &c : kCases) {
        const auto *dev = reg.findByPid(c.wpid);
        ASSERT_NE(dev, nullptr) << c.name;
        EXPECT_EQ(dev->deviceName(), QString::fromUtf8(c.name));
    }
}

// The Easy-Switch channel list has one row per slot, so the slot count must
// match the host count Options+ reports (flow.hostCount): MX Ergo has 2.
TEST(DeviceRegistry, EasySwitchSlotCountMatchesHostCount) {
    logitune::DeviceRegistry reg;
    const struct { uint16_t pid; qsizetype hosts; } kCases[] = {
        { 0xb01d, 2 },  // MX Ergo
        { 0xb01a, 3 },  // MX Anywhere 2S
        { 0xb031, 3 },  // LIFT
        { 0xb033, 3 },  // LIFT for Business
    };
    for (const auto &c : kCases) {
        const auto *dev = reg.findByPid(c.pid);
        ASSERT_NE(dev, nullptr) << std::hex << c.pid;
        EXPECT_EQ(dev->easySwitchSlotPositions().size(), c.hosts) << dev->deviceName().toStdString();
    }
}

TEST(DeviceRegistry, ProductImagesExistOnDisk) {
    logitune::DeviceRegistry reg;
    // MX Anywhere 2S has no underside shot, so no back image (the
    // Easy-Switch tab stays hidden for it).
    const struct { uint16_t pid; bool hasBack; } kCases[] = {
        { 0xb01d, true },   // MX Ergo
        { 0xb01a, false },  // MX Anywhere 2S
        { 0xb031, true },   // LIFT
        { 0xb033, true },   // LIFT for Business
    };
    for (const auto &c : kCases) {
        const auto *dev = reg.findByPid(c.pid);
        ASSERT_NE(dev, nullptr) << std::hex << c.pid;
        const std::string name = dev->deviceName().toStdString();
        EXPECT_TRUE(QFileInfo::exists(dev->frontImagePath())) << name;
        EXPECT_TRUE(QFileInfo::exists(dev->sideImagePath())) << name;
        if (c.hasBack)
            EXPECT_TRUE(QFileInfo::exists(dev->backImagePath())) << name;
        else
            EXPECT_TRUE(dev->backImagePath().isEmpty()) << name;
    }
}

TEST(DeviceRegistry, MxMaster3sHasNoDpiCycleRing) {
    logitune::DeviceRegistry reg;
    const auto *dev = reg.findByName(QStringLiteral("MX Master 3S"));
    ASSERT_NE(dev, nullptr);
    EXPECT_TRUE(dev->dpiCycleRing().empty());
}

// Every devices/*/descriptor.json must have a matching entry in kDevices[]
// above so the parameterized suite actually validates the new descriptor.
// Without this check, a new descriptor can be added (hardware PID, controls,
// hotspots, gestures...) with zero test coverage — issues only surface at
// runtime for an end user. Inverse check catches kDevices entries that no
// longer correspond to a descriptor on disk (e.g. after a rename).
TEST(DeviceRegistry, EveryDescriptorHasSpecCoverage) {
    // Descriptors that share a PID with another entry can't be parameterized
    // via kDevices because DeviceRegistry::findByPid returns the first-loaded
    // match, so the FindsByPid assertion on the shared-PID variant would flap.
    // They're covered by dedicated tests instead
    // (MxVerticalDpiButtonIsNotConfigurable iterates both MX Vertical variants).
    static const QSet<QString> kPidCollisionExemptions = {
        QStringLiteral("MX Vertical for Business"),
    };

    QSet<QString> specNames;
    for (const auto &s : kDevices) {
        specNames.insert(QString::fromUtf8(s.name));
    }
    specNames.unite(kPidCollisionExemptions);

    QSet<QString> descriptorNames;
    const QDir devicesDir(QStringLiteral(SOURCE_ROOT "/devices"));
    ASSERT_TRUE(devicesDir.exists()) << "devices/ dir not found at " << devicesDir.path().toStdString();
    const auto slugs = devicesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &slug : slugs) {
        const QString descPath = devicesDir.filePath(slug) + QStringLiteral("/descriptor.json");
        if (!QFile::exists(descPath))
            continue;
        auto dev = logitune::JsonDevice::load(devicesDir.filePath(slug));
        ASSERT_NE(dev, nullptr) << slug.toStdString();
        descriptorNames.insert(dev->deviceName());
    }

    // Descriptors on disk with no DeviceSpec entry.
    const QSet<QString> uncovered = descriptorNames - specNames;
    if (!uncovered.isEmpty()) {
        QStringList sorted = uncovered.values();
        std::sort(sorted.begin(), sorted.end());
        ADD_FAILURE() << "Descriptors missing a DeviceSpec entry in kDevices[]:\n"
                      << "  " << sorted.join(QStringLiteral(", ")).toStdString() << "\n"
                      << "  Add matching entries to tests/test_device_registry.cpp "
                         "(kDevices[] near the top of the file).";
    }

    // DeviceSpec entries with no descriptor on disk (stale test data).
    const QSet<QString> orphaned = specNames - descriptorNames;
    if (!orphaned.isEmpty()) {
        QStringList sorted = orphaned.values();
        std::sort(sorted.begin(), sorted.end());
        ADD_FAILURE() << "kDevices[] has entries with no descriptor on disk:\n"
                      << "  " << sorted.join(QStringLiteral(", ")).toStdString() << "\n"
                      << "  Remove them from tests/test_device_registry.cpp or "
                         "restore the missing devices/<slug>/descriptor.json.";
    }
}
