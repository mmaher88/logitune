#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <optional>

class QDBusVariant;

namespace logitune {

/// Follows the desktop's light/dark preference through the XDG desktop
/// portal (org.freedesktop.appearance color-scheme). Reads the value once on
/// request and emits darkChanged whenever the desktop switches, so the window
/// can follow tools like darkman or GNOME's own toggle without a restart.
class SystemColorScheme : public QObject {
    Q_OBJECT
public:
    explicit SystemColorScheme(QObject *parent = nullptr);

    /// Current preference from the portal, or nullopt if the portal did not
    /// answer within the timeout.
    std::optional<bool> readDark() const;

    /// Maps a portal color-scheme value to dark/light. The value may arrive
    /// wrapped in one or two QDBusVariant layers. 1 means "prefer dark";
    /// 0 (no preference) and 2 (prefer light) both mean light.
    static std::optional<bool> darkFromPortalValue(const QVariant &value);

signals:
    void darkChanged(bool dark);

public slots:
    void onSettingChanged(const QString &ns, const QString &key, const QDBusVariant &value);

private:
    mutable std::optional<bool> m_lastDark;
};

} // namespace logitune
