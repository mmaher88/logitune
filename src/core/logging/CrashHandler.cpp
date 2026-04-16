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
    if (!QFile::exists(lockFilePath()))
        return false;

    // Catchable crashes (SIGSEGV, SIGABRT, exceptions) already show
    // the crash dialog at the moment they happen via the signal handler.
    // Uncatchable exits (SIGKILL, OOM, power loss, reboot) leave the
    // lock behind but the user already knows — no value in showing a
    // dialog on next launch. Silently clean up.
    QFile::remove(lockFilePath());
    return false;
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
