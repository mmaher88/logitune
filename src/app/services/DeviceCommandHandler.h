#pragma once
#include <QObject>
#include <QString>

namespace logitune {

class ActiveDeviceResolver;

/// Routes UI change requests (from DeviceModel signals) to the active
/// DeviceSession. Emits userChangedSomething() after each mutation so
/// ProfileOrchestrator (wired in AppRoot) can trigger a save.
///
/// No-op if there is no active session (ActiveDeviceResolver returns null).
class DeviceCommandHandler : public QObject {
    Q_OBJECT
public:
    explicit DeviceCommandHandler(ActiveDeviceResolver *selection, QObject *parent = nullptr);

public slots:
    void requestDpi(int value);
    void requestSmartShift(bool enabled, int threshold);
    void requestScrollConfig(bool hiRes, bool invert);
    void requestThumbWheelMode(const QString &mode);
    void requestThumbWheelInvert(bool invert);

signals:
    /// Emitted after any successful mutation. Subscribers should call
    /// saveCurrentProfile() in response.
    void userChangedSomething();

private:
    ActiveDeviceResolver *m_selection;
};

} // namespace logitune
