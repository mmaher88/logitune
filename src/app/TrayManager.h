#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMap>

namespace logitune {

class DeviceModel;
class PhysicalDevice;

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(DeviceModel *dm, QObject *parent = nullptr);
    ~TrayManager() override;

    QSystemTrayIcon *trayIcon() { return &m_trayIcon; }
    QMenu *menu() { return &m_menu; }
    QAction *showAction() { return m_showAction; }
    QAction *quitAction() { return m_quitAction; }

    void show();

signals:
    void showWindowRequested();

private:
    struct DeviceEntry {
        QAction *header    = nullptr;  // disabled, shows device name
        QAction *battery   = nullptr;  // disabled, shows "Battery: N%"
        QAction *separator = nullptr;  // separator after the battery row
        QMetaObject::Connection stateConn;
    };

    void rebuildEntries();
    void refreshEntry(PhysicalDevice *device);
    void refreshTooltip();

    QSystemTrayIcon m_trayIcon;
    QMenu m_menu;
    QAction *m_showAction = nullptr;
    QAction *m_quitAction = nullptr;

    DeviceModel *m_deviceModel = nullptr;
    QMap<PhysicalDevice *, DeviceEntry> m_entries;
};

} // namespace logitune
