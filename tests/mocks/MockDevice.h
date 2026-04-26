#pragma once
#include "interfaces/IDevice.h"
#include "ButtonAction.h"
#include <QString>
#include <QList>
#include <QMap>
#include <vector>

namespace logitune::test {

class MockDevice : public logitune::IDevice {
public:
    MockDevice() = default;

    // --- Configurable member data ---
    QString m_deviceName        = QStringLiteral("Mock Device");
    std::vector<uint16_t> m_productIds;
    QList<ControlDescriptor> m_controls;
    QList<HotspotDescriptor> m_buttonHotspots;
    QList<HotspotDescriptor> m_scrollHotspots;
    FeatureSupport m_features;
    QString m_frontImagePath;
    QString m_sideImagePath;
    QString m_backImagePath;
    QMap<QString, ButtonAction> m_defaultGestures;
    int m_minDpi       = 200;
    int m_maxDpi       = 8000;
    int m_dpiStep      = 200;
    std::vector<int> m_dpiCycleRing;
    QList<EasySwitchSlotPosition> m_easySwitchSlotPositions;

    // --- IDevice implementation ---

    QString deviceName() const override { return m_deviceName; }
    std::vector<uint16_t> productIds() const override { return m_productIds; }
    bool matchesPid(uint16_t pid) const override {
        for (auto id : m_productIds)
            if (id == pid) return true;
        return false;
    }
    QList<ControlDescriptor> controls() const override { return m_controls; }
    QList<HotspotDescriptor> buttonHotspots() const override { return m_buttonHotspots; }
    QList<HotspotDescriptor> scrollHotspots() const override { return m_scrollHotspots; }
    FeatureSupport features() const override { return m_features; }
    QString frontImagePath() const override { return m_frontImagePath; }
    QString sideImagePath() const override { return m_sideImagePath; }
    QString backImagePath() const override { return m_backImagePath; }
    QMap<QString, ButtonAction> defaultGestures() const override { return m_defaultGestures; }
    int minDpi() const override { return m_minDpi; }
    int maxDpi() const override { return m_maxDpi; }
    int dpiStep() const override { return m_dpiStep; }
    std::vector<int> dpiCycleRing() const override { return m_dpiCycleRing; }
    QList<EasySwitchSlotPosition> easySwitchSlotPositions() const override { return m_easySwitchSlotPositions; }

    // --- Test helpers ---

    /// Populates 8 standard MX Master 3S control descriptors.
    /// CIDs: 0x0050(btn0), 0x0051(btn1), 0x0052(btn2), 0x0053(btn3),
    ///       0x0056(btn4), 0x00C3(btn5), 0x00C4(btn6), 0x0000(btn7)
    void setupMxControls() {
        m_deviceName  = QStringLiteral("MX Master 3S");
        m_productIds  = { 0xb034 };
        m_features.reprogControls = true;
        m_features.battery        = true;
        m_features.adjustableDpi  = true;
        m_features.smartShift     = true;
        m_features.hiResWheel     = true;
        m_features.thumbWheel     = true;

        struct Entry { uint16_t cid; int idx; const char *name; const char *action; };
        static const Entry entries[8] = {
            { 0x0050, 0, "Left Button",       "default" },
            { 0x0051, 1, "Right Button",      "default" },
            { 0x0052, 2, "Middle Button",     "default" },
            { 0x0053, 3, "Back Button",       "default" },
            { 0x0056, 4, "Forward Button",    "default" },
            { 0x00C3, 5, "Gesture Button",    "gesture-trigger" },
            { 0x00C4, 6, "Easy Switch",       "default" },
            { 0x0000, 7, "Thumb Wheel Click", "default" },
        };

        m_controls.clear();
        for (const auto &e : entries) {
            ControlDescriptor cd;
            cd.controlId        = e.cid;
            cd.buttonIndex      = e.idx;
            cd.defaultName      = QString::fromUtf8(e.name);
            cd.defaultActionType = QString::fromUtf8(e.action);
            cd.configurable     = true;
            m_controls.append(cd);
        }
    }
};

} // namespace logitune::test
