#include "MxMaster4Descriptor.h"

namespace logitune {

QString MxMaster4Descriptor::deviceName() const
{
    return QStringLiteral("MX Master 4");
}

std::vector<uint16_t> MxMaster4Descriptor::productIds() const
{
    return { 0xb042 };  // Bolt receiver-reported PID; BT PID discovered via device name
}

bool MxMaster4Descriptor::matchesPid(uint16_t pid) const
{
    for (auto id : productIds()) {
        if (id == pid)
            return true;
    }
    return false;
}

QList<ControlDescriptor> MxMaster4Descriptor::controls() const
{
    return {
        { 0x0050, 0, QStringLiteral("Left click"),        QStringLiteral("default"),          false },
        { 0x0051, 1, QStringLiteral("Right click"),       QStringLiteral("default"),          false },
        { 0x0052, 2, QStringLiteral("Middle click"),      QStringLiteral("default"),          true  },
        { 0x0053, 3, QStringLiteral("Back"),              QStringLiteral("default"),          true  },
        { 0x0056, 4, QStringLiteral("Forward"),           QStringLiteral("default"),          true  },
        { 0x00C3, 5, QStringLiteral("Gesture button"),    QStringLiteral("gesture-trigger"),  true  },
        { 0x00C4, 6, QStringLiteral("Shift wheel mode"),  QStringLiteral("smartshift-toggle"),true  },
        { 0x0000, 7, QStringLiteral("Thumb wheel"),       QStringLiteral("default"),          true  },
    };
}

QList<HotspotDescriptor> MxMaster4Descriptor::buttonHotspots() const
{
    return {
        { 2, 0.75, 0.23,  QStringLiteral("right"), 0.0  },
        { 6, 0.81, 0.43,  QStringLiteral("right"), 0.0  },
        { 7, 0.55, 0.515, QStringLiteral("right"), 0.20  },
        { 4, 0.43, 0.50,  QStringLiteral("left"),  0.0  },
        { 3, 0.47, 0.60,  QStringLiteral("left"),  0.20 },
        { 5, 0.39, 0.38,  QStringLiteral("left"),  -0.05  },
    };
}

QList<HotspotDescriptor> MxMaster4Descriptor::scrollHotspots() const
{
    return {
        { -1, 0.73, 0.23, QStringLiteral("right"), 0.0 },
        { -2, 0.55, 0.51, QStringLiteral("left"),  0.0 },
        { -3, 0.82, 0.43, QStringLiteral("right"), 0.0 },
    };
}

FeatureSupport MxMaster4Descriptor::features() const
{
    FeatureSupport f;
    f.battery        = true;
    f.adjustableDpi  = true;
    f.smartShift     = true;
    f.hiResWheel     = true;
    f.thumbWheel     = true;
    f.reprogControls = true;
    f.gestureV2      = false;
    f.smoothScroll   = false;
    return f;
}

QString MxMaster4Descriptor::frontImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-master-4.png");
}

QString MxMaster4Descriptor::sideImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-master-4-side.png");
}

QString MxMaster4Descriptor::backImagePath() const
{
    return QStringLiteral("qrc:/Logitune/qml/assets/mx-master-4-back.png");
}

QMap<QString, ButtonAction> MxMaster4Descriptor::defaultGestures() const
{
    QMap<QString, ButtonAction> g;
    g[QStringLiteral("up")]    = { ButtonAction::Default,   {} };
    g[QStringLiteral("down")]  = { ButtonAction::Keystroke, QStringLiteral("Super+D") };
    g[QStringLiteral("left")]  = { ButtonAction::Keystroke, QStringLiteral("Ctrl+Super+Left") };
    g[QStringLiteral("right")] = { ButtonAction::Keystroke, QStringLiteral("Ctrl+Super+Right") };
    g[QStringLiteral("click")] = { ButtonAction::Keystroke, QStringLiteral("Super+W") };
    return g;
}

int MxMaster4Descriptor::minDpi() const  { return 200; }
int MxMaster4Descriptor::maxDpi() const  { return 8000; }
int MxMaster4Descriptor::dpiStep() const { return 50; }

QList<EasySwitchSlotPosition> MxMaster4Descriptor::easySwitchSlotPositions() const
{
    return {
        { 0.335, 0.688 }, // 1
        { 0.393, 0.675 }, // 2
        { 0.453, 0.689 }, // 3
    };
}

} // namespace logitune
