#pragma once
#include <QLoggingCategory>
#include <QMutex>
#include <QFile>
#include <QDir>
#include <QString>

// Declare logging categories for each module
Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcProfile)
Q_DECLARE_LOGGING_CATEGORY(lcDevice)
Q_DECLARE_LOGGING_CATEGORY(lcHidpp)
Q_DECLARE_LOGGING_CATEGORY(lcHidppRaw)
Q_DECLARE_LOGGING_CATEGORY(lcInput)
Q_DECLARE_LOGGING_CATEGORY(lcFocus)
Q_DECLARE_LOGGING_CATEGORY(lcUi)

namespace logitune {

class LogManager {
public:
    static LogManager &instance();

    /// Initialize logging. Call once before any logging happens.
    /// If debugMode is true, all categories enabled regardless of settings.
    void init(bool debugMode = false);

    /// Enable/disable debug logging at runtime (from Settings toggle).
    void setLoggingEnabled(bool enabled);
    bool isLoggingEnabled() const;

    /// Get the current log file path (empty if logging disabled).
    QString currentLogPath() const;

    /// Get the previous session's log file path (for crash recovery).
    QString previousLogPath() const;

    /// Read the last N lines from the current (or specified) log file.
    QStringList tailLog(int lines = 50, const QString &path = {}) const;

    /// Rotate log files — keep last 5, delete older.
    void rotateLogFiles();

    /// Flush and close the log file.
    void shutdown();

    /// Write a pre-formatted line to the log file (thread-safe).
    /// Public so CrashHandler can write directly during signal handling.
    void writeToFile(const QString &formatted);

    /// Whether a message of this type is echoed to stderr. Warnings and above
    /// always are; debug and info only when stderr is a terminal. Under a
    /// desktop session stderr lands in the journal, and the log file already
    /// keeps the full trace.
    static bool echoesToStderr(QtMsgType type, bool stderrIsTerminal);

private:
    LogManager() = default;
    ~LogManager();
    LogManager(const LogManager &) = delete;
    LogManager &operator=(const LogManager &) = delete;

    static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

    QString logDir() const;
    QString logFileName() const;

    QMutex m_mutex;
    QFile m_logFile;
    bool m_enabled = false;
    bool m_initialized = false;
    QString m_previousLogPath;
};

} // namespace logitune
