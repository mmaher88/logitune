#pragma once
#include "interfaces/IDevice.h"

namespace logitune {

class MxMaster2sDescriptor : public IDevice {
public:
    QString deviceName() const override;
    std::vector<uint16_t> productIds() const override;
    bool matchesPid(uint16_t pid) const override;
    QList<ControlDescriptor> controls() const override;
    QList<HotspotDescriptor> buttonHotspots() const override;
    QList<HotspotDescriptor> scrollHotspots() const override;
    FeatureSupport features() const override;
    QString frontImagePath() const override;
    QString sideImagePath() const override;
    QString backImagePath() const override;
    QMap<QString, ButtonAction> defaultGestures() const override;
    int minDpi() const override;
    int maxDpi() const override;
    int dpiStep() const override;
    QList<EasySwitchSlotPosition> easySwitchSlotPositions() const override;
};

} // namespace logitune
