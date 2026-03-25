#include "Transport.h"

#include <QThread>

namespace logitune::hidpp {

Transport::Transport(HidrawDevice *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{}

std::optional<Report> Transport::sendRequest(const Report &request, int timeoutMs)
{
    return trySend(request, timeoutMs, /*retriesLeft=*/3);
}

std::optional<Report> Transport::trySend(const Report &request, int timeoutMs, int retriesLeft)
{
    auto bytes = request.serialize();
    int written = m_device->writeReport(bytes);
    if (written < 0)
        return std::nullopt;

    auto responseBytes = m_device->readReport(timeoutMs);
    if (responseBytes.empty()) {
        // Timeout — retry once
        if (retriesLeft > 0)
            return trySend(request, timeoutMs, retriesLeft - 1);
        return std::nullopt;
    }

    auto response = Report::parse(responseBytes);
    if (!response)
        return std::nullopt;

    if (response->isError()) {
        ErrorCode ec = response->errorCode();

        emit deviceError(ec, response->params[0]);

        switch (ec) {
        case ErrorCode::Busy:
            if (retriesLeft > 0) {
                QThread::msleep(100);
                return trySend(request, timeoutMs, retriesLeft - 1);
            }
            return std::nullopt;

        case ErrorCode::OutOfRange:
            // Clamp and retry once — caller is responsible for adjusting params;
            // here we simply retry with the same request once.
            if (retriesLeft > 0)
                return trySend(request, timeoutMs, 0);
            return std::nullopt;

        case ErrorCode::Unsupported:
            return std::nullopt;

        default:
            return std::nullopt;
        }
    }

    return response;
}

void Transport::run()
{
    m_running = true;
    while (m_running) {
        auto bytes = m_device->readReport(/*timeoutMs=*/100);
        if (bytes.empty())
            continue;

        auto report = Report::parse(bytes);
        if (!report)
            continue;

        if (report->isError()) {
            emit deviceError(report->errorCode(), report->params[0]);
        } else {
            emit notificationReceived(*report);
        }
    }
}

void Transport::stop()
{
    m_running = false;
}

} // namespace logitune::hidpp
