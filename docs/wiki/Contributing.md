# Contributing

## Getting Set Up

### Fork and Clone

```bash
# Fork the repo on GitHub, then:
git clone https://github.com/<your-username>/logitune.git
cd logitune
```

### Development Environment

**Option A: Devcontainer (recommended)**

Open the repo in VS Code and accept the "Reopen in Container" prompt. The devcontainer builds the project automatically on creation. See [Building — Devcontainer](Building#devcontainer--github-codespaces).

**Option B: Local**

Install dependencies for your distro (see [Building — Prerequisites](Building#prerequisites)), then:

```bash
make build
make setup-hooks
make test-all
```

### Pre-Push Hook

Install the git pre-push hook to catch failures before they reach CI:

```bash
make setup-hooks
```

Since PR #97, `cmake -B build` auto-activates the tracked `hooks/` directory via `core.hooksPath` — there's no manual step. `hooks/pre-push` runs five checks before any push:

1. **README devices table lint** (`scripts/generate-readme-devices.py --check`) — regenerates the table on disk and aborts if `devices/*/descriptor.json` has drifted.
2. **C++ unit + integration tests** (`logitune-tests`).
3. **Tray tests** (`logitune-tray-tests`).
4. **QML tests** (`logitune-qml-tests`).
5. **Python extractor tests** (`pytest tests/scripts/test_extractor.py`).

If any stage fails the push is blocked; for the README lint the hook leaves the regenerated file in your working tree for you to amend/commit.

## Branch Workflow

```bash
# Create a feature branch from master
git checkout master
git pull
git checkout -b feature/my-feature

# Make changes, commit, push
git add <files>
git commit -m "feat: add DPI shift button action"
git push -u origin feature/my-feature

# Open a PR against master
```

## Code Style

### C++

- **Standard**: C++20
- **Naming**: camelCase for variables and methods, PascalCase for types and classes, `m_` prefix for member variables, `k` prefix for constants
- **Includes**: Group by standard library, Qt, project headers; separated by blank lines
- **Namespaces**: `logitune` for production code, `logitune::test` for tests, `logitune::hidpp` for protocol layer
- **Qt conventions**: Use `QStringLiteral()` for string literals, `Q_OBJECT` macro for signal/slot classes, `connect()` with member function pointers (not string-based)
- **No extra abstractions**: Don't add interfaces, factories, or patterns unless they solve a concrete problem. The codebase deliberately avoids unnecessary indirection.
- **Error handling**: Use `std::optional` for fallible operations, `qCWarning/qCDebug` for logging, no exceptions in normal flow

### QML

- **Style**: Follow Qt Quick coding conventions
- **Theme**: All colors, fonts, and spacing come from the `Theme` singleton — never hardcode values
- **Naming**: camelCase for properties and functions, PascalCase for component files

### Header Files

```cpp
#pragma once       // Always use pragma once (not include guards)
#include "..."     // Project includes first
#include <...>     // System/Qt includes second

namespace logitune {

class MyClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value NOTIFY valueChanged)

public:
    explicit MyClass(QObject *parent = nullptr);

    int value() const;

signals:
    void valueChanged();

private slots:
    void onSomethingHappened();

private:
    int m_value = 0;
};

} // namespace logitune
```

## Commit Message Format

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>: <description>

[optional body]

[optional footer]
```

### Types

| Type | When |
|------|------|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `test` | Adding or updating tests |
| `docs` | Documentation only |
| `chore` | Build, CI, tooling changes |

### Examples

```
feat: add DPI shift button action

Hold a button to temporarily lower DPI for precision aiming.
Implemented as ButtonAction::DpiShift with configurable target DPI.

feat: native package builds (.deb, .rpm, Arch), devcontainer for GitHub Codespaces

fix: thumb wheel direction — clockwise should zoom in

Read defaultDirection from HID++ GetInfo to normalize
clockwise = positive in software.

refactor: extract TrayManager, fix battery initial value

test: add profile switch behavior tests for display vs hardware profile
```

### Multi-topic Commits

For large changes touching multiple subsystems, use a summary line followed by subsection headers in the body:

```
feat: thumb wheel overhaul — defaultDirection, invert, command queue, reconnect

Thumb wheel:
- Read defaultDirection from HID++ GetInfo to normalize clockwise=positive
- Add thumbWheelInvert as a proper profile field with UI toggle
- Add horizontal scroll injection (REL_HWHEEL) for scroll mode

HID++ command queue:
- New CommandQueue sends commands sequentially with 10ms pacing
- Eliminates HwError from flooding device during profile switches
```

## PR Checklist

Before opening a pull request, verify:

- [ ] `make test-all` passes locally (or pre-push hook passed)
- [ ] New features have tests
- [ ] No hardcoded values in QML — use Theme singleton
- [ ] No `fprintf`/`qDebug()` — use Qt logging categories (`qCDebug(lcXxx)`, `qCInfo(lcXxx)`, `qCWarning(lcXxx)`)
- [ ] New files added to the appropriate `CMakeLists.txt`
- [ ] Commit messages follow conventional commit format
- [ ] PR description explains what and why (not how)

## Adding a device

The preferred path for new device descriptors is the in-app editor:

1. Fork the repo and bootstrap a `descriptor.json` for your device
   (see [Adding a Device](Adding-a-Device) for the JSON schema).
2. Run `logitune --edit` (pair with `--simulate-all` if you do not
   own the hardware) and use the editor to position hotspots, drop in
   device images, and polish labels.
3. Save. `git diff devices/<slug>/` should show the in-memory changes
   you made.
4. Submit a PR with `"status": "beta"` unless you have
   hardware-verified the descriptor.

For the full walkthrough, see [Editor Mode](Editor-Mode) and
[Adding a Device](Adding-a-Device).

## Where to Find Things

| You want to... | Look in... |
|----------------|------------|
| Add a new HID++ feature | `src/core/hidpp/features/` then a capability-table entry in `src/core/hidpp/capabilities/` |
| Add a new device | `devices/<slug>/` at the repo root: see [Adding a Device](Adding-a-Device) |
| Edit a wiki page | `docs/wiki/*.md` in this repo. Wiki is one-way synced from master; edits on the GitHub wiki itself are overwritten on next sync. |
| Add a new desktop environment | `src/core/desktop/` — see [Adding a Desktop Environment](Adding-a-Desktop-Environment) |
| Add a new button action type | `src/core/ButtonAction.h` and `src/app/AppController.cpp` (onDivertedButtonPressed) |
| Add a new QML page | `src/app/qml/pages/` and register in `src/app/CMakeLists.txt` |
| Add a new QML component | `src/app/qml/components/` and register in `src/app/CMakeLists.txt` |
| Add a new model | `src/app/models/` — create class, register in `main.cpp` as QML singleton |
| Add a new test | `tests/test_*.cpp` and add to `tests/CMakeLists.txt` |
| Add a new QML test | `tests/qml/tst_*.qml` and add to `tests/qml/CMakeLists.txt` |
| Change the protocol layer | `src/core/hidpp/` — Transport, FeatureDispatcher, CommandQueue |
| Change signal wiring | `src/app/AppController.cpp` — `wireSignals()` method |
| Change the UI layout | `src/app/qml/Main.qml` (sidebar + page switcher) |
| Debug device communication | Run with `--debug`, check `lcHidpp` and `lcDevice` log categories |

## Key Design Decisions

These are intentional choices — please don't "fix" them:

1. **No daemon**: Logitune runs as a user application, not a system service. Profile switching happens in-process.

2. **Direct hidraw**: No libhidapi, no libusb. Direct `open()/read()/write()` on `/dev/hidrawN` with `QSocketNotifier` for async I/O.

3. **CommandQueue for pacing**: All hardware writes go through a 10ms-paced queue. This prevents HwError from command flooding. Do not bypass the queue.

4. **Display vs hardware profile**: The UI can show a different profile than what's running on hardware. This prevents accidental hardware writes when browsing profiles.

5. **softwareId for response matching**: HID++ responses use rotating softwareId (1-15) to distinguish from notifications. Without this, async responses get misinterpreted as input events.

6. **Friend classes for test access**: `AppControllerFixture` and `test::AppControllerFixture` are friends of `AppController` and `DeviceManager`. This is intentional — it enables behavioral tests without adding test-only public methods.

7. **Value members, not heap**: AppController owns its subsystems as value members (not pointers). DeviceRegistry, DeviceManager, ProfileEngine, models — they are all stack-allocated inside AppController. Only desktop integration and input injection use pointer indirection (for DI).

8. **KWin script, not polling**: On KDE, focus tracking uses a KWin script that calls back via D-Bus, not polling. The poll timer is only a fallback that installs the script on first tick, then stops.

For more details, see [Architecture](Architecture).
