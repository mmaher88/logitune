# AppIndicator Extension Detection Fix — Design

**Status:** approved, ready for implementation plan
**Issue:** #70 (Ubuntu default GNOME: recommend AppIndicator + fix detection banner)
**Target release:** next beta
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-20

## Summary

Three small fixes for the AppIndicator GNOME extension story:

1. `GnomeDesktop::appIndicatorStatus` wrongly reports
   `AppIndicatorDisabled` when the extension is not installed at all.
   Root cause: empty DBus reply dict parses as `state=0`, which falls
   into the disabled branch.
2. The banner's "not installed" remediation hard-codes
   `sudo pacman -S ...` regardless of distro. Broken on Ubuntu,
   Fedora, etc.
3. The deb does not pull the extension package automatically on
   Ubuntu, so users who `apt install logitune` hit the banner on
   every login.

## Root cause (bug 1)

`src/core/desktop/GnomeDesktop.cpp` lines 78-95 call
`org.gnome.Shell.Extensions.GetExtensionInfo(uuid)` with a 2-second
timeout. The code checks only whether the reply type is a successful
`ReplyMessage`.

GNOME Shell returns an empty `a{sv}` dict on success when the
requested uuid does not exist. That is still a `ReplyMessage`, so the
current guard misses it. The empty map goes to the `else` branch,
`info.value("state").toDouble()` returns `0.0` (no such key), and the
code maps `state != 1.0` to `AppIndicatorDisabled`.

Net effect: every system without the extension installed reports
"installed but not enabled", and the banner tells the user to run
`gnome-extensions enable ...` which fails with "Extension does not
exist".

## Fixes

### 1. Detection

Refactor the existing if/else so the "not installed" branch covers both
the error-reply case AND the successful-but-empty-dict case. Do not
read `info` before confirming the reply is a `ReplyMessage` and has at
least one argument, to avoid UB if the reply is an error without a
body:

```cpp
bool installed = false;
QVariantMap info;
if (infoReply.type() == QDBusMessage::ReplyMessage && !infoReply.arguments().isEmpty()) {
    info = qdbus_cast<QVariantMap>(infoReply.arguments().first());
    installed = !info.isEmpty();
}

if (!installed) {
    m_appIndicatorStatus = AppIndicatorNotInstalled;
    qCWarning(lcFocus) << "AppIndicator extension not installed -- "
                          "tray icon will not be visible. Install: "
                          "gnome-shell-extension-appindicator";
} else {
    double state = info.value("state").toDouble();
    if (state == 1.0) {
        m_appIndicatorStatus = AppIndicatorActive;
        qCInfo(lcFocus) << "AppIndicator extension active";
    } else {
        m_appIndicatorStatus = AppIndicatorDisabled;
        qCWarning(lcFocus) << "AppIndicator extension installed but not enabled "
                              "(state:" << state << ") -- run: gnome-extensions "
                              "enable" << kAppIndicatorUuid;
    }
}
```

`info.isEmpty()` covers both "dict has no entries" and "dict missing
uuid key", both of which indicate the extension is not on the system.

### 2. Distro-aware banner copy

Add a distro detector that parses `/etc/os-release` and maps the `ID`
(and `ID_LIKE` fallback) to one of four families: `arch`, `debian`,
`fedora`, `other`. Expose via a new `DeviceModel` getter so QML can
render the right install command.

```
arch family   -> sudo pacman -S gnome-shell-extension-appindicator
debian family -> sudo apt install gnome-shell-extension-appindicator
fedora family -> sudo dnf install gnome-shell-extension-appindicator
other         -> "Install gnome-shell-extension-appindicator via your
                 package manager."
```

QML `HomeView.qml` banner reads the new getter instead of hard-coding
pacman.

### 3. Deb `Recommends`

Add `Recommends: gnome-shell-extension-appindicator` to the deb
control in `scripts/package-deb.sh`. Debian/Ubuntu install recommended
packages by default, so `apt install logitune` pulls the extension
automatically. Users opt out with `--no-install-recommends`.

No change to the RPM spec or pacman PKGBUILD: those package managers
do not have an equivalent "opt-in-by-default recommendation" that fits
this use case, and the same distros do not consistently package the
extension under a stable name anyway.

## Code surface

### `src/core/desktop/GnomeDesktop.cpp`

Patch the detection block as above. No header change (enum already
includes `AppIndicatorNotInstalled`).

### `src/core/util/DistroDetector.{h,cpp}` (new)

Small utility under `src/core/util/`:

```cpp
namespace logitune::util {

enum class DistroFamily { Unknown, Arch, Debian, Fedora };

// Parses /etc/os-release once per process. Returns Unknown when
// the file is missing or malformed.
DistroFamily detectDistroFamily();

} // namespace logitune::util
```

Implementation reads `/etc/os-release`, extracts `ID` and `ID_LIKE`
(space-separated list of additional identities), and matches:

- `ID=arch` or `ID_LIKE` contains `arch` -> `Arch`.
- `ID=debian`, `ID=ubuntu`, or `ID_LIKE` contains `debian` or `ubuntu` -> `Debian`.
- `ID=fedora`, `ID=rhel`, `ID=rocky`, `ID=almalinux`, or `ID_LIKE` contains `fedora` or `rhel` -> `Fedora`.
- Otherwise `Unknown`.

Cached in a function-local static so repeated calls are free.

### `src/app/models/DeviceModel.{h,cpp}`

Add:

```cpp
Q_INVOKABLE QString appIndicatorInstallCommand() const;
```

Returns the apt/pacman/dnf command matching the detected family, or a
generic sentence for `Unknown`. QML reads via
`DeviceModel.appIndicatorInstallCommand()` instead of the current
hard-coded string.

### `src/app/qml/HomeView.qml`

Banner's "not installed" text pulls from
`DeviceModel.appIndicatorInstallCommand()`. The "disabled" text stays
as is (the `gnome-extensions enable ...` command is distro-agnostic).

### `scripts/package-deb.sh`

Append `Recommends:` line to the Debian control template:

```
Recommends: gnome-shell-extension-appindicator
```

## Tests

- `tests/test_distro_detector.cpp` (new): parameterized over a few
  synthetic `/etc/os-release` contents via a temp-file helper. Asserts
  the family mapping for Arch, Ubuntu, Debian, Fedora, and Unknown.
- `tests/test_device_model.cpp`: one test that asserts
  `appIndicatorInstallCommand()` returns a non-empty string. The exact
  command depends on the machine running the tests, so do not assert
  a specific substring. Just assert non-empty and that the word
  "gnome-shell-extension-appindicator" appears.
- No GTest for `GnomeDesktop::appIndicatorStatus` itself. DBus mocking
  is out of proportion for the fix. Manual verification on two VMs:
  Ubuntu 24.04 without the extension reports "not installed"; CachyOS
  with the extension enabled reports "active".

## Rollout

Branch `fix-appindicator-detection`. Four commits:

1. `feat(util): DistroDetector for /etc/os-release family mapping`.
   New file + tests.
2. `feat(device-model): appIndicatorInstallCommand per distro family`.
   DeviceModel getter wired to DistroDetector. QML banner reads it.
3. `fix(gnome): treat empty GetExtensionInfo as not-installed`.
   One-line guard in GnomeDesktop.cpp + manual VM verification.
4. `build(deb): Recommends gnome-shell-extension-appindicator`.
   One-line add in package-deb.sh.

Ships in the next beta release alongside whatever else is in flight.

## Known risks

- **`ID_LIKE` variance.** Not every distro sets `ID_LIKE`; Pop!_OS
  sets `ID_LIKE=debian`, CachyOS sets `ID_LIKE=arch`, etc. The
  mapping above should cover the common cases; unknown distros fall
  through to the generic hint. Low risk.
- **`appIndicatorInstallCommand()` called before DeviceModel is
  instantiated**. QML reads the getter at render time; DeviceModel
  exists before QML loads. No race.

## Out of scope

- Auto-enabling the extension via DBus after installation.
- Offering to install the extension via the app (one-click install).
- Changing the extension UUID we depend on.
- pacman/dnf-package Recommends-equivalent.
