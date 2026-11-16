#include "hidpp/features/DeviceInfo.h"

namespace logitune::hidpp::features {

namespace {
constexpr int kEntityCountOffset     = 0;
constexpr int kTransportsOffset      = 5;
constexpr int kModelIdsOffset        = 7;
constexpr int kExtendedModelIdOffset = 13;
} // namespace

DeviceInfoIds DeviceInfo::parseDeviceInfo(const Report &r)
{
    DeviceInfoIds ids;
    if (r.paramLength > kEntityCountOffset)
        ids.entityCount = r.params[kEntityCountOffset];
    if (r.paramLength <= kExtendedModelIdOffset)
        return ids;

    ids.transports = static_cast<uint16_t>(
        (r.params[kTransportsOffset] << 8) | r.params[kTransportsOffset + 1]);
    for (size_t i = 0; i < ids.modelIds.size(); ++i) {
        const int at = kModelIdsOffset + static_cast<int>(i) * 2;
        ids.modelIds[i] = static_cast<uint16_t>((r.params[at] << 8) | r.params[at + 1]);
    }
    ids.extendedModelId = r.params[kExtendedModelIdOffset];
    return ids;
}

} // namespace logitune::hidpp::features
