#include "PhysicalDevice.h"
#include "DeviceSession.h"

namespace logitune {

PhysicalDevice::PhysicalDevice(const QString &serial, QObject *parent)
    : QObject(parent)
    , m_serial(serial)
{
}

QList<DeviceSession *> PhysicalDevice::transports() const
{
    QList<DeviceSession *> result;
    if (m_primary)
        result.append(m_primary);
    result.append(m_alternates);
    return result;
}

int PhysicalDevice::transportCount() const
{
    return (m_primary ? 1 : 0) + m_alternates.size();
}

void PhysicalDevice::attachTransport(DeviceSession *session)
{
    if (!session)
        return;
    if (session == m_primary || m_alternates.contains(session))
        return;

    connectSessionSignals(session);

    if (!m_primary) {
        m_primary = session;
    } else if (!m_primary->isConnected() && session->isConnected()) {
        m_alternates.append(m_primary);
        m_primary = session;
    } else {
        m_alternates.append(session);
    }

    emit stateChanged();
}

bool PhysicalDevice::detachTransport(DeviceSession *session)
{
    if (!session)
        return m_primary == nullptr && m_alternates.isEmpty();

    disconnectSessionSignals(session);

    if (m_primary == session) {
        m_primary = nullptr;
        if (!m_alternates.isEmpty()) {
            m_primary = m_alternates.takeFirst();
            // Prefer a connected alternate if available
            for (int i = 0; i < m_alternates.size(); ++i) {
                if (m_alternates[i]->isConnected() && !m_primary->isConnected()) {
                    std::swap(m_primary, m_alternates[i]);
                    break;
                }
            }
        }
    } else {
        m_alternates.removeOne(session);
    }

    emit stateChanged();
    return m_primary == nullptr && m_alternates.isEmpty();
}

void PhysicalDevice::promoteBest()
{
    // If primary is offline but an alternate is connected, swap.
    if (!m_primary || m_primary->isConnected())
        return;
    for (int i = 0; i < m_alternates.size(); ++i) {
        if (m_alternates[i]->isConnected()) {
            std::swap(m_primary, m_alternates[i]);
            break;
        }
    }
}

void PhysicalDevice::connectSessionSignals(DeviceSession *session)
{
    connect(session, &DeviceSession::setupComplete, this, [this]() {
        promoteBest();
        emit transportSetupComplete();
        emit stateChanged();
    });
    connect(session, &DeviceSession::disconnected, this, [this]() {
        promoteBest();
        emit stateChanged();
    });
    connect(session, &DeviceSession::batteryChanged, this,
            [this](int, bool) { emit stateChanged(); });
    connect(session, &DeviceSession::smartShiftChanged, this,
            [this](bool, int) { emit stateChanged(); });
    connect(session, &DeviceSession::scrollConfigChanged, this,
            [this]() { emit stateChanged(); });
    connect(session, &DeviceSession::thumbWheelModeChanged, this,
            [this]() { emit stateChanged(); });

    // Forwarded input events.
    connect(session, &DeviceSession::gestureRawXY,
            this, &PhysicalDevice::gestureRawXY);
    connect(session, &DeviceSession::divertedButtonPressed,
            this, &PhysicalDevice::divertedButtonPressed);
    connect(session, &DeviceSession::thumbWheelRotation,
            this, &PhysicalDevice::thumbWheelRotation);
}

void PhysicalDevice::disconnectSessionSignals(DeviceSession *session)
{
    disconnect(session, nullptr, this, nullptr);
}

// ---------------------------------------------------------------------------
// Read-only delegation to primary
// ---------------------------------------------------------------------------

bool PhysicalDevice::isConnected() const
{
    return m_primary && m_primary->isConnected();
}

QString PhysicalDevice::deviceName() const
{
    return m_primary ? m_primary->deviceName() : QString();
}

QString PhysicalDevice::connectionType() const
{
    return m_primary ? m_primary->connectionType() : QString();
}

int PhysicalDevice::batteryLevel() const
{
    return m_primary ? m_primary->batteryLevel() : 0;
}

bool PhysicalDevice::batteryCharging() const
{
    return m_primary && m_primary->batteryCharging();
}

int PhysicalDevice::currentDPI() const
{
    return m_primary ? m_primary->currentDPI() : 0;
}

int PhysicalDevice::minDPI() const
{
    return m_primary ? m_primary->minDPI() : 200;
}

int PhysicalDevice::maxDPI() const
{
    return m_primary ? m_primary->maxDPI() : 8000;
}

int PhysicalDevice::dpiStep() const
{
    return m_primary ? m_primary->dpiStep() : 50;
}

bool PhysicalDevice::smartShiftEnabled() const
{
    return m_primary && m_primary->smartShiftEnabled();
}

int PhysicalDevice::smartShiftThreshold() const
{
    return m_primary ? m_primary->smartShiftThreshold() : 0;
}

bool PhysicalDevice::scrollHiRes() const
{
    return m_primary && m_primary->scrollHiRes();
}

bool PhysicalDevice::scrollInvert() const
{
    return m_primary && m_primary->scrollInvert();
}

bool PhysicalDevice::scrollRatchet() const
{
    return m_primary ? m_primary->scrollRatchet() : true;
}

QString PhysicalDevice::thumbWheelMode() const
{
    return m_primary ? m_primary->thumbWheelMode() : QStringLiteral("scroll");
}

bool PhysicalDevice::thumbWheelInvert() const
{
    return m_primary && m_primary->thumbWheelInvert();
}

int PhysicalDevice::thumbWheelDefaultDirection() const
{
    return m_primary ? m_primary->thumbWheelDefaultDirection() : 1;
}

int PhysicalDevice::currentHost() const
{
    return m_primary ? m_primary->currentHost() : -1;
}

int PhysicalDevice::hostCount() const
{
    return m_primary ? m_primary->hostCount() : 0;
}

bool PhysicalDevice::isHostPaired(int host) const
{
    return m_primary && m_primary->isHostPaired(host);
}

QString PhysicalDevice::deviceSerial() const
{
    return m_serial;
}

QString PhysicalDevice::firmwareVersion() const
{
    return m_primary ? m_primary->firmwareVersion() : QString();
}

const IDevice *PhysicalDevice::descriptor() const
{
    return m_primary ? m_primary->descriptor() : nullptr;
}

} // namespace logitune
