#pragma once
#include <QString>
#include <optional>

namespace logitune {

// Haptic feedback strength and thumb-panel press force, saved per device. These belong to the
// mouse, not to an application, so they live beside the device's profiles instead of in them,
// and are reapplied on every connect because the device doesn't reliably keep them.
struct StoredPanelSettings {
    std::optional<bool> hapticsEnabled;
    std::optional<int> hapticsLevel;
    std::optional<int> panelForce;
};

QString panelSettingsPath(const QString &serial);
StoredPanelSettings loadPanelSettings(const QString &file);
void saveHaptics(const QString &file, bool enabled, int level);
void savePanelForce(const QString &file, int force);

} // namespace logitune
