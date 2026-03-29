# Logging & Bug Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Production-grade structured logging with per-module categories, crash handling with stack traces, and one-click GitHub bug reporting.

**Architecture:** LogManager singleton handles categories + file output + rotation. CrashHandler captures signals + exceptions with backtrace. CrashReportDialog (QDialog) shows crash info and opens pre-filled GitHub issue URL. Settings page toggle enables/disables logging and report bug button.

**Tech Stack:** Qt logging categories, execinfo.h backtrace, QMutex, QDialog, QDesktopServices

---

### Task 1: LogManager — categories, file handler, rotation

**Files:**
- Create: `src/core/logging/LogManager.h`
- Create: `src/core/logging/LogManager.cpp`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: Create LogManager.h**

```cpp
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

private:
    LogManager() = default;
    ~LogManager();
    LogManager(const LogManager &) = delete;
    LogManager &operator=(const LogManager &) = delete;

    static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
    void writeToFile(const QString &formatted);

    QString logDir() const;
    QString logFileName() const;

    QMutex m_mutex;
    QFile m_logFile;
    bool m_enabled = false;
    bool m_initialized = false;
    QString m_previousLogPath;
};

} // namespace logitune
```

- [ ] **Step 2: Create LogManager.cpp**

```cpp
#include "logging/LogManager.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>

// Define logging categories — disabled by default except warnings
Q_LOGGING_CATEGORY(lcApp,     "logitune.app",     QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfile, "logitune.profile", QtWarningMsg)
Q_LOGGING_CATEGORY(lcDevice,  "logitune.device",  QtWarningMsg)
Q_LOGGING_CATEGORY(lcHidpp,   "logitune.hidpp",   QtWarningMsg)
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
            "logitune.profile.debug=true\n"
            "logitune.device.debug=true\n"
            "logitune.hidpp.debug=true\n"
            "logitune.input.debug=true\n"
            "logitune.focus.debug=true\n"
            "logitune.ui.debug=true\n"
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
    QString formatted = QStringLiteral("[%1] [%-18s] [%-5s] %4")
        .arg(timestamp, category, QString::fromUtf8(level), msg);

    // Always write to stderr
    fprintf(stderr, "%s\n", qPrintable(formatted));

    // Write to file if enabled (or always for warnings+)
    auto &mgr = instance();
    if (mgr.m_enabled || type >= QtWarningMsg) {
        mgr.writeToFile(formatted);
    }
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
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add to `src/core/CMakeLists.txt` target_sources:

```cmake
    logging/LogManager.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly. All 335 tests still pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/logging/ src/core/CMakeLists.txt
git commit -m "feat: LogManager — Qt logging categories, thread-safe file output, rotation"
```

---

### Task 2: CrashHandler — signals, backtrace, lock file

**Files:**
- Create: `src/core/logging/CrashHandler.h`
- Create: `src/core/logging/CrashHandler.cpp`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: Create CrashHandler.h**

```cpp
#pragma once
#include <QString>
#include <QStringList>
#include <functional>

namespace logitune {

/// Crash info captured by signal handler or set_terminate
struct CrashInfo {
    QString type;           // "SIGSEGV", "std::bad_alloc", etc.
    int signalNumber = 0;   // 0 if exception, not signal
    QStringList stackTrace; // demangled frames
    QStringList logTail;    // last N log lines before crash
    bool fromPreviousSession = false; // lock file recovery
};

class CrashHandler {
public:
    static CrashHandler &instance();

    /// Install signal handlers and set_terminate. Call once in main().
    void install();

    /// Create the lock file. Call after app starts.
    void createLockFile();

    /// Remove the lock file. Call on clean shutdown.
    void removeLockFile();

    /// Check if a previous session crashed (lock file exists).
    bool previousSessionCrashed() const;

    /// Get crash info for the previous session (from lock file + log).
    CrashInfo previousSessionCrashInfo() const;

    /// Set callback to show crash dialog. Called from signal handler context
    /// (for caught crashes) or from main() (for recovery).
    using CrashCallback = std::function<void(const CrashInfo &)>;
    void setCrashCallback(CrashCallback cb);

    /// Capture a backtrace right now (for use in set_terminate).
    static QStringList captureBacktrace(int maxFrames = 20);

private:
    CrashHandler() = default;
    static void signalHandler(int sig);
    static void terminateHandler();

    CrashCallback m_callback;
    QString lockFilePath() const;
};

} // namespace logitune
```

- [ ] **Step 2: Create CrashHandler.cpp**

```cpp
#include "logging/CrashHandler.h"
#include "logging/LogManager.h"
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <execinfo.h>
#include <cxxabi.h>

namespace logitune {

CrashHandler &CrashHandler::instance()
{
    static CrashHandler inst;
    return inst;
}

void CrashHandler::install()
{
    // Signal handlers for hard crashes
    signal(SIGSEGV, CrashHandler::signalHandler);
    signal(SIGABRT, CrashHandler::signalHandler);
    signal(SIGFPE,  CrashHandler::signalHandler);
    signal(SIGBUS,  CrashHandler::signalHandler);

    // Terminate handler for uncaught exceptions (stack still intact)
    std::set_terminate(CrashHandler::terminateHandler);
}

void CrashHandler::createLockFile()
{
    QDir().mkpath(QFileInfo(lockFilePath()).path());
    QFile f(lockFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray::number(QCoreApplication::applicationPid()));
        f.close();
    }
}

void CrashHandler::removeLockFile()
{
    QFile::remove(lockFilePath());
}

bool CrashHandler::previousSessionCrashed() const
{
    return QFile::exists(lockFilePath());
}

CrashInfo CrashHandler::previousSessionCrashInfo() const
{
    CrashInfo info;
    info.type = QStringLiteral("Previous session did not shut down cleanly");
    info.fromPreviousSession = true;

    // Get log tail from previous session
    QString prevLog = LogManager::instance().previousLogPath();
    if (!prevLog.isEmpty())
        info.logTail = LogManager::instance().tailLog(50, prevLog);

    return info;
}

void CrashHandler::setCrashCallback(CrashCallback cb)
{
    m_callback = std::move(cb);
}

QStringList CrashHandler::captureBacktrace(int maxFrames)
{
    QStringList result;
    void *buffer[64];
    int nFrames = backtrace(buffer, qMin(maxFrames, 64));
    char **symbols = backtrace_symbols(buffer, nFrames);
    if (!symbols) return result;

    for (int i = 0; i < nFrames; ++i) {
        QString frame = QString::fromUtf8(symbols[i]);

        // Try to demangle: extract mangled name between '(' and '+'
        QString raw = frame;
        int start = raw.indexOf('(');
        int end = raw.indexOf('+', start);
        if (start > 0 && end > start) {
            QString mangled = raw.mid(start + 1, end - start - 1);
            int status = 0;
            char *demangled = abi::__cxa_demangle(mangled.toUtf8().constData(), nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                frame = QStringLiteral("  #%1  %2").arg(i).arg(QString::fromUtf8(demangled));
                free(demangled);
            } else {
                frame = QStringLiteral("  #%1  %2").arg(i).arg(raw);
            }
        } else {
            frame = QStringLiteral("  #%1  %2").arg(i).arg(raw);
        }
        result.append(frame);
    }

    free(symbols);
    return result;
}

void CrashHandler::signalHandler(int sig)
{
    // Map signal to name
    const char *name = "UNKNOWN";
    switch (sig) {
    case SIGSEGV: name = "SIGSEGV"; break;
    case SIGABRT: name = "SIGABRT"; break;
    case SIGFPE:  name = "SIGFPE";  break;
    case SIGBUS:  name = "SIGBUS";  break;
    }

    // Write crash info to log (use fprintf — async-signal-safe-ish for stderr)
    fprintf(stderr, "[FATAL] Signal %d (%s) received\n", sig, name);

    CrashInfo info;
    info.type = QString::fromUtf8(name);
    info.signalNumber = sig;
    info.stackTrace = captureBacktrace();
    info.logTail = LogManager::instance().tailLog(50);

    // Log the stack trace
    auto &log = LogManager::instance();
    log.writeToFile(QStringLiteral("[FATAL] Signal %1 (%2) received").arg(sig).arg(info.type));
    log.writeToFile(QStringLiteral("[FATAL] Stack trace (%1 frames):").arg(info.stackTrace.size()));
    for (const auto &frame : info.stackTrace)
        log.writeToFile(frame);

    // Reset signal handler to default and re-raise after callback
    signal(sig, SIG_DFL);

    auto &handler = instance();
    if (handler.m_callback) {
        handler.m_callback(info);
    }

    raise(sig); // re-raise to get the default behavior (core dump, etc.)
}

void CrashHandler::terminateHandler()
{
    fprintf(stderr, "[FATAL] std::terminate called (uncaught exception)\n");

    CrashInfo info;
    info.stackTrace = captureBacktrace();
    info.logTail = LogManager::instance().tailLog(50);

    // Try to get exception info
    if (auto eptr = std::current_exception()) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception &e) {
            int status = 0;
            char *demangled = abi::__cxa_demangle(typeid(e).name(), nullptr, nullptr, &status);
            info.type = (status == 0 && demangled)
                ? QString::fromUtf8(demangled) + ": " + QString::fromUtf8(e.what())
                : QString::fromUtf8(e.what());
            free(demangled);
        } catch (...) {
            info.type = QStringLiteral("Unknown exception");
        }
    } else {
        info.type = QStringLiteral("std::terminate (no exception)");
    }

    auto &log = LogManager::instance();
    log.writeToFile(QStringLiteral("[FATAL] %1").arg(info.type));
    log.writeToFile(QStringLiteral("[FATAL] Stack trace (%1 frames):").arg(info.stackTrace.size()));
    for (const auto &frame : info.stackTrace)
        log.writeToFile(frame);

    auto &handler = instance();
    if (handler.m_callback) {
        handler.m_callback(info);
    }

    std::abort();
}

QString CrashHandler::lockFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/running.lock";
}

} // namespace logitune
```

- [ ] **Step 3: Add to CMakeLists.txt and add -rdynamic**

Add to `src/core/CMakeLists.txt` target_sources:
```cmake
    logging/CrashHandler.cpp
```

Add to `src/app/CMakeLists.txt` after `target_link_libraries(logitune ...)`:
```cmake
target_link_options(logitune PRIVATE -rdynamic)
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly. All 335 tests still pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/logging/CrashHandler.h src/core/logging/CrashHandler.cpp src/core/CMakeLists.txt src/app/CMakeLists.txt
git commit -m "feat: CrashHandler — signal handler, set_terminate, backtrace, lock file"
```

---

### Task 3: GitHubIssueBuilder — URL construction, privacy

**Files:**
- Create: `src/app/dialogs/GitHubIssueBuilder.h`
- Create: `src/app/dialogs/GitHubIssueBuilder.cpp`
- Modify: `src/app/CMakeLists.txt` (add to logitune-app-lib)

- [ ] **Step 1: Create GitHubIssueBuilder.h**

```cpp
#pragma once
#include <QString>
#include <QStringList>
#include <QUrl>

namespace logitune {

struct CrashInfo; // forward declare

class GitHubIssueBuilder {
public:
    static constexpr const char *kRepoUrl = "https://github.com/mmaher88/logitune";

    void setDescription(const QString &desc);
    void setDeviceName(const QString &name);
    void setDeviceSerial(const QString &serial);
    void setCrashInfo(const QString &type, const QStringList &stackTrace);
    void setLogTail(const QStringList &lines);

    /// Build the full URL. Truncates log if URL exceeds ~8000 chars.
    QUrl buildUrl() const;

    /// Hash a device serial for privacy: first4***last4
    static QString hashSerial(const QString &serial);

    /// Sanitize paths: replace home dir with ~
    static QString sanitizePath(const QString &path);

private:
    QString m_description;
    QString m_deviceName;
    QString m_serial;
    QString m_crashType;
    QStringList m_stackTrace;
    QStringList m_logTail;
};

} // namespace logitune
```

- [ ] **Step 2: Create GitHubIssueBuilder.cpp**

```cpp
#include "dialogs/GitHubIssueBuilder.h"
#include <QSysInfo>
#include <QDir>
#include <QCoreApplication>
#include <QUrlQuery>

namespace logitune {

void GitHubIssueBuilder::setDescription(const QString &desc) { m_description = desc; }
void GitHubIssueBuilder::setDeviceName(const QString &name) { m_deviceName = name; }
void GitHubIssueBuilder::setDeviceSerial(const QString &serial) { m_serial = serial; }

void GitHubIssueBuilder::setCrashInfo(const QString &type, const QStringList &stackTrace)
{
    m_crashType = type;
    m_stackTrace = stackTrace;
}

void GitHubIssueBuilder::setLogTail(const QStringList &lines)
{
    m_logTail = lines;
}

QString GitHubIssueBuilder::hashSerial(const QString &serial)
{
    if (serial.length() <= 8) return serial;
    return serial.left(4) + "***" + serial.right(4);
}

QString GitHubIssueBuilder::sanitizePath(const QString &path)
{
    QString home = QDir::homePath();
    QString result = path;
    result.replace(home, "~");
    return result;
}

QUrl GitHubIssueBuilder::buildUrl() const
{
    // Title
    QString title = m_crashType.isEmpty()
        ? QStringLiteral("Bug: ") + m_description.left(60)
        : QStringLiteral("Crash: ") + m_crashType.left(60);

    // Body
    QString body;
    body += QStringLiteral("## Bug Report\n\n");
    body += QStringLiteral("**Description:**\n%1\n\n").arg(
        m_description.isEmpty() ? QStringLiteral("(no description)") : m_description);

    body += QStringLiteral("**App Version:** %1\n").arg(
        QCoreApplication::applicationVersion().isEmpty()
            ? QStringLiteral("dev") : QCoreApplication::applicationVersion());
    if (!m_deviceName.isEmpty())
        body += QStringLiteral("**Device:** %1 (serial: %2)\n").arg(m_deviceName, hashSerial(m_serial));
    body += QStringLiteral("**OS:** %1 %2\n").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    body += QStringLiteral("**Kernel:** %1\n").arg(QSysInfo::kernelVersion());
    body += QStringLiteral("**Qt:** %1\n\n").arg(QString::fromUtf8(qVersion()));

    // Crash info
    if (!m_crashType.isEmpty()) {
        body += QStringLiteral("**Crash Info:**\n```\n%1\n").arg(m_crashType);
        for (const auto &frame : m_stackTrace)
            body += sanitizePath(frame) + "\n";
        body += QStringLiteral("```\n\n");
    }

    // Log tail
    if (!m_logTail.isEmpty()) {
        body += QStringLiteral("**Log (last %1 lines):**\n```\n").arg(m_logTail.size());
        for (const auto &line : m_logTail)
            body += sanitizePath(line) + "\n";
        body += QStringLiteral("```\n");
    }

    // Truncate if URL would be too long
    constexpr int kMaxUrlLength = 7500;
    while (body.toUtf8().size() > kMaxUrlLength && !m_logTail.isEmpty()) {
        // Remove log lines from the front until it fits
        body.clear();
        // Rebuild without log (keep crash info)
        // Simpler: just note the file path
        body += QStringLiteral("## Bug Report\n\n");
        body += QStringLiteral("**Description:**\n%1\n\n").arg(m_description);
        body += QStringLiteral("**App Version:** dev\n");
        if (!m_deviceName.isEmpty())
            body += QStringLiteral("**Device:** %1\n").arg(m_deviceName);
        body += QStringLiteral("**OS:** %1\n**Kernel:** %2\n**Qt:** %3\n\n")
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(), QString::fromUtf8(qVersion()));
        if (!m_crashType.isEmpty()) {
            body += QStringLiteral("**Crash Info:**\n```\n%1\n```\n\n").arg(m_crashType);
        }
        body += QStringLiteral("**Logs too large for URL.** Full log at: `~/.local/share/Logitune/logs/`\n");
        break;
    }

    QUrl url(QStringLiteral("%1/issues/new").arg(QLatin1String(kRepoUrl)));
    QUrlQuery query;
    query.addQueryItem("title", title);
    query.addQueryItem("labels", "bug");
    query.addQueryItem("body", body);
    url.setQuery(query);

    return url;
}

} // namespace logitune
```

- [ ] **Step 3: Add to logitune-app-lib in CMakeLists.txt**

Add `dialogs/GitHubIssueBuilder.cpp` to the `logitune-app-lib` STATIC library sources in `src/app/CMakeLists.txt`.

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/app/dialogs/ src/app/CMakeLists.txt
git commit -m "feat: GitHubIssueBuilder — pre-filled issue URL with privacy sanitization"
```

---

### Task 4: CrashReportDialog — dark-themed QDialog

**Files:**
- Create: `src/app/dialogs/CrashReportDialog.h`
- Create: `src/app/dialogs/CrashReportDialog.cpp`
- Modify: `src/app/CMakeLists.txt`

- [ ] **Step 1: Create CrashReportDialog.h**

```cpp
#pragma once
#include "logging/CrashHandler.h"
#include <QDialog>

class QTextEdit;
class QLineEdit;

namespace logitune {

class CrashReportDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { ManualReport, CaughtCrash, RecoveryReport };

    explicit CrashReportDialog(Mode mode, const CrashInfo &info = {},
                                QWidget *parent = nullptr);

    /// Set device info for the GitHub issue body
    void setDeviceInfo(const QString &name, const QString &serial);

private slots:
    void onReportClicked();

private:
    void setupUi(Mode mode, const CrashInfo &info);
    void applyDarkStyle();

    QTextEdit *m_logView = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QString m_deviceName;
    QString m_deviceSerial;
    CrashInfo m_crashInfo;
};

} // namespace logitune
```

- [ ] **Step 2: Create CrashReportDialog.cpp**

```cpp
#include "dialogs/CrashReportDialog.h"
#include "dialogs/GitHubIssueBuilder.h"
#include "logging/LogManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QDesktopServices>

namespace logitune {

CrashReportDialog::CrashReportDialog(Mode mode, const CrashInfo &info, QWidget *parent)
    : QDialog(parent), m_crashInfo(info)
{
    applyDarkStyle();
    setupUi(mode, info);
}

void CrashReportDialog::setDeviceInfo(const QString &name, const QString &serial)
{
    m_deviceName = name;
    m_deviceSerial = serial;
}

void CrashReportDialog::applyDarkStyle()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1a1a1a; }"
        "QLabel { color: #ffffff; }"
        "QLineEdit { background-color: #2a2a2a; color: #ffffff; border: 1px solid #444; "
        "  border-radius: 6px; padding: 8px; font-size: 13px; }"
        "QTextEdit { background-color: #111111; color: #cccccc; border: 1px solid #333; "
        "  border-radius: 6px; padding: 8px; font-family: monospace; font-size: 11px; }"
        "QPushButton { background-color: #00EAD0; color: #000000; border: none; "
        "  border-radius: 6px; padding: 10px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #33FFDD; }"
        "QPushButton#closeBtn { background-color: transparent; color: #888888; "
        "  border: 1px solid #444; }"
        "QPushButton#closeBtn:hover { color: #ffffff; border-color: #666; }"
    ));
}

void CrashReportDialog::setupUi(Mode mode, const CrashInfo &info)
{
    setWindowTitle(QStringLiteral("Logitune — Report Bug"));
    setMinimumSize(520, 400);
    resize(580, 500);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 24, 24, 24);

    // Title
    auto *title = new QLabel(this);
    switch (mode) {
    case CaughtCrash:
        title->setText(QStringLiteral("Logitune crashed unexpectedly"));
        break;
    case RecoveryReport:
        title->setText(QStringLiteral("Logitune didn't shut down cleanly last time"));
        break;
    case ManualReport:
        title->setText(QStringLiteral("Report a bug"));
        break;
    }
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    layout->addWidget(title);

    // Crash type badge (if applicable)
    if (!info.type.isEmpty()) {
        auto *badge = new QLabel(info.type, this);
        badge->setStyleSheet(QStringLiteral(
            "background-color: #cc3333; color: white; padding: 4px 12px; "
            "border-radius: 4px; font-size: 12px; font-family: monospace;"));
        badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        layout->addWidget(badge);
    }

    // Description field
    auto *descLabel = new QLabel(QStringLiteral("What were you doing?"), this);
    descLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #aaaaaa;"));
    layout->addWidget(descLabel);

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(QStringLiteral("Describe what happened..."));
    layout->addWidget(m_descriptionEdit);

    // Log viewer
    auto *logLabel = new QLabel(QStringLiteral("Recent log:"), this);
    logLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #888888;"));
    layout->addWidget(logLabel);

    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);

    QStringList logLines = info.logTail;
    if (logLines.isEmpty())
        logLines = LogManager::instance().tailLog(50);

    // Add stack trace if present
    QString logText;
    if (!info.stackTrace.isEmpty()) {
        logText += QStringLiteral("=== Stack Trace ===\n");
        for (const auto &frame : info.stackTrace)
            logText += frame + "\n";
        logText += QStringLiteral("\n=== Recent Log ===\n");
    }
    for (const auto &line : logLines)
        logText += line + "\n";

    m_logView->setPlainText(logText);
    layout->addWidget(m_logView, 1); // stretch

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(closeBtn);

    auto *reportBtn = new QPushButton(QStringLiteral("Report Bug"), this);
    connect(reportBtn, &QPushButton::clicked, this, &CrashReportDialog::onReportClicked);
    btnLayout->addWidget(reportBtn);

    layout->addLayout(btnLayout);
}

void CrashReportDialog::onReportClicked()
{
    GitHubIssueBuilder builder;
    builder.setDescription(m_descriptionEdit->text());
    builder.setDeviceName(m_deviceName);
    builder.setDeviceSerial(m_deviceSerial);

    if (!m_crashInfo.type.isEmpty())
        builder.setCrashInfo(m_crashInfo.type, m_crashInfo.stackTrace);

    QStringList logLines = m_crashInfo.logTail;
    if (logLines.isEmpty())
        logLines = LogManager::instance().tailLog(50);
    builder.setLogTail(logLines);

    QDesktopServices::openUrl(builder.buildUrl());
    accept();
}

} // namespace logitune
```

- [ ] **Step 3: Add to logitune-app-lib**

Add `dialogs/CrashReportDialog.cpp` to `logitune-app-lib` in `src/app/CMakeLists.txt`. Also ensure `Qt6::Widgets` is linked (it should be already via the executable, but the library may need it for QDialog).

Add to `target_link_libraries(logitune-app-lib ...)`:
```cmake
target_link_libraries(logitune-app-lib PUBLIC logitune-core Qt6::Quick Qt6::Widgets)
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly. All 335 tests still pass.

- [ ] **Step 5: Commit**

```bash
git add src/app/dialogs/CrashReportDialog.h src/app/dialogs/CrashReportDialog.cpp src/app/CMakeLists.txt
git commit -m "feat: CrashReportDialog — dark-themed QDialog with log viewer and GitHub issue creation"
```

---

### Task 5: Settings UI — logging toggle + report bug button

**Files:**
- Modify: `src/app/qml/pages/SettingsPage.qml`
- Modify: `src/app/models/DeviceModel.h` (add logging properties)
- Modify: `src/app/models/DeviceModel.cpp`

- [ ] **Step 1: Add logging properties to DeviceModel**

DeviceModel is already a QML singleton. Add properties for logging state:

In `DeviceModel.h`, add:
```cpp
    Q_PROPERTY(bool loggingEnabled READ loggingEnabled WRITE setLoggingEnabled NOTIFY loggingEnabledChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath NOTIFY loggingEnabledChanged)

    bool loggingEnabled() const;
    void setLoggingEnabled(bool enabled);
    QString logFilePath() const;
    Q_INVOKABLE void openBugReport();

signals:
    void loggingEnabledChanged();
```

In `DeviceModel.cpp`, implement:
```cpp
#include "logging/LogManager.h"
#include "dialogs/CrashReportDialog.h"

bool DeviceModel::loggingEnabled() const { return LogManager::instance().isLoggingEnabled(); }

void DeviceModel::setLoggingEnabled(bool enabled) {
    if (enabled == loggingEnabled()) return;
    LogManager::instance().setLoggingEnabled(enabled);
    // Persist to QSettings
    QSettings s;
    s.setValue("logging/enabled", enabled);
    emit loggingEnabledChanged();
}

QString DeviceModel::logFilePath() const { return LogManager::instance().currentLogPath(); }

void DeviceModel::openBugReport() {
    CrashReportDialog dlg(CrashReportDialog::ManualReport);
    dlg.setDeviceInfo(deviceName(), deviceSerial());
    dlg.exec();
}
```

- [ ] **Step 2: Update SettingsPage.qml**

Add after the dark mode toggle section:

```qml
            // Separator
            Rectangle { width: parent.width; height: 1; color: Theme.border }

            // Debug logging toggle
            Row {
                width: parent.width
                Text {
                    text: "Debug logging"
                    font.pixelSize: 13
                    color: Theme.text
                    width: parent.width - loggingToggle.width
                    anchors.verticalCenter: parent.verticalCenter
                }
                LogituneToggle {
                    id: loggingToggle
                    checked: DeviceModel.loggingEnabled
                    onToggled: DeviceModel.loggingEnabled = checked
                }
            }

            // Log file path (visible when logging enabled)
            Text {
                visible: DeviceModel.loggingEnabled
                text: DeviceModel.logFilePath || ""
                font.pixelSize: 11
                color: Theme.textSecondary
                wrapMode: Text.WrapAnywhere
                width: parent.width
            }

            // Report Bug button (disabled when logging off)
            Rectangle {
                width: 180; height: 40
                radius: 4
                color: DeviceModel.loggingEnabled
                    ? (reportHover.hovered ? Theme.accent : "transparent")
                    : "transparent"
                border.color: DeviceModel.loggingEnabled ? Theme.accent : Theme.border
                border.width: 1
                opacity: DeviceModel.loggingEnabled ? 1.0 : 0.4

                Text {
                    anchors.centerIn: parent
                    text: "Report Bug"
                    font.pixelSize: 13
                    color: DeviceModel.loggingEnabled
                        ? (reportHover.hovered ? "#000000" : Theme.accent)
                        : Theme.textSecondary
                }

                HoverHandler { id: reportHover; enabled: DeviceModel.loggingEnabled }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: DeviceModel.loggingEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: DeviceModel.loggingEnabled
                    onClicked: DeviceModel.openBugReport()
                }
            }
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp src/app/qml/pages/SettingsPage.qml
git commit -m "feat: Settings UI — debug logging toggle and report bug button"
```

---

### Task 6: Wire into main.cpp — handlers, try/catch, --debug, recovery

**Files:**
- Modify: `src/app/main.cpp`

- [ ] **Step 1: Update main.cpp**

Replace the entire main.cpp with the following. Key changes:
- Install LogManager + CrashHandler before anything else
- Parse `--debug` CLI flag
- Wrap `app.exec()` in try/catch
- Check for previous crash on startup (lock file)
- Remove old `qInstallMessageHandler` and `fprintf` calls (LogManager handles all logging now)
- Create + delete lock file
- Connect `aboutToQuit` to clean shutdown

```cpp
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QQuickWindow>
#include <QQuickImageProvider>
#include <QIcon>
#include <QTimer>

#include "AppController.h"
#include "logging/LogManager.h"
#include "logging/CrashHandler.h"
#include "dialogs/CrashReportDialog.h"

int main(int argc, char *argv[])
{
    // Ignore SIGPIPE — hidraw writes to wrong interface cause EPIPE
    signal(SIGPIPE, SIG_IGN);

    QApplication app(argc, argv);
    app.setOrganizationName("Logitune");
    app.setApplicationName("Logitune");
    app.setApplicationVersion("0.1.0");

    // Parse --debug flag
    bool debugMode = app.arguments().contains("--debug");

    // Initialize logging (before anything else)
    auto &logMgr = logitune::LogManager::instance();
    logMgr.init(debugMode);

    // Restore logging state from settings (unless --debug overrides)
    if (!debugMode) {
        QSettings settings;
        if (settings.value("logging/enabled", false).toBool())
            logMgr.setLoggingEnabled(true);
    }

    qCInfo(lcApp) << "Application started, PID" << QCoreApplication::applicationPid();

    // Install crash handler
    auto &crashHandler = logitune::CrashHandler::instance();
    crashHandler.install();

    // Set crash callback — shows dialog on caught crash
    crashHandler.setCrashCallback([](const logitune::CrashInfo &info) {
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
    });

    // Check for previous crash (lock file from unclean shutdown)
    if (crashHandler.previousSessionCrashed()) {
        qCInfo(lcApp) << "Previous session crashed — showing recovery dialog";
        auto info = crashHandler.previousSessionCrashInfo();
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::RecoveryReport, info);
        dlg.exec();
    }

    // Create lock file (removed on clean exit)
    crashHandler.createLockFile();

    // Clean shutdown: remove lock file, flush logs
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qCInfo(lcApp) << "Application shutting down cleanly";
        crashHandler.removeLockFile();
        logMgr.shutdown();
    });

    // Detect dark mode
    QColor windowBg = app.palette().window().color();
    double lum = windowBg.redF() * 0.299 + windowBg.greenF() * 0.587 + windowBg.blueF() * 0.114;
    bool isDark = lum < 0.5;

    // AppController
    logitune::AppController controller;
    controller.init();

    // QML engine
    QQmlApplicationEngine engine;

    class IconProvider : public QQuickImageProvider {
    public:
        IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
        QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override {
            QIcon icon = QIcon::fromTheme(id);
            QSize s = requestedSize.isValid() ? requestedSize : QSize(22, 22);
            QPixmap pm = icon.pixmap(s);
            if (size) *size = pm.size();
            return pm;
        }
    };
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);

    qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",  controller.deviceModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",  controller.buttonModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionModel",  controller.actionModel());
    qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel", controller.profileModel());

    engine.load(QUrl(QStringLiteral("qrc:/Logitune/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcApp) << "QML failed to load — no root objects";
        return -1;
    }

    // Set theme
    if (auto *theme = engine.singletonInstance<QObject*>("Logitune", "Theme"))
        theme->setProperty("dark", isDark);

    controller.startMonitoring();

    // System tray
    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(QIcon::fromTheme("input-mouse"));
    trayIcon.setToolTip("Logitune - MX Master 3S");

    QMenu trayMenu;
    QAction *showAction = trayMenu.addAction("Show Logitune");
    trayMenu.addSeparator();
    QAction *batteryAction = trayMenu.addAction("Battery: ---%");
    batteryAction->setEnabled(false);
    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction("Quit");
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    auto showWindow = [&engine]() {
        for (auto *obj : engine.rootObjects()) {
            if (auto *window = qobject_cast<QQuickWindow*>(obj)) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    };

    QObject::connect(showAction, &QAction::triggered, showWindow);
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated,
        [showWindow](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) showWindow();
        });

    QObject::connect(controller.deviceModel(), &logitune::DeviceModel::batteryLevelChanged,
        [batteryAction, dm = controller.deviceModel()]() {
            batteryAction->setText(dm->batteryStatusText());
        });

    qCInfo(lcApp) << "Startup complete";

    // Run with exception safety net
    try {
        return app.exec();
    } catch (const std::exception &e) {
        qCCritical(lcApp) << "Unhandled exception in event loop:" << e.what();
        logitune::CrashInfo info;
        info.type = QString::fromUtf8(typeid(e).name()) + ": " + QString::fromUtf8(e.what());
        info.logTail = logMgr.tailLog(50);
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
        return -1;
    } catch (...) {
        qCCritical(lcApp) << "Unknown exception in event loop";
        logitune::CrashInfo info;
        info.type = QStringLiteral("Unknown exception");
        info.logTail = logMgr.tailLog(50);
        logitune::CrashReportDialog dlg(logitune::CrashReportDialog::CaughtCrash, info);
        dlg.exec();
        return -1;
    }
}
```

- [ ] **Step 2: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly. All 335 tests still pass.

Test manually:
- `./build/src/app/logitune` — starts normally, no crash dialog
- `./build/src/app/logitune --debug` — starts with logging enabled, log file created
- Kill the app (`kill -9`), restart — should show recovery dialog

- [ ] **Step 3: Commit**

```bash
git add src/app/main.cpp
git commit -m "feat: wire logging, crash handler, and recovery into main — --debug flag, try/catch, lock file"
```

---

### Task 7: Replace existing fprintf/qDebug with proper categories

**Files:**
- Modify: `src/app/AppController.cpp`
- Modify: `src/core/DeviceManager.cpp`
- Modify: `src/core/hidpp/Transport.cpp`
- Modify: `src/core/hidpp/HidrawDevice.cpp`
- Modify: `src/core/ProfileEngine.cpp`
- Modify: `src/core/desktop/KDeDesktop.cpp`

- [ ] **Step 1: Replace all fprintf/qDebug with categorized logging**

In each file, add the appropriate include:
```cpp
#include "logging/LogManager.h"
```

Replace patterns:
- `fprintf(stderr, "[logitune] ...")` → `qCInfo(lcApp) << ...`
- `qDebug() << "[AppController]..."` → `qCDebug(lcApp) << ...`
- `qDebug() << "[DeviceManager]..."` → `qCDebug(lcDevice) << ...`
- `qDebug() << "[ProfileEngine]..."` → `qCDebug(lcProfile) << ...`
- `fprintf(stderr, "[HidrawDevice]...")` → `qCWarning(lcHidpp) << ...`

Do NOT add new log statements — just replace existing ones with the correct category. The goal is to migrate, not to add coverage. Adding proper log coverage throughout the codebase is a separate task.

- [ ] **Step 2: Build and verify**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Builds cleanly. All 335 tests still pass.

- [ ] **Step 3: Commit**

```bash
git add src/app/AppController.cpp src/core/DeviceManager.cpp src/core/hidpp/Transport.cpp src/core/hidpp/HidrawDevice.cpp src/core/ProfileEngine.cpp src/core/desktop/KDeDesktop.cpp
git commit -m "refactor: replace fprintf/qDebug with Qt logging categories"
```

---

## Summary

7 tasks producing:
- **LogManager** — thread-safe file output, 7 categories, rotation, runtime toggle
- **CrashHandler** — SIGSEGV/SIGABRT handler, set_terminate, backtrace with demangling, lock file
- **GitHubIssueBuilder** — pre-filled issue URL, serial hashing, path sanitization, URL length handling
- **CrashReportDialog** — dark-themed QDialog matching app style, log viewer, description field
- **Settings UI** — toggle + report bug button, log file path display
- **main.cpp** — --debug flag, try/catch, lock file lifecycle, recovery dialog
- **Category migration** — all existing log statements use proper categories
