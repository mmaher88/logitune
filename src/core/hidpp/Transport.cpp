#include "Transport.h"
#include "logging/LogManager.h"

#include <QThread>

namespace logitune::hidpp {

Transport::Transport(HidrawDevice *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{}

std::optional<Report> Transport::sendRequest(const Report &request, int timeoutMs)
{
    return trySend(request, timeoutMs, /*retriesLeft=*/1);
}

bool Transport::sendRequestAsync(const Report &request)
{
    auto bytes = request.serialize();
    int written = m_device->writeReport(std::span<const uint8_t>(bytes));
    return written > 0;
}

std::optional<Report> Transport::trySend(const Report &request, int timeoutMs, int retriesLeft)
{
    auto bytes = request.serialize();
    int written = m_device->writeReport(bytes);
    if (written < 0) {
        qCWarning(lcHidpp) << "write failed";
        return std::nullopt;
    }

    // Read responses, skipping HID++ 1.0 DJ noise and non-matching reports
    int readAttempts = 0;
    while (readAttempts < 20) {
        auto responseBytes = m_device->readReport(timeoutMs);
        if (responseBytes.empty()) {
            if (retriesLeft > 0)
                return trySend(request, timeoutMs, retriesLeft - 1);
            qCDebug(lcHidpp) << "timeout waiting for response";
            return std::nullopt;
        }

        // Skip HID++ 1.0 DJ notifications (reportId=0x10, byte2 >= 0x80)
        // These are receiver-level messages, not responses to our 2.0 requests.
        // Don't count them against the attempt limit.
        if (responseBytes.size() >= 3 && responseBytes[0] == kShortReportId &&
            responseBytes[2] >= 0x80) {
            continue; // DJ noise — keep reading without incrementing attempts
        }

        readAttempts++;

        auto response = Report::parse(responseBytes);
        if (!response) {
            continue;
        }

        // Check if this response matches our request.
        // Must match on deviceIndex, featureIndex, functionId AND softwareId —
        // all four are needed because multiple in-flight requests to the same
        // feature (e.g. Root.getFeatureID during enumeration) only differ in
        // softwareId. Without this, stale responses from before a reconnect
        // get mis-matched to new requests, scrambling the feature table.
        if (response->deviceIndex == request.deviceIndex &&
            response->featureIndex == request.featureIndex &&
            response->functionId == request.functionId &&
            response->softwareId == request.softwareId) {
            if (response->isError()) {
                ErrorCode ec = response->errorCode();
                qCDebug(lcHidpp) << "error response: code=" << static_cast<int>(ec);
                emit deviceError(ec, response->params[0]);
                switch (ec) {
                case ErrorCode::Busy:
                    if (retriesLeft > 0) {
                        QThread::msleep(100);
                        return trySend(request, timeoutMs, retriesLeft - 1);
                    }
                    return std::nullopt;
                case ErrorCode::OutOfRange:
                    if (retriesLeft > 0)
                        return trySend(request, timeoutMs, 0);
                    return std::nullopt;
                default:
                    return std::nullopt;
                }
            }
            return response;
        }

        // Not our response — it's a notification. Emit it and keep reading.
        if (!response->isError()) {
            emit notificationReceived(*response);
        }
    }

    qCDebug(lcHidpp) << "exhausted read attempts";
    return std::nullopt;
}


} // namespace logitune::hidpp
