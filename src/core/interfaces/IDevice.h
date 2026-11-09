#pragma once
#include "hidpp/HidppTypes.h"
#include "ButtonAction.h"
#include <QString>
#include <QList>
#include <QMap>
#include <vector>

namespace logitune {

struct ControlDescriptor {
    uint16_t controlId;
    int buttonIndex;
    QString defaultName;
    QString defaultActionType;
    bool configurable;
    QString displayName;
};

struct HotspotDescriptor {
    int buttonIndex;
    double xPct;
    double yPct;
    QString side;
    double labelOffsetYPct;
    QString kind;   // "scrollwheel" | "thumbwheel" | "pointer"; empty for button hotspots
};

struct FeatureSupport {
    bool battery = false;
    bool adjustableDpi = false;
    bool extendedDpi = false;
    bool smartShift = false;
    bool hiResWheel = false;
    bool hiResScrolling = false;
    bool lowResWheel = false;
    bool smoothScroll = true;
    bool thumbWheel = false;
    bool reprogControls = false;
    bool gestureV2 = false;
    bool mouseGesture = false;
    bool hapticFeedback = false;
    bool forceSensingButton = false;
    bool crown = false;
    bool reportRate = false;
    bool extendedReportRate = false;
    bool pointerSpeed = false;
    bool leftRightSwap = false;
    bool surfaceTuning = false;
    bool angleSnapping = false;
    bool colorLedEffects = false;
    bool rgbEffects = false;
    bool onboardProfiles = false;
    bool gkey = false;
    bool mkeys = false;
    bool persistentRemappableAction = false;
};

// A model sold under the descriptor's PIDs but with its own name or look,
// e.g. MX Master 3 for Mac. The device reports which one it is through the
// extended model ID in DeviceInformation (0x0003) getDeviceInfo. Empty
// fields fall back to the descriptor's.
struct DeviceVariant {
    uint8_t extendedModelId = 0;
    QString name;
    QString frontImagePath;
    QString sideImagePath;
    QString backImagePath;
};

struct EasySwitchSlotPosition {
    double xPct;
    double yPct;
    QString label;
};

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual QString deviceName() const = 0;
    virtual std::vector<uint16_t> productIds() const = 0;
    virtual bool matchesPid(uint16_t pid) const = 0;
    virtual QList<ControlDescriptor> controls() const = 0;
    virtual QList<HotspotDescriptor> buttonHotspots() const = 0;
    virtual QList<HotspotDescriptor> scrollHotspots() const = 0;
    virtual FeatureSupport features() const = 0;
    virtual QString frontImagePath() const = 0;
    virtual QString sideImagePath() const = 0;
    virtual QString backImagePath() const = 0;
    virtual QMap<QString, ButtonAction> defaultGestures() const = 0;
    virtual int minDpi() const = 0;
    virtual int maxDpi() const = 0;
    virtual int dpiStep() const = 0;
    virtual std::vector<int> dpiCycleRing() const = 0;
    virtual QList<EasySwitchSlotPosition> easySwitchSlotPositions() const = 0;
    virtual QList<DeviceVariant> variants() const = 0;
};

} // namespace logitune
