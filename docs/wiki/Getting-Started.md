# Getting Started

## Installation

Download the latest package for your distribution from the [Releases](https://github.com/logitune/logitune/releases) page.

### Ubuntu / Debian

```bash
sudo apt install ./logitune-VERSION_amd64.deb
```

### Fedora

```bash
sudo dnf install logitune-VERSION.rpm
```

### Arch Linux (AUR)

```bash
makepkg -si
```

Or using an AUR helper:

```bash
yay -S logitune
```

Native packages automatically install udev rules and set device permissions — no manual `udevadm` steps needed.

### From Source

See [Building](Building) for full instructions. The short version:

```bash
git clone https://github.com/logitune/logitune.git
cd logitune
make build
make install
```

## Device support status

Every device descriptor carries a `status` field with one of two values:

- **`verified`** (green check badge): the maintainer or a trusted
  contributor owns the device and has confirmed it works end-to-end
  on real hardware. All ship-bundled descriptors currently ship as
  verified.
- **`beta`** (amber badge): community-submitted or shipped without
  hardware verification. Core HID++ functionality almost always
  works, but hotspot positions, slot-circle positions, and device
  images may need polish. Report issues or open a PR via the
  [editor mode](Editor-Mode) workflow.

When you contribute a new descriptor, set `"status": "beta"` in
`descriptor.json` unless you have hardware-verified it and a
maintainer agrees to promote it to `verified`.

## First Run

### udev Rules

Logitune needs read/write access to hidraw devices and uinput for keystroke injection. The udev rules file (`data/71-logitune.rules`) contains:

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="046d", TAG+="uaccess"
KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess"
```

**If you installed a native package** (`.deb`, `.rpm`, or AUR), the rules are installed and activated automatically — no manual steps needed.

**If you installed from source** (`make install`), the rules are also installed automatically.

**If you built without installing**, install the rules manually:

```bash
sudo cp data/71-logitune.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

The `uaccess` tag grants access to the currently logged-in user without requiring `root` or group membership. After installing the rules, unplug and replug your device (or reboot).

### Permissions Check

If Logitune starts but shows "No device connected":

1. **Check hidraw access**: `ls -la /dev/hidraw*` — your user should have `rw` access to Logitech devices
2. **Check uinput access**: `ls -la /dev/uinput` — needed for keystroke injection
3. **Check the device is detected**: Run with `--debug` flag to see device scanning logs

```bash
logitune --debug
# or, if running from the build directory:
./build/src/app/logitune --debug
```

### Single-Instance Guard

Logitune uses a lock file at `$TMPDIR/logitune.lock` to prevent two instances from fighting over the device. If a previous instance crashed without cleanup, delete the lock file manually:

```bash
rm /tmp/logitune.lock
```

## UI Overview

The application window is organized into four main areas:

### Sidebar Navigation

A vertical navigation bar on the left with icons for each page:

| Icon | Page | Description |
|------|------|-------------|
| Mouse | **Point & Scroll** | DPI slider, SmartShift toggle/threshold, hi-res scroll, natural scrolling |
| Grid | **Buttons** | Interactive device render with hotspot callouts for each button, plus action picker panel |
| Switch | **Easy-Switch** | View paired hosts, active slot |
| Gear | **Settings** | Debug logging toggle, bug report button, about info |

### Device Render

The Buttons page shows a rendered image of the device (front view by default, side view available). Interactive hotspots are overlaid at positions defined by the device descriptor — clicking a hotspot opens the action picker for that button.

### Profile Bar

A horizontal tab bar at the bottom of the window:

- **Default** tab is always present — this profile is used for applications without a specific binding
- **App profiles** appear as additional tabs with the application's icon
- The **hardware-active** profile (the one currently applied to the device) is indicated with a highlight
- The **display** profile (the one you're currently viewing/editing) may differ from the hardware-active profile
- Click the **+** button to add a new app profile from the list of installed applications

### Settings Storage

Profiles are stored per-device under:

```
~/.config/Logitune/devices/<device-serial>/profiles/
```

Each profile is a `.conf` file (QSettings INI format). App bindings are stored in `app-bindings.conf` in the same directory.

## Tray Icon

Logitune runs as a tray application — closing the window hides it to the tray rather than quitting. The tray menu shows:

- **Battery level** — e.g., "Battery: 85%"
- **Show** — brings the window back
- **Quit** — exits the application

The application sets `quitOnLastWindowClosed(false)` so the tray icon keeps the event loop alive.

## Command-Line Options

| Flag | Description |
|------|-------------|
| `--debug` | Enable debug logging to file and console at startup |

Debug logging can also be toggled at runtime from the Settings page.

## What Happens at Startup

1. **Single-instance check** — attempts to acquire `logitune.lock`
2. **Log manager init** — sets up Qt logging categories, optional file output
3. **Crash recovery** — checks for previous unclean shutdown, offers to file a bug report
4. **AppController init** — creates DeviceManager, ProfileEngine, models, wires signals
5. **QML engine load** — registers model singletons, loads `Main.qml`
6. **Start monitoring** — DeviceManager begins udev scanning and KDE desktop starts focus tracking
7. **Device connect** — on first hidraw match, enumerates HID++ features, reads state, creates command queue
8. **Profile load** — loads or seeds the default profile, applies settings to hardware
9. **Tray icon** — system tray icon appears with battery info
