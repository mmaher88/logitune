#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/Transport.h"
#include <QObject>
#include <unordered_map>
#include <vector>
#include <span>

namespace logitune::hidpp {

struct FeatureIdHash {
    std::size_t operator()(FeatureId id) const {
        return std::hash<uint16_t>{}(static_cast<uint16_t>(id));
    }
};

class FeatureDispatcher : public QObject {
    Q_OBJECT
public:
    explicit FeatureDispatcher(QObject *parent = nullptr);

    // Enumerate features from device via Root feature (0x0000)
    // Sends getFeatureID request for each known feature
    bool enumerate(Transport *transport, uint8_t deviceIndex);

    // Manual set for testing (bypasses transport)
    void setFeatureTable(std::vector<std::pair<FeatureId, uint8_t>> table);

    std::optional<uint8_t> featureIndex(FeatureId id) const;
    bool hasFeature(FeatureId id) const;

    // Send a feature call: resolves FeatureId to index, builds Report, sends via transport
    std::optional<Report> call(Transport *transport, uint8_t deviceIndex,
                               FeatureId feature, uint8_t functionId,
                               std::span<const uint8_t> params = {});

private:
    std::unordered_map<FeatureId, uint8_t, FeatureIdHash> m_featureMap;
};

} // namespace logitune::hidpp
