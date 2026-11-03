#include "hidpp/features/ReprogControls.h"
#include <algorithm>

namespace logitune::hidpp::features {

std::vector<std::pair<uint16_t, bool>> DivertedButtons::update(const Report &report)
{
    std::vector<uint16_t> held;
    for (int offset = 0; offset + 1 < std::min(8, report.paramLength); offset += 2) {
        const uint16_t cid = (report.params[offset] << 8) | report.params[offset + 1];
        if (cid && std::find(held.begin(), held.end(), cid) == held.end()) held.push_back(cid);
    }
    std::vector<std::pair<uint16_t, bool>> events;
    for (auto cid : m_held)
        if (std::find(held.begin(), held.end(), cid) == held.end()) events.emplace_back(cid, false);
    for (auto cid : held)
        if (std::find(m_held.begin(), m_held.end(), cid) == m_held.end()) events.emplace_back(cid, true);
    m_held = std::move(held);
    return events;
}

int ReprogControls::parseControlCount(const Report &r)
{
    return static_cast<int>(r.params[0]);
}

ControlInfo ReprogControls::parseControlInfo(const Report &r)
{
    ControlInfo info;
    info.controlId  = static_cast<uint16_t>((r.params[0] << 8) | r.params[1]);
    info.taskId     = static_cast<uint16_t>((r.params[2] << 8) | r.params[3]);
    info.divertable = (r.params[4] & 0x01) != 0;
    info.persist    = (r.params[4] & 0x02) != 0;
    info.rawXY      = (r.params[4] & 0x20) != 0; // additionalFlags bit 5 = rawXY capable
    return info;
}

// From logid ReprogControlsV4::setControlReporting:
// params[2] = flags byte (all flags in one byte):
//   bit 0: TemporaryDiverted
//   bit 1: ChangeTemporaryDivert
//   bit 2: PersistentlyDiverted
//   bit 3: ChangePersistentDivert
//   bit 4: RawXYDiverted
//   bit 5: ChangeRawXYDivert
// params[3-4] = remap target CID (0 = no remap)
std::vector<uint8_t> ReprogControls::buildSetDivert(uint16_t controlId, bool divert, bool rawXY)
{
    uint8_t flags = 0;
    if (divert) {
        flags |= 0x03; // TemporaryDiverted + ChangeTemporaryDivert
    } else {
        flags |= 0x02; // ChangeTemporaryDivert (clear divert)
    }
    if (rawXY) {
        flags |= 0x30; // RawXYDiverted + ChangeRawXYDivert
    }
    return {
        static_cast<uint8_t>(controlId >> 8),
        static_cast<uint8_t>(controlId & 0xFF),
        flags,
        0x00, 0x00  // remap target CID (none)
    };
}

uint16_t ReprogControls::parseDivertedButtonEvent(const Report &r)
{
    return static_cast<uint16_t>((r.params[0] << 8) | r.params[1]);
}

// DivertedRawXYEvent: params[0-1]=dx (int16 BE), params[2-3]=dy (int16 BE)
RawXYEvent ReprogControls::parseDivertedRawXYEvent(const Report &r)
{
    RawXYEvent evt;
    evt.dx = static_cast<int16_t>((r.params[0] << 8) | r.params[1]);
    evt.dy = static_cast<int16_t>((r.params[2] << 8) | r.params[3]);
    return evt;
}

} // namespace logitune::hidpp::features
