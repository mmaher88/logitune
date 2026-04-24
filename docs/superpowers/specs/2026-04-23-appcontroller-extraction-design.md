# AppController Extraction and Rename to AppRoot

**Status:** Design, pending implementation
**Issue:** [#107](https://github.com/mmaher88/logitune/issues/107)
**Target branch:** `refactor-appcontroller-extract`

## Goal

Extract behavior out of `AppController` (currently 857 lines mixing composition root, profile orchestration, device input interpretation, and hardware command relays) into focused services, and rename the resulting class to `AppRoot` to reflect its true role.

This is mechanical extraction plus a rename. No changes to MVVM structure, QML bindings, protocol layer, or descriptor format.

## Final role of `AppRoot`

After extraction and rename, `AppRoot`'s role is:

1. Instantiate and own long-lived singletons (ViewModels, services, engines)
2. Accept injected interfaces (`IDesktopIntegration*`, `IInputInjector*`) and pass them down
3. Wire the signal graph between services at startup
4. Handle runtime device lifecycle (`onPhysicalDeviceAdded/Removed`): attach new `PhysicalDevice` instances into the graph
5. Expose ViewModels via accessors for QML registration in `main.cpp`
6. Bootstrap via `startMonitoring()`

Equivalent to a hand-written DI container plus a signal wiring layer. `AppRoot` does not implement any user-facing behavior.

The name `AppController` is misleading in an MVVM stack because "Controller" implies MVC-style behavior that, after this refactor, lives in `ProfileOrchestrator`, `ButtonActionDispatcher`, and `DeviceCommandHandler`. Renaming to `AppRoot` locks in the composition-root terminology and prevents future drift of behavior back into the class.

## New architecture

### Services (new)

All new service classes live in `src/app/services/`, with matching fixtures in `tests/services/`.

**`ActiveDeviceResolver`** (~40 LOC)
- Resolves the currently selected `PhysicalDevice` / `DeviceSession` / serial from `ProfileModel::selectedDeviceIndex()` and `DeviceManager`
- Exposes `activeDevice()`, `activeSession()`, `activeSerial()` + `selectionChanged()` signal
- Read-only; single source of truth for "who is selected"

**`DeviceCommandHandler`** (~80 LOC, replaces 5 passthrough slots on AppController)
- Receives UI change requests: `requestDpi`, `requestSmartShift`, `requestScrollConfig`, `requestThumbWheelMode`, `requestThumbWheelInvert`
- Resolves active session via `ActiveDeviceResolver`, calls the corresponding `DeviceSession::setXxx`
- Emits `userChangedSomething()` after each successful mutation
- Handles "no active session" as a no-op (no crash)

**`ButtonActionDispatcher`** (~120 LOC)
- Owns `PerDeviceState` struct (private) + `kGestureThreshold`, `kThumbThreshold`
- Handles `PhysicalDevice::gestureRawXY`, `divertedButtonPressed`, `thumbWheelRotation`
- Translates button presses into actions (SmartShift toggle, DPI cycle, keystroke injection, gesture start, app launch) via `ActionExecutor`
- On `profileApplied` signal from `ProfileOrchestrator`, resets `thumbAccum` for the given serial

**`ProfileOrchestrator`** (~350 LOC, the main behavior extraction)
- Owns: `applyProfileToHardware`, `saveCurrentProfile`, `pushDisplayValues`, `restoreButtonModelFromProfile`, `setupProfileForDevice`, `onWindowFocusChanged`, `onTabSwitched`, `onDisplayProfileChanged`, `onUserButtonChanged`
- Emits `profileApplied(serial)` after every hardware apply so `ButtonActionDispatcher` can reset its state
- Subscribes to `DeviceCommandHandler::userChangedSomething` and `ButtonModel::userActionChanged` (existing signal) to trigger `saveCurrentProfile`

### `ActionModel` gains

- `buttonActionToName(ButtonAction)` and `buttonEntryToAction(type, name)` move from `AppController` onto `ActionModel` as member functions. These translate between the `ButtonAction` domain type and the `(typeName, displayName)` UI pair.

### Ownership and dependency rules

These rules apply to the final state (post-rename). During extraction commits, `AppController` holds the role; the rename in the final commit swaps the class name without changing any role boundaries.

- `AppRoot` owns all ViewModels, services, engines, the `DeviceRegistry`, `DeviceManager`, and `DeviceFetcher`
- Services hold raw (non-owning) pointers to: models they read/write, engines (`ProfileEngine`, `ActionExecutor`), and `ActiveDeviceResolver`
- Services do not hold pointers to other services
- Services do not call `connect()`; they expose signals and slots and are wired by `AppRoot`
- Cross-service communication is via Qt signals, wired in `AppRoot::wireSignals()` or `onPhysicalDeviceAdded`

This is the dependency rule:

> Services hold pointers only to models, engines, and `ActiveDeviceResolver`. Cross-service communication is always via signal, wired in `AppRoot`.

### Runtime device attach

`onPhysicalDeviceAdded` stays in `AppRoot`. It:

1. Adds the device to `DeviceModel`
2. Connects `PhysicalDevice::gestureRawXY` / `divertedButtonPressed` / `thumbWheelRotation` directly to `ButtonActionDispatcher` slots
3. Connects `PhysicalDevice::transportSetupComplete` to a handler that calls `ProfileOrchestrator::onTransportSetupComplete(device)`
4. Calls `ProfileOrchestrator::setupProfileForDevice(device)` to initialize profile state

`onPhysicalDeviceRemoved` reverses: notifies `DeviceModel`, `ProfileOrchestrator`, `ButtonActionDispatcher` (to drop `PerDeviceState` entry).

## Testing strategy

Per-service fixtures for unit coverage, plus the existing integration fixture (renamed in the final commit) for end-to-end coverage.

- `ActiveDeviceResolverFixture` — selection resolution edge cases (out-of-range index, transport switch within a `PhysicalDevice`)
- `DeviceCommandHandlerFixture` — null session no-op, signal emission exactly once per request, clamping pass-through
- `ButtonActionDispatcherFixture` — gesture threshold math, thumb wheel accumulation, action dispatch for each `ButtonAction` type
- `ProfileOrchestratorFixture` — save/apply cycle, window-focus profile switch, display vs hardware profile divergence, `thumbAccum` reset signal on apply
- `AppControllerFixture` (existing, renamed to `AppRootFixture` in the final commit) — end-to-end integration covering the wired signal graph

Real-hardware smoke test on MX Master 3S is mandatory after each commit in the sequence. Any commit that fails to verify on hardware gets amended or reverted before proceeding.

## Commit sequence

Seven commits, each independently building, passing tests, and smoke-tested on hardware. The rename is the last commit so all extraction commits read naturally against the familiar `AppController` name.

1. **Add `ActiveDeviceResolver`** — new service + fixture. Replace `AppController::selectedDevice/Session/Serial` helpers and `onSelectedDeviceChanged` with calls into `ActiveDeviceResolver`. Update any slot that used those helpers.
2. **Add `DeviceCommandHandler`** — new service + fixture. Move the 5 `*ChangeRequested` slots into it; delete from `AppController`. Temporarily wire `DeviceCommandHandler::userChangedSomething` to `AppController::saveCurrentProfile` (the method still lives on AppController until commit 4; this one `connect()` call gets redirected to `ProfileOrchestrator` then).
3. **Add `ButtonActionDispatcher`** — new service + fixture. Move `onDivertedButtonPressed`, `onThumbWheelRotation`, `PerDeviceState`, `kGestureThreshold`, `kThumbThreshold`, and the `gestureRawXY` lambda. Rewire the per-device connects in `onPhysicalDeviceAdded` to point at the dispatcher. Delete from `AppController`.
4. **Add `ProfileOrchestrator`** — new service + fixture. Move the 9 profile-related methods and slots. Wire `profileApplied` signal into `ButtonActionDispatcher` and the `userChangedSomething` signals from `DeviceCommandHandler` / `ButtonModel` into `ProfileOrchestrator::saveCurrentProfile`. Remove the shim from commit 2.
5. **Move translation helpers** — `buttonActionToName` / `buttonEntryToAction` go onto `ActionModel`; update callers in `ProfileOrchestrator`.
6. **Cleanup** — add contract docstring to the class stating its composition-root role, remove now-unused `#include`s, remove dead code. Class is still named `AppController` at this point.
7. **Rename `AppController` to `AppRoot`** — rename class, rename `src/app/AppController.{h,cpp}` to `src/app/AppRoot.{h,cpp}`, rename `AppControllerFixture` to `AppRootFixture`, update every `#include`, forward declaration, friend class, CMake source list, wiki page, diagram label, and docstring reference. Purely mechanical. No behavior changes.

## Out of scope

- Any changes to MVVM structure or QML bindings
- Any protocol-layer or descriptor-format changes
- Extracting `EditorModel`-related logic into an `EditorOrchestrator` (separate concern; `EditorModel` is already `unique_ptr` on AppController)

## Risks

- **Wiring regressions** — `wireSignals()` fans out to 4 new classes. Easy to wire the same signal twice or to the wrong instance. Mitigation: the integration fixture covers the full signal graph end-to-end; every commit must keep it green.
- **Lost behavior in extraction** — subtle ordering of slot execution or side effects that matter in practice but aren't covered by tests. Mitigation: smoke test on real hardware after each commit before proceeding.
- **Rename commit touches many files** — the final commit is large but purely mechanical. Mitigation: it lands last, after every extraction commit is green. Reviewers can verify correctness by checking that the rename diff is entirely s/AppController/AppRoot/ with no incidental changes.
- **Per-service fixtures over-invest relative to value** — not all services are complex. Accepted; the fixture per service is cheap (~20 LOC each) and documents the service contract.

## Follow-ups (issues to file after this PR merges)

1. Extract `EditorOrchestrator` for editor-mode state (currently `EditorModel` is a `unique_ptr` stub on AppRoot; behavior is spread across QML and AppRoot bits)
