#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QByteArray>
#include <functional>
#include <optional>

class QLocalSocket;
class QJsonObject;
class QTimer;

namespace logitune {

class ActionPresetRegistry;

class HyprlandDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    using BindReader = std::function<QByteArray()>;

    explicit HyprlandDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;

    /// Test seam: point to a registry the resolver should query.
    /// Takes non-owning pointer; the registry must outlive this object.
    void setPresetRegistry(const ActionPresetRegistry *registry) { m_registry = registry; }

    /// Test seam: replace the default Hyprland command-socket bind reader.
    void setBindReader(BindReader reader) { m_bindReader = std::move(reader); }

    /// Test seam: override $XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE.
    void setSocketDirForTest(const QString &socketDir) { m_socketDirOverride = socketDir; }

private:
    void connectEventSocket();
    void scheduleReconnect();
    void onEventReadyRead();
    void handleEventLine(const QByteArray &line);

    QString socketDir() const;
    QString eventSocketPath() const;
    QString commandSocketPath() const;
    QByteArray readHyprlandCommand(const QByteArray &command) const;
    QByteArray readBindsJson() const;
    std::optional<ButtonAction> resolveHyprlandBind(const QJsonObject &spec) const;

    static QString bindingToKeystroke(int modmask, const QString &key);
    static QString normalizeKeyName(const QString &key);

    QLocalSocket *m_eventSocket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QByteArray m_eventBuffer;
    QString m_lastAppId;
    QString m_socketDirOverride;
    bool m_available = false;
    const ActionPresetRegistry *m_registry = nullptr;
    BindReader m_bindReader;
};

} // namespace logitune
