# Logging & Bug Report System Design

## Goal

Production-grade structured logging with per-module categories, file output, crash handling, and one-click bug reporting to GitHub.

## Logging Infrastructure

### Qt Logging Categories

| Category | Module | Files |
|----------|--------|-------|
| `logitune.app` | AppController, main | AppController.cpp, main.cpp |
| `logitune.profile` | ProfileEngine, ProfileModel | ProfileEngine.cpp, ProfileModel.cpp |
| `logitune.device` | DeviceManager, DeviceModel | DeviceManager.cpp, DeviceModel.cpp |
| `logitune.hidpp` | Transport, FeatureDispatcher, HidrawDevice | Transport.cpp, FeatureDispatcher.cpp, HidrawDevice.cpp |
| `logitune.input` | UinputInjector, ActionExecutor | UinputInjector.cpp, ActionExecutor.cpp |
| `logitune.focus` | KDeDesktop, GenericDesktop | KDeDesktop.cpp, GenericDesktop.cpp |
| `logitune.ui` | QML, ButtonModel, ActionModel | ButtonModel.cpp, ActionModel.cpp |

### Log Format

```
[2026-03-29 01:23:45.678] [logitune.device]  [DEBUG] Applied profile "Google Chrome": DPI=2000 ThumbWheel=zoom
```

- Millisecond precision via `QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")`
- Category name padded for alignment
- Level: DEBUG, INFO, WARNING, CRITICAL, FATAL

### Thread Safety

Custom `qInstallMessageHandler` writes through a `QMutex`-protected file handle. Qt's logging macros (`qCDebug`, `qCInfo`, etc.) are already thread-safe at message generation — the mutex protects only the file write.

### Default State

- All categories disabled except WARNING and CRITICAL
- Enabled via:
  - Settings toggle → writes to `~/.config/Logitune/Logitune.conf` via QSettings, takes effect immediately
  - CLI `--debug` flag → enables all categories for that session
  - Env var `QT_LOGGING_RULES` for power users

### Log File

- Path: `~/.local/share/Logitune/logs/logitune-YYYY-MM-DD.log`
- Rotation: keep last 5 files, delete older on startup
- Created on first log write (not on app start if logging is disabled)
- Warnings and criticals always written to file regardless of toggle state

## Crash Handling

### Two Crash Paths

**Caught crash** — Signal handler or `std::set_terminate`:
1. `backtrace()` captures stack trace (up to 20 frames)
2. `abi::__cxa_demangle` produces readable function names
3. Stack trace + last 50 log lines written to log file
4. `CrashReportDialog` shown (QDialog, not QML)
5. User can report or dismiss

**Uncaught crash** — app killed, power loss, driver freeze:
1. On startup, check for `~/.local/share/Logitune/running.lock`
2. Lock exists → previous session crashed
3. Show recovery dialog with previous session's log file
4. User can report or dismiss
5. Lock file created on start, deleted on clean `QCoreApplication::aboutToQuit`

### Signal Handler

Catches: `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGBUS`

Uses async-signal-safe functions only for the critical path (write to pre-opened fd). The dialog is shown after the signal handler returns via a flag that the event loop checks — or for fatal signals, via `_exit()` after forking a child process that shows the dialog.

### std::set_terminate

Catches uncaught exceptions before stack unwind. Calls `backtrace()` (stack still intact), logs exception type via `abi::__cxa_demangle(abi::__cxa_current_exception_type()->name())` and `std::current_exception`, then shows crash dialog.

### try/catch in main()

Safety net for exceptions that escape the event loop. Stack is already unwound — no backtrace available. Logs exception type + `what()` message only. Shows crash dialog.

### Log Output on Crash

```
[2026-03-29 01:23:50.678] [logitune.app]     [FATAL] Signal 11 (SIGSEGV) received
[2026-03-29 01:23:50.678] [logitune.app]     [FATAL] Stack trace (17 frames):
  #0  0x00007f3a2b1c4d90 logitune::DeviceManager::handleNotification(hidpp::Report const&) at DeviceManager.cpp:742
  #1  0x00007f3a2b1c3a40 logitune::Transport::onReadyRead() at Transport.cpp:89
  #2  0x00007f3a2e8b1234 QSocketNotifier::activated(QSocketDescriptor, QSocketNotifier::Type)
  ...
```

## Crash Report Dialog

A `QDialog` (not QML — rendering engine may be dead) styled to match the app's dark theme:

- Background: #1a1a1a, accent: teal (#00EAD0), text: white
- Title: "Logitune crashed unexpectedly" (crash) or "Logitune didn't shut down cleanly" (recovery)
- Red badge showing crash type (e.g., "SIGSEGV" or "std::bad_alloc")
- Collapsible log viewer showing last 50 lines (read-only QTextEdit)
- Text field: "What were you doing?" for user description
- Two buttons: "Report Bug" (opens browser) and "Close"

Same `CrashReportDialog` class for all three entry points:
1. Signal/exception crash → shows with crash info
2. Startup recovery (lock file) → shows with previous log
3. Manual "Report Bug" from Settings → shows without crash info

## Settings UI Integration

In the existing Settings page (SettingsPage.qml):

- **"Debug logging" toggle** — LogituneToggle, off by default. Persists to QSettings. Takes effect immediately via `QLoggingCategory::setFilterRules`.
- **"Report Bug" button** — below the toggle, disabled when logging is off. Opens CrashReportDialog in manual mode.
- **Log file path** — small secondary text showing current log path when enabled.

## GitHub Issue URL Builder

Constructs a pre-filled issue URL:

```
https://github.com/mmaher88/logitune/issues/new?title=...&labels=bug&body=...
```

### Body Template

```markdown
## Bug Report

**Description:**
<user's text from dialog>

**App Version:** 0.1.0
**Device:** MX Master 3S (serial: a8f2***c301)
**OS:** CachyOS Linux 6.19.9
**Kernel:** 6.19.9-1-cachyos
**Qt:** 6.9.0

**Crash Info:**
Signal 11 (SIGSEGV)
Stack trace:
  #0  DeviceManager::handleNotification ...
  ...

**Log (last 50 lines):**
```
[01:23:44.252] [profile] Loaded 3 profiles
[01:23:45.100] [focus]   Focus: google-chrome -> "Google Chrome"
...
```
```

### URL Length Handling

URL-encoded via `QUrl::toPercentEncoding`. If total exceeds 8000 chars, truncate log lines until it fits. Append: "Full log at: `~/.local/share/Logitune/logs/logitune-YYYY-MM-DD.log`"

### Privacy

Device serial hashed: first 4 chars + `***` + last 4 chars. Home directory paths replaced with `~`.

## File Structure

```
src/core/logging/
    LogManager.h/.cpp           — singleton: categories, file handler, rotation, mutex
    CrashHandler.h/.cpp         — signal handler, set_terminate, backtrace, lock file

src/app/dialogs/
    CrashReportDialog.h/.cpp    — QDialog: dark-themed crash/bug report UI
    GitHubIssueBuilder.h/.cpp   — URL construction, body formatting, privacy sanitization

src/app/main.cpp                — modify: install LogManager + CrashHandler, try/catch
src/app/qml/pages/SettingsPage.qml — modify: add toggle + report bug button
```

### Build Flag

Add `-rdynamic` to linker flags for readable `backtrace_symbols()` output:
```cmake
target_link_options(logitune PRIVATE -rdynamic)
```

## What Gets Logged (by category)

### logitune.app (INFO)
- App started/stopped, PID, version
- Device setup complete
- Profile directory path

### logitune.profile (INFO + DEBUG)
- Profile loaded/saved/created/removed
- Display/hardware profile switched
- App bindings changed
- DEBUG: cached profile DPI/settings values on change

### logitune.device (INFO + DEBUG)
- Device connected/disconnected, name, serial (hashed)
- Profile applied to hardware (DPI, SmartShift, thumbwheel mode)
- Button divert/undivert with CID
- DEBUG: battery level changes

### logitune.hidpp (DEBUG)
- Feature table enumeration
- Request/response pairs
- Notification routing (feature index, function ID)
- Transport open/close

### logitune.input (DEBUG)
- Keystroke dispatch: CID → action name → key codes
- Ctrl+scroll injection
- App launch commands
- Gesture resolution: direction + keystroke

### logitune.focus (DEBUG)
- Window focus change: wmClass → profile name
- Desktop component filtered
- KWin script installed/callback received

### logitune.ui (DEBUG)
- Tab switched
- Button model loaded from profile
- Display values pushed to QML
