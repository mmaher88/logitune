#include "DeviceCommands.h"
#include "DeviceSelection.h"
#include "DeviceSession.h"

namespace logitune {

DeviceCommands::DeviceCommands(DeviceSelection *selection, QObject *parent)
    : QObject(parent)
    , m_selection(selection)
{}

void DeviceCommands::requestDpi(int value)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setDPI(value);
    emit userChangedSomething();
}

void DeviceCommands::requestSmartShift(bool enabled, int threshold)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setSmartShift(enabled, threshold);
    emit userChangedSomething();
}

void DeviceCommands::requestScrollConfig(bool hiRes, bool invert)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setScrollConfig(hiRes, invert);
    emit userChangedSomething();
}

// Pair-idiom: setThumbWheelMode writes both mode and invert, so the two
// setters below read the other value from the session (not the profile)
// to avoid clobbering hardware state with a stale profile value if an
// earlier HID++ write silently failed.
void DeviceCommands::requestThumbWheelMode(const QString &mode)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setThumbWheelMode(mode, session->thumbWheelInvert());
    emit userChangedSomething();
}

void DeviceCommands::requestThumbWheelInvert(bool invert)
{
    auto *session = m_selection ? m_selection->activeSession() : nullptr;
    if (!session) return;
    session->setThumbWheelMode(session->thumbWheelMode(), invert);
    emit userChangedSomething();
}

} // namespace logitune
