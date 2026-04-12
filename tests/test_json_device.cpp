#include <gtest/gtest.h>
#include "devices/JsonDevice.h"

#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

using namespace logitune;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QJsonObject makeControl(const QString& cid, int idx, const QString& name,
                               const QString& action, bool configurable)
{
    return QJsonObject{
        {QStringLiteral("controlId"), cid},
        {QStringLiteral("buttonIndex"), idx},
        {QStringLiteral("defaultName"), name},
        {QStringLiteral("defaultActionType"), action},
        {QStringLiteral("configurable"), configurable},
    };
}

static QJsonObject makeHotspot(int idx, double x, double y,
                               const QString& side, double labelOff = 0.0)
{
    return QJsonObject{
        {QStringLiteral("buttonIndex"), idx},
        {QStringLiteral("xPct"), x},
        {QStringLiteral("yPct"), y},
        {QStringLiteral("side"), side},
        {QStringLiteral("labelOffsetYPct"), labelOff},
    };
}

static QJsonObject makeMinimalImplemented()
{
    QJsonObject root;
    root[QStringLiteral("name")] = QStringLiteral("Test Device");
    root[QStringLiteral("status")] = QStringLiteral("implemented");
    root[QStringLiteral("productIds")] = QJsonArray{QStringLiteral("0xAAAA")};
    root[QStringLiteral("features")] = QJsonObject{
        {QStringLiteral("battery"), true},
        {QStringLiteral("smoothScroll"), true},
    };
    root[QStringLiteral("dpi")] = QJsonObject{
        {QStringLiteral("min"), 400},
        {QStringLiteral("max"), 4000},
        {QStringLiteral("step"), 100},
    };
    root[QStringLiteral("controls")] = QJsonArray{
        makeControl(QStringLiteral("0x0050"), 0, QStringLiteral("Left"), QStringLiteral("default"), false),
        makeControl(QStringLiteral("0x00C3"), 1, QStringLiteral("Gesture"), QStringLiteral("gesture-trigger"), true),
    };
    root[QStringLiteral("hotspots")] = QJsonObject{
        {QStringLiteral("buttons"), QJsonArray{
            makeHotspot(0, 0.5, 0.5, QStringLiteral("right")),
        }},
        {QStringLiteral("scroll"), QJsonArray{
            makeHotspot(-1, 0.7, 0.2, QStringLiteral("right")),
        }},
    };
    root[QStringLiteral("images")] = QJsonObject{
        {QStringLiteral("front"), QStringLiteral("front.png")},
        {QStringLiteral("side"), QStringLiteral("side.png")},
        {QStringLiteral("back"), QStringLiteral("back.png")},
    };
    root[QStringLiteral("easySwitchSlots")] = QJsonArray{
        QJsonObject{{QStringLiteral("xPct"), 0.3}, {QStringLiteral("yPct"), 0.6}},
        QJsonObject{{QStringLiteral("xPct"), 0.4}, {QStringLiteral("yPct"), 0.6}},
    };
    root[QStringLiteral("defaultGestures")] = QJsonObject{
        {QStringLiteral("up"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Default")},
            {QStringLiteral("payload"), QStringLiteral("")},
        }},
        {QStringLiteral("down"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Keystroke")},
            {QStringLiteral("payload"), QStringLiteral("Super+D")},
        }},
    };
    return root;
}

static QJsonObject makeMinimalPlaceholder()
{
    QJsonObject root;
    root[QStringLiteral("name")] = QStringLiteral("Placeholder Mouse");
    root[QStringLiteral("status")] = QStringLiteral("placeholder");
    root[QStringLiteral("productIds")] = QJsonArray{QStringLiteral("0xBBBB")};
    root[QStringLiteral("features")] = QJsonObject{};
    root[QStringLiteral("controls")] = QJsonArray{};
    root[QStringLiteral("hotspots")] = QJsonObject{
        {QStringLiteral("buttons"), QJsonArray{}},
        {QStringLiteral("scroll"), QJsonArray{}},
    };
    root[QStringLiteral("images")] = QJsonObject{};
    return root;
}

static void writeJson(const QString& dirPath, const QJsonObject& obj)
{
    QFile f(dirPath + QStringLiteral("/descriptor.json"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
}

static void writeDummyImage(const QString& dirPath, const QString& name)
{
    QFile f(dirPath + QStringLiteral("/") + name);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("PNG");
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(JsonDevice, LoadValidImplemented)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    writeJson(dir, makeMinimalImplemented());
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);

    EXPECT_EQ(dev->deviceName(), QStringLiteral("Test Device"));
    EXPECT_EQ(dev->status(), JsonDevice::Status::Implemented);

    // Product IDs
    auto pids = dev->productIds();
    ASSERT_EQ(pids.size(), 1u);
    EXPECT_EQ(pids[0], 0xAAAA);
    EXPECT_TRUE(dev->matchesPid(0xAAAA));
    EXPECT_FALSE(dev->matchesPid(0xFFFF));

    // Features
    auto f = dev->features();
    EXPECT_TRUE(f.battery);
    EXPECT_TRUE(f.smoothScroll);

    // DPI
    EXPECT_EQ(dev->minDpi(), 400);
    EXPECT_EQ(dev->maxDpi(), 4000);
    EXPECT_EQ(dev->dpiStep(), 100);

    // Controls
    auto ctrls = dev->controls();
    ASSERT_EQ(ctrls.size(), 2);
    EXPECT_EQ(ctrls[0].controlId, 0x0050);
    EXPECT_EQ(ctrls[1].controlId, 0x00C3);
    EXPECT_EQ(ctrls[1].defaultActionType, QStringLiteral("gesture-trigger"));
    EXPECT_TRUE(ctrls[1].configurable);

    // Hotspots
    EXPECT_EQ(dev->buttonHotspots().size(), 1);
    EXPECT_EQ(dev->scrollHotspots().size(), 1);

    // Easy switch slot positions
    auto easySlots = dev->easySwitchSlotPositions();
    ASSERT_EQ(easySlots.size(), 2);
    EXPECT_DOUBLE_EQ(easySlots[0].xPct, 0.3);

    // Default gestures
    auto gestures = dev->defaultGestures();
    EXPECT_EQ(gestures.size(), 2);
    EXPECT_EQ(gestures[QStringLiteral("up")].type, ButtonAction::Default);
    EXPECT_EQ(gestures[QStringLiteral("down")].type, ButtonAction::Keystroke);
    EXPECT_EQ(gestures[QStringLiteral("down")].payload, QStringLiteral("Super+D"));
}

TEST(JsonDevice, LoadValidPlaceholder)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    writeJson(dir, makeMinimalPlaceholder());

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("Placeholder Mouse"));
    EXPECT_EQ(dev->status(), JsonDevice::Status::Placeholder);
    EXPECT_TRUE(dev->controls().isEmpty());
    EXPECT_TRUE(dev->buttonHotspots().isEmpty());
}

TEST(JsonDevice, MissingFileReturnsNull)
{
    auto dev = JsonDevice::load(QStringLiteral("/tmp/nonexistent_json_device_dir_12345"));
    EXPECT_EQ(dev, nullptr);
}

TEST(JsonDevice, InvalidJsonReturnsNull)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QFile f(tmp.path() + QStringLiteral("/descriptor.json"));
    (void)f.open(QIODevice::WriteOnly);
    f.write("this is not json {{{");
    f.close();

    auto dev = JsonDevice::load(tmp.path());
    EXPECT_EQ(dev, nullptr);
}

TEST(JsonDevice, ImplementedMissingControlsReturnsNull)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    auto obj = makeMinimalImplemented();
    obj[QStringLiteral("controls")] = QJsonArray{}; // empty controls
    writeJson(dir, obj);
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    EXPECT_EQ(dev, nullptr);
}

TEST(JsonDevice, PlaceholderMissingControlsLoads)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    auto obj = makeMinimalPlaceholder();
    // controls already empty in placeholder
    writeJson(dir, obj);

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);
    EXPECT_TRUE(dev->controls().isEmpty());
}

TEST(JsonDevice, CidParsing)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    auto obj = makeMinimalImplemented();
    // Replace controls with one that has the CID we want to verify
    obj[QStringLiteral("controls")] = QJsonArray{
        makeControl(QStringLiteral("0x00C3"), 0, QStringLiteral("Gesture"), QStringLiteral("gesture-trigger"), true),
    };
    writeJson(dir, obj);
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);
    ASSERT_EQ(dev->controls().size(), 1);
    EXPECT_EQ(dev->controls()[0].controlId, 0x00C3);
}

TEST(JsonDevice, UnknownKeysIgnored)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    auto obj = makeMinimalImplemented();
    obj[QStringLiteral("unknownTopLevel")] = QStringLiteral("should be ignored");
    auto feat = obj[QStringLiteral("features")].toObject();
    feat[QStringLiteral("futureFeature")] = true;
    obj[QStringLiteral("features")] = feat;
    writeJson(dir, obj);
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->deviceName(), QStringLiteral("Test Device"));
}

TEST(JsonDevice, ImagePathResolution)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    writeJson(dir, makeMinimalImplemented());
    writeDummyImage(dir, QStringLiteral("front.png"));

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);

    // Must be an absolute path ending with /front.png
    EXPECT_TRUE(dev->frontImagePath().endsWith(QStringLiteral("/front.png")));
    EXPECT_TRUE(QDir::isAbsolutePath(dev->frontImagePath()));

    // side and back are relative in JSON but resolved to absolute
    EXPECT_TRUE(dev->sideImagePath().endsWith(QStringLiteral("/side.png")));
    EXPECT_TRUE(dev->backImagePath().endsWith(QStringLiteral("/back.png")));
}

TEST(JsonDevice, DefaultGesturesParsing)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    auto obj = makeMinimalPlaceholder();
    obj[QStringLiteral("defaultGestures")] = QJsonObject{
        {QStringLiteral("up"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Default")},
            {QStringLiteral("payload"), QStringLiteral("")},
        }},
        {QStringLiteral("down"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("Keystroke")},
            {QStringLiteral("payload"), QStringLiteral("Ctrl+C")},
        }},
        {QStringLiteral("click"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("SmartShiftToggle")},
            {QStringLiteral("payload"), QStringLiteral("")},
        }},
    };
    writeJson(dir, obj);

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);

    auto g = dev->defaultGestures();
    EXPECT_EQ(g.size(), 3);
    EXPECT_EQ(g[QStringLiteral("up")].type, ButtonAction::Default);
    EXPECT_EQ(g[QStringLiteral("down")].type, ButtonAction::Keystroke);
    EXPECT_EQ(g[QStringLiteral("down")].payload, QStringLiteral("Ctrl+C"));
    EXPECT_EQ(g[QStringLiteral("click")].type, ButtonAction::SmartShiftToggle);
}

TEST(JsonDevice, FeatureFlagDefaults)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.path();

    // Placeholder with empty features object
    auto obj = makeMinimalPlaceholder();
    obj[QStringLiteral("features")] = QJsonObject{};
    writeJson(dir, obj);

    auto dev = JsonDevice::load(dir);
    ASSERT_NE(dev, nullptr);

    auto f = dev->features();
    // smoothScroll defaults to true, all others to false
    EXPECT_TRUE(f.smoothScroll);
    EXPECT_FALSE(f.battery);
    EXPECT_FALSE(f.adjustableDpi);
    EXPECT_FALSE(f.smartShift);
    EXPECT_FALSE(f.hiResWheel);
    EXPECT_FALSE(f.reprogControls);
    EXPECT_FALSE(f.gestureV2);
    EXPECT_FALSE(f.persistentRemappableAction);
}
