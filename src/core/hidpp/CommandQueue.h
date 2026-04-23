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
class CommandQueue : public QObject {
    Q_OBJECT
public:
    explicit CommandQueue(FeatureDispatcher *features, Transport *transport,
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

    // MX Master 3S firmware returns HWError (0x04) on setCidReporting when
    // commands arrive faster than ~15-20ms apart. Observed in the field
    // during rapid focus changes that burst-send 5+ divert commands: the
    // device rejects most of them and button diversion state silently
    // desyncs, so remaps fail until the next successful re-apply. 30ms
    // matches Solaar's default pacing and logid's 30ms constant.
    static constexpr int kInterCommandDelayMs = 30;
    static constexpr int kMaxRetries = 3;
    static constexpr int kRetryDelayMs = 50;
};

} // namespace logitune::hidpp
