#include "ProXWirelessDescriptor.h"

namespace logitune {

QString ProXWirelessDescriptor::deviceName() const
{
    return QStringLiteral("PRO X Wireless");
}

std::vector<uint16_t> ProXWirelessDescriptor::productIds() const
{
    return { 0xC094, 0x4093 };  // USB direct + Lightspeed WPID
}

bool ProXWirelessDescriptor::matchesPid(uint16_t pid) const
{
    for (auto id : productIds()) {
        if (id == pid)
            return true;
    }
    return false;
}

QList<ControlDescriptor> ProXWirelessDescriptor::controls() const
{
    return {
        { 0x0050, 0, QStringLiteral("Left click"),   QStringLiteral("default"), false },
        { 0x0051, 1, QStringLiteral("Right click"),  QStringLiteral("default"), false },
        { 0x0052, 2, QStringLiteral("Middle click"), QStringLiteral("default"), true  },
        { 0x0053, 3, QStringLiteral("Back"),         QStringLiteral("default"), true  },
        { 0x0056, 4, QStringLiteral("Forward"),      QStringLiteral("default"), true  },
    };
}

QList<HotspotDescriptor> ProXWirelessDescriptor::buttonHotspots() const
{
    return {
        { 2, 0.50, 0.15, QStringLiteral("center"), 0.0 },
        { 3, 0.30, 0.40, QStringLiteral("left"),   0.0 },
        { 4, 0.70, 0.40, QStringLiteral("right"),  0.0 },
    };
}

QList<HotspotDescriptor> ProXWirelessDescriptor::scrollHotspots() const
{
    return {
        { -1, 0.50, 0.25, QStringLiteral("center"), 0.0 },
    };
}

FeatureSupport ProXWirelessDescriptor::features() const
{
    FeatureSupport f;
    f.battery        = true;
    f.adjustableDpi  = true;
    f.smartShift     = false;
    f.hiResWheel     = false;
    f.thumbWheel     = false;
    f.reprogControls = false;
    f.gestureV2      = false;
    return f;
}

QString ProXWirelessDescriptor::frontImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/pro-x-wireless.png");
}

QString ProXWirelessDescriptor::sideImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/pro-x-wireless-side.png");
}

QString ProXWirelessDescriptor::backImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/pro-x-wireless-back.png");
}

QMap<QString, ButtonAction> ProXWirelessDescriptor::defaultGestures() const
{
    return {};  // No gesture button on this mouse
}

int ProXWirelessDescriptor::minDpi() const  { return 100; }
int ProXWirelessDescriptor::maxDpi() const  { return 25600; }
int ProXWirelessDescriptor::dpiStep() const { return 50; }

int ProXWirelessDescriptor::easySwitchSlots() const { return 0; }

} // namespace logitune
