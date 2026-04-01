<p align="center">
  <img src="data/com.logitune.Logitune.svg" width="80">
  <h1 align="center">Logitune</h1>
  <p align="center">A Linux configurator for Logitech peripherals — per-application profiles, gesture mapping, thumb wheel modes, and a dark-themed Qt Quick UI matching Logitech Options+.</p>
</p>

<p align="center">
  <a href="https://github.com/mmaher88/logitune/actions/workflows/ci.yml"><img src="https://github.com/mmaher88/logitune/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License">
  <img src="https://img.shields.io/badge/Qt-6-green" alt="Qt">
  <img src="https://img.shields.io/badge/C%2B%2B-20-orange" alt="C++">
  <img src="https://img.shields.io/badge/device-MX%20Master%203S-teal" alt="Device">
</p>

<p align="center">
  <img src="docs/images/buttons-page.jpeg" width="800" alt="Logitune — Buttons Page">
</p>

## ✨ Features

- 🖱️ **Per-app profiles** — automatic button/scroll/DPI switching on window focus
- ⌨️ **Button remapping** — keystrokes, app launch, gestures, SmartShift toggle
- 🎛️ **Thumb wheel modes** — volume, zoom, horizontal scroll with invert control
- 👆 **Gesture support** — hold + swipe for desktop switching, task view, custom keystrokes
- ⚡ **DPI / SmartShift / Scroll** — full control with live preview
- 🔋 **System tray** — battery status, minimize to tray
- 📡 **HID++ 2.0** — direct communication via Bolt receiver, no daemon needed
- 🔄 **Disconnect/reconnect** — automatic re-enumeration and profile reapplication

## 📸 Screenshots

<table>
<tr>
<td width="50%">
<img src="docs/images/buttons-page.jpeg" alt="Buttons Page">
<p align="center"><em>Button remapping with callout cards</em></p>
</td>
<td width="50%">
<img src="docs/images/buttons-actions-panel.jpeg" alt="Actions Panel">
<p align="center"><em>Action selection panel</em></p>
</td>
</tr>
<tr>
<td>
<img src="docs/images/point-scroll-page.jpeg" alt="Point & Scroll">
<p align="center"><em>Scroll, thumb wheel & pointer speed</em></p>
</td>
<td>
<img src="docs/images/point-scroll-detail-panel.jpeg" alt="Scroll Settings">
<p align="center"><em>Scroll direction, SmartShift, smooth scrolling</em></p>
</td>
</tr>
<tr>
<td>
<img src="docs/images/settings-page.jpeg" alt="Settings">
<p align="center"><em>Device info, dark mode, debug logging</em></p>
</td>
<td>
<img src="docs/images/easy-switch-page.jpeg" alt="Easy-Switch">
<p align="center"><em>Easy-Switch channel management</em></p>
</td>
</tr>
</table>

## 🚀 Quick Start

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

## 📚 Documentation

| Guide | Description |
|-------|-------------|
| [🏁 Getting Started](https://github.com/mmaher88/logitune/wiki/Getting-Started) | Installation, permissions, UI overview |
| [🔨 Building](https://github.com/mmaher88/logitune/wiki/Building) | Prerequisites, build commands, Flatpak, devcontainer |
| [🏗️ Architecture](https://github.com/mmaher88/logitune/wiki/Architecture) | System design, signal flow, 14 Mermaid diagrams |
| [🖱️ Adding a Device](https://github.com/mmaher88/logitune/wiki/Adding-a-Device) | Step-by-step guide with code examples |
| [🖥️ Adding a Desktop Environment](https://github.com/mmaher88/logitune/wiki/Adding-a-Desktop-Environment) | Interface, GNOME scaffold, DE comparison |
| [🧪 Testing](https://github.com/mmaher88/logitune/wiki/Testing) | Philosophy, 4 test tiers, writing tests |
| [📡 HID++ Protocol](https://github.com/mmaher88/logitune/wiki/HID++-Protocol) | Report format, features, Bolt receiver, async matching |
| [🤝 Contributing](https://github.com/mmaher88/logitune/wiki/Contributing) | Workflow, code style, commit format |

## 🖱️ Supported Devices

| Device | Status |
|--------|--------|
| MX Master 3S | ✅ Full support |

## 🛠️ Tech Stack

C++20 · Qt 6 Quick · CMake · HID++ 2.0 · GTest · Flatpak

## 📄 License

GPL-3.0
