# Semantic Action Presets with Per-DE Invocation — Design Spec

**Issue:** [#110](https://github.com/mmaher88/logitune/issues/110)
**Date:** 2026-04-24
**Branch:** `semantic-action-presets`

## Goal

Introduce a semantic action preset layer above `ActionExecutor` so that actions like "Show Desktop", "Task Switcher", and "Switch Desktop Left/Right" work correctly on the active desktop environment, regardless of the user's keyboard shortcut configuration, and so that profiles remain portable when the user switches DE.

## Problem

Button actions today collapse to raw payloads in `ButtonAction`:

- `Keystroke` / `Media` / `DBus` / `AppLaunch` carry a user-typed string
- Device-local types (`SmartShiftToggle`, `DpiCycle`, `GestureTrigger`) are handled on-device

All DE, distro, and user-shortcut variance is pushed onto the payload. `ActionModel` currently ships hardcoded keystrokes like `Super+D` for "Show desktop" that only match GNOME-family defaults. On KDE, XFCE, MATE, and tiling WMs, those keystrokes either do the wrong thing or do nothing. Profiles do not survive a DE switch. The action picker cannot show named icon-labelled entries the way Options+ does.

## Actions Requiring This Work

From `src/app/models/ActionModel.cpp`:

| Action name | Current payload | Why it breaks |
|---|---|---|
| Show desktop | `Super+D` | Only correct on GNOME-family. KDE: `Ctrl+F12`. XFCE/MATE: `Ctrl+Alt+D`. Tiling WMs: no default |
| Task switcher | `Super+W` | KDE-specific (Present Windows). Different or absent on every other DE |
| Switch desktop left | `Ctrl+Super+Left` | Not universal. GNOME uses `Super+Page_Up`, tiling WMs use IPC |
| Switch desktop right | `Ctrl+Super+Right` | Same |
| Screenshot | `Print` | Mostly works but the screenshot tool is DE-specific |
| Close window | `Alt+F4` | Mostly universal, but GNOME and tiling WMs have different conventions |
| Calculator | `kcalc` | AppLaunch payload is KDE-only. Other DEs want `gnome-calculator`, `qalc`, etc. |

Out of scope (already portable or user-custom, no change needed):

- Kernel/compositor-honored XF86 keysyms: brightness, volume, mute, play/pause, media controls
- App-level conventions: back, forward, copy, cut, paste, undo, redo
- Device-local: DPI cycle, Shift wheel mode, Gestures, Middle click, Do nothing
- User escape hatch: Keyboard shortcut (raw user-typed)

## Architecture

### Core pieces

1. **`ActionPreset`** (Core / Domain, new type in `src/core/actions/`):
   `{ id: string, label: string, icon: string, category: string, variants: map<variantKey -> JSON variant object> }`.
   Static data loaded from a shipped `actions.json` resource. Each variant carries a DE-native hint shape (e.g. `{kglobalaccel: {component, name}}`, `{gsettings: {schema, key}}`, `{hyprctl: {dispatch, args}}`, `{app-launch: {binary}}`), not a raw keystroke.

2. **`ActionPresetRegistry`** (Core / Domain, new class in `src/core/actions/`):
   Parses `actions.json`, indexes presets by id. Exposes:
   - `preset(id) -> const ActionPreset*`
   - `supportedBy(id, variantKey) -> bool` (used by UI to filter the picker)
   - `allPresets() -> view of all presets` (for UI listing)
   - `variantData(id, variantKey) -> optional<JSON>` (used by DE impls to read their variant hint)

3. **`IDesktopIntegration::variantKey()`** (Core / Integration, new virtual method):
   Each impl returns a string key (`"kde"`, `"gnome"`, `"generic"`, ...). No `DesktopKind` enum. Adding a new DE is one impl + one JSON key, no core rebuild.

4. **`IDesktopIntegration::resolveNamedAction(id) -> std::optional<ButtonAction>`** (Core / Integration, new virtual method):
   Each impl turns a preset id into a concrete `ButtonAction` using its platform's mechanism. KDE returns `{DBus, kglobalaccel-spec}`. GNOME does the gsettings lookup (honoring the user's live binding) and returns `{Keystroke, resolved-combo}`. Generic returns `nullopt` for non-universal ids. `nullopt` means "not available on this DE".

5. **`ActionExecutor` unchanged**:
   `executeAction(ButtonAction)` stays the single fire point. Resolution happens upstream in the DE impl. All existing executor tests continue to pass without changes.

6. **`ButtonAction::PresetRef` — new type variant**:
   `ButtonAction::Type` gains a `PresetRef` value. `payload` carries the preset id. Old payload types (`Keystroke`, `DBus`, `AppLaunch`, `Media`, ...) remain unchanged. No lossy migration of existing profiles; old payloads keep working forever, new profiles use preset ids for semantic actions.

7. **Execution path for `PresetRef`**:
   At fire time, `ActionExecutor::executeAction` (or `ButtonActionDispatcher` upstream) sees `type == PresetRef`, calls `desktop.resolveNamedAction(payload)`, and either fires the returned concrete `ButtonAction` through the same switch or logs an error if resolution returned `nullopt`.

8. **Escape hatch**:
   Existing raw `Keystroke` / `DBus` / `AppLaunch` types stay. The UI labels them "Custom keystroke / Custom DBus call / Custom command" so users can still type raw payloads for anything not in the preset catalog.

### Live-binding validation

Some DEs (GNOME-family, XFCE, MATE, Cinnamon) have no binding-independent invoke. Their `resolveNamedAction` reads the user's current binding from gsettings/xfconf. If the user has cleared that binding, the lookup returns an empty string and `resolveNamedAction` returns `nullopt`.

UX treatment:

- At preset-list time, the UI calls `resolveNamedAction(id)` once per preset and greys out any that return `nullopt`, with a tooltip like "no binding set in GNOME Settings" (or equivalent for the DE).
- At fire time, if a previously-resolved preset suddenly returns `nullopt` (user cleared the binding between list-time and fire-time), the executor logs a warning and surfaces a user-visible notification rather than injecting anything.

### Support matrix, not "generic defaults"

Each preset declares which DE variants it supports by which keys appear under `variants`. The action picker calls `registry.supportedBy(id, desktop.variantKey())` and hides unsupported presets. There is no silent generic keystroke for actions that have no portable binding (Show Desktop, Task Switcher, Switch Desktop, ...).

For the tiny set of genuinely universal actions, the preset can carry a `generic` variant with a raw keystroke (for example media keys via XF86 keysyms). The v1 catalog does not include any semantic action with a generic keystroke fallback, because the seven actions in scope are not portable by keystroke.

### Per-DE invocation strategy — shipping with this issue

| DE / WM | variantKey | Mechanism |
|---|---|---|
| KDE Plasma | `kde` | `kglobalaccel` DBus: invoke by named action, binding-independent |
| GNOME / Budgie / Pantheon | `gnome` | `gsettings` lookup of user's current binding, then inject via uinput |
| Generic fallback | `generic` | Returns `nullopt` for actions with no portable binding; returns a raw `ButtonAction` for universal ones (none in v1) |

### Design pattern for follow-up DEs

New DE support is strictly additive under the same pattern: one new `IDesktopIntegration` impl, one new `variantKey()` string, a `resolveNamedAction()` method using whatever native mechanism fits, and new variant entries in `actions.json`. The following are candidate targets (each its own follow-up issue, not in this spec):

| DE / WM | variantKey | Likely mechanism |
|---|---|---|
| Deepin | `deepin` | `com.deepin.daemon.Keybinding` DBus |
| LXQt | `lxqt` | `lxqt-globalkeysd` DBus |
| Hyprland / Sway / i3 | `hyprland` / `sway` / `i3` | IPC dispatch by name |
| Wayfire | `wayfire` | IPC plugin dispatch by name |
| Cinnamon / MATE | `cinnamon` / `mate` | `gsettings` (`org.cinnamon.*`, `org.mate.*`) lookup + inject |
| XFCE | `xfce` | `xfconf-query` lookup + inject |

Closing this issue only lands KDE + GNOME + Generic. Every other entry is a separate follow-up issue.

### Example preset

```json
{
  "id": "show-desktop",
  "label": "Show Desktop",
  "icon": "desktop",
  "category": "workspace",
  "variants": {
    "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
    "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
  }
}
```

Does not appear in the picker on the generic fallback (or on DEs whose variant key has no entry) until someone contributes a variant.

### Data flow diagram

```
Profile (stored id)
      |
      v
ButtonAction{PresetRef, "show-desktop"}
      |
      v
ButtonActionDispatcher.handleButtonEvent()
      |
      v
ActionExecutor.executeAction()
      |  switch on type
      |  case PresetRef:
      v
IDesktopIntegration.resolveNamedAction("show-desktop")
      |  KDE:  ActionPresetRegistry.variantData("show-desktop", "kde") -> {kglobalaccel: {...}}
      |        returns ButtonAction{DBus, kglobalaccel-dbus-spec}
      |  GNOME: ActionPresetRegistry.variantData("show-desktop", "gnome") -> {gsettings: {...}}
      |         gsettings get ... -> "<Super>d" (or "" if unbound)
      |         returns ButtonAction{Keystroke, "Super+D"} (or nullopt)
      |  Generic: returns nullopt
      v
std::optional<ButtonAction>
      |
      v  (if set)
ActionExecutor.executeAction() recurses on the resolved ButtonAction
(executes as DBus or Keystroke exactly as today)
      |
      v  (if nullopt)
Log warning + user-visible error
```

### Seven presets to ship in v1

Based on the current `ActionModel` entries:

1. `show-desktop` — variants: kde (kwin Show Desktop), gnome (gsettings show-desktop)
2. `task-switcher` — variants: kde (kwin Expose), gnome (gsettings switch-applications)
3. `switch-desktop-left` — variants: kde (kwin Switch to Previous Desktop), gnome (gsettings switch-to-workspace-left)
4. `switch-desktop-right` — variants: kde (kwin Switch to Next Desktop), gnome (gsettings switch-to-workspace-right)
5. `screenshot` — variants: kde (kwin/spectacle ScreenShot2 DBus), gnome (gsettings screenshot)
6. `close-window` — variants: kde (kwin Window Close), gnome (gsettings close)
7. `calculator` — special-case app-launch variants: kde (app-launch kcalc), gnome (app-launch gnome-calculator), generic (app-launch qalc with fallbacks)

`ActionModel` migrates from hardcoded keystroke payloads to `ButtonAction::PresetRef` referencing these seven ids for the corresponding entries. The other action entries (Copy, Paste, media keys, device-local, etc.) stay as raw `Keystroke`/device-local types.

## File Structure

### New files

- `src/core/actions/ActionPreset.h` + `.cpp` — struct, parser
- `src/core/actions/ActionPresetRegistry.h` + `.cpp` — registry, lookup
- `src/core/actions/actions.json` — shipped data, compiled into binary via Qt resource
- `tests/core/actions/test_action_preset.cpp` — unit tests for preset parsing
- `tests/core/actions/test_action_preset_registry.cpp` — unit tests for registry queries
- `tests/core/desktop/test_kde_desktop_resolve.cpp` — unit tests for KDE resolver (DBus mocked)
- `tests/core/desktop/test_gnome_desktop_resolve.cpp` — unit tests for GNOME resolver (gsettings mocked)

### Modified files

- `src/core/ButtonAction.h` + `.cpp` — add `PresetRef` to `Type` enum, update `parse`/`serialize`
- `src/core/interfaces/IDesktopIntegration.h` — add `variantKey()` and `resolveNamedAction()` pure virtuals
- `src/core/desktop/KDeDesktop.h` + `.cpp` — implement the two new methods
- `src/core/desktop/GnomeDesktop.h` + `.cpp` — implement the two new methods
- `src/core/desktop/GenericDesktop.h` + `.cpp` — implement the two new methods
- `src/core/ActionExecutor.cpp` — handle `PresetRef` case by resolving + recursing
- `src/app/models/ActionModel.cpp` — change seven hardcoded entries to `PresetRef` payloads
- `src/core/CMakeLists.txt` — add new sources
- `tests/CMakeLists.txt` — add new tests
- Qt resource file — bundle `actions.json` into the binary

### Possibly modified

- `src/app/services/ButtonActionDispatcher.cpp` — may need to thread `IDesktopIntegration*` through to call `resolveNamedAction`, depending on which layer does the resolution (see Open Questions)

## Testing Strategy

Following the project's TDD convention:

1. **Preset parsing tests**: parse valid/invalid JSON, verify variant lookup by key, verify missing-variant handling.
2. **Registry tests**: loading from resource, id lookup, `supportedBy` returns correct bool per DE key.
3. **KDE resolver tests**: mock DBus-interface-factory and assert correct component + action name are looked up; verify `nullopt` returned when preset has no `kde` variant.
4. **GNOME resolver tests**: mock the gsettings reader; verify correct schema/key lookup, correct parse of returned `<Super>d` style strings into `Super+D` payloads, `nullopt` on empty result.
5. **Generic resolver tests**: verify `nullopt` for every non-universal id; verify raw keystroke pass-through for the (small) universal set if any are added.
6. **ButtonAction round-trip tests**: `PresetRef` serializes and parses back correctly.
7. **ActionExecutor tests**: existing tests continue to pass. New test: `executeAction({PresetRef, id})` calls `desktop.resolveNamedAction(id)` and recurses into the resolved `ButtonAction`; logs and returns without firing when `resolveNamedAction` returns `nullopt`.
8. **ActionModel tests**: verify the seven migrated entries have `actionType == "preset"` (or the chosen serialized string) with correct preset ids.
9. **Smoke test on real hardware**: MX Master 3S on the developer's KDE Plasma machine. Bind gesture-down to `show-desktop` preset, verify KDE DBus invocation fires KWin's Show Desktop action regardless of current Super+D binding.
10. **Live binding manual test**: on a GNOME VM or dev setup, clear the `show-desktop` gsettings binding, verify the action is greyed out in the picker; reset the binding, verify it becomes pickable and fires correctly.

## Open Questions

These are small and the plan will decide them. Noted here so the plan author knows they exist.

1. **Resolution call site**: resolve in `ActionExecutor::executeAction` (executor gets an `IDesktopIntegration*` dependency, simpler but adds a coupling) vs. resolve in `ButtonActionDispatcher` before calling executor (dispatcher already owns the desktop pointer, cleaner layering). Recommendation: resolve in dispatcher, pass the concrete `ButtonAction` to executor unchanged.

2. **DBus vs `QDBus` vs shell-out for kglobalaccel**: Qt's `QDBusInterface` is the obvious choice, matches existing DBus usage. Confirm no new system dependencies.

3. **gsettings parsing**: the returned values are GLib variant strings like `['<Super>d']` (array of keystroke bindings). Need a tiny parser to extract the first non-empty binding and convert to uinput combo format. This is not rocket science but the plan should spell out the transform.

4. **Qt resource vs shipped file**: baking `actions.json` into the binary via Qt resource makes the app self-contained and avoids yet another install path. Recommendation: Qt resource.

5. **`ActionModel` UI entry format**: today rows carry `actionType` + `payload` strings. Adding `PresetRef` means the UI needs to know what to show for a preset-backed row. Recommendation: keep the existing `actionType`/`payload` surface, add `actionType == "preset"` and put the preset id in `payload`. QML binds the same way.
