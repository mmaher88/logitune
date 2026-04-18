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
    if (s == QStringLiteral("verified") || s == QStringLiteral("implemented"))
        return JsonDevice::Status::Verified;
    return JsonDevice::Status::Beta;
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
    if (lower == QStringLiteral("dpi-cycle") || lower == QStringLiteral("dpicycle"))
        return ButtonAction::DpiCycle;
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
        cd.displayName = obj.value(QStringLiteral("displayName")).toString();
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
        hd.kind = obj.value(QStringLiteral("kind")).toString();
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

bool JsonDevice::parseFromDir(const QString& dirPath)
{
    const QDir dir(dirPath);
    const QString filePath = dir.absoluteFilePath(QStringLiteral("descriptor.json"));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDevice) << "JsonDevice: cannot open" << filePath;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcDevice) << "JsonDevice: parse error in" << filePath
                            << ":" << parseError.errorString();
        return false;
    }

    if (!parseFromObject(doc.object(), dirPath, /*strict=*/true))
        return false;

    m_loadedMtime = QFileInfo(filePath).lastModified().toSecsSinceEpoch();
    return true;
}

bool JsonDevice::parseFromObject(const QJsonObject& root, const QString& dirPath, bool strict)
{
    const QDir dir(dirPath);
    const QString filePath = dir.absoluteFilePath(QStringLiteral("descriptor.json"));

    m_status = parseStatus(root.value(QStringLiteral("status")).toString());
    m_name = root.value(QStringLiteral("name")).toString();

    const QJsonArray pidArr = root.value(QStringLiteral("productIds")).toArray();
    for (const auto& v : pidArr) {
        bool ok = false;
        const uint16_t pid = v.toString().toUInt(&ok, 16);
        if (!ok) {
            qCWarning(lcDevice) << "JsonDevice: invalid PID hex:" << v.toString();
            continue;
        }
        m_pids.push_back(pid);
    }

    m_features = parseFeatures(root.value(QStringLiteral("features")).toObject());

    const QJsonObject dpiObj = root.value(QStringLiteral("dpi")).toObject();
    if (!dpiObj.isEmpty()) {
        m_minDpi = dpiObj.value(QStringLiteral("min")).toInt(200);
        m_maxDpi = dpiObj.value(QStringLiteral("max")).toInt(8000);
        m_dpiStep = dpiObj.value(QStringLiteral("step")).toInt(50);
        const QJsonArray cycleRingArr = dpiObj.value(QStringLiteral("cycleRing")).toArray();
        m_dpiCycleRing.clear();
        m_dpiCycleRing.reserve(cycleRingArr.size());
        for (const auto &v : cycleRingArr) {
            const int value = v.toInt(-1);
            if (value < m_minDpi || value > m_maxDpi) {
                qCWarning(lcDevice) << "dpi.cycleRing value" << value
                                    << "out of range [" << m_minDpi << "," << m_maxDpi
                                    << "] in" << dir.absolutePath() << ", dropped";
                continue;
            }
            if (m_dpiStep > 0 && (value - m_minDpi) % m_dpiStep != 0) {
                qCWarning(lcDevice) << "dpi.cycleRing value" << value
                                    << "not a multiple of step" << m_dpiStep
                                    << "from min" << m_minDpi
                                    << "in" << dir.absolutePath() << ", dropped";
                continue;
            }
            m_dpiCycleRing.push_back(value);
        }
    }

    m_controls = parseControls(root.value(QStringLiteral("controls")).toArray());

    const QJsonObject hotspotsObj = root.value(QStringLiteral("hotspots")).toObject();
    m_buttonHotspots = parseHotspots(hotspotsObj.value(QStringLiteral("buttons")).toArray());
    m_scrollHotspots = parseHotspots(hotspotsObj.value(QStringLiteral("scroll")).toArray());

    const QJsonObject imagesObj = root.value(QStringLiteral("images")).toObject();
    const QString front = imagesObj.value(QStringLiteral("front")).toString();
    const QString side  = imagesObj.value(QStringLiteral("side")).toString();
    const QString back  = imagesObj.value(QStringLiteral("back")).toString();
    if (!front.isEmpty())
        m_frontImage = dir.absoluteFilePath(front);
    if (!side.isEmpty())
        m_sideImage = dir.absoluteFilePath(side);
    if (!back.isEmpty())
        m_backImage = dir.absoluteFilePath(back);

    const QJsonArray slotsArr = root.value(QStringLiteral("easySwitchSlots")).toArray();
    for (const auto& v : slotsArr) {
        const QJsonObject slotObj = v.toObject();
        m_easySwitchSlots.append({
            slotObj.value(QStringLiteral("xPct")).toDouble(),
            slotObj.value(QStringLiteral("yPct")).toDouble(),
            slotObj.value(QStringLiteral("label")).toString()
        });
    }

    m_defaultGestures = parseDefaultGestures(
        root.value(QStringLiteral("defaultGestures")).toObject());

    const bool strictGate = strict && (m_status == Status::Verified);

    if (strict && m_name.isEmpty()) {
        qCWarning(lcDevice) << "JsonDevice: missing name in" << filePath;
        return false;
    }

    if (strict && m_pids.empty()) {
        qCWarning(lcDevice) << "JsonDevice: no productIds in" << filePath;
        return false;
    }

    if (strictGate) {
        if (m_controls.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: verified device has no controls in" << filePath;
            return false;
        }
        if (m_buttonHotspots.isEmpty()) {
            qCWarning(lcDevice) << "JsonDevice: verified device has no button hotspots in" << filePath;
            return false;
        }
        if (!QFileInfo::exists(m_frontImage)) {
            qCWarning(lcDevice) << "JsonDevice: front image not found:" << m_frontImage;
            return false;
        }
    }

    m_sourcePath = QFileInfo(dirPath).canonicalFilePath();

    qCDebug(lcDevice) << "JsonDevice: parsed" << m_name << "from" << dirPath
                      << (strict ? "(strict)" : "(relaxed)");
    return true;
}

std::unique_ptr<JsonDevice> JsonDevice::load(const QString& dirPath)
{
    auto dev = std::unique_ptr<JsonDevice>(new JsonDevice());
    if (!dev->parseFromDir(dirPath))
        return nullptr;
    return dev;
}

bool JsonDevice::refresh()
{
    if (m_sourcePath.isEmpty())
        return false;
    const QString src = m_sourcePath;
    m_pids.clear();
    m_controls.clear();
    m_buttonHotspots.clear();
    m_scrollHotspots.clear();
    m_easySwitchSlots.clear();
    m_defaultGestures.clear();
    m_frontImage.clear();
    m_sideImage.clear();
    m_backImage.clear();
    m_features = FeatureSupport{};
    m_name.clear();
    m_status = Status::Beta;
    m_minDpi = 200;
    m_maxDpi = 8000;
    m_dpiStep = 50;
    m_dpiCycleRing.clear();
    return parseFromDir(src);
}

bool JsonDevice::refreshFromObject(const QJsonObject &root)
{
    if (m_sourcePath.isEmpty())
        return false;
    const QString src = m_sourcePath;
    // Clear mutable parsed state (mirrors refresh())
    m_pids.clear();
    m_controls.clear();
    m_buttonHotspots.clear();
    m_scrollHotspots.clear();
    m_easySwitchSlots.clear();
    m_defaultGestures.clear();
    m_frontImage.clear();
    m_sideImage.clear();
    m_backImage.clear();
    m_features = FeatureSupport{};
    m_name.clear();
    m_status = Status::Beta;
    m_minDpi = 200;
    m_maxDpi = 8000;
    m_dpiStep = 50;
    m_dpiCycleRing.clear();
    return parseFromObject(root, src, /*strict=*/false);
}

} // namespace logitune
