#pragma once
#include <QObject>
#include <QString>

namespace logitune {

class DeviceModel;
class PhysicalDevice;
class DeviceSession;

/// Resolves the currently selected PhysicalDevice / DeviceSession / serial
/// from DeviceModel's selection index and its ordered device list.
/// Emits selectionChanged whenever the resolution changes.
///
/// Read-only, single source of truth. Other services hold a pointer to this
/// and either query on demand (active*()) or subscribe to selectionChanged.
class DeviceSelection : public QObject {
    Q_OBJECT
public:
    explicit DeviceSelection(DeviceModel *deviceModel, QObject *parent = nullptr);

    PhysicalDevice *activeDevice() const;
    DeviceSession  *activeSession() const;
    QString         activeSerial() const;

signals:
    void selectionChanged();

public slots:
    void onSelectionIndexChanged();

private:
    DeviceModel *m_deviceModel;
};

} // namespace logitune
