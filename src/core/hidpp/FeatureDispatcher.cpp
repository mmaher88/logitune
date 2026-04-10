#include "FeatureDispatcher.h"
#include "logging/LogManager.h"

namespace logitune::hidpp {

// All FeatureIds known to this implementation — used by enumerate()
static constexpr FeatureId kKnownFeatures[] = {
    FeatureId::Root,
    FeatureId::FeatureSet,
    FeatureId::DeviceInfo,
    FeatureId::DeviceName,
    FeatureId::BatteryUnified,
    FeatureId::ChangeHost,
    FeatureId::ReprogControlsV4,
    FeatureId::SmartShift,
    FeatureId::SmartShiftEnhanced,
    FeatureId::HiResWheel,
    FeatureId::ThumbWheel,
    FeatureId::AdjustableDPI,
    FeatureId::GestureV2,
};

FeatureDispatcher::FeatureDispatcher(QObject *parent)
    : QObject(parent)
{}

bool FeatureDispatcher::enumerate(Transport *transport, uint8_t deviceIndex)
{
    m_featureMap.clear();

    // Bootstrap: Root is always at index 0x00 by the HID++ 2.0 spec.
    // We need it in the map before call() can route getFeatureID requests.
    m_featureMap[FeatureId::Root] = 0x00;

    for (FeatureId feature : kKnownFeatures) {
        auto featureIdVal = static_cast<uint16_t>(feature);
        uint8_t hi = static_cast<uint8_t>(featureIdVal >> 8);
        uint8_t lo = static_cast<uint8_t>(featureIdVal & 0xFF);

        // Root feature (index 0x00), function 0x00 = getFeatureID
        // params: [featureId_high, featureId_low]
        std::array<uint8_t, 2> queryParams = {hi, lo};
        auto response = call(transport, deviceIndex, FeatureId::Root, 0x00,
                             std::span<const uint8_t>(queryParams));
        if (!response)
            continue;

        uint8_t index = response->params[0];
        qCDebug(lcHidpp) << "feature" << Qt::hex << static_cast<uint16_t>(feature)
                         << "-> index" << index;
        if (index == 0 && feature != FeatureId::Root)
            continue; // Feature not supported on this device

        m_featureMap[feature] = index;
    }

    return true;
}

void FeatureDispatcher::setFeatureTable(std::vector<std::pair<FeatureId, uint8_t>> table)
{
    m_featureMap.clear();
    for (auto &[id, index] : table)
        m_featureMap[id] = index;
}

std::optional<uint8_t> FeatureDispatcher::featureIndex(FeatureId id) const
{
    auto it = m_featureMap.find(id);
    if (it == m_featureMap.end())
        return std::nullopt;
    return it->second;
}

bool FeatureDispatcher::hasFeature(FeatureId id) const
{
    return m_featureMap.count(id) > 0;
}

std::optional<Report> FeatureDispatcher::call(Transport *transport, uint8_t deviceIndex,
                                               FeatureId feature, uint8_t functionId,
                                               std::span<const uint8_t> params)
{
    auto idx = featureIndex(feature);
    if (!idx)
        return std::nullopt;

    Report req;
    req.reportId     = kLongReportId;
    req.deviceIndex  = deviceIndex;
    req.featureIndex = *idx;
    req.functionId   = functionId;
    req.softwareId   = 0x01;

    int len = static_cast<int>(params.size());
    if (len > 16) len = 16;
    req.paramLength = len;
    for (int i = 0; i < len; ++i)
        req.params[i] = params[i];

    return transport->sendRequest(req);
}

uint8_t FeatureDispatcher::nextSoftwareId()
{
    uint8_t id = m_nextSwId;
    m_nextSwId = (m_nextSwId % 15) + 1;  // rotate 1-15
    return id;
}

uint8_t FeatureDispatcher::callAsync(Transport *transport, uint8_t deviceIndex,
                                     FeatureId feature, uint8_t functionId,
                                     std::span<const uint8_t> params,
                                     ResponseCallback callback)
{
    auto idx = featureIndex(feature);
    if (!idx)
        return 0;

    uint8_t swId = nextSoftwareId();

    Report req;
    req.reportId     = kLongReportId;
    req.deviceIndex  = deviceIndex;
    req.featureIndex = *idx;
    req.functionId   = functionId;
    req.softwareId   = swId;

    int len = static_cast<int>(params.size());
    if (len > 16) len = 16;
    req.paramLength = len;
    for (int i = 0; i < len; ++i)
        req.params[i] = params[i];

    if (callback)
        m_pendingCallbacks[swId] = std::move(callback);

    transport->sendRequestAsync(req);
    return swId;
}

bool FeatureDispatcher::handleResponse(const Report &report)
{
    auto it = m_pendingCallbacks.find(report.softwareId);
    if (it == m_pendingCallbacks.end())
        return false;

    auto cb = std::move(it->second);
    m_pendingCallbacks.erase(it);
    cb(report);
    return true;
}

} // namespace logitune::hidpp
