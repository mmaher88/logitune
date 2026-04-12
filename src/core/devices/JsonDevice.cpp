#include "devices/JsonDevice.h"
#include "logging/LogManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

namespace logitune {

bool JsonDevice::matchesPid(uint16_t pid) const
{
    for (auto id : m_pids) {
        if (id == pid)
            return true;
    }
    return false;
}

static JsonDevice::Status parseStatus(const QString& s)
{
    if (s == QStringLiteral("implemented"))
        return JsonDevice::Status::Implemented;
    if (s == QStringLiteral("community-verified"))
        return JsonDevice::Status::CommunityVerified;
    if (s == QStringLiteral("community-local"))
        return JsonDevice::Status::CommunityLocal;
    return JsonDevice::Status::Placeholder;
}

static ButtonAction::Type parseButtonActionType(const QString& s)
{
    const QString lower = s.toLower();
    if (lower == QStringLiteral("keystroke"))
        return ButtonAction::Keystroke;
    if (lower == QStringLiteral("gesture-trigger") || lower == QStringLiteral("gesturetrigger"))
        return ButtonAction::GestureTrigger;
    if (lower == QStringLiteral("smartshift-toggle") || lower == QStringLiteral("smartshifttoggle"))
        return ButtonAction::SmartShiftToggle;
    if (lower == QStringLiteral("app-launch") || lower == QStringLiteral("applaunch"))
        return ButtonAction::AppLaunch;
    if (lower == QStringLiteral("dbus"))
        return ButtonAction::DBus;
    if (lower == QStringLiteral("media"))
        return ButtonAction::Media;
    return ButtonAction::Default;
}

static FeatureSupport parseFeatures(const QJsonObject& obj)
{
    FeatureSupport f;
    f.battery                   = obj.value(QStringLiteral("battery")).toBool(false);
    f.adjustableDpi             = obj.value(QStringLiteral("adjustableDpi")).toBool(false);
    f.extendedDpi               = obj.value(QStringLiteral("extendedDpi")).toBool(false);
    f.smartShift                = obj.value(QStringLiteral("smartShift")).toBool(false);
    f.hiResWheel                = obj.value(QStringLiteral("hiResWheel")).toBool(false);
    f.hiResScrolling            = obj.value(QStringLiteral("hiResScrolling")).toBool(false);
    f.lowResWheel               = obj.value(QStringLiteral("lowResWheel")).toBool(false);
    f.smoothScroll              = obj.value(QStringLiteral("smoothScroll")).toBool(true);
    f.thumbWheel                = obj.value(QStringLiteral("thumbWheel")).toBool(false);
    f.reprogControls            = obj.value(QStringLiteral("reprogControls")).toBool(false);
    f.gestureV2                 = obj.value(QStringLiteral("gestureV2")).toBool(false);
    f.mouseGesture              = obj.value(QStringLiteral("mouseGesture")).toBool(false);
    f.hapticFeedback            = obj.value(QStringLiteral("hapticFeedback")).toBool(false);
    f.forceSensingButton        = obj.value(QStringLiteral("forceSensingButton")).toBool(false);
    f.crown                     = obj.value(QStringLiteral("crown")).toBool(false);
    f.reportRate                = obj.value(QStringLiteral("reportRate")).toBool(false);
    f.extendedReportRate        = obj.value(QStringLiteral("extendedReportRate")).toBool(false);
    f.pointerSpeed              = obj.value(QStringLiteral("pointerSpeed")).toBool(false);
    f.leftRightSwap             = obj.value(QStringLiteral("leftRightSwap")).toBool(false);
    f.surfaceTuning             = obj.value(QStringLiteral("surfaceTuning")).toBool(false);
    f.angleSnapping             = obj.value(QStringLiteral("angleSnapping")).toBool(false);
    f.colorLedEffects           = obj.value(QStringLiteral("colorLedEffects")).toBool(false);
    f.rgbEffects                = obj.value(QStringLiteral("rgbEffects")).toBool(false);
    f.onboardProfiles           = obj.value(QStringLiteral("onboardProfiles")).toBool(false);
    f.gkey                      = obj.value(QStringLiteral("gkey")).toBool(false);
    f.mkeys                     = obj.value(QStringLiteral("mkeys")).toBool(false);
    f.persistentRemappableAction = obj.value(QStringLiteral("persistentRemappableAction")).toBool(false);
    return f;
}

static QList<ControlDescriptor> parseControls(const QJsonArray& arr)
{
    QList<ControlDescriptor> result;
    for (const auto& val : arr) {
        const QJsonObject obj = val.toObject();
        bool ok = false;
        const QString cidStr = obj.value(QStringLiteral("controlId")).toString();
        const uint16_t cid = cidStr.toUInt(&ok, 16);
        if (!ok) {
            qCWarning(lcDevice) << "JsonDevice: invalid CID hex:" << cidStr;
            continue;
        }
        ControlDescriptor cd;
        cd.controlId = cid;
        cd.buttonIndex = obj.value(QStringLiteral("buttonIndex")).toInt();
        cd.defaultName = obj.value(QStringLiteral("defaultName")).toString();
        cd.defaultActionType = obj.value(QStringLiteral("defaultActionType")).toString();
        cd.configurable = obj.value(QStringLiteral("configurable")).toBool(false);
        result.append(cd);
    }
    return result;
}

static QList<HotspotDescriptor> parseHotspots(const QJsonArray& arr)
{
    QList<HotspotDescriptor> result;
    for (const auto& val : arr) {
        const QJsonObject obj = val.toObject();
        HotspotDescriptor hd;
        hd.buttonIndex = obj.value(QStringLiteral("buttonIndex")).toInt();
        hd.xPct = obj.value(QStringLiteral("xPct")).toDouble();
        hd.yPct = obj.value(QStringLiteral("yPct")).toDouble();
        hd.side = obj.value(QStringLiteral("side")).toString();
        hd.labelOffsetYPct = obj.value(QStringLiteral("labelOffsetYPct")).toDouble(0.0);
        result.append(hd);
    }
    return result;
}

static QMap<QString, ButtonAction> parseDefaultGestures(const QJsonObject& obj)
{
    QMap<QString, ButtonAction> result;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QJsonObject gestureObj = it.value().toObject();
        ButtonAction action;
        action.type = parseButtonActionType(gestureObj.value(QStringLiteral("type")).toString());
        action.payload = gestureObj.value(QStringLiteral("payload")).toString();
        result.insert(it.key(), action);
    }
    return result;
}

std::unique_ptr<JsonDevice> JsonDevice::load(const QString& dirPath)
{
    const QDir dir(dirPath);
    const QString filePath = dir.absoluteFilePath(QStringLiteral("descriptor.json"));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDevice) << "JsonDevice: cannot open" << filePath;
        return nullptr;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcDevice) << "JsonDevice: parse error in" << filePath
                            << ":" << parseError.errorString();
        return nullptr;
    }

    const QJsonObject root = doc.object();

    auto dev = std::unique_ptr<JsonDevice>(new JsonDevice());

    // Status
    dev->m_status = parseStatus(root.value(QStringLiteral("status")).toString());

    // Name
    dev->m_name = root.value(QStringLiteral("name")).toString();

    // Product IDs
    const QJsonArray pidArr = root.value(QStringLiteral("productIds")).toArray();
    for (const auto& v : pidArr) {
        bool ok = false;
        const uint16_t pid = v.toString().toUInt(&ok, 16);
        if (!ok) {
            qCWarning(lcDevice) << "JsonDevice: invalid PID hex:" << v.toString();
            continue;
        }
        dev->m_pids.push_back(pid);
    }

    // Features
    dev->m_features = parseFeatures(root.value(QStringLiteral("features")).toObject());

    // DPI
    const QJsonObject dpiObj = root.value(QStringLiteral("dpi")).toObject();
    if (!dpiObj.isEmpty()) {
        dev->m_minDpi = dpiObj.value(QStringLiteral("min")).toInt(200);
        dev->m_maxDpi = dpiObj.value(QStringLiteral("max")).toInt(8000);
        dev->m_dpiStep = dpiObj.value(QStringLiteral("step")).toInt(50);
    }

    // Controls
    dev->m_controls = parseControls(root.value(QStringLiteral("controls")).toArray());

    // Hotspots
    const QJsonObject hotspotsObj = root.value(QStringLiteral("hotspots")).toObject();
    dev->m_buttonHotspots = parseHotspots(hotspotsObj.value(QStringLiteral("buttons")).toArray());
    dev->m_scrollHotspots = parseHotspots(hotspotsObj.value(QStringLiteral("scroll")).toArray());

    // Images — resolve relative paths against dirPath
    const QJsonObject imagesObj = root.value(QStringLiteral("images")).toObject();
    const QString front = imagesObj.value(QStringLiteral("front")).toString();
    const QString side  = imagesObj.value(QStringLiteral("side")).toString();
    const QString back  = imagesObj.value(QStringLiteral("back")).toString();
    if (!front.isEmpty())
        dev->m_frontImage = dir.absoluteFilePath(front);
    if (!side.isEmpty())
        dev->m_sideImage = dir.absoluteFilePath(side);
    if (!back.isEmpty())
        dev->m_backImage = dir.absoluteFilePath(back);

    // Easy Switch Slots
    const QJsonArray slotsArr = root.value(QStringLiteral("easySwitchSlots")).toArray();
    for (const auto& v : slotsArr) {
        const QJsonObject slotObj = v.toObject();
        dev->m_easySwitchSlots.append({
            slotObj.value(QStringLiteral("xPct")).toDouble(),
            slotObj.value(QStringLiteral("yPct")).toDouble()
        });
    }

    // Default Gestures
    dev->m_defaultGestures = parseDefaultGestures(
        root.value(QStringLiteral("defaultGestures")).toObject());

    // Validation
    const bool strict = (dev->m_status == Status::Implemented
                         || dev->m_status == Status::CommunityVerified);

    if (dev->m_name.isEmpty()) {
        qCWarning(lcDevice) << "JsonDevice: missing name in" << filePath;
        return nullptr;
    }

    if (dev->m_pids.empty()) {
        qCWarning(lcDevice) << "JsonDevice: no productIds in" << filePath;
        return nullptr;
    }

    if (strict) {
        if (dev->m_controls.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: implemented device has no controls in" << filePath;
            return nullptr;
        }
        if (dev->m_buttonHotspots.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: implemented device has no button hotspots in" << filePath;
            return nullptr;
        }
        if (!QFileInfo::exists(dev->m_frontImage)) {
            qCWarning(lcDevice) << "JsonDevice: front image not found:" << dev->m_frontImage;
            return nullptr;
        }
    }

    qCDebug(lcDevice) << "JsonDevice: loaded" << dev->m_name << "from" << dirPath;
    return dev;
}

} // namespace logitune
