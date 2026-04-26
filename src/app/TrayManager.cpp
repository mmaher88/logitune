#include "TrayManager.h"
#include "PhysicalDevice.h"
#include "models/DeviceModel.h"
#include <QIcon>

namespace logitune {

TrayManager::TrayManager(DeviceModel *dm, QObject *parent)
    : QObject(parent)
    , m_deviceModel(dm)
{
    m_trayIcon.setIcon(QIcon(":/Logitune/qml/assets/logitune-tray.svg"));

    // Skeleton: Show / separator / (device sections inserted here) / Quit
    m_showAction = m_menu.addAction(QStringLiteral("Show Logitune"));
    m_menu.addSeparator();
    m_quitAction = m_menu.addAction(QStringLiteral("Quit"));
    m_trayIcon.setContextMenu(&m_menu);

    connect(m_showAction, &QAction::triggered, this,
            &TrayManager::showWindowRequested);
    connect(&m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit showWindowRequested();
    });

    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::countChanged,
                this, &TrayManager::rebuildEntries);
        rebuildEntries();
    } else {
        refreshTooltip();
    }
}

TrayManager::~TrayManager()
{
    for (auto &entry : m_entries)
        QObject::disconnect(entry.stateConn);
}

void TrayManager::show()
{
    m_trayIcon.show();
}

void TrayManager::rebuildEntries()
{
    if (!m_deviceModel) {
        refreshTooltip();
        return;
    }

    const auto &devices = m_deviceModel->devices();

    // Remove entries for devices no longer present
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        PhysicalDevice *d = it.key();
        if (!devices.contains(d)) {
            m_menu.removeAction(it.value().separator);
            m_menu.removeAction(it.value().battery);
            m_menu.removeAction(it.value().header);
            delete it.value().separator;
            delete it.value().battery;
            delete it.value().header;
            QObject::disconnect(it.value().stateConn);
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }

    // Add entries for new devices, inserted before the trailing Quit action
    for (PhysicalDevice *d : devices) {
        if (m_entries.contains(d))
            continue;

        DeviceEntry entry;
        entry.header = new QAction(d->deviceName(), &m_menu);
        entry.header->setEnabled(false);
        entry.battery = new QAction(QStringLiteral("    Battery: ---%"), &m_menu);
        entry.battery->setEnabled(false);
        entry.separator = new QAction(&m_menu);
        entry.separator->setSeparator(true);

        m_menu.insertAction(m_quitAction, entry.header);
        m_menu.insertAction(m_quitAction, entry.battery);
        m_menu.insertAction(m_quitAction, entry.separator);

        entry.stateConn = connect(d, &PhysicalDevice::stateChanged, this,
            [this, d]() {
                refreshEntry(d);
                refreshTooltip();
            });

        m_entries.insert(d, entry);
        refreshEntry(d);
    }

    refreshTooltip();
}

void TrayManager::refreshEntry(PhysicalDevice *device)
{
    auto it = m_entries.find(device);
    if (it == m_entries.end())
        return;

    it.value().header->setText(device->deviceName());

    const int level = device->batteryLevel();
    const bool charging = device->batteryCharging();
    // Leading spaces indent the battery row under its device header so the
    // menu has a visible parent/child hierarchy. QAction renders text
    // verbatim, so this is the simplest portable indent.
    QString text = QStringLiteral("    Battery: %1%").arg(level);
    if (charging)
        text.append(QStringLiteral(" \u26A1"));  // U+26A1 HIGH VOLTAGE SIGN
    it.value().battery->setText(text);
}

void TrayManager::refreshTooltip()
{
    if (!m_deviceModel || m_deviceModel->devices().isEmpty()) {
        m_trayIcon.setToolTip(QStringLiteral("Logitune"));
        return;
    }

    const auto &devices = m_deviceModel->devices();
    if (devices.size() == 1) {
        PhysicalDevice *d = devices.first();
        QString line = QStringLiteral("Logitune \u2014 %1: %2%")
            .arg(d->deviceName()).arg(d->batteryLevel());
        if (d->batteryCharging())
            line.append(QStringLiteral(" \u26A1"));
        m_trayIcon.setToolTip(line);
        return;
    }

    QStringList parts;
    parts.reserve(devices.size());
    for (PhysicalDevice *d : devices) {
        QString part = QStringLiteral("%1: %2%")
            .arg(d->deviceName()).arg(d->batteryLevel());
        if (d->batteryCharging())
            part.append(QStringLiteral(" \u26A1"));
        parts << part;
    }
    m_trayIcon.setToolTip(QStringLiteral("Logitune\n%1")
                          .arg(parts.join(QStringLiteral(" \u2022 "))));
}

} // namespace logitune
