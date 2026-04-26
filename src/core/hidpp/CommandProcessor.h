#pragma once
#include "hidpp/HidppTypes.h"
#include "hidpp/FeatureDispatcher.h"
#include "hidpp/Transport.h"
#include <QObject>
#include <QTimer>
#include <functional>
#include <queue>

namespace logitune::hidpp {

/// A command to send to the device.
struct Command {
    FeatureId feature;
    uint8_t functionId;
    std::vector<uint8_t> params;
    FeatureDispatcher::ResponseCallback callback;
};

/// Sends HID++ commands sequentially with pacing and retry on the main thread.
/// Commands are enqueued and processed one at a time via a QTimer.
/// Runs on the main thread — no fd contention with QSocketNotifier.
class CommandProcessor : public QObject {
    Q_OBJECT
public:
    explicit CommandProcessor(FeatureDispatcher *features, Transport *transport,
                          uint8_t deviceIndex, QObject *parent = nullptr);

    /// Enqueue a command. Can be called from any thread.
    void enqueue(FeatureId feature, uint8_t functionId,
                 std::span<const uint8_t> params,
                 FeatureDispatcher::ResponseCallback callback = nullptr);

    /// Clear all pending commands.
    void clear();

    /// Start processing.
    void start();

    /// Stop processing.
    void stop();

    /// Number of pending commands.
    int pending() const;

signals:
    void queueDrained();

private slots:
    void processNext();

private:
    FeatureDispatcher *m_features;
    Transport *m_transport;
    uint8_t m_deviceIndex;

    std::queue<Command> m_queue;
    QTimer m_timer;
    bool m_running = false;

    static constexpr int kInterCommandDelayMs = 10;
    static constexpr int kMaxRetries = 3;
    static constexpr int kRetryDelayMs = 50;
};

} // namespace logitune::hidpp
