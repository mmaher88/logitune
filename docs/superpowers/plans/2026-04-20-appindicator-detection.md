# AppIndicator Detection Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop reporting "AppIndicator extension is installed but not enabled" on systems where it is not installed at all; render the banner's install command per detected distro family; pull the extension automatically on Ubuntu/Debian via deb `Recommends`.

**Architecture:** Four surgical changes. A new `logitune::DistroDetector` parses `/etc/os-release` once and classifies the OS as `Arch`, `Debian`, `Fedora`, or `Unknown`. `DeviceModel::appIndicatorInstallCommand()` returns the correct install command for the QML banner. `GnomeDesktop::appIndicatorStatus` detection guards for the empty-dict success-reply case. `scripts/package-deb.sh` gains a `Recommends:` line.

**Tech Stack:** C++20 / Qt 6 / GTest. No new third-party dependencies.

**Design spec:** `docs/superpowers/specs/2026-04-20-appindicator-detection-design.md`. Read it before Task 1.

---

## Global rules

- **No em-dashes (U+2014 "—")** in any file you create or modify. Pre-existing em-dashes in files you otherwise touch are fine; just do not add new ones.
- **No co-author signatures** in commit messages.
- **Branch is `fix-appindicator-detection`.** Already created with the spec committed. Do NOT push. Maintainer pushes after final verification.
- **Working directory:** `/home/mina/repos/logitune`.
- **Never amend commits.** Each task makes a new commit on top.
- **Build + test** after every task: `cmake --build build -j"$(nproc)"` must exit 0; `XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests` must show `[  PASSED  ] <N> tests.` Baseline count before this plan is 553.

---

## File Structure

### Created

```
src/core/DistroDetector.h                          (new, declares DistroFamily + detectDistroFamily())
src/core/DistroDetector.cpp                        (new, parses /etc/os-release, cached)
tests/test_distro_detector.cpp                     (new, parameterized unit tests)
```

### Modified

- `src/core/CMakeLists.txt`: add `DistroDetector.cpp` to the `logitune-core` source list.
- `tests/CMakeLists.txt`: add `test_distro_detector.cpp` to the `logitune-tests` source list.
- `src/app/models/DeviceModel.h`: declare `Q_INVOKABLE QString appIndicatorInstallCommand() const;`.
- `src/app/models/DeviceModel.cpp`: implement the getter using `DistroDetector`.
- `src/app/qml/HomeView.qml`: banner "not installed" path reads `DeviceModel.appIndicatorInstallCommand()` instead of the hard-coded pacman line.
- `src/core/desktop/GnomeDesktop.cpp`: fix the empty-dict-is-installed bug (guard the detection branch).
- `scripts/package-deb.sh`: add `Recommends: gnome-shell-extension-appindicator` to the control-file template.

### Unchanged

- `src/core/desktop/GnomeDesktop.h`: `AppIndicatorState` enum already has the four states needed.
- `scripts/package-rpm.sh`, `scripts/package-arch.sh`: out of scope per spec.

---

## Task 1: DistroDetector utility

**Files:**
- Create: `src/core/DistroDetector.h`
- Create: `src/core/DistroDetector.cpp`
- Create: `tests/test_distro_detector.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Follow TDD: write the parser spec as a test first, watch it fail, then implement.

### Step 1: Write the failing test

Create `tests/test_distro_detector.cpp`:

```cpp
#include <gtest/gtest.h>
#include <QTemporaryFile>
#include <QTextStream>
#include "DistroDetector.h"

using logitune::util::DistroFamily;

namespace {

// Writes the given body to a temp file and returns its path. The temp
// file stays alive for the duration of the test via the caller's
// std::unique_ptr so QTemporaryFile's destructor does not delete it
// before the detector reads it.
std::unique_ptr<QTemporaryFile> writeOsRelease(const QString &body) {
    auto f = std::make_unique<QTemporaryFile>();
    f->setAutoRemove(true);
    EXPECT_TRUE(f->open());
    QTextStream ts(f.get());
    ts << body;
    ts.flush();
    f->close();
    return f;
}

} // namespace

TEST(DistroDetector, ArchLinux) {
    auto f = writeOsRelease(R"(NAME="Arch Linux"
ID=arch
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Arch);
}

TEST(DistroDetector, CachyOSViaIdLike) {
    auto f = writeOsRelease(R"(NAME="CachyOS Linux"
ID=cachyos
ID_LIKE=arch
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Arch);
}

TEST(DistroDetector, Ubuntu) {
    auto f = writeOsRelease(R"(NAME="Ubuntu"
ID=ubuntu
ID_LIKE=debian
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, PopOsViaIdLike) {
    auto f = writeOsRelease(R"(NAME="Pop!_OS"
ID=pop
ID_LIKE="ubuntu debian"
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, PlainDebian) {
    auto f = writeOsRelease(R"(NAME="Debian GNU/Linux"
ID=debian
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Debian);
}

TEST(DistroDetector, Fedora) {
    auto f = writeOsRelease(R"(NAME="Fedora Linux"
ID=fedora
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Fedora);
}

TEST(DistroDetector, RockyLinuxViaIdLike) {
    auto f = writeOsRelease(R"(NAME="Rocky Linux"
ID=rocky
ID_LIKE="centos rhel fedora"
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Fedora);
}

TEST(DistroDetector, UnknownDistro) {
    auto f = writeOsRelease(R"(NAME="Frankendistro"
ID=frankendistro
)");
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(f->fileName()),
              DistroFamily::Unknown);
}

TEST(DistroDetector, MissingFileIsUnknown) {
    EXPECT_EQ(logitune::util::detectDistroFamilyFromFile(
                  QStringLiteral("/nonexistent/path/to/os-release")),
              DistroFamily::Unknown);
}
```

Note: the tests exercise the explicit-path overload `detectDistroFamilyFromFile(const QString&)` so they can feed synthetic content. Production code uses the zero-arg `detectDistroFamily()` which reads `/etc/os-release`.

### Step 2: Create the header

Create `src/core/DistroDetector.h`:

```cpp
#pragma once

#include <QString>

namespace logitune::util {

enum class DistroFamily {
    Unknown,
    Arch,
    Debian,
    Fedora,
};

// Parse /etc/os-release once per process and classify the distro.
// Result is cached in a function-local static for subsequent calls.
DistroFamily detectDistroFamily();

// Test hook: classify from a specific file path. Does NOT cache.
// Intended for unit tests that feed synthetic content.
DistroFamily detectDistroFamilyFromFile(const QString &path);

} // namespace logitune::util
```

### Step 3: Create the implementation

Create `src/core/DistroDetector.cpp`:

```cpp
#include "DistroDetector.h"

#include <QFile>
#include <QSet>
#include <QTextStream>

namespace logitune::util {

namespace {

// Parses a single line of /etc/os-release into (key, value) where the
// value has surrounding double quotes stripped.
bool parseLine(const QString &line, QString &key, QString &value) {
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) return false;
    key = line.left(eq).trimmed();
    QString raw = line.mid(eq + 1).trimmed();
    if (raw.size() >= 2 && raw.startsWith(QLatin1Char('"')) && raw.endsWith(QLatin1Char('"')))
        raw = raw.mid(1, raw.size() - 2);
    value = raw;
    return true;
}

DistroFamily classify(const QString &id, const QStringList &idLike) {
    // Exact-ID matches first.
    if (id == QLatin1String("arch"))
        return DistroFamily::Arch;
    if (id == QLatin1String("debian") || id == QLatin1String("ubuntu"))
        return DistroFamily::Debian;
    if (id == QLatin1String("fedora") || id == QLatin1String("rhel")
        || id == QLatin1String("rocky") || id == QLatin1String("almalinux")
        || id == QLatin1String("centos"))
        return DistroFamily::Fedora;

    // Fall back to ID_LIKE (space-separated tokens).
    for (const QString &like : idLike) {
        if (like == QLatin1String("arch"))
            return DistroFamily::Arch;
        if (like == QLatin1String("debian") || like == QLatin1String("ubuntu"))
            return DistroFamily::Debian;
        if (like == QLatin1String("fedora") || like == QLatin1String("rhel"))
            return DistroFamily::Fedora;
    }
    return DistroFamily::Unknown;
}

DistroFamily parseOsRelease(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return DistroFamily::Unknown;

    QString id;
    QStringList idLike;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString key, value;
        if (!parseLine(ts.readLine(), key, value)) continue;
        if (key == QLatin1String("ID")) {
            id = value;
        } else if (key == QLatin1String("ID_LIKE")) {
            idLike = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
    }
    return classify(id, idLike);
}

} // namespace

DistroFamily detectDistroFamily() {
    static const DistroFamily cached = parseOsRelease(QStringLiteral("/etc/os-release"));
    return cached;
}

DistroFamily detectDistroFamilyFromFile(const QString &path) {
    return parseOsRelease(path);
}

} // namespace logitune::util
```

### Step 4: Register with CMake

Open `src/core/CMakeLists.txt`. Find the `target_sources(logitune-core PRIVATE ...)` block and append `DistroDetector.cpp` to the list (preserve existing ordering):

```cmake
target_sources(logitune-core PRIVATE
    DeviceManager.cpp
    DeviceSession.cpp
    PhysicalDevice.cpp
    ...
    DeviceFetcher.cpp
    DistroDetector.cpp
)
```

Open `tests/CMakeLists.txt`. Find the source list for `logitune-tests` (similar `target_sources` or `add_executable` pattern) and append `test_distro_detector.cpp` alphabetically or in the existing ordering convention.

### Step 5: Build and run the new tests

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests --gtest_filter='DistroDetector.*' 2>&1 | tail -15
```

Expected: 9 tests pass.

### Step 6: Full suite

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 562 tests.` (553 baseline + 9 new).

### Step 7: Verify no em-dashes on added lines

```bash
git diff HEAD -- src/core/DistroDetector.h src/core/DistroDetector.cpp tests/test_distro_detector.cpp src/core/CMakeLists.txt tests/CMakeLists.txt | grep "^+" | grep -c "—"
```

Expected: `0`.

### Step 8: Commit

```bash
git add src/core/DistroDetector.h src/core/DistroDetector.cpp \
        tests/test_distro_detector.cpp \
        src/core/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(core): DistroDetector for /etc/os-release family mapping

Small utility that parses /etc/os-release once per process and
returns a DistroFamily enum (Arch, Debian, Fedora, Unknown). Uses
ID first, falls back to ID_LIKE space-separated tokens. The public
API has a zero-arg variant that caches the result and a path-arg
variant for tests.

Nine parameterized tests cover Arch, CachyOS (ID_LIKE=arch),
Ubuntu, Pop!_OS (ID_LIKE includes ubuntu+debian), plain Debian,
Fedora, Rocky Linux, an unknown distro, and a missing file.

No consumer yet: DeviceModel wires the mapping to the
AppIndicator banner in the next commit."
```

---

## Task 2: DeviceModel::appIndicatorInstallCommand

**Files:**
- Modify: `src/app/models/DeviceModel.h`
- Modify: `src/app/models/DeviceModel.cpp`
- Modify: `src/app/qml/HomeView.qml`

No new tests specifically for this getter; the DistroDetector tests already cover the mapping logic. A single smoke test in `test_device_model.cpp` confirms the getter returns the canonical package name.

### Step 1: Declare the getter

Open `src/app/models/DeviceModel.h`. Find the existing `Q_INVOKABLE QString gnomeTrayStatus() const;` declaration (around line 143). Add immediately after:

```cpp
Q_INVOKABLE QString appIndicatorInstallCommand() const;
```

### Step 2: Implement the getter

Open `src/app/models/DeviceModel.cpp`. Add `#include "DistroDetector.h"` near the other `#include` lines at the top of the file (alphabetical with neighbours).

After the existing `QString DeviceModel::gnomeTrayStatus() const { ... }` block (ends around line 821), add:

```cpp
QString DeviceModel::appIndicatorInstallCommand() const
{
    switch (logitune::util::detectDistroFamily()) {
    case logitune::util::DistroFamily::Arch:
        return QStringLiteral("sudo pacman -S gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Debian:
        return QStringLiteral("sudo apt install gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Fedora:
        return QStringLiteral("sudo dnf install gnome-shell-extension-appindicator");
    case logitune::util::DistroFamily::Unknown:
    default:
        return QStringLiteral("Install gnome-shell-extension-appindicator via your package manager.");
    }
}
```

### Step 3: Add a narrow test

Open `tests/test_device_model.cpp`. Add a single test at a place that matches the file's existing style (other `DeviceModelTest` cases around line 26-160):

```cpp
TEST_F(DeviceModelTest, AppIndicatorInstallCommandNonEmpty) {
    const QString cmd = model.appIndicatorInstallCommand();
    EXPECT_FALSE(cmd.isEmpty());
    EXPECT_TRUE(cmd.contains(QStringLiteral("gnome-shell-extension-appindicator")))
        << "command: " << cmd.toStdString();
}
```

This does not pin the exact command (depends on where the test runs), but asserts non-empty + canonical package name is present in the output.

### Step 4: Update QML banner

Open `src/app/qml/HomeView.qml`. Find the banner's "not-installed" remediation text around line 236. Current:

```qml
if (status === "not-installed")
    return "Run: sudo pacman -S gnome-shell-extension-appindicator\nThen log out and back in."
```

Replace with:

```qml
if (status === "not-installed")
    return "Run: " + DeviceModel.appIndicatorInstallCommand() + "\nThen log out and back in."
```

The "disabled" path stays unchanged; `gnome-extensions enable` is distro-agnostic.

### Step 5: Build and run tests

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 563 tests.` (562 after Task 1 + 1 new).

### Step 6: Smoke-render the banner

Launch the app with simulate mode and eyeball the banner:

```bash
pkill -x logitune 2>/dev/null; sleep 1
nohup env XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/src/app/logitune --simulate-all > /tmp/logitune-banner.log 2>&1 & disown
sleep 3
```

The banner will only render if `DeviceModel.gnomeTrayStatus()` returns `"not-installed"` or `"disabled"` on the host running the test. On CachyOS with the extension enabled, the banner will be hidden (status `""`). That is expected; this smoke only confirms the app still starts cleanly.

```bash
grep -iE "error|QML.*error" /tmp/logitune-banner.log | head -5
pkill -x logitune 2>/dev/null
```

Expected: no QML errors. The banner content itself is verified on the Ubuntu VM in Task 5.

### Step 7: Verify no em-dashes on added lines

```bash
git diff HEAD -- src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp src/app/qml/HomeView.qml tests/test_device_model.cpp | grep "^+" | grep -c "—"
```

Expected: `0`.

### Step 8: Commit

```bash
git add src/app/models/DeviceModel.h src/app/models/DeviceModel.cpp \
        src/app/qml/HomeView.qml tests/test_device_model.cpp
git commit -m "feat(device-model): appIndicatorInstallCommand per distro family

New Q_INVOKABLE that maps the detected DistroFamily to the correct
package-manager invocation:
 - Arch family -> sudo pacman -S gnome-shell-extension-appindicator
 - Debian/Ubuntu -> sudo apt install gnome-shell-extension-appindicator
 - Fedora/RHEL -> sudo dnf install gnome-shell-extension-appindicator
 - Unknown -> generic hint with the package name

HomeView.qml's not-installed banner reads the getter instead of
hard-coding the pacman command, so Ubuntu users see apt, Fedora
users see dnf, etc. The enable-extension remediation (gnome-extensions
enable ...) is already distro-agnostic and stays unchanged.

One smoke test pins the canonical package name in the returned
string; exact command depends on the host running the test."
```

---

## Task 3: Fix GnomeDesktop AppIndicator detection

**Files:**
- Modify: `src/core/desktop/GnomeDesktop.cpp`

No test. DBus mocking for this one probe is heavier than the win. Manual verification on two VMs (Task 5).

### Step 1: Patch the detection block

Open `src/core/desktop/GnomeDesktop.cpp`. Find the block around lines 63-97 that calls `GetExtensionInfo` and maps the result to an `AppIndicatorState`. Current:

```cpp
if (infoReply.type() != QDBusMessage::ReplyMessage) {
    m_appIndicatorStatus = AppIndicatorNotInstalled;
    qCWarning(lcFocus) << "AppIndicator extension not installed -- tray icon will not be visible."
                       << "Install: gnome-shell-extension-appindicator";
} else {
    // Extension exists -- check if enabled (state 1 = ENABLED/ACTIVE)
    QVariantMap info = qdbus_cast<QVariantMap>(infoReply.arguments().first());
    double state = info.value(QStringLiteral("state")).toDouble();
    // GNOME Shell extension states: 1=ENABLED, 2=DISABLED, 3=ERROR, 4=OUT_OF_DATE, 5=DOWNLOADING, 6=INITIALIZED
    if (state == 1.0) {
        m_appIndicatorStatus = AppIndicatorActive;
        qCInfo(lcFocus) << "AppIndicator extension active";
    } else {
        m_appIndicatorStatus = AppIndicatorDisabled;
        qCWarning(lcFocus) << "AppIndicator extension installed but not enabled (state:" << state << ")"
                           << "-- run: gnome-extensions enable" << kAppIndicatorUuid;
    }
}
```

Replace with:

```cpp
bool installed = false;
QVariantMap info;
if (infoReply.type() == QDBusMessage::ReplyMessage && !infoReply.arguments().isEmpty()) {
    info = qdbus_cast<QVariantMap>(infoReply.arguments().first());
    installed = !info.isEmpty();
}

if (!installed) {
    m_appIndicatorStatus = AppIndicatorNotInstalled;
    qCWarning(lcFocus) << "AppIndicator extension not installed -- tray icon will not be visible."
                       << "Install: gnome-shell-extension-appindicator";
} else {
    // GNOME Shell extension states: 1=ENABLED, 2=DISABLED, 3=ERROR,
    //                               4=OUT_OF_DATE, 5=DOWNLOADING, 6=INITIALIZED.
    double state = info.value(QStringLiteral("state")).toDouble();
    if (state == 1.0) {
        m_appIndicatorStatus = AppIndicatorActive;
        qCInfo(lcFocus) << "AppIndicator extension active";
    } else {
        m_appIndicatorStatus = AppIndicatorDisabled;
        qCWarning(lcFocus) << "AppIndicator extension installed but not enabled (state:" << state << ")"
                           << "-- run: gnome-extensions enable" << kAppIndicatorUuid;
    }
}
```

Key changes:
- Read `info` only when the reply is a `ReplyMessage` AND has at least one argument. Prevents UB on `arguments().first()` for error replies.
- `installed` is true only when `info` has entries. An empty dict means GNOME Shell returned success but the uuid does not exist, so the extension is not installed.
- Log messages unchanged.

### Step 2: Build

```bash
cmake --build build -j"$(nproc)" 2>&1 | tail -3
```

Expected: exit 0, no new warnings.

### Step 3: Full suite

```bash
XDG_DATA_DIRS="$(pwd)/build:/usr/local/share:/usr/share" ./build/tests/logitune-tests 2>&1 | tail -3
```

Expected: `[  PASSED  ] 563 tests.` (unchanged from Task 2).

### Step 4: Verify no em-dashes on added lines

```bash
git diff HEAD -- src/core/desktop/GnomeDesktop.cpp | grep "^+" | grep -c "—"
```

Expected: `0`.

### Step 5: Commit

```bash
git add src/core/desktop/GnomeDesktop.cpp
git commit -m "fix(gnome): treat empty GetExtensionInfo as not-installed

GNOME Shell's org.gnome.Shell.Extensions.GetExtensionInfo returns
an empty a{sv} dict (still a successful ReplyMessage) for uuids that
are not installed. The previous detection only guarded against
ErrorMessage, so the empty dict fell into the else branch where
info.value('state').toDouble() returned 0.0 and we mapped that to
AppIndicatorDisabled. Every user without the extension saw the UI
banner 'installed but not enabled' with a remediation command that
then failed with 'Extension does not exist'.

Guard now requires ReplyMessage AND a non-empty dict before parsing
state. ErrorMessage replies also fall through to AppIndicatorNotInstalled
via the same branch, and arguments().first() is only called when the
reply has arguments, preventing UB on error replies with no body.

No unit test: DBus mocking is out of proportion for this single
probe. Manual verification on Ubuntu 24.04 (no extension installed:
reports not-installed) and CachyOS (extension enabled: reports
active)."
```

---

## Task 4: Deb `Recommends`

**Files:**
- Modify: `scripts/package-deb.sh`

### Step 1: Add the Recommends line

Open `scripts/package-deb.sh`. Find the `control` heredoc that emits the deb control file. Look for the `Depends:` line (around line 33-35). Immediately after the `Depends:` line, add:

```
Recommends: gnome-shell-extension-appindicator
```

Resulting context:

```
Package: logitune
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libqt6core6 (>= 6.4), libqt6qml6, libqt6quick6, ...
Recommends: gnome-shell-extension-appindicator
Maintainer: ...
```

### Step 2: Build a fresh .deb from this branch to verify the control file

```bash
GITHUB_REF_NAME=v0.3.3-dev bash scripts/package-deb.sh 2>&1 | tail -3
```

Expected: ends with `logitune-0.3.3~dev_amd64.deb`.

### Step 3: Inspect the control file inside the new deb

```bash
dpkg-deb -f logitune-0.3.3~dev_amd64.deb Recommends 2>&1
```

Expected: `Recommends: gnome-shell-extension-appindicator`.

Clean up the local deb so it does not pollute the repo root:

```bash
rm -f logitune-0.3.3~dev_amd64.deb
```

### Step 4: Verify no em-dashes on added lines

```bash
git diff HEAD -- scripts/package-deb.sh | grep "^+" | grep -c "—"
```

Expected: `0`.

### Step 5: Commit

```bash
git add scripts/package-deb.sh
git commit -m "build(deb): Recommends gnome-shell-extension-appindicator

Ubuntu and Debian install Recommends by default, so 'apt install
logitune' on a fresh system pulls the AppIndicator extension
automatically and the tray icon works out of the box. Users who
want to opt out still can with 'apt install --no-install-recommends
logitune'.

No change to the RPM spec or pacman PKGBUILD: those ecosystems do
not have an equivalent install-by-default recommendation mechanism
we want to lean on here."
```

---

## Task 5: Final verification

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

Expected: 563 core + 72 QML.

### Step 3: Build and test install on Ubuntu VM (if available)

If the maintainer has an Ubuntu 24.04 VM or container accessible (e.g.
at `172.16.218.131` on their local network, as seen in the prior
autostart cycle), build a deb and test install:

```bash
GITHUB_REF_NAME=v0.3.3-dev bash scripts/package-deb.sh 2>&1 | tail -3
scp logitune-0.3.3~dev_amd64.deb 172.16.218.131:/tmp/logitune.deb 2>&1 | tail -2
ssh 172.16.218.131 'sudo dpkg -i /tmp/logitune.deb; sudo apt-get install -f -y 2>&1 | tail -5'
ssh 172.16.218.131 'dpkg-query -W -f="${Status}\n" gnome-shell-extension-appindicator 2>&1'
```

Expected:
- Clean `ii` install of logitune.
- `gnome-shell-extension-appindicator` shows `install ok installed` (pulled via Recommends).

Clean up:

```bash
rm -f logitune-0.3.3~dev_amd64.deb
```

### Step 4: Manual smoke on the Ubuntu VM desktop

Log in on the VM (or ask the maintainer to). After login, the
autostart should fire with `--minimized`. With the extension now
installed, the tray icon should appear and the main window should
stay hidden.

In a terminal on the VM:

```bash
journalctl --user -u graphical-session.target -b --since "5 minutes ago" 2>&1 | grep -i logitune
```

Look for `Startup: minimized to tray` (from the autostart PR's log
line). If the line says `Startup: --minimized requested but tray is
not visible, showing window`, either the extension is not enabled or
the tray host is still initialising; user can open the banner via
the app and confirm status.

### Step 5: Branch commit list

```bash
git log --oneline master..HEAD
```

Expected: the spec commit plus four implementation commits (Tasks
1-4). No amendments.

### Step 6: Em-dash scan on touched files

```bash
git diff --name-only master..HEAD \
  | grep -vE '\.(png|svg)$' \
  | xargs -I{} sh -c 'printf "%s: " "{}"; grep -c "—" "{}" 2>/dev/null || echo "N/A"'
```

Expected: C++ / CMake / QML / shell / test files print `0`. Docs may
print non-zero (pre-existing).

### Step 7: Hand-off

Do NOT push the branch. Maintainer pushes and opens the PR.

---

## Done criteria

- Clean Debug rebuild from scratch: 0 errors, 0 warnings introduced.
- 563 core tests pass (553 baseline + 9 DistroDetector + 1 DeviceModel). 72 QML tests pass.
- Ubuntu 24.04 `apt install logitune` pulls `gnome-shell-extension-appindicator` via Recommends.
- In-app banner on a system without the extension reports "not installed" (not "installed but not enabled") with the correct install command for the detected distro.
- Four implementation commits on the branch, each reviewable in isolation, none amended.
- `grep -c "—"` on every touched C++/CMake/QML/shell/test file prints `0`.
