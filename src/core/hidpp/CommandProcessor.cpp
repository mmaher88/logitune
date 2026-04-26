#include "hidpp/CommandProcessor.h"
#include "logging/LogManager.h"
#include <QThread>

namespace logitune::hidpp {

CommandProcessor::CommandProcessor(FeatureDispatcher *features, Transport *transport,
                            uint8_t deviceIndex, QObject *parent)
    : QObject(parent)
    , m_features(features)
    , m_transport(transport)
    , m_deviceIndex(deviceIndex)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &CommandProcessor::processNext);
}

void CommandProcessor::enqueue(FeatureId feature, uint8_t functionId,
                            std::span<const uint8_t> params,
                            FeatureDispatcher::ResponseCallback callback)
{
    Command cmd;
    cmd.feature = feature;
    cmd.functionId = functionId;
    cmd.params.assign(params.begin(), params.end());
    cmd.callback = std::move(callback);

    m_queue.push(std::move(cmd));

    if (m_running && !m_timer.isActive())
        m_timer.start(0);
}

void CommandProcessor::clear()
{
    std::queue<Command> empty;
    m_queue.swap(empty);
}

int CommandProcessor::pending() const
{
    return static_cast<int>(m_queue.size());
}

void CommandProcessor::start()
{
    m_running = true;
    if (!m_queue.empty())
        m_timer.start(0);
}

void CommandProcessor::stop()
{
    m_running = false;
    m_timer.stop();
}

void CommandProcessor::processNext()
{
    if (!m_running || m_queue.empty())
        return;

    Command cmd = std::move(m_queue.front());
    m_queue.pop();

    // Use callAsync — the response arrives via handleNotification -> handleResponse.
    // The callback is invoked by FeatureDispatcher::handleResponse when the
    // matching softwareId response arrives.
    m_features->callAsync(m_transport, m_deviceIndex,
                          cmd.feature, cmd.functionId,
                          std::span<const uint8_t>(cmd.params),
                          std::move(cmd.callback));

    // Schedule next command with inter-command delay for pacing
    if (!m_queue.empty() && m_running) {
        m_timer.start(kInterCommandDelayMs);
    } else if (m_queue.empty()) {
        emit queueDrained();
    }
}

} // namespace logitune::hidpp
