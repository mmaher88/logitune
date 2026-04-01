# Logitune

A Linux configurator for Logitech peripherals, starting with the MX Master 3S. Per-application profiles, gesture mapping, thumb wheel modes, and a dark-themed Qt Quick UI matching Logitech Options+.

[![CI](https://github.com/mmaher88/logitune/actions/workflows/ci.yml/badge.svg)](https://github.com/mmaher88/logitune/actions/workflows/ci.yml)

## Features

- **Per-app profiles** — automatic button/scroll/DPI switching on window focus
- **Button remapping** — keystrokes, app launch, gestures, SmartShift toggle
- **Thumb wheel modes** — volume, zoom, horizontal scroll with invert control
- **Gesture support** — hold + swipe for desktop switching, task view, custom keystrokes
- **DPI / SmartShift / Scroll** — full control with live preview
- **System tray** — battery status, minimize to tray
- **HID++ 2.0** — direct communication via Bolt receiver, no daemon needed
- **Disconnect/reconnect** — automatic re-enumeration and profile reapplication

## Quick Start

**Flatpak:**
```bash
flatpak install logitune.flatpak
flatpak run com.logitune.Logitune
```

**From source:**
```bash
make build
make test-all
make run
```

**GitHub Codespaces:** Click "Create codespace" — full dev environment in one click.

## Documentation

| Guide | Description |
|-------|-------------|
| [Getting Started](docs/wiki/Getting-Started.md) | Installation, permissions, UI overview |
| [Building](docs/wiki/Building.md) | Prerequisites, build commands, Flatpak, devcontainer |
| [Architecture](docs/wiki/Architecture.md) | System design, signal flow, 14 Mermaid diagrams |
| [Adding a Device](docs/wiki/Adding-a-Device.md) | Step-by-step guide with code examples |
| [Adding a Desktop Environment](docs/wiki/Adding-a-Desktop-Environment.md) | Interface, GNOME scaffold, DE comparison |
| [Testing](docs/wiki/Testing.md) | Philosophy, 4 test tiers, writing tests |
| [HID++ Protocol](docs/wiki/HID++-Protocol.md) | Report format, features, Bolt receiver, async matching |
| [Contributing](docs/wiki/Contributing.md) | Workflow, code style, commit format |

## Supported Devices

| Device | Status |
|--------|--------|
| MX Master 3S | Full support |

## Tech Stack

C++20 · Qt 6 Quick · CMake · HID++ 2.0 · GTest · Flatpak

## License

GPL-3.0
