#pragma once
#include "ButtonAction.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

namespace logitune {

struct RingSlot {
    QString icon;
    QString name;
    ButtonAction action;
    QList<RingSlot> children;
    // Set for an adjuster slot (scroll to change a level, e.g. volume): shell commands under
    // "up", "down", "read" (prints the current level) and "press" (on click). Empty otherwise.
    // Kept as raw JSON because only the ring overlay interprets it.
    QJsonObject adjust;
};

QList<RingSlot> defaultRingSlots();
QJsonArray ringSlotsToJson(const QList<RingSlot> &entries);
QList<RingSlot> ringSlotsFromJson(const QJsonArray &array);

class ActionsRingTrigger {
public:
    enum Result { None, Open, Close, Select };
    Result press(qint64 now) {
        if (m_open) { m_held = false; return Close; }
        m_open = true;
        m_held = true;
        m_pressedAt = now;
        return Open;
    }
    Result release(qint64 now) {
        if (!m_held) return None;
        m_held = false;
        return m_open && now - m_pressedAt > 500 ? Select : None;
    }
    void closed() { m_open = false; m_held = false; }
private:
    bool m_open = false;
    bool m_held = false;
    qint64 m_pressedAt = 0;
};

} // namespace logitune
