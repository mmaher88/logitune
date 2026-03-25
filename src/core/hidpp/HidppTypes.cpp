#include "HidppTypes.h"

namespace logitune::hidpp {

std::vector<uint8_t> Report::serialize() const
{
    if (reportId == kShortReportId) {
        std::vector<uint8_t> buf(kShortReportSize, 0);
        buf[0] = reportId;
        buf[1] = deviceIndex;
        buf[2] = featureIndex;
        buf[3] = static_cast<uint8_t>((functionId << 4) | (softwareId & 0x0F));
        for (int i = 0; i < 3 && i < paramLength; ++i)
            buf[4 + i] = params[i];
        return buf;
    } else {
        // Long report (default)
        std::vector<uint8_t> buf(kLongReportSize, 0);
        buf[0] = reportId;
        buf[1] = deviceIndex;
        buf[2] = featureIndex;
        buf[3] = static_cast<uint8_t>((functionId << 4) | (softwareId & 0x0F));
        for (int i = 0; i < 16 && i < paramLength; ++i)
            buf[4 + i] = params[i];
        return buf;
    }
}

std::optional<Report> Report::parse(std::span<const uint8_t> data)
{
    if (data.size() < static_cast<size_t>(kShortReportSize))
        return std::nullopt;

    Report r;
    r.reportId     = data[0];
    r.deviceIndex  = data[1];
    r.featureIndex = data[2];
    r.functionId   = (data[3] >> 4) & 0x0F;
    r.softwareId   = data[3] & 0x0F;

    if (r.reportId == kShortReportId) {
        r.paramLength = 3;
        for (int i = 0; i < 3; ++i)
            r.params[i] = data[4 + i];
    } else {
        // Long report: need at least kLongReportSize bytes
        r.paramLength = 16;
        for (int i = 0; i < 16 && (4 + i) < static_cast<int>(data.size()); ++i)
            r.params[i] = data[4 + i];
    }

    return r;
}

bool Report::isError() const
{
    return featureIndex == 0xFF;
}

ErrorCode Report::errorCode() const
{
    // Error report layout: byte[5] = error code
    return static_cast<ErrorCode>(params[1]);
}

} // namespace logitune::hidpp
