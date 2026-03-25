#pragma once
#include "HidppTypes.h"
#include "HidrawDevice.h"

#include <QObject>
#include <optional>

namespace logitune::hidpp {

class Transport : public QObject {
    Q_OBJECT
public:
    explicit Transport(HidrawDevice *device, QObject *parent = nullptr);

    std::optional<Report> sendRequest(const Report &request, int timeoutMs = 2000);
    void run();   // I/O loop — blocks until stop() is called
    void stop();

signals:
    void notificationReceived(const Report &report);
    void deviceError(ErrorCode code, uint8_t featureIndex);
    void deviceDisconnected();

private:
    std::optional<Report> trySend(const Report &request, int timeoutMs, int retriesLeft);

    HidrawDevice *m_device;
    bool m_running = false;
};

} // namespace logitune::hidpp
