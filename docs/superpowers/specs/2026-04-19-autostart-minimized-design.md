# Autostart Minimized Design

**Status:** approved, ready for implementation plan
**Issue:** #10 (Feature request: Option to automatically start the app)
**Target release:** next beta after `v0.3.1-beta.1`
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-19

## Summary

Ship Logitune so every user's session autostarts it minimized to the
system tray on login. No in-app UI toggle in this pass. Users who want
to opt out use the XDG-standard user-level override.

## Motivation

Issue #10 asks for an autostart option. Today the package installs a
`.desktop` file with `X-GNOME-Autostart-enabled=true`, but to the wrong
path (`/usr/etc/xdg/autostart/`), so no desktop environment picks it up.
Users have to configure autostart manually via their DE, which is the
friction the issue is asking us to remove.

"Plain and simple" per the maintainer: the app should autostart
minimized when the user logs in. No first-run prompts, no Settings-page
toggle in this PR.

## Behavior

- Login to any XDG-compliant desktop environment (GNOME, KDE Plasma,
  XFCE, LXDE, Mate, Cinnamon).
- `/etc/xdg/autostart/logitune.desktop` is read by the session
  autostart mechanism.
- `Exec=logitune --minimized` launches the app.
- Tray icon appears. Main window does NOT appear.
- User clicks tray icon, main window shows (existing
  `TrayManager::showWindowRequested` flow).
- User closes main window, it hides to tray (existing
  `setQuitOnLastWindowClosed(false)` behavior).
- User selects Quit from tray menu, app exits. Next login the session
  autostarts Logitune again.

**Manual launch unchanged.** Running `logitune` from a terminal or app
launcher (no `--minimized` flag) shows the main window as today.

**No-tray fallback.** If `QSystemTrayIcon::isSystemTrayAvailable()`
returns false, `--minimized` degrades to a no-op and the window is
shown. Prevents the app from running invisibly with no recovery path
on sessions that lack a system tray.

**Opt-out.** Users who do not want autostart drop a user-level
override at `~/.config/autostart/logitune.desktop` with `Hidden=true`
or `X-GNOME-Autostart-enabled=false`. Standard XDG autostart semantics.
Documented in release notes; not implemented as an in-app toggle in
this PR.

## Approach

Three alternatives considered:

1. **XDG autostart `.desktop` + `--minimized` CLI flag** (chosen).
   Universal across every XDG-compliant DE. One file, one flag. No
   per-DE special-casing.
2. **systemd user unit.** Desktop-environment-agnostic but requires
   `systemctl --user enable`, per-distro packaging, and loses
   support on distros without user-mode systemd.
3. **Per-DE native autostart APIs.** GNOME has its own, KDE has its
   own. Fragments the implementation surface across DEs for zero
   practical benefit over XDG.

XDG autostart is the standard mechanism every supported DE reads.
Going with it.

## Code surface

### `src/app/main.cpp`

Add a `QCommandLineOption` next to the existing `--simulate-all` and
`--edit` options:

```cpp
QCommandLineOption minimizedOption(
    QStringLiteral("minimized"),
    QStringLiteral("Start hidden to the system tray. Intended for autostart "
                   "launchers; ignored if no system tray is available."));
parser.addOption(minimizedOption);
```

After `parser.process(app)`:

```cpp
const bool startMinimized = parser.isSet(minimizedOption);
```

After QML load (near the existing theme-application block around
line 229), if `startMinimized && QSystemTrayIcon::isSystemTrayAvailable()`
is true, iterate `engine.rootObjects()` and call `hide()` on each
`QQuickWindow`. Otherwise leave the window visible.

Log one line for observability: "Startup: minimized to tray" or
"Startup: showing window".

### `src/app/qml/Main.qml`

No change. The root window keeps `visible: true`; the C++ layer hides
it after load when `--minimized` is set. Rationale: a QML-level
`visible: false` would introduce a context-property dependency purely
for startup and make the default manual launch marginally slower
(briefly invisible, then shown). Hiding at the C++ layer is one line
and keeps Main.qml declarative defaults.

### `src/app/TrayManager.{cpp,h}`

No change. Existing `showWindowRequested` -> `window->show()` chain
already restores the window from the tray icon click.

### `data/logitune.desktop`

Change one line:

```
- Exec=logitune
+ Exec=logitune --minimized
```

Keep `X-GNOME-Autostart-enabled=true` and the other entries as-is. The
file is installed twice (launcher + autostart); the launcher install
also gets the `--minimized` flag, which is wrong for manual app-launcher
invocations since those should show the window. Handle this either:

Option A: **Two separate files**, `data/logitune.desktop` (no flag,
for `share/applications`) and `data/logitune-autostart.desktop`
(with `--minimized`, for `/etc/xdg/autostart`). Cleanest.

Option B: **Single file with `--minimized`**, accept that the app
launcher also runs the app minimized. Not ideal: clicking "Logitune"
from the DE's launcher would do nothing visible except the tray icon
on first invocation (and nothing at all on subsequent invocations
because the single-instance lock prevents a second run).

Going with **Option A**. One extra file in `data/`, one extra
`install(FILES ...)` line in CMakeLists.txt, no runtime branching on
how the app was invoked.

### `CMakeLists.txt`

Current (around lines 27-28):

```cmake
install(FILES data/logitune.desktop DESTINATION share/applications)
install(FILES data/logitune.desktop DESTINATION etc/xdg/autostart)
```

After:

```cmake
install(FILES data/logitune.desktop DESTINATION share/applications)
install(FILES data/logitune-autostart.desktop
        DESTINATION /etc/xdg/autostart
        RENAME logitune.desktop)
```

Two edits:

1. The autostart line uses a distinct source file
   (`logitune-autostart.desktop`) so the `--minimized` flag does not
   leak into the launcher entry.
2. The destination becomes an absolute path `/etc/xdg/autostart`. The
   previous relative path combined with `CMAKE_INSTALL_PREFIX=/usr`
   produced `/usr/etc/xdg/autostart/`, which no XDG-compliant desktop
   environment reads. `RENAME` keeps the filename `logitune.desktop`
   as XDG expects.

## Packaging scripts

- `scripts/package-deb.sh`, `scripts/package-rpm.sh`,
  `scripts/package-arch.sh`: no source edits. Each script runs
  `cmake --install`, which respects the new destinations. The
  resulting packages will ship `/etc/xdg/autostart/logitune.desktop`
  (corrected path) with the minimized Exec.

## Tests

GTest coverage is minimal here because the feature is mostly config +
a single CLI flag wired to one window call.

- `tests/test_autostart_desktop.cpp` (new, small): read
  `data/logitune-autostart.desktop`, assert:
  - Contains `Exec=logitune --minimized`.
  - Contains `X-GNOME-Autostart-enabled=true`.
  - Name and Icon match the launcher entry.
- No test for the C++ hide-window path. Manual verification in the
  rollout step. Adding a GTest that instantiates QApplication + QML
  engine + system tray is heavy for one branch.

## Rollout

Branch `feat-autostart-minimized`. Three commits:

1. `build: fix autostart desktop entry install path` — CMakeLists edit
   + new `data/logitune-autostart.desktop`. No runtime change; just
   ships the file to the right place.
2. `feat(desktop): autostart invocation adds --minimized flag` —
   update `data/logitune-autostart.desktop` Exec line. Still a no-op
   until main.cpp supports the flag.
3. `feat(app): --minimized CLI flag hides window to tray` — add the
   option, wire hide-on-startup with tray-available fallback, log
   line. New test.

Lands in the next beta release. Release notes include:

- One-line feature callout: "Logitune now autostarts minimized to the
  system tray on login. Opt out by placing
  `~/.config/autostart/logitune.desktop` with
  `Hidden=true`."

## Known risks

- **Session without a system tray.** Some Wayland compositors or
  stripped-down sessions do not expose
  `QSystemTrayIcon::isSystemTrayAvailable() == true`. In that case
  `--minimized` becomes a no-op and the window appears. User sees a
  window on login they did not ask for. Mitigation: log it, and
  accept the degraded behavior as preferable to "invisible with no
  recovery path".
- **Opt-out discoverability.** Users who dislike the autostart have
  to know about the `~/.config/autostart/logitune.desktop` override.
  The release note covers it; a future PR can add a UI toggle in
  Settings if feedback warrants.
- **Manual launch race with single-instance lock.** If autostart
  already launched Logitune and the user manually clicks the app
  launcher, the second invocation quits immediately due to the
  existing lock file. Same behavior as today for simulate-all /
  normal launches; nothing changes here.

## Out of scope

- In-app "Launch at login" toggle (opt-out stays at the XDG file
  level).
- "Start minimized" user preference (distinct from the autostart flag
  — today's manual launch always shows the window; no toggle to
  change that).
- systemd user unit packaging.
- Per-DE native autostart mechanisms.
- Changes to tray-close-vs-quit semantics.
