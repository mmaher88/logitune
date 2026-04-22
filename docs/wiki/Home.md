# Logitune

[![CI](https://github.com/mmaher88/logitune/actions/workflows/ci.yml/badge.svg)](https://github.com/mmaher88/logitune/actions/workflows/ci.yml)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](https://github.com/mmaher88/logitune/blob/master/LICENSE)
[![Release](https://img.shields.io/github/v/release/mmaher88/logitune?include_prereleases)](https://github.com/mmaher88/logitune/releases)

**Logitune** is a native Linux configurator for Logitech HID++ peripherals, starting with the MX Master 3S. It communicates directly with the device over hidraw using the HID++ 2.0 protocol — no Solaar, no logiops, no daemon. Just a Qt 6 / QML desktop application with a system tray icon.

## What It Does

- **Per-application profiles** — DPI, SmartShift, scroll, button assignments, and gestures switch automatically when you change window focus
- **Button remapping** — Assign keystrokes, app launches, gesture triggers, or SmartShift toggle to any configurable button
- **Gesture system** — Hold the gesture button and swipe up/down/left/right or click for five independent actions
- **Thumb wheel modes** — Horizontal scroll, zoom (Ctrl+scroll), or volume control
- **SmartShift control** — Toggle ratchet/freespin mode and adjust the auto-disengage threshold
- **Hi-res scroll** — Toggle high-resolution scrolling and natural (inverted) direction
- **DPI slider** — 200 to 8000 DPI in 50-step increments
- **Battery monitoring** — Real-time level and charging status in the system tray
- **Easy-Switch** — View paired hosts and active slot
- **Transport failover** — Seamlessly switch between Bolt receiver and Bluetooth connections
- **Crash reporting** — Built-in crash handler with one-click GitHub issue filing

The UI is modeled after Logitech's Options+ application — a sidebar navigation with pages for Point & Scroll, Buttons, Easy-Switch, and Settings, plus a profile bar at the bottom.

## Quick Links

| Page | Description |
|------|-------------|
| [Getting Started](Getting-Started) | Installation, first run, permissions, UI overview |
| [Building](Building) | Build from source, native packages, devcontainer, CI |
| [Architecture](Architecture) | System design, signal flow, MVVM, HID++ protocol layer |
| [Adding a Device](Adding-a-Device) | Step-by-step JSON descriptor workflow with a worked MX Master 3S example |
| [Editor Mode](Editor-Mode) | Visual editor for existing device descriptors: positions hotspots, slot circles, image uploads |
| [Adding a Desktop Environment](Adding-a-Desktop-Environment) | How to add GNOME, Hyprland, or other DE support |
| [Testing](Testing) | Test philosophy, tiers, infrastructure, how to write tests |
| [Contributing](Contributing) | Fork workflow, code style, commit format, PR checklist |
| [HID++ Protocol](HID++-Protocol) | Deep dive into HID++ 2.0, feature discovery, report format |

## Project Status

Logitune supports nine Logitech mice today — see the [Supported Devices](#supported-devices) table. The architecture is designed to make adding new devices straightforward: most contributions are a single JSON descriptor. See [Adding a Device](Adding-a-Device).

### Supported Devices

| Device | PID | Connection | Status |
|--------|-----|------------|--------|
| MX Master 2S                | `0xb019`          | Bolt / Bluetooth | ✅ Verified |
| MX Master 3S                | `0xb034`          | Bolt / Bluetooth | ✅ Verified |
| MX Master 4                 | `0xb042`          | Bolt / Bluetooth | ✅ Verified |
| MX Anywhere 3               | `0xb025`, `0x4090`| Bolt / Bluetooth | 🧪 Beta |
| MX Anywhere 3 for Business  | `0xb02d`          | Bolt / Bluetooth | 🧪 Beta |
| MX Anywhere 3S              | `0xb037`          | Bolt / Bluetooth | 🧪 Beta |
| MX Anywhere 3S for Business | `0xb038`          | Bolt / Bluetooth | 🧪 Beta |
| MX Vertical                 | `0xb020`, `0x407b`| Bolt / Bluetooth | 🧪 Beta |
| MX Vertical for Business    | `0xb020`          | Bolt / Bluetooth | 🧪 Beta |

The full per-feature table (Battery, DPI, SmartShift, Thumb wheel, Button remap, Gestures, Smooth scroll, Easy-Switch) lives in the [README](https://github.com/mmaher88/logitune#supported-devices) and is auto-generated from `devices/*/descriptor.json`.

### Supported Desktop Environments

| DE | Compositor | Focus Tracking | Status |
|----|-----------|----------------|--------|
| KDE Plasma 6        | KWin   | KWin script via D-Bus + polling fallback (KWin 6 quirk) | Fully supported |
| GNOME 42+ (Wayland) | Mutter | GNOME Shell extension (auto-installed on first launch) via D-Bus | Fully supported |
| Generic (other DEs) | Any    | Polling fallback                                        | Basic support — device configuration works; per-app profile switching does not |
