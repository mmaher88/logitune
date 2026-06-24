#pragma once
#include "ButtonAction.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <optional>

namespace logitune {

class IDesktopIntegration : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IDesktopIntegration() = default;

    virtual void start() = 0;
    virtual bool available() const = 0;
    virtual QString desktopName() const = 0;
    virtual QStringList detectedCompositors() const = 0;

    /// Short key identifying this DE in the action preset variants map
    /// (e.g. "kde", "gnome", "hyprland", "generic"). Matches a top-level key under
    /// "variants" in actions.json.
    virtual QString variantKey() const = 0;

    /// Resolve a semantic preset id (e.g. "show-desktop") to a concrete
    /// ButtonAction the ActionExecutor can fire. Returns nullopt when
    /// the preset is not supported on this DE or the user's live binding
    /// is empty/unreachable.
    virtual std::optional<ButtonAction> resolveNamedAction(const QString &id) const = 0;

    /// Block/unblock global shortcuts during keystroke capture.
    virtual void blockGlobalShortcuts(bool block) = 0;

    /// Return list of running graphical applications.
    /// Each entry is a QVariantMap with keys "wmClass" and "title".
    virtual QVariantList runningApplications() const = 0;

signals:
    void activeWindowChanged(const QString &wmClass, const QString &title);
};

} // namespace logitune
