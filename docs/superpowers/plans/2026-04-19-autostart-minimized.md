# Autostart Minimized Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Logitune so every user autostarts it minimized to the tray on login via `/etc/xdg/autostart/logitune.desktop` with `Exec=logitune --minimized`. The app recognises the flag and hides the main window when a system tray is available.

**Architecture:** Two .desktop files under `data/` (launcher + autostart) so the app launcher does not inherit the minimized flag. CMake install line uses an absolute `/etc/xdg/autostart` destination to fix a pre-existing relative-path bug that landed the file at `/usr/etc/xdg/autostart/`. `main.cpp` adds a `--minimized` `QCommandLineOption` and hides each root `QQuickWindow` if `QSystemTrayIcon::isSystemTrayAvailable()` returns true.

**Tech Stack:** C++20 / Qt 6 / CMake / GTest. No new third-party dependencies.

**Design spec:** `docs/superpowers/specs/2026-04-19-autostart-minimized-design.md`. Read it before Task 1.

---

## Global rules

- **No em-dashes (U+2014 "—")** in any file you create or modify. The only acceptable occurrence is inside a `grep -c "—"` verification command.
- **No co-author signatures** in commit messages.
- **Branch is `feat-autostart-minimized`.** Already created with the spec committed. Do NOT push. Maintainer pushes after final verification.
- **Working directory:** `/home/mina/repos/logitune`.
- **Never amend commits.** Each task makes a new commit on top.
- **Build + test verification** after every task that touches C++:
  - `cmake --build build -j"$(nproc)" 2>&1 | tail -3` (must exit 0)
  - `XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -5` (must show `[  PASSED  ] <N> tests.`). Baseline count before this plan is 550.

---

## File Structure

### Created

```
data/logitune-autostart.desktop                  (new, copy of logitune.desktop with --minimized)
tests/test_autostart_desktop.cpp                 (new, sanity tests for the autostart desktop entry)
```

### Modified

- `CMakeLists.txt`: update the autostart install destination from the relative `etc/xdg/autostart` to absolute `/etc/xdg/autostart`, switch source file to `data/logitune-autostart.desktop`, add `RENAME logitune.desktop` so the installed filename remains `logitune.desktop`.
- `src/app/main.cpp`: add the `--minimized` `QCommandLineOption`, parse it, and after QML load iterate root objects and hide each `QQuickWindow` when the flag is set and a system tray is available.
- `tests/CMakeLists.txt`: add `test_autostart_desktop.cpp` to the `logitune-tests` source list.

### Unchanged

- `data/logitune.desktop` (no `--minimized`, still installed to `share/applications` for the launcher entry).
- `src/app/qml/Main.qml` (no QML changes; window is hidden at the C++ layer).
- `src/app/TrayManager.{cpp,h}` (existing show-window-from-tray flow already works).
- All packaging scripts (`scripts/package-deb.sh`, `scripts/package-rpm.sh`, `scripts/package-arch.sh`). They invoke `cmake --install`, which respects the new destinations.

---

## Task 1: Create the autostart desktop entry and fix its install path

**Files:**
- Create: `data/logitune-autostart.desktop`
- Modify: `CMakeLists.txt`

### Step 1: Create the new autostart entry

Create `data/logitune-autostart.desktop` with exactly this content:

```
[Desktop Entry]
Type=Application
Name=Logitune
Comment=Configure your Logitech devices
Exec=logitune --minimized
Icon=com.logitune.Logitune
Categories=Settings;HardwareSettings;
Keywords=logitech;mouse;keyboard;
X-GNOME-Autostart-enabled=true
```

Note the only difference from `data/logitune.desktop` is `Exec=logitune --minimized`. Everything else is identical byte-for-byte.

### Step 2: Update the CMake install line

Open `CMakeLists.txt`. Find lines 27-28:

```cmake
install(FILES data/logitune.desktop DESTINATION share/applications)
install(FILES data/logitune.desktop DESTINATION etc/xdg/autostart)
```

Replace the second line:

```cmake
install(FILES data/logitune.desktop DESTINATION share/applications)
install(FILES data/logitune-autostart.desktop
        DESTINATION /etc/xdg/autostart
        RENAME logitune.desktop)
```

Two edits on that second line:
- The source file changes from `data/logitune.desktop` to `data/logitune-autostart.desktop`.
- The destination becomes the absolute path `/etc/xdg/autostart` (previously `etc/xdg/autostart`, which combined with `CMAKE_INSTALL_PREFIX=/usr` produced `/usr/etc/xdg/autostart/`, the broken path no DE reads).
- `RENAME logitune.desktop` keeps the installed filename as `logitune.desktop` per XDG autostart convention.

### Step 3: Verify install staging produces the right layout

```bash
rm -rf /tmp/logitune-stage
cmake -B build-stage -DCMAKE_INSTALL_PREFIX=/tmp/logitune-stage-prefix -DBUILD_TESTING=OFF -Wno-dev 2>&1 | tail -3
DESTDIR=/tmp/logitune-stage cmake --install build-stage 2>&1 | tail -10
find /tmp/logitune-stage -name "logitune.desktop" 2>/dev/null
```

Expected output: two filenames, both named `logitune.desktop`:
- `/tmp/logitune-stage/tmp/logitune-stage-prefix/share/applications/logitune.desktop` (launcher, no minimized flag)
- `/tmp/logitune-stage/etc/xdg/autostart/logitune.desktop` (autostart, with minimized flag)

Inspect the autostart copy to confirm it has `Exec=logitune --minimized`:

```bash
grep "^Exec=" /tmp/logitune-stage/etc/xdg/autostart/logitune.desktop
```

Expected: `Exec=logitune --minimized`.

Inspect the launcher copy to confirm it does NOT:

```bash
grep "^Exec=" /tmp/logitune-stage/tmp/logitune-stage-prefix/share/applications/logitune.desktop
```

Expected: `Exec=logitune`.

Clean up:

```bash
rm -rf /tmp/logitune-stage /tmp/logitune-stage-prefix build-stage
```

### Step 4: Verify the main build is still clean

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: build exits 0. Tests still pass at the baseline count (550). The CMake edit has no runtime effect until the package is installed.

### Step 5: Verify no em-dashes

```bash
grep -c "—" data/logitune-autostart.desktop CMakeLists.txt
```

Expected: `0` for each.

### Step 6: Commit

```bash
git add data/logitune-autostart.desktop CMakeLists.txt
git commit -m "build: install autostart desktop entry to /etc/xdg/autostart

Previously the package installed data/logitune.desktop twice: once to
share/applications and once to 'etc/xdg/autostart' (relative). With
CMAKE_INSTALL_PREFIX=/usr that second install landed at
/usr/etc/xdg/autostart/, which no XDG-compliant desktop environment
reads, so the in-tree X-GNOME-Autostart-enabled=true flag never
actually triggered autostart for anyone.

Switch the autostart install to an absolute /etc/xdg/autostart
destination and source it from a new data/logitune-autostart.desktop
variant whose Exec line is 'logitune --minimized'. The launcher
install at share/applications keeps the unflagged file, so clicking
the app launcher still shows the window normally.

No runtime change yet: main.cpp learns about --minimized in a later
commit. Ships together with the CLI flag as a single behavioral
change."
```

---

## Task 2: Add the autostart desktop entry test

**Files:**
- Create: `tests/test_autostart_desktop.cpp`
- Modify: `tests/CMakeLists.txt`

### Step 1: Inspect the existing test registration pattern

```bash
grep -n "test_" tests/CMakeLists.txt | head -20
```

You should see a list of `tests/test_*.cpp` source files being appended to the `logitune-tests` target. If the file uses an explicit list (which is what this project does), note the pattern; you will append to it in Step 3.

### Step 2: Write the test

Create `tests/test_autostart_desktop.cpp`:

```cpp
#include <gtest/gtest.h>
#include <QFile>
#include <QString>
#include <QTextStream>

namespace {

QString readDesktopEntry(const QString &path) {
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
        << "Failed to open " << path.toStdString();
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST(AutostartDesktopEntry, ExecHasMinimizedFlag) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune --minimized\n")))
        << "Autostart entry must invoke logitune with --minimized so the app "
           "starts hidden to the tray on login";
}

TEST(AutostartDesktopEntry, HasGnomeAutostartEnabled) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune-autostart.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("X-GNOME-Autostart-enabled=true")))
        << "GNOME autostart requires the explicit opt-in key";
}

TEST(AutostartDesktopEntry, LauncherEntryDoesNotMinimize) {
    const QString content = readDesktopEntry(
        QStringLiteral(SOURCE_ROOT "/data/logitune.desktop"));
    EXPECT_TRUE(content.contains(QStringLiteral("Exec=logitune\n")))
        << "Manual app-launcher entry must not inherit --minimized; the user "
           "clicking the app launcher expects the window to appear";
    EXPECT_FALSE(content.contains(QStringLiteral("--minimized")))
        << "Launcher .desktop leaked the autostart flag; split files failed";
}
```

The `SOURCE_ROOT` macro is defined in `tests/CMakeLists.txt` (verify in Step 3). If the project uses a different macro name (for example `PROJECT_SOURCE_DIR`), use whichever one is already in scope for existing tests that read repo-relative files. Grep `tests/CMakeLists.txt` for `target_compile_definitions` to find the pattern.

### Step 3: Register the test with CMake

Open `tests/CMakeLists.txt`. Find the list of test sources that populates `logitune-tests`. Append `test_autostart_desktop.cpp` (preserve the existing style — alphabetical, grouped, or chronological as the file does).

If `SOURCE_ROOT` (or the equivalent macro) is not already defined for the tests target, it is. The existing tests (e.g. `test_device_registry.cpp`) reference repo paths through a compile-time macro. Mirror whatever that macro is called when writing the test in Step 2.

### Step 4: Build and run

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests --gtest_filter='AutostartDesktopEntry.*' 2>&1 | tail -10
```

Expected: all three tests pass.

Full run:

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 553 tests.` (550 baseline + 3 new).

### Step 5: Verify no em-dashes

```bash
grep -c "—" tests/test_autostart_desktop.cpp tests/CMakeLists.txt
```

Expected: `0` for each.

### Step 6: Commit

```bash
git add tests/test_autostart_desktop.cpp tests/CMakeLists.txt
git commit -m "test(autostart): cover desktop entry invariants

Three narrow regression tests covering the two desktop entries this
project ships:
 - data/logitune-autostart.desktop contains Exec=logitune --minimized
 - data/logitune-autostart.desktop enables X-GNOME-Autostart-enabled
 - data/logitune.desktop (launcher) does NOT inherit --minimized

Protects against the class of bug that kept autostart broken for
months: the Exec line silently regressing to something the autostart
session ignores (missing flag) or something the launcher invokes in
the wrong mode (flag leaked into launcher)."
```

---

## Task 3: Parse the --minimized flag and hide the window

**Files:**
- Modify: `src/app/main.cpp`

### Step 1: Read the existing command-line option setup

Open `src/app/main.cpp`. Find the block around line 63 that starts with `QCommandLineParser parser;` and ends with `parser.process(app);`. Note the pattern for adding an option:

```cpp
QCommandLineOption simulateAllOption(
    QStringLiteral("simulate-all"),
    QStringLiteral("..."));
parser.addOption(simulateAllOption);
```

and reading it:

```cpp
const bool simulateAll = parser.isSet(simulateAllOption);
```

### Step 2: Add the --minimized option

Immediately after the `editOption` definition and `parser.addOption(editOption)` call (around line 85), add:

```cpp
QCommandLineOption minimizedOption(
    QStringLiteral("minimized"),
    QStringLiteral("Start hidden to the system tray. Intended for autostart "
                   "launchers; ignored if no system tray is available."));
parser.addOption(minimizedOption);
```

After `parser.process(app);` and the existing `const bool simulateAll = ...`, `const bool editMode = ...` lines, add:

```cpp
const bool startMinimized = parser.isSet(minimizedOption);
```

### Step 3: Include QSystemTrayIcon

If `QSystemTrayIcon` is not already included in `src/app/main.cpp` (grep for `#include <QSystemTrayIcon>`), add near the other Qt includes:

```cpp
#include <QSystemTrayIcon>
```

Note: `TrayManager` internally uses `QSystemTrayIcon`, so the header is already pulled in transitively through `TrayManager.h`, but including it directly makes the availability check self-documenting.

### Step 4: Hide the window after QML load when requested

Find the theme-application block around line 229:

```cpp
if (!engine.rootObjects().isEmpty()) {
    QObject *root = engine.rootObjects().first();
    QQmlExpression expr(QQmlEngine::contextForObject(root), root,
        isDark ? QStringLiteral("Theme.dark = true")
               : QStringLiteral("Theme.dark = false"));
    QVariant result = expr.evaluate();
    if (expr.hasError())
        qCWarning(lcApp) << "Theme expression error:" << expr.error().toString();
    else
        qCInfo(lcApp) << "Theme.dark applied:" << isDark;
}
```

Immediately after that closing brace, add:

```cpp
const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
if (startMinimized && trayAvailable) {
    for (QObject *obj : engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow*>(obj))
            window->hide();
    }
    qCInfo(lcApp) << "Startup: minimized to tray";
} else if (startMinimized && !trayAvailable) {
    qCInfo(lcApp) << "Startup: --minimized requested but no system tray "
                     "available, showing window";
} else {
    qCDebug(lcApp) << "Startup: showing window";
}
```

### Step 5: Build

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: exit 0. No new warnings.

### Step 6: Run the suite

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 553 tests.` (no new tests in this task; regressions would surface here).

### Step 7: Verify no em-dashes

```bash
grep -c "—" src/app/main.cpp
```

Expected: matches whatever the file had before this commit (pre-existing em-dashes are fine, new ones are not). Compare:

```bash
before=$(git show HEAD:src/app/main.cpp | grep -c "—")
after=$(grep -c "—" src/app/main.cpp)
echo "before=$before after=$after"
[ "$before" = "$after" ] && echo OK || echo FAIL
```

Expected: `OK`.

### Step 8: Manual verification (run two invocations)

Without `--minimized` (window should appear):

```bash
pkill -f logitune 2>/dev/null; sleep 1
nohup env XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/src/app/logitune > /tmp/logitune-no-flag.log 2>&1 & disown
sleep 3
grep -E "Startup:" /tmp/logitune-no-flag.log
pkill -f logitune 2>/dev/null
```

Expected: log line `Startup: showing window`. Window visible on the desktop.

With `--minimized` (window should be hidden, tray icon visible):

```bash
pkill -f logitune 2>/dev/null; sleep 1
nohup env XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/src/app/logitune --minimized > /tmp/logitune-minimized.log 2>&1 & disown
sleep 3
grep -E "Startup:" /tmp/logitune-minimized.log
pkill -f logitune 2>/dev/null
```

Expected: log line `Startup: minimized to tray` (assuming your desktop has a system tray). No window appears; tray icon does.

### Step 9: Commit

```bash
git add src/app/main.cpp
git commit -m "feat(app): --minimized CLI flag hides window to tray

New QCommandLineOption on main.cpp: when --minimized is set and a
system tray is available, iterate engine.rootObjects() after QML load
and hide each QQuickWindow. Tray icon still appears via the existing
TrayManager.show() path.

Fallback when QSystemTrayIcon::isSystemTrayAvailable() returns false:
skip the hide so the window is visible. Prevents the app from running
invisibly with no way for the user to restore it on sessions that
lack a system tray (some Wayland compositors, stripped-down shells).

One info-level log line per case ('Startup: minimized to tray',
'Startup: --minimized requested but no system tray available,
showing window', 'Startup: showing window') for debugging.

Paired with the /etc/xdg/autostart/logitune.desktop install at
Exec=logitune --minimized (prior commit) to deliver issue #10:
the app autostarts minimized when the user logs in.

Closes #10."
```

---

## Task 4: Final verification

**Files:** none modified.

### Step 1: Clean rebuild

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: both exit 0.

### Step 2: Full test run

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3
```

Expected: 553 core tests pass, 72 QML tests pass.

### Step 3: Install to a staging directory and verify layout

```bash
rm -rf /tmp/logitune-stage
DESTDIR=/tmp/logitune-stage cmake --install build 2>&1 | grep -iE "desktop|install" | tail -10
```

Expected among the output: install lines referencing the launcher at `share/applications/logitune.desktop` and the autostart entry at `/etc/xdg/autostart/logitune.desktop`.

Verify both files exist and have the expected Exec lines:

```bash
find /tmp/logitune-stage -name "logitune.desktop" -exec sh -c 'echo "=== {} ==="; grep "^Exec=" "{}"' \;
```

Expected (two entries):
- `.../share/applications/logitune.desktop` with `Exec=logitune`
- `.../etc/xdg/autostart/logitune.desktop` with `Exec=logitune --minimized`

Clean up staging:

```bash
rm -rf /tmp/logitune-stage
```

### Step 4: Branch commit list

```bash
git log --oneline master..HEAD
```

Expected: the spec commit plus three implementation commits (Task 1, Task 2, Task 3). No amendments.

### Step 5: Em-dash scan on touched files

```bash
git diff --name-only master..HEAD \
  | grep -vE '\.(png|svg)$' \
  | xargs -I{} sh -c 'printf "%s: " "{}"; grep -c "—" "{}" 2>/dev/null || echo "N/A"'
```

Expected: C++ / CMake / desktop / test files print `0`. `docs/superpowers/specs/*` may print non-zero (pre-existing).

### Step 6: Hand-off

Do NOT push the branch. Maintainer pushes and opens the PR.

Summarize for the PR body (the maintainer will edit):

- Fixed the broken autostart install path (`/usr/etc/xdg/autostart/` -> `/etc/xdg/autostart/`).
- Autostart file now invokes `logitune --minimized`.
- New `--minimized` CLI flag hides the main window to tray at startup when a tray is available; falls through to a visible window when no tray is present.
- Three tests on the desktop entries, manual verification of the CLI flag.
- Closes #10.

---

## Done criteria

- Clean Debug rebuild from scratch: 0 errors, 0 warnings introduced by this plan.
- 553 core tests pass (550 baseline + 3 new `AutostartDesktopEntry.*`), 72 QML tests pass.
- `cmake --install` staging shows the autostart file at `/etc/xdg/autostart/logitune.desktop` with `Exec=logitune --minimized`, and the launcher file at `share/applications/logitune.desktop` with `Exec=logitune`.
- Manual run of `./build/src/app/logitune` shows the window; `./build/src/app/logitune --minimized` hides it (tray icon visible).
- Three implementation commits on the branch, each reviewable in isolation, none amended.
- `grep -c "—"` on every touched C++/CMake/desktop/test file prints `0`.
