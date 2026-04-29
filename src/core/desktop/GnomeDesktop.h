#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <functional>
#include <optional>

namespace logitune {

class ActionPresetRegistry;

class GnomeDesktop : public LinuxDesktopBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logitune.FocusWatcher")
public:
    enum AppIndicatorState { AppIndicatorUnknown, AppIndicatorNotInstalled, AppIndicatorDisabled, AppIndicatorActive };

    /// Reader takes (schema, key) and returns the raw gsettings output
    /// (e.g. "['<Super>d']" or "" on failure).
    using GsettingsReader = std::function<QString(const QString &, const QString &)>;

    explicit GnomeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;

    AppIndicatorState appIndicatorStatus() const { return m_appIndicatorStatus; }

    /// Test seam: point to a registry the resolver should query.
    /// Takes non-owning pointer; the registry must outlive this object.
    void setPresetRegistry(const ActionPresetRegistry *registry) { m_registry = registry; }

    /// Test seam: replace the default QProcess-based gsettings reader.
    void setGsettingsReader(GsettingsReader reader) { m_reader = std::move(reader); }

    /// Static helper: parse gsettings output ("['<Super>d']") into a
    /// keystroke payload ("Super+d"). Returns empty string on failure.
    static QString gsettingsToKeystroke(const QString &gsettingsOutput);

public slots:
    void focusChanged(const QString &appId, const QString &title);

private:
    bool ensureExtensionInstalled();
    int detectShellMajorVersion();
    void detectAppIndicatorStatus();

    QString m_lastAppId;
    bool m_available = false;
    AppIndicatorState m_appIndicatorStatus = AppIndicatorUnknown;
    const ActionPresetRegistry *m_registry = nullptr;
    GsettingsReader m_reader;
};

} // namespace logitune
