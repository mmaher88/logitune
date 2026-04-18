#pragma once
#include "interfaces/IDevice.h"
#include <memory>
#include <vector>
#include <QString>
#include <QJsonObject>

namespace logitune {

class JsonDevice : public IDevice {
public:
    enum class Status { Verified, Beta };

    static std::unique_ptr<JsonDevice> load(const QString& dirPath);

    bool refresh();
    bool refreshFromObject(const QJsonObject &root);

    Status status() const { return m_status; }
    QString sourcePath() const { return m_sourcePath; }
    qint64 loadedMtime() const { return m_loadedMtime; }

    QString deviceName() const override { return m_name; }
    std::vector<uint16_t> productIds() const override { return m_pids; }
    bool matchesPid(uint16_t pid) const override;
    QList<ControlDescriptor> controls() const override { return m_controls; }
    QList<HotspotDescriptor> buttonHotspots() const override { return m_buttonHotspots; }
    QList<HotspotDescriptor> scrollHotspots() const override { return m_scrollHotspots; }
    FeatureSupport features() const override { return m_features; }
    QString frontImagePath() const override { return m_frontImage; }
    QString sideImagePath() const override { return m_sideImage; }
    QString backImagePath() const override { return m_backImage; }
    QMap<QString, ButtonAction> defaultGestures() const override { return m_defaultGestures; }
    int minDpi() const override { return m_minDpi; }
    int maxDpi() const override { return m_maxDpi; }
    int dpiStep() const override { return m_dpiStep; }
    std::vector<int> dpiCycleRing() const override { return m_dpiCycleRing; }
    QList<EasySwitchSlotPosition> easySwitchSlotPositions() const override { return m_easySwitchSlots; }

private:
    JsonDevice() = default;
    bool parseFromDir(const QString& dirPath);
    bool parseFromObject(const QJsonObject& root, const QString& dirPath, bool strict = true);

    Status m_status = Status::Beta;
    QString m_name;
    std::vector<uint16_t> m_pids;
    FeatureSupport m_features;
    int m_minDpi = 200, m_maxDpi = 8000, m_dpiStep = 50;
    std::vector<int> m_dpiCycleRing;
    QList<ControlDescriptor> m_controls;
    QList<HotspotDescriptor> m_buttonHotspots;
    QList<HotspotDescriptor> m_scrollHotspots;
    QString m_frontImage, m_sideImage, m_backImage;
    QList<EasySwitchSlotPosition> m_easySwitchSlots;
    QMap<QString, ButtonAction> m_defaultGestures;
    QString m_sourcePath;
    qint64 m_loadedMtime = 0;
};

} // namespace logitune
