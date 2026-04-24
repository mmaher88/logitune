# Architecture

Logitune is a Qt 6 / QML application that communicates with Logitech HID++ 2.0 devices through the Linux hidraw subsystem. This page documents the system design, signal flow, protocol layer, and key architectural decisions.

## System Overview

At a glance, one button press on the mouse turns into one row update in the QML UI. Each layer has one job, and `AppRoot` is a thin composition root that wires them together (not a place where behavior lives):

<img src="diagrams/system-overview.png" alt="Logitune system overview: QML UI on top, then the app library band showing AppRoot, the four services (DeviceSelection, DeviceCommands, ButtonActionDispatcher, ProfileOrchestrator), and Models + TrayManager; below that the core library with three sub-bands (integration, aggregation, protocol), then Linux kernel and hardware at the bottom" width="800"/>

> Source: `docs/wiki/diagrams/system-overview.svg`. Re-render with `rsvg-convert -w 1600 -h 1240 docs/wiki/diagrams/system-overview.svg -o docs/wiki/diagrams/system-overview.png` after edits.

The app-lib band contains AppRoot plus four focused services (`DeviceSelection`, `DeviceCommands`, `ButtonActionDispatcher`, `ProfileOrchestrator`) that sit between the QML UI / ViewModels and the core library's engines + HID++ stack. Each layer below has its own detailed diagram elsewhere on this page:

| Layer | Detail |
|---|---|
| Core — HID++ stack | [HID++ protocol stack](#stack), [feature discovery](#feature-discovery), [command queue](#command-queue), [async matching](#async-response-matching) |
| Core — Desktop integration | [Interface hierarchy](#interface-hierarchy), [KDE focus tracking](#kde-focus-tracking) |
| Core — Device lifecycle | [PhysicalDevice transport aggregation](#physicaldevice-transport-aggregation), [Discovery flow](#discovery-flow), [Disconnect and reconnect](#disconnect-and-reconnect) |
| App — Services | [Services](#services) |
| App — Models | [MVVM pattern](#mvvm-pattern), [Model roles](#model-roles), [Model registration](#model-registration) |
| App — Composition root | [AppRoot wiring](#approot-wiring) |
| Cross-cutting flow | [Window focus → profile switch → hardware commands](#window-focus-change---profile-switch---hardware-commands) |

### Two Static Libraries

The project is split into two static libraries:

| Library | Contents | Dependencies |
|---------|----------|-------------|
| `logitune-core` | DeviceManager, `PhysicalDevice`, `DeviceSession`, HID++ protocol + capability dispatch, ProfileEngine, ActionExecutor, `DeviceRegistry`, `JsonDevice`, `DescriptorWriter`, `LinuxDesktopBase` + KDE/GNOME/Generic implementations, input injection, logging | Qt6::Core, Qt6::DBus, libudev |
| `logitune-app-lib` | AppRoot, `EditorModel`, models (DeviceModel, ButtonModel, ActionModel, `ActionFilterModel`, ProfileModel, `SettingsModel`), TrayManager, QML module, dialogs | logitune-core, Qt6::Quick, Qt6::Widgets |

This split allows tests to link against `logitune-core` and `logitune-app-lib` without pulling in the executable's `main()`.

## Signal Flow

### Window Focus Change -> Profile Switch -> Hardware Commands

This is the central flow of the application. When the user switches to a different window, the active profile changes and hardware settings are updated.

```mermaid
sequenceDiagram
    participant KWin as KWin Script
    participant KDE as KDeDesktop
    participant AR as AppRoot
    participant PO as ProfileOrchestrator
    participant DS as DeviceSelection
    participant PE as ProfileEngine
    participant DM as DeviceModel
    participant DSess as DeviceSession
    participant CQ as CommandQueue
    participant FD as FeatureDispatcher
    participant TR as Transport
    participant PM as ProfileModel
    participant BM as ButtonModel

    KWin->>KDE: callDBus focusChanged(resourceClass, title, desktopFileName)
    KDE->>KDE: resolveDesktopFile(resourceClass)
    KDE->>AR: activeWindowChanged(appId, title)

    Note over AR: wireSignals routes the signal to ProfileOrchestrator
    AR->>PO: onWindowFocusChanged(wmClass, title)

    Note over PO: Skip shell components (plasmashell, krunner, gnome-shell)

    PO->>DM: setActiveWmClass(wmClass)
    PO->>DS: activeSerial()
    DS-->>PO: serial
    PO->>PE: profileForApp(serial, wmClass)
    PE-->>PO: profileName (or "default")

    Note over PO: Skip if profileName matches hardware profile

    PO->>PE: cachedProfile(serial, profileName)
    PO->>PE: setHardwareProfile(serial, profileName)
    PO->>PO: applyProfileToHardware(profile)

    PO->>DS: activeSession()
    DS-->>PO: DeviceSession* (active transport)

    Note over PO: applyProfileToHardware calls the session directly. DeviceCommands is only used for user UI changes.

    par Apply all settings on the selected session
        PO->>DSess: setDPI(value)
        DSess->>CQ: enqueue(AdjustableDPI, setSensorDpi, params)
    and
        PO->>DSess: setSmartShift(enabled, threshold)
        DSess->>CQ: enqueue(SmartShift, setRatchetControl, params)
    and
        PO->>DSess: setScrollConfig(hiRes, invert)
        DSess->>CQ: enqueue(HiResWheel, setWheelMode, params)
    and
        PO->>DSess: setThumbWheelMode(mode, invert)
        DSess->>CQ: enqueue(ThumbWheel, setThumbwheelReporting, params)
    and
        PO->>DSess: divertButton(CID, divert, rawXY) (per button)
        DSess->>CQ: enqueue(ReprogControlsV4, setCidReporting, params)
    end

    loop For each enqueued command (FIFO, paced)
        CQ->>FD: callAsync(feature, functionId, params)
        FD->>TR: sendRequestAsync(report)
        TR->>TR: write to hidraw fd
        Note over CQ: Wait 10ms before next command
    end

    PO-->>AR: profileApplied(serial) (signal)
    Note over AR: Wired to ButtonActionDispatcher.onProfileApplied (resets thumbAccum)

    PO->>PM: setHwActiveByProfileName(profileName)
    Note over BM: ButtonModel is refreshed via onDisplayProfileChanged when the display profile tracks the applied hardware profile.
```

### Key Design Decision: Display vs Hardware Profile

The ProfileEngine maintains two independent profile pointers:

- **displayProfile** — the profile the user is currently viewing/editing in the UI
- **hardwareProfile** — the profile currently applied to the device hardware

These can differ. When the user clicks a profile tab, only the display profile changes (UI updates, no hardware writes). When the focused window changes, the hardware profile changes (hardware writes, and if the user was viewing a different tab, the UI stays on that tab).

This prevents accidental hardware writes when the user is just browsing profiles.

## HID++ Protocol Layer

### Stack

```mermaid
graph TB
    subgraph "Application"
        DevMgr[DeviceManager]
    end

    subgraph "Protocol"
        CQ[CommandQueue<br/>10ms pacing, retry]
        FD[FeatureDispatcher<br/>Feature table, callAsync, softwareId]
        TR[Transport<br/>send/receive, timeout, error handling]
        HR[HidrawDevice<br/>open, read, write, poll]
    end

    subgraph "Kernel"
        hidraw[hidraw fd]
    end

    DevMgr --> CQ
    DevMgr --> FD
    CQ --> FD
    FD --> TR
    TR --> HR
    HR --> hidraw
```

### Feature Discovery

On device connect, `FeatureDispatcher::enumerate()` queries the Root feature (0x0000) to build a feature index table:

```mermaid
sequenceDiagram
    participant FD as FeatureDispatcher
    participant TR as Transport
    participant Dev as Device

    loop For each known FeatureId
        FD->>TR: send Root.getFeatureID(featureId)
        TR->>Dev: HID++ report
        Dev-->>TR: response with featureIndex
        TR-->>FD: Report
        Note over FD: Store featureId -> featureIndex mapping
    end
```

The feature table maps `FeatureId` enums to device-assigned 8-bit indices. For example, `FeatureId::AdjustableDPI (0x2201)` might map to index `0x07` on one device and `0x09` on another. All subsequent calls use the resolved index.

Known features (from `HidppTypes.h`):

| Feature | ID | Description |
|---------|-----|-------------|
| Root | `0x0000` | Feature discovery |
| FeatureSet | `0x0001` | List all features |
| DeviceName | `0x0005` | Read device name string |
| BatteryStatus | `0x1000` | Battery level (legacy format, MX Master 2S and older) |
| BatteryUnified | `0x1004` | Battery level and charging status (MX Master 3S+) |
| ChangeHost | `0x1814` | Easy-Switch host info |
| ReprogControlsV4 | `0x1b04` | Button diversion and remapping |
| SmartShift | `0x2110` | SmartShift V1 ratchet/freespin control |
| SmartShiftEnhanced | `0x2111` | SmartShift V2 (MX Master 4, different function IDs) |
| HiResWheel | `0x2121` | Scroll wheel mode and ratchet |
| ThumbWheel | `0x2150` | Thumb wheel diversion and direction |
| AdjustableDPI | `0x2201` | DPI range and current value |
| GestureV2 | `0x6501` | Gesture engine (reserved) |

Features with multiple variants (Battery, SmartShift) are resolved at enumeration time via capability dispatch tables in `src/core/hidpp/capabilities/`. DeviceManager stores the resolved variant and uses it everywhere, so adding new variants requires only a table entry with zero DeviceManager changes.

### Command Queue

The CommandQueue exists to solve a specific problem: **HwError flooding**.

When a profile switch happens, Logitune needs to send many HID++ commands in rapid succession (divert 6 buttons + set DPI + set SmartShift + set scroll config + set thumb wheel = ~10 commands). Sending them all at once causes `HwError` (error code `0x04`) responses because the device's internal command processor cannot keep up.

```mermaid
sequenceDiagram
    participant App as DeviceManager
    participant CQ as CommandQueue
    participant Timer as QTimer (10ms)
    participant FD as FeatureDispatcher
    participant TR as Transport

    App->>CQ: enqueue(SetDPI, ...)
    App->>CQ: enqueue(SetSmartShift, ...)
    App->>CQ: enqueue(DivertButton, ...)
    App->>CQ: enqueue(DivertButton, ...)

    Note over CQ: Queue: [SetDPI, SetSmartShift, Divert, Divert]

    Timer->>CQ: processNext()
    CQ->>FD: callAsync(SetDPI, ...)
    FD->>TR: sendRequestAsync(report)

    Note over CQ: Wait 10ms

    Timer->>CQ: processNext()
    CQ->>FD: callAsync(SetSmartShift, ...)

    Note over CQ: Wait 10ms

    Timer->>CQ: processNext()
    CQ->>FD: callAsync(DivertButton, ...)

    Note over CQ: Continue until queue empty
    CQ-->>App: queueDrained()
```

Key properties:

- **10ms inter-command delay** (`kInterCommandDelayMs = 10`) — enough for the device to process each command
- **3 retries** (`kMaxRetries = 3`) with 50ms retry delay
- **Main thread only** — uses `QTimer`, no mutex needed, no fd contention with `QSocketNotifier`
- **Created after feature enumeration** — the command queue is instantiated inside `enumerateAndSetup()` after the feature table is populated

### Async Response Matching

`FeatureDispatcher::callAsync()` uses a rotating `softwareId` (1-15) to match responses to requests:

```mermaid
sequenceDiagram
    participant CQ as CommandQueue
    participant FD as FeatureDispatcher
    participant TR as Transport
    participant Notif as QSocketNotifier

    CQ->>FD: callAsync(feature, fn, params, callback)
    Note over FD: Assign softwareId = 3 (rotating 1-15)
    FD->>TR: sendRequestAsync(report with swId=3)

    Note over TR: Later, device responds...

    Notif->>Notif: hidraw fd readable
    Notif->>Notif: readReport -> parse Report
    Note over Notif: report.softwareId = 3 (non-zero)
    Notif->>FD: handleResponse(report)
    Note over FD: Look up callback for swId=3
    FD->>FD: callback(report)
    Note over FD: Remove pending callback
```

The `softwareId` field (lower 4 bits of byte[3] in HID++ reports) distinguishes responses from notifications:

- **softwareId = 0** — unsolicited notification from the device (battery change, button press, wheel rotation)
- **softwareId 1-15** — response to a specific request sent by the host

This was a critical fix: without it, async responses from thumb wheel SetReporting were being misinterpreted as thumb wheel rotation events (the "delta=256 bug").

## Profile System

### Profile Struct

```cpp
struct Profile {
    int version = 1;
    QString name;
    QString icon;
    int dpi = 1000;
    bool smartShiftEnabled = true;
    int smartShiftThreshold = 128;
    bool smoothScrolling = false;
    QString scrollDirection = "standard";  // "standard" or "natural"
    bool hiResScroll = true;
    std::array<ButtonAction, 16> buttons;  // indexed by ControlDescriptor::buttonIndex
    std::map<QString, ButtonAction> gestures;  // "up","down","left","right","click"
    QString thumbWheelMode = "scroll";  // "scroll", "zoom", "volume", "none"
    bool thumbWheelInvert = false;
};
```

### ProfileEngine

```mermaid
graph TB
    subgraph "ProfileEngine"
        Cache["In-Memory Cache<br/>QMap of serial to Profile"]
        Disk["Disk Storage<br/>~/.config/Logitune/devices/{serial}/profiles/"]
        Bindings["App Bindings<br/>app-bindings.conf"]
        Display["displayProfile<br/>(what UI shows)"]
        Hardware["hardwareProfile<br/>(what device runs)"]
    end

    subgraph "Files"
        Default["default.conf"]
        Firefox["firefox.conf"]
        VSCode["code.conf"]
        AppBindConf["app-bindings.conf"]
    end

    Cache --> Default
    Cache --> Firefox
    Cache --> VSCode
    Bindings --> AppBindConf
    
    Display --> Cache
    Hardware --> Cache
```

### Profile Lifecycle

1. **Device connects** — `onDeviceSetupComplete()` creates the profile directory under `~/.config/Logitune/devices/<serial>/profiles/`
2. **First connect** — seeds `default.conf` from current device hardware state (DPI, SmartShift, scroll config, button defaults from descriptor, default gestures)
3. **Profile load** — `setDeviceConfigDir()` scans the directory for `.conf` files and loads them into the in-memory cache
4. **Focus change** — `profileForApp(wmClass)` looks up the app binding; if none found, returns "default"
5. **Hardware apply** — `applyProfileToHardware()` sends all profile settings via CommandQueue
6. **User edit** — UI changes go through DeviceModel -> DeviceCommands -> ProfileOrchestrator -> ProfileEngine cache -> disk save
7. **Cache vs disk** — the cache is the source of truth during runtime; saves to disk are immediate but loads only happen at startup

### ProfileDelta

The `ProfileDelta` struct tracks which fields changed between two profiles:

```cpp
struct ProfileDelta {
    bool dpiChanged = false;
    bool smartShiftChanged = false;
    bool scrollChanged = false;
    bool buttonsChanged = false;
    bool gesturesChanged = false;
};
```

This enables future optimizations where only changed settings are sent to hardware during profile switches.

## MVVM Pattern

Logitune uses a Model-View-ViewModel pattern where C++ models serve as the ViewModel layer between QML views and core logic.

Services do not all sit in the same MVVM stratum. `DeviceSelection` is a VM-side query helper, `DeviceCommands` bridges VM intent to Model operations, `ButtonActionDispatcher` is Model-side (reacts to hardware events), and `ProfileOrchestrator` is a coordinator that spans the whole stack. The diagram splits these visually.

```mermaid
graph LR
    subgraph "View (QML)"
        PointScroll[PointScrollPage.qml]
        Buttons[ButtonsPage.qml]
        EasySwitch[EasySwitchPage.qml]
        Settings[SettingsPage.qml]
        ProfileBar[AppProfileBar.qml]
    end

    subgraph "ViewModel (C++ Models)"
        DM[DeviceModel]
        BM[ButtonModel]
        AM[ActionModel]
        PM[ProfileModel]
    end

    subgraph "Services (VM to Model bridges)"
        DSel[DeviceSelection]
        DCmd[DeviceCommands]
        BAD[ButtonActionDispatcher]
    end

    PO[ProfileOrchestrator<br/>Coordinator]

    subgraph "Model (Core)"
        DMgr[DeviceManager]
        PE[ProfileEngine]
        AE[ActionExecutor]
    end

    PointScroll --> DM
    Buttons --> BM
    Buttons --> AM
    Buttons --> DM
    EasySwitch --> DM
    Settings --> DM
    ProfileBar --> PM

    DM -->|ChangeRequested| DCmd
    BM -->|userActionChanged| PO
    PM -->|profileSwitched| PO

    DSel --> DM
    DCmd --> DSel
    DCmd --> DMgr
    BAD --> DSel
    BAD --> PE
    BAD --> AE

    PO --> DSel
    PO --> PE
    PO --> AE
    PO -->|pushDisplayValues| DM
    PO -->|restoreButtons| BM
    PO -->|setHwActive| PM

    classDef view fill:#831843,stroke:#f472b6,color:#fce7f3
    classDef vm fill:#1e3a8a,stroke:#60a5fa,color:#dbeafe
    classDef service fill:#064e3b,stroke:#34d399,color:#d1fae5
    classDef coord fill:#4c1d95,stroke:#a78bfa,color:#ede9fe
    classDef core fill:#78350f,stroke:#fbbf24,color:#fef3c7

    class PointScroll,Buttons,EasySwitch,Settings,ProfileBar view
    class DM,BM,AM,PM vm
    class DSel,DCmd,BAD service
    class PO coord
    class DMgr,PE,AE core
```

### Model Roles

**DeviceModel** — QObject singleton exposed to QML. Provides:

- Device state (connected, name, battery, connection type)
- Settings (DPI, SmartShift, scroll, thumb wheel)
- Device descriptor info (images, hotspots, Easy-Switch slots)
- Display values that may differ from hardware (when viewing non-active profile)
- Logging control (enable/disable, bug report)

**ButtonModel** — `QAbstractListModel` with roles:

| Role | Type | Description |
|------|------|-------------|
| `ButtonIdRole` | int | Button index (0-7) |
| `ButtonNameRole` | QString | Display name from device descriptor |
| `ActionNameRole` | QString | Current action display name |
| `ActionTypeRole` | QString | Action type: "default", "keystroke", "gesture-trigger", etc. |

**ActionModel** — `QAbstractListModel` catalog of available actions:

| Role | Type | Description |
|------|------|-------------|
| `NameRole` | QString | Display name (e.g., "Copy") |
| `DescriptionRole` | QString | Help text |
| `ActionTypeRole` | QString | "default", "keystroke", "app-launch", etc. |
| `PayloadRole` | QString | Keystroke combo or app command |

**ProfileModel** — `QAbstractListModel` for the profile tab bar:

| Role | Type | Description |
|------|------|-------------|
| `NameRole` | QString | Profile display name |
| `IconRole` | QString | Application icon name |
| `WmClassRole` | QString | Window manager class for app binding |
| `IsActiveRole` | bool | User's selected tab |
| `IsHwActiveRole` | bool | Currently active on hardware |

### Model Registration

Models are registered as QML singletons in `main.cpp`:

```cpp
qmlRegisterSingletonInstance("Logitune", 1, 0, "DeviceModel",        controller.deviceModel());
qmlRegisterSingletonInstance("Logitune", 1, 0, "ButtonModel",        controller.buttonModel());
qmlRegisterSingletonInstance("Logitune", 1, 0, "ActionFilterModel",  controller.actionFilterModel());
qmlRegisterSingletonInstance("Logitune", 1, 0, "ProfileModel",       controller.profileModel());
qmlRegisterSingletonInstance("Logitune", 1, 0, "SettingsModel",      controller.settingsModel());
```

`ActionFilterModel` wraps the raw `ActionModel` catalog and hides entries the selected device can't execute (PR #82). QML code always binds to the filter model, never to the raw catalog. `SettingsModel` exposes the persisted user prefs (dark mode, logging, autostart, minimized, bug reports) as a single Q_PROPERTY surface.

```mermaid
flowchart LR
    QML["QML import Logitune 1.0<br/>(any .qml file)"]

    DM["DeviceModel<br/><i>rows: PhysicalDevice *</i>"]
    BM["ButtonModel<br/><i>rows: visible buttons of<br/>selected device profile</i>"]
    AFM["ActionFilterModel<br/><i>catalog minus actions the<br/>selected device can't run</i>"]
    PM["ProfileModel<br/><i>rows: user's profiles</i>"]
    SM["SettingsModel<br/><i>dark mode, logging,<br/>autostart, …</i>"]

    QML -->|singleton| DM
    QML -->|singleton| BM
    QML -->|singleton| AFM
    QML -->|singleton| PM
    QML -->|singleton| SM
```

All five are registered in `src/app/main.cpp` against `controller.xxxModel()` accessors — AppRoot owns them, QML borrows them. No other QML-visible C++ classes.

## Desktop Integration

### Interface Hierarchy

```mermaid
classDiagram
    class IDesktopIntegration {
        <<abstract>>
        +start()
        +available() bool
        +desktopName() QString
        +detectedCompositors() QStringList
        +blockGlobalShortcuts(bool block)
        +runningApplications() QVariantList
        +activeWindowChanged(wmClass, title) signal
    }

    class LinuxDesktopBase {
        +runningApplications()
        #resolveDesktopFile(appId) QString
        #desktopDirs() QStringList
    }

    class KDeDesktop {
        +focusChanged(resourceClass, title, desktopFileName)
        -m_kwin : QDBusInterface
        -m_pollTimer : QTimer
    }

    class GnomeDesktop {
        +focusChanged(appId, title)
        -ensureExtensionInstalled() bool
        -detectAppIndicatorStatus()
        -m_appIndicatorStatus : AppIndicatorStatus
    }

    class GenericDesktop {
        +start()
    }

    IDesktopIntegration <|-- LinuxDesktopBase
    LinuxDesktopBase <|-- KDeDesktop
    LinuxDesktopBase <|-- GnomeDesktop
    IDesktopIntegration <|-- GenericDesktop
```

`GnomeDesktop` (Wayland-only) auto-installs and enables a GNOME Shell extension on first launch that pipes focus events to a D-Bus-registered callback in-process — event-driven, no polling. It also detects AppIndicator support via `org.kde.StatusNotifierWatcher` so the tray icon can tell users when to install `gnome-shell-extension-appindicator`. `KDeDesktop` uses a KWin script + polling fallback (the KWin 6 signal quirk).

### KDE Focus Tracking

KDeDesktop uses a KWin script to track window focus changes:

```mermaid
sequenceDiagram
    participant KWin as KWin Compositor
    participant Script as Focus Watcher Script
    participant DBus as D-Bus Session Bus
    participant KDE as KDeDesktop
    participant AC as AppRoot

    Note over KDE: On start, register D-Bus service com.logitune.app
    KDE->>KWin: loadScript(logitune_focus_watcher.js)
    KDE->>KWin: start()

    Note over Script: workspace.windowActivated.connect(update)

    KWin->>Script: windowActivated
    Script->>DBus: callDBus('com.logitune.app', '/FocusWatcher', focusChanged, resourceClass, caption, desktopFileName)
    DBus->>KDE: focusChanged(resourceClass, title, desktopFileName)

    KDE->>KDE: resolveDesktopFile(resourceClass)
    Note over KDE: 1. Use desktopFileName if present<br/>2. Search .desktop files by name/StartupWMClass<br/>3. Fall back to resourceClass

    KDE->>AC: activeWindowChanged(appId, title)
```

### Window Identity Resolution

A critical problem: the same application can have different identifiers depending on how it's packaged:

- Zoom: `resourceClass="zoom"`, but `.desktop` file is `us.zoom.Zoom.desktop`
- Firefox: `desktopFileName="org.mozilla.firefox"`
- Native KDE apps: `desktopFileName="org.kde.dolphin"`

`resolveDesktopFile()` searches these directories:

1. `/usr/share/applications`
2. `~/.local/share/applications`
3. `/var/lib/flatpak/exports/share/applications` (Flatpak apps installed on the host)
4. `~/.local/share/flatpak/exports/share/applications`
5. `/var/lib/snapd/desktop/applications`

It matches by:
1. Last component of the `.desktop` filename (e.g., "Zoom" from "us.zoom.Zoom")
2. `StartupWMClass` field in the `.desktop` file

Results are cached in `m_resolveCache` to avoid repeated filesystem scans.

### blockGlobalShortcuts

During keystroke capture (when the user is pressing a key combo to assign to a button), KDE global shortcuts are temporarily disabled via:

```cpp
QDBusMessage msg = QDBusMessage::createMethodCall(
    "org.kde.kglobalaccel", "/kglobalaccel",
    "org.kde.KGlobalAccel", "blockGlobalShortcuts");
msg << block;
QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
```

This prevents Ctrl+Super+Left (assigned to "switch desktop left") from actually switching desktops while the user is trying to capture it as a button binding.

## Device Discovery and Connection

### PhysicalDevice: transport aggregation

A single MX Master 3S mouse typically appears on the host as *two* hidraw nodes when both transports are active — once via the Bolt/Unifying receiver, once via direct Bluetooth. HID++ unit serial (from `DeviceInfo.getSerial`) identifies them as the same physical unit. `src/core/PhysicalDevice.{h,cpp}` is the abstraction that collapses them:

- Owned by `DeviceManager`, keyed by serial.
- Holds a non-owning list of `DeviceSession *` transports.
- Exposes a single `primary()` pointer — commands route there.
- Picks primary based on connection state: if the current primary goes stale (udev remove, ping timeout), switches to any other connected transport without the UI seeing a disconnect event.
- Emits `stateChanged` / `deviceNameChanged` / `batteryChanged` once per underlying transition, not per transport — models bind to `PhysicalDevice`, not the raw `DeviceSession`.

The UI, `DeviceModel`, `ProfileEngine`, and tray all deal in `PhysicalDevice *`. `DeviceSession *` is an implementation detail of the transport layer.

```mermaid
flowchart LR
    DM["DeviceManager<br/><i>keyed by unit serial</i>"]
    PD["PhysicalDevice<br/><i>serial = 'ABCD…'</i>"]
    DS1["DeviceSession<br/><i>Bolt receiver</i><br/>✅ primary"]
    DS2["DeviceSession<br/><i>Bluetooth direct</i><br/>standby"]

    DM -->|owns| PD
    PD -->|non-owning, active| DS1
    PD -.->|non-owning, fallback| DS2

    DS1 -. "pings fail →<br/>PhysicalDevice swaps primary" .-> DS2
```

When the active transport drops (udev remove, HID++ ping timeout), `PhysicalDevice::setPrimary()` picks any remaining connected session. The UI never sees a disconnect event — `stateChanged` still fires, but `DeviceModel`'s row for this serial stays.

### Discovery Flow

```mermaid
flowchart TD
    Start[DeviceManager::start] --> InitUdev[Initialize libudev monitor]
    InitUdev --> Scan[scanExistingDevices]
    Scan --> ForEach{For each /dev/hidrawN}

    ForEach --> CheckVendor{Vendor == 0x046d?}
    CheckVendor -->|No| ForEach
    CheckVendor -->|Yes| Probe[probeDevice]

    Probe --> CheckDesc{sysfs report_descriptor<br/>has HID++ report ID 0x11?}
    CheckDesc -->|No| Skip[Skip - wrong interface]
    CheckDesc -->|Yes| Open[Open hidraw fd]
    Open --> CheckPID{PID matches<br/>Bolt/Unifying receiver?}

    CheckPID -->|Receiver| PingSlots[Ping slots 1-6]
    PingSlots --> Found{Response from slot?}
    Found -->|Yes| Connect[Store device + index]
    Found -->|No| KeepOpen[Keep receiver open<br/>for DJ notifications]

    CheckPID -->|Direct device| SetDirect[deviceIndex = 0xFF]
    SetDirect --> Connect

    Connect --> Enumerate[enumerateAndSetup]
    Enumerate --> Features[FeatureDispatcher::enumerate]
    Features --> ReadState[Read battery, DPI, SmartShift, scroll, thumb wheel, Easy-Switch]
    ReadState --> LookupDesc[DeviceRegistry::findByPid/findByName]
    LookupDesc --> Undivert[Undivert all buttons + thumb wheel]
    Undivert --> CreateQueue[Create CommandQueue]
    CreateQueue --> Signal[emit deviceSetupComplete]
```

### Report Descriptor Check

Before opening a hidraw device, Logitune checks the sysfs report descriptor for the HID++ long report ID (`0x11`). This is critical because:

- Each HID device exposes multiple hidraw interfaces (keyboard, mouse, vendor-specific)
- Opening and writing to the wrong interface can "poison" sibling interfaces
- The sysfs check at `/sys/class/hidraw/hidrawN/device/report_descriptor` avoids this without opening the fd

### Bolt Receiver Slot Probing

For receiver connections, Logitune pings device indices 1-6 with a HID++ 2.0 Root feature request. The receiver may respond with:

- HID++ 2.0 long report (success)
- HID++ 1.0 short report (legacy device)
- HID++ 1.0 error with code 0x09 (no device on slot)
- HID++ 2.0 error (device not present)

If no device is found on any slot, the receiver fd is kept open and a `QSocketNotifier` watches for incoming traffic, indicating a device has connected.

## Device Registry

`DeviceRegistry` loads device descriptors at startup from three sources
(scanned in order; earlier entries win on PID collisions via
`findByPid` returning the first match):

1. `$XDG_DATA_DIRS/logitune/devices/<slug>/descriptor.json` (system,
   where `cmake --install` places the `devices/` folder from the repo)
2. `$XDG_CACHE_HOME/logitune/devices/<slug>/descriptor.json` (cache,
   rarely used directly)
3. `$XDG_DATA_HOME/logitune/devices/<slug>/descriptor.json` (user
   override, for iterating on a community descriptor without
   rebuilding; remove the matching system descriptor first if you
   need the user version to take precedence)

Each descriptor is wrapped in a `JsonDevice` instance that exposes the
`IDevice` interface consumed by the rest of the app. `JsonDevice` is
the only concrete `IDevice` subclass; there are no per-device C++
classes. A new device is a `descriptor.json` file plus three images.

### Key Components

- **`JsonDevice`** (`src/core/devices/JsonDevice.{h,cpp}`): parses `descriptor.json` and adapts to the `IDevice` interface. Tracks the source directory path and modification time for live reload support.
- **`DescriptorWriter`** (`src/core/devices/DescriptorWriter.{h,cpp}`): atomic writes to `descriptor.json`, preserving unknown fields so hand-edited entries survive a round-trip through the editor.
- **`EditorModel`** (`src/app/models/EditorModel.{h,cpp}`): `--edit` mode state machine, undo/redo command stack, and file-conflict detection. Drives the in-app descriptor editor.

```mermaid
graph LR
    EditorModel --> DescriptorWriter
    DescriptorWriter --> JsonDevice
    JsonDevice --> DeviceRegistry
```

For the contributor-facing workflow, see
[Adding a Device](Adding-a-Device). For the visual-editing tool, see
[Editor Mode](Editor-Mode).

## Disconnect and Reconnect

### Bolt Receiver DJ Notifications

When a device disconnects from a Bolt receiver (e.g., turned off, moved out of range), the receiver sends a HID++ 1.0 DeviceConnection notification (register `0x41`):

```mermaid
sequenceDiagram
    participant Dev as Device
    participant Recv as Bolt Receiver
    participant DMgr as DeviceManager

    Dev->>Recv: (device powers off)
    Recv->>DMgr: HID++ 1.0 notification<br/>featureIndex=0x41<br/>params[0] bit 6 = 1 (link not established)
    
    Note over DMgr: Soft disconnect:<br/>- Clear CommandQueue<br/>- Reset features<br/>- Keep hidraw fd open<br/>- Emit deviceDisconnected

    Dev->>Recv: (device powers on)
    Recv->>DMgr: HID++ 1.0 notification<br/>featureIndex=0x41<br/>params[0] bit 6 = 0 (link established)
    
    Note over DMgr: Start 1500ms reconnect timer<br/>(debounce — device sends multiple<br/>notifications during boot)
    
    DMgr->>DMgr: Timer fires: enumerateAndSetup()
    Note over DMgr: Re-enumerate features,<br/>re-read state,<br/>re-create CommandQueue,<br/>emit deviceSetupComplete
```

Key details:

- **Soft disconnect** — the hidraw fd stays open. Only logical state (features, command queue, connected flag) is reset.
- **1500ms debounce** — the device sends multiple DJ notifications during boot, and HID++ calls fail with HwError if sent too early.
- **Reconnect timer cancellation** — if multiple link-established notifications arrive, only the last one triggers re-enumeration.

### Transport Failover

When a device is connected via both Bolt and Bluetooth:

1. New hidraw device appears via udev "add" event
2. DeviceManager pings the current device
3. If the current device is unresponsive, switches to the new transport
4. Emits `transportSwitched(newType)`

### Sleep/Wake Detection

`checkSleepWake()` monitors the gap between HID++ responses. If no response has been received for 2 minutes (`kSleepThresholdMs = 120000`), the device is assumed to have been sleeping. On the next response:

1. Wait 500ms for the device to fully wake
2. Re-enumerate features (firmware may have reset state)
3. Emit `deviceWoke()`

The `touchResponseTime()` method is called before intentional hardware writes to prevent false sleep/wake detection during profile switches.

## Gesture System

The gesture system intercepts raw mouse XY deltas when the gesture button is held down:

```mermaid
stateDiagram-v2
    [*] --> Idle

    Idle --> GestureActive : Gesture button pressed<br/>(CID 0x00C3, diverted)
    
    GestureActive --> GestureActive : Raw XY deltas<br/>accumulate dx, dy
    
    GestureActive --> ResolveGesture : Button released<br/>(controlId = 0, all released)
    
    ResolveGesture --> ExecuteAction : |dx| or |dy| > 50
    ResolveGesture --> ExecuteClick : |dx| and |dy| <= 50

    ExecuteAction --> Idle : Inject keystroke<br/>(Up/Down/Left/Right)
    ExecuteClick --> Idle : Inject keystroke<br/>(Click gesture)
```

Direction resolution:

- If `|dx| > |dy|`: Left (dx < 0) or Right (dx > 0)
- If `|dy| > |dx|`: Up (dy < 0) or Down (dy > 0)
- If neither exceeds threshold (50 units): Click

The gesture button (CID `0x00C3` on MX Master 3S) is diverted with `rawXY=true`, which causes the device to send `DivertedRawXYEvent` notifications instead of normal mouse movement.

## Thumb Wheel

### Mode Processing

The thumb wheel supports four modes:

| Mode | HID++ | Action |
|------|-------|--------|
| `scroll` | Not diverted | Native horizontal scroll (no software processing) |
| `zoom` | Diverted | Ctrl+scroll injection (Ctrl held + vertical scroll event) |
| `volume` | Diverted | VolumeUp/VolumeDown key injection |
| `none` | Not diverted | No action |

When diverted, the device sends thumb wheel rotation events with raw delta values. These are:

1. **Normalized** by `thumbWheelDefaultDirection` (read from ThumbWheel GetInfo) so clockwise = positive
2. **Accumulated** in `m_thumbAccum`
3. **Thresholded** at `kThumbThreshold = 15` to convert continuous rotation into discrete steps
4. **Executed** as the appropriate action for each step

### Direction Normalization

The MX Master 3S reports `defaultDirection = 0` (positive when left/back), so `thumbWheelDefaultDirection = -1`. Multiplying raw deltas by -1 makes clockwise = positive, which is the natural direction for zoom-in and volume-up.

## Services

Behavior that responds to user events or mutates application state lives in one of four focused services in `src/app/services/`. Each service holds non-owning pointers to the models, engines, or `DeviceSelection` instance it needs, has zero `connect()` calls of its own, and communicates with its peers only via Qt signals wired by `AppRoot`.

The dependency rule, enforced at the code level:

> Services hold pointers only to models, engines, and `DeviceSelection`. Cross-service communication is always via signal, wired in `AppRoot`.

### DeviceSelection

Resolves the currently selected `PhysicalDevice` / `DeviceSession` / serial from `DeviceModel`'s selection index and its ordered device list. This is the single source of truth for "who is selected", so every other service asks here rather than re-deriving it.

- **Constructor dependencies:** `DeviceModel*`
- **Public accessors:** `activeDevice()`, `activeSession()`, `activeSerial()`
- **Public slots:** `onSelectionIndexChanged()`
- **Signals:** `selectionChanged()`
- **State:** none (pure projection over `DeviceModel`)

### DeviceCommands

Routes UI change requests (slider drag, toggle click) to the active `DeviceSession`. Every mutator is a no-op when there is no active session, which makes the service safe to invoke before any device attaches.

- **Constructor dependencies:** `DeviceSelection*`
- **Public slots:** `requestDpi(int)`, `requestSmartShift(bool, int)`, `requestScrollConfig(bool, bool)`, `requestThumbWheelMode(QString)`, `requestThumbWheelInvert(bool)`
- **Signals:** `userChangedSomething()` emitted after every successful mutation, for `ProfileOrchestrator::saveCurrentProfile` to latch on to
- **State:** none
- **Note:** `applyProfileToHardware` (the burst during a profile switch) does *not* go through `DeviceCommands`. It calls the session directly. `DeviceCommands` is specifically for user-initiated control changes from the UI.

### ButtonActionDispatcher

Interprets raw HID++ input events (`gestureRawXY`, `divertedButtonPressed`, `thumbWheelRotation`) and dispatches high-level actions: keystroke injection, app launch, DPI cycle, SmartShift toggle, gesture direction resolution, and thumb-wheel-mode actions. Owns the per-device gesture + thumb-wheel accumulator state.

- **Constructor dependencies:** `ProfileEngine*`, `ActionExecutor*`, `DeviceSelection*`
- **Public slots:** `onGestureRaw(int16_t, int16_t)`, `onDivertedButtonPressed(uint16_t, bool)`, `onThumbWheelRotation(int)`, `onProfileApplied(QString)`, `onCurrentDeviceChanged(const IDevice*)`
- **Public methods:** `onDeviceRemoved(QString)` to drop per-device state
- **State:** `QMap<QString, PerDeviceState>` keyed by serial. Each entry holds `gestureAccumX/Y`, `thumbAccum`, `gestureActive`, `gestureControlId`. Gesture threshold is 50, thumb threshold is 15.

### ProfileOrchestrator

Owns the save, apply, push, and window-focus flow. Holds no device state beyond a current `IDevice*` pointer; reads from models and engines, writes to them, and emits `profileApplied(serial)` after every hardware apply so the dispatcher can reset its thumb accumulator.

- **Constructor dependencies:** `ProfileEngine*`, `ActionExecutor*`, `DeviceSelection*`, `DeviceModel*`, `ButtonModel*`, `ActionModel*`, `ProfileModel*`, `IDesktopIntegration*`
- **Public methods:** `setupProfileForDevice(PhysicalDevice*)`, `applyProfileToHardware(const Profile&)`, `applyDisplayedChange<Mutator, HardwareForward>(...)` (the templated bridge used by `AppRoot` for the five `DeviceModel` `*ChangeRequested` signals)
- **Public slots:** `saveCurrentProfile()`, `onUserButtonChanged(int, QString, QString)`, `onTabSwitched(QString)`, `onDisplayProfileChanged(QString, const Profile&)`, `onWindowFocusChanged(QString, QString)`, `onTransportSetupComplete(PhysicalDevice*)`, `onCurrentDeviceChanged(const IDevice*)`
- **Signals:** `profileApplied(QString serial)`, `currentDeviceChanged(const IDevice*)`
- **State:** `m_currentDevice` (non-owning `const IDevice*`)

## AppRoot Wiring

`AppRoot` is the composition root. It owns the long-lived singletons (ViewModels, services, engines, `DeviceRegistry`, `DeviceManager`, `DeviceFetcher`), wires the signal graph between them at startup, attaches `PhysicalDevice` instances into the graph at runtime, and exposes ViewModels via accessors for QML registration in `main.cpp`. It does not implement user-facing behavior; every `connect()` call in the app library lives either in `wireSignals()` or in `onPhysicalDeviceAdded()`.

The wiring falls into three groups:

- **(a) QML → AppRoot ViewModel → Service.** User events from QML update a ViewModel; `AppRoot` has a `connect()` from the ViewModel's signal into the relevant service slot.
- **(b) Service → Service.** Cross-service communication, always via a signal emitted by one service and a slot on another, with the edge living in `wireSignals()`.
- **(c) Hardware → AppRoot → Service.** Per-device runtime wiring in `onPhysicalDeviceAdded`: each new `PhysicalDevice` gets its input signals (`gestureRawXY`, `divertedButtonPressed`, `thumbWheelRotation`, `transportSetupComplete`) hooked into the appropriate service slots.

### Signal wiring reference

Key edges wired in `AppRoot::wireSignals()` and `AppRoot::onPhysicalDeviceAdded()`:

| # | Source | Signal | Sink | Group |
|---|--------|--------|------|-------|
| 1 | ButtonModel | `userActionChanged` | ProfileOrchestrator::`onUserButtonChanged` | a |
| 2 | IDesktopIntegration | `activeWindowChanged` | ProfileOrchestrator::`onWindowFocusChanged` | a |
| 3 | ProfileModel | `profileSwitched` | ProfileOrchestrator::`onTabSwitched` | a |
| 4 | ProfileEngine | `deviceDisplayProfileChanged` | ProfileOrchestrator::`onDisplayProfileChanged` | a |
| 5 | DeviceModel | `selectedChanged` | DeviceSelection::`onSelectionIndexChanged` | a |
| 6 | DeviceSelection | `selectionChanged` | AppRoot::`onSelectionChanged` (internal refresh) | b |
| 7 | DeviceModel | `userGestureChanged` | lambda → ProfileOrchestrator::`saveCurrentProfile` | a |
| 8 | DeviceCommands | `userChangedSomething` | ProfileOrchestrator::`saveCurrentProfile` | b |
| 9 | ProfileOrchestrator | `profileApplied` | ButtonActionDispatcher::`onProfileApplied` | b |
| 10 | ProfileOrchestrator | `currentDeviceChanged` | ButtonActionDispatcher::`onCurrentDeviceChanged` | b |
| 11 | DeviceModel | `dpiChangeRequested` / `smartShiftChangeRequested` / `scrollConfigChangeRequested` / `thumbWheelModeChangeRequested` / `thumbWheelInvertChangeRequested` | lambda → `ProfileOrchestrator::applyDisplayedChange` + `DeviceCommands::request*` | a |
| 12 | ProfileModel | `profileAdded` / `profileRemoved` | lambda → ProfileEngine::`createProfileForApp` / `removeAppProfile` | a |
| 13 | DeviceManager | `physicalDeviceAdded` / `physicalDeviceRemoved` | AppRoot::`onPhysicalDeviceAdded` / `onPhysicalDeviceRemoved` | c |
| 14 | PhysicalDevice | `gestureRawXY` / `divertedButtonPressed` / `thumbWheelRotation` | ButtonActionDispatcher equivalents | c |
| 15 | PhysicalDevice | `transportSetupComplete` | lambda → ProfileOrchestrator::`onTransportSetupComplete` | c |

Rows 13 to 15 are per-device runtime wiring in `onPhysicalDeviceAdded`; the rest are startup wiring in `wireSignals()`. Row 11 is a single `applyDisplayedChange` bridge reused for all five `*ChangeRequested` signals; it persists the cached profile, refreshes the UI, and only forwards to hardware via `DeviceCommands` when the displayed profile is also the active hardware profile.

### Dependency Injection

AppRoot accepts optional `IDesktopIntegration*` and `IInputInjector*` in its constructor:

```cpp
AppRoot(IDesktopIntegration *desktop, IInputInjector *injector, QObject *parent = nullptr);
```

- If `nullptr` is passed (production), it creates `KDeDesktop` and `UinputInjector` internally
- In tests, `MockDesktop` and `MockInjector` are injected for deterministic behavior
- The injected pointers are **not owned** by AppRoot (raw pointers); internally created ones are held in `unique_ptr`

This is the sole DI point: the rest of the subsystems are value members of AppRoot, which simplifies lifetime management.
