#include "logging/LogManager.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <unistd.h>

// Define logging categories — disabled by default except warnings
Q_LOGGING_CATEGORY(lcApp,     "logitune.app",     QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfile, "logitune.profile", QtWarningMsg)
Q_LOGGING_CATEGORY(lcDevice,  "logitune.device",  QtWarningMsg)
Q_LOGGING_CATEGORY(lcHidpp,   "logitune.hidpp",   QtWarningMsg)
// Every incoming HID report and notification, one line each. Left out of the debug preset below;
// enable with QT_LOGGING_RULES="logitune.hidpp.raw.debug=true".
Q_LOGGING_CATEGORY(lcHidppRaw, "logitune.hidpp.raw", QtWarningMsg)
Q_LOGGING_CATEGORY(lcInput,   "logitune.input",   QtWarningMsg)
Q_LOGGING_CATEGORY(lcFocus,   "logitune.focus",   QtWarningMsg)
Q_LOGGING_CATEGORY(lcUi,      "logitune.ui",      QtWarningMsg)

namespace logitune {

LogManager &LogManager::instance()
{
    static LogManager inst;
    return inst;
}

LogManager::~LogManager()
{
    shutdown();
}

void LogManager::init(bool debugMode)
{
    if (m_initialized) return;
    m_initialized = true;

    QDir().mkpath(logDir());
    rotateLogFiles();

    // Find the most recent log file (previous session) before we create a new one
    QDir dir(logDir());
    QStringList logs = dir.entryList({"logitune-*.log"}, QDir::Files, QDir::Time);
    if (!logs.isEmpty())
        m_previousLogPath = dir.filePath(logs.first());

    // Install our message handler
    qInstallMessageHandler(LogManager::messageHandler);

    if (debugMode) {
        setLoggingEnabled(true);
    }
}

void LogManager::setLoggingEnabled(bool enabled)
{
    m_enabled = enabled;

    if (enabled) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "logitune.app.debug=true\n"
            "logitune.app.info=true\n"
            "logitune.profile.debug=true\n"
            "logitune.profile.info=true\n"
            "logitune.device.debug=true\n"
            "logitune.device.info=true\n"
            "logitune.hidpp.debug=true\n"
            "logitune.hidpp.info=true\n"
            "logitune.input.debug=true\n"
            "logitune.input.info=true\n"
            "logitune.focus.debug=true\n"
            "logitune.focus.info=true\n"
            "logitune.ui.debug=true\n"
            "logitune.ui.info=true\n"
        ));

        // Open log file
        if (!m_logFile.isOpen()) {
            m_logFile.setFileName(logDir() + "/" + logFileName());
            m_logFile.open(QIODevice::Append | QIODevice::Text);
        }
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "logitune.*.debug=false\n"
            "logitune.*.info=false\n"
        ));

        if (m_logFile.isOpen())
            m_logFile.close();
    }
}

bool LogManager::isLoggingEnabled() const
{
    return m_enabled;
}

QString LogManager::currentLogPath() const
{
    return m_logFile.isOpen() ? m_logFile.fileName() : QString();
}

QString LogManager::previousLogPath() const
{
    return m_previousLogPath;
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    // Level string
    const char *level = "DEBUG";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO";  break;
    case QtWarningMsg:  level = "WARN";  break;
    case QtCriticalMsg: level = "CRIT";  break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    // Category (from context)
    QString category = ctx.category ? QString::fromUtf8(ctx.category) : QStringLiteral("default");

    // Timestamp with millisecond precision
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));

    // Format: [timestamp] [category] [level] message
    QString formatted = QStringLiteral("[%1] [%2] [%3] %4")
        .arg(timestamp, category.leftJustified(18), QString::fromUtf8(level).leftJustified(5), msg);

    static const bool stderrIsTerminal = isatty(STDERR_FILENO);
    if (echoesToStderr(type, stderrIsTerminal))
        fprintf(stderr, "%s\n", qPrintable(formatted));

    // Write to file if enabled (or always for warnings+)
    auto &mgr = instance();
    if (mgr.m_enabled || type >= QtWarningMsg) {
        mgr.writeToFile(formatted);
    }
}

bool LogManager::echoesToStderr(QtMsgType type, bool stderrIsTerminal)
{
    // QtInfoMsg sorts after QtFatalMsg in the enum, so compare by name.
    if (type == QtDebugMsg || type == QtInfoMsg)
        return stderrIsTerminal;
    return true;
}

void LogManager::writeToFile(const QString &formatted)
{
    QMutexLocker lock(&m_mutex);
    if (!m_logFile.isOpen()) {
        // Open lazily for warnings/criticals even when logging is "disabled"
        QDir().mkpath(logDir());
        m_logFile.setFileName(logDir() + "/" + logFileName());
        m_logFile.open(QIODevice::Append | QIODevice::Text);
    }
    if (m_logFile.isOpen()) {
        m_logFile.write(formatted.toUtf8());
        m_logFile.write("\n");
        m_logFile.flush();
    }
}

QStringList LogManager::tailLog(int lines, const QString &path) const
{
    QString filePath = path.isEmpty() ? m_logFile.fileName() : path;
    if (filePath.isEmpty()) return {};

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    QStringList allLines;
    QTextStream in(&f);
    while (!in.atEnd())
        allLines.append(in.readLine());

    return allLines.mid(qMax(0, allLines.size() - lines));
}

void LogManager::rotateLogFiles()
{
    QDir dir(logDir());
    QStringList logs = dir.entryList({"logitune-*.log"}, QDir::Files, QDir::Time);
    // Keep last 5, delete older
    for (int i = 5; i < logs.size(); ++i)
        dir.remove(logs[i]);
}

void LogManager::shutdown()
{
    QMutexLocker lock(&m_mutex);
    if (m_logFile.isOpen())
        m_logFile.close();
}

QString LogManager::logDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
}

QString LogManager::logFileName() const
{
    return "logitune-" + QDate::currentDate().toString("yyyy-MM-dd") + ".log";
}

} // namespace logitune
