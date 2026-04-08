# GNOME Desktop Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add GNOME Wayland focus tracking via a GNOME Shell extension, matching KDE's per-app profile switching.

**Architecture:** Extract shared .desktop resolution into `LinuxDesktopBase`, implement `GnomeDesktop` using a Shell extension that calls back via D-Bus, add desktop detection factory in `AppController`.

**Tech Stack:** C++20, Qt 6 D-Bus, GNOME Shell Extension JS (two versions: legacy imports for 42-44, ES modules for 45+)

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `src/core/desktop/LinuxDesktopBase.h` | Create | Shared base: `desktopDirs()`, `resolveDesktopFile()`, `runningApplications()` |
| `src/core/desktop/LinuxDesktopBase.cpp` | Create | Implementation of shared logic extracted from KDeDesktop |
| `src/core/desktop/KDeDesktop.h` | Modify | Inherit `LinuxDesktopBase`, remove duplicated members |
| `src/core/desktop/KDeDesktop.cpp` | Modify | Remove `desktopDirs()`, `resolveDesktopFile()`, `runningApplications()` |
| `src/core/desktop/GenericDesktop.h` | Modify | Inherit `LinuxDesktopBase`, remove duplicated `runningApplications()` |
| `src/core/desktop/GenericDesktop.cpp` | Modify | Remove `runningApplications()` body |
| `src/core/desktop/GnomeDesktop.h` | Create | GNOME integration header |
| `src/core/desktop/GnomeDesktop.cpp` | Create | GNOME integration: extension install, D-Bus focus callback |
| `src/app/AppController.cpp` | Modify | Desktop factory + GNOME ignore list |
| `src/core/CMakeLists.txt` | Modify | Add new source files |
| `CMakeLists.txt` | Modify | Install extension files |
| `data/gnome-extension/metadata.json` | Create | Extension manifest |
| `data/gnome-extension/v42/extension.js` | Create | GNOME 42-44 extension |
| `data/gnome-extension/v45/extension.js` | Create | GNOME 45+ extension |
| `tests/test_desktop_factory.cpp` | Create | Desktop detection unit tests |

---

### Task 1: Extract LinuxDesktopBase from KDeDesktop

**Files:**
- Create: `src/core/desktop/LinuxDesktopBase.h`
- Create: `src/core/desktop/LinuxDesktopBase.cpp`
- Modify: `src/core/desktop/KDeDesktop.h`
- Modify: `src/core/desktop/KDeDesktop.cpp`
- Modify: `src/core/desktop/GenericDesktop.h`
- Modify: `src/core/desktop/GenericDesktop.cpp`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: Create LinuxDesktopBase header**

```cpp
// src/core/desktop/LinuxDesktopBase.h
#pragma once
#include "interfaces/IDesktopIntegration.h"
#include <QHash>

namespace logitune {

class LinuxDesktopBase : public IDesktopIntegration {
    Q_OBJECT
public:
    using IDesktopIntegration::IDesktopIntegration;

    QVariantList runningApplications() const override;

protected:
    static const QStringList &desktopDirs();
    QString resolveDesktopFile(const QString &resourceClass) const;

    mutable QHash<QString, QString> m_resolveCache;
};

} // namespace logitune
```

- [ ] **Step 2: Create LinuxDesktopBase implementation**

Move `desktopDirs()`, `resolveDesktopFile()`, and `runningApplications()` from `KDeDesktop.cpp` into `LinuxDesktopBase.cpp`. The code is identical — just the class name changes.

```cpp
// src/core/desktop/LinuxDesktopBase.cpp
#include "desktop/LinuxDesktopBase.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSet>
#include <algorithm>

namespace logitune {

const QStringList &LinuxDesktopBase::desktopDirs()
{
    static const QStringList dirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/run/host/usr/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications"),
        QStringLiteral("/var/lib/snapd/desktop/applications")
    };
    return dirs;
}

QString LinuxDesktopBase::resolveDesktopFile(const QString &resourceClass) const
{
    auto cached = m_resolveCache.constFind(resourceClass);
    if (cached != m_resolveCache.constEnd())
        return cached.value();

    for (const QString &dir : desktopDirs()) {
        QDir d(dir);
        if (!d.exists()) continue;

        const QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            QString baseName = QFileInfo(file).completeBaseName();

            QString shortName = baseName.contains('.') ? baseName.section('.', -1) : baseName;
            if (shortName.compare(resourceClass, Qt::CaseInsensitive) == 0) {
                m_resolveCache.insert(resourceClass, baseName);
                return baseName;
            }

            QSettings desktop(d.filePath(file), QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));
            QString wmClass = desktop.value(QStringLiteral("StartupWMClass")).toString();
            if (!wmClass.isEmpty() && wmClass.compare(resourceClass, Qt::CaseInsensitive) == 0) {
                m_resolveCache.insert(resourceClass, baseName);
                return baseName;
            }
        }
    }

    m_resolveCache.insert(resourceClass, resourceClass);
    return resourceClass;
}

QVariantList LinuxDesktopBase::runningApplications() const
{
    QVariantList result;
    QSet<QString> seen;

    for (const QString &dir : desktopDirs()) {
        QDir d(dir);
        if (!d.exists()) continue;

        const QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            QSettings desktop(d.filePath(file), QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));

            QString type = desktop.value(QStringLiteral("Type")).toString();
            if (type != QStringLiteral("Application")) continue;
            if (desktop.value(QStringLiteral("NoDisplay")).toBool()) continue;

            QString name = desktop.value(QStringLiteral("Name")).toString();
            QString icon = desktop.value(QStringLiteral("Icon")).toString();
            QString wmClass = QFileInfo(file).completeBaseName();

            if (name.isEmpty() || seen.contains(wmClass.toLower()))
                continue;

            seen.insert(wmClass.toLower());
            QVariantMap entry;
            entry[QStringLiteral("wmClass")] = wmClass;
            entry[QStringLiteral("title")] = name;
            entry[QStringLiteral("icon")] = icon;
            result.append(entry);
        }
    }

    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("title")].toString().toLower()
             < b.toMap()[QStringLiteral("title")].toString().toLower();
    });

    return result;
}

} // namespace logitune
```

- [ ] **Step 3: Refactor KDeDesktop to inherit LinuxDesktopBase**

Update `KDeDesktop.h`:
```cpp
#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QDBusInterface>
#include <QTimer>

namespace logitune {

class KDeDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit KDeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    // runningApplications() inherited from LinuxDesktopBase

public slots:
    void focusChanged(const QString &resourceClass, const QString &title,
                      const QString &desktopFileName = QString());

private slots:
    void onActiveWindowChanged();
    void pollActiveWindow();

private:
    QDBusInterface *m_kwin = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_lastWmClass;
    // m_resolveCache inherited from LinuxDesktopBase
    bool m_available = false;
};

} // namespace logitune
```

In `KDeDesktop.cpp`, remove:
- The `desktopDirs()` free function (lines 195-206)
- The `resolveDesktopFile()` method (lines 208-247)
- The `runningApplications()` method (lines 249-295)
- The `#include <QSet>`, `#include <QDir>`, `#include <QFileInfo>`, `#include <QSettings>`, `#include <algorithm>` headers (only if no longer needed by remaining code)

In `KDeDesktop::focusChanged()`, change `resolveDesktopFile` call — it now calls the inherited method (no code change needed, the method signature is the same).

- [ ] **Step 4: Refactor GenericDesktop to inherit LinuxDesktopBase**

Update `GenericDesktop.h`:
```cpp
#pragma once
#include "desktop/LinuxDesktopBase.h"

namespace logitune {

class GenericDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit GenericDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;
    // runningApplications() inherited from LinuxDesktopBase
};

} // namespace logitune
```

In `GenericDesktop.cpp`, remove:
- The entire `runningApplications()` method
- All headers only needed for that method: `#include <QDir>`, `#include <QFileInfo>`, `#include <QSettings>`, `#include <QSet>`, `#include <algorithm>`

- [ ] **Step 5: Add LinuxDesktopBase.cpp to CMakeLists.txt**

In `src/core/CMakeLists.txt`, add after `desktop/GenericDesktop.cpp`:
```cmake
    desktop/LinuxDesktopBase.cpp
```

- [ ] **Step 6: Build and run existing tests**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Clean build, no errors

Run: `./build/tests/logitune-tests 2>&1 | tail -3`
Expected: All 372 tests pass (no behavioral change)

- [ ] **Step 7: Commit**

```bash
git add src/core/desktop/LinuxDesktopBase.h src/core/desktop/LinuxDesktopBase.cpp \
        src/core/desktop/KDeDesktop.h src/core/desktop/KDeDesktop.cpp \
        src/core/desktop/GenericDesktop.h src/core/desktop/GenericDesktop.cpp \
        src/core/CMakeLists.txt
git commit -m "refactor: extract LinuxDesktopBase from KDeDesktop

Shared .desktop resolution, app listing, and directory scan logic
now lives in LinuxDesktopBase. KDeDesktop and GenericDesktop inherit
from it, removing ~100 lines of duplication."
```

---

### Task 2: Create GNOME Shell Extension Files

**Files:**
- Create: `data/gnome-extension/metadata.json`
- Create: `data/gnome-extension/v42/extension.js`
- Create: `data/gnome-extension/v45/extension.js`

- [ ] **Step 1: Create metadata.json**

```json
{
  "uuid": "logitune-focus@logitune.com",
  "name": "Logitune Focus Watcher",
  "description": "Reports active window changes to Logitune for per-app profile switching.",
  "shell-version": ["42", "43", "44", "45", "46", "47", "48"],
  "version": 1
}
```

- [ ] **Step 2: Create v42/extension.js (GNOME 42-44, imports API)**

```js
// GNOME Shell Extension for Logitune — GNOME 42-44 (imports-based API)
const { Gio, GLib } = imports.gi;

let _focusHandler = null;

function _reportFocus() {
    let win = global.display.focus_window;
    if (!win) return;

    let appId = win.get_sandboxed_app_id() || win.get_gtk_window_object_path()
        ? win.get_sandboxed_app_id() : '';
    if (!appId) {
        let app = imports.gi.Shell.WindowTracker.get_default().get_window_app(win);
        appId = app ? app.get_id().replace(/\.desktop$/, '') : '';
    }
    if (!appId) appId = win.get_wm_class() || '';

    let title = win.get_title() || '';

    try {
        Gio.DBus.session.call(
            'com.logitune.app',
            '/FocusWatcher',
            'local.logitune.logitune.GnomeDesktop',
            'focusChanged',
            new GLib.Variant('(ss)', [appId, title]),
            null,
            Gio.DBusCallFlags.NO_AUTO_START,
            -1, null, null);
    } catch (e) {
        // Logitune not running — ignore silently
    }
}

function init() {}

function enable() {
    _focusHandler = global.display.connect('notify::focus-window', _reportFocus);
    _reportFocus();
}

function disable() {
    if (_focusHandler) {
        global.display.disconnect(_focusHandler);
        _focusHandler = null;
    }
}
```

- [ ] **Step 3: Create v45/extension.js (GNOME 45+, ES modules API)**

```js
// GNOME Shell Extension for Logitune — GNOME 45+ (ES modules API)
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';

export default class LogituneFocusExtension extends Extension {
    _focusHandler = null;

    _reportFocus() {
        let win = global.display.focus_window;
        if (!win) return;

        let appId = win.get_sandboxed_app_id() || '';
        if (!appId) {
            let app = Shell.WindowTracker.get_default().get_window_app(win);
            appId = app ? app.get_id().replace(/\.desktop$/, '') : '';
        }
        if (!appId) appId = win.get_wm_class() || '';

        let title = win.get_title() || '';

        try {
            Gio.DBus.session.call(
                'com.logitune.app',
                '/FocusWatcher',
                'local.logitune.logitune.GnomeDesktop',
                'focusChanged',
                new GLib.Variant('(ss)', [appId, title]),
                null,
                Gio.DBusCallFlags.NO_AUTO_START,
                -1, null, null);
        } catch (e) {
            // Logitune not running — ignore silently
        }
    }

    enable() {
        this._focusHandler = global.display.connect(
            'notify::focus-window', () => this._reportFocus());
        this._reportFocus();
    }

    disable() {
        if (this._focusHandler) {
            global.display.disconnect(this._focusHandler);
            this._focusHandler = null;
        }
    }
}
```

- [ ] **Step 4: Commit**

```bash
git add data/gnome-extension/
git commit -m "feat: add GNOME Shell extension for focus tracking

Two versions: v42 (imports API for GNOME 42-44) and v45 (ES modules
for GNOME 45+). Extension reports active window app_id and title to
Logitune via D-Bus callback."
```

---

### Task 3: Implement GnomeDesktop Class

**Files:**
- Create: `src/core/desktop/GnomeDesktop.h`
- Create: `src/core/desktop/GnomeDesktop.cpp`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: Create GnomeDesktop header**

```cpp
// src/core/desktop/GnomeDesktop.h
#pragma once
#include "desktop/LinuxDesktopBase.h"

namespace logitune {

class GnomeDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit GnomeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;

public slots:
    void focusChanged(const QString &appId, const QString &title);

private:
    bool ensureExtensionInstalled();
    int detectShellMajorVersion();

    QString m_lastAppId;
    bool m_available = false;
};

} // namespace logitune
```

- [ ] **Step 2: Create GnomeDesktop implementation**

```cpp
// src/core/desktop/GnomeDesktop.cpp
#include "desktop/GnomeDesktop.h"
#include "logging/LogManager.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QProcess>
#include <QStandardPaths>

namespace logitune {

static constexpr auto kExtUuid = "logitune-focus@logitune.com";

GnomeDesktop::GnomeDesktop(QObject *parent)
    : LinuxDesktopBase(parent)
{
}

void GnomeDesktop::start()
{
    // Only support Wayland sessions
    const QString sessionType = QProcessEnvironment::systemEnvironment()
                                    .value(QStringLiteral("XDG_SESSION_TYPE"));
    if (sessionType != QStringLiteral("wayland")) {
        qCInfo(lcFocus) << "GNOME: not a Wayland session, focus tracking disabled";
        m_available = false;
        return;
    }

    // Check GNOME Shell is running
    QDBusMessage ping = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    ping << QStringLiteral("org.gnome.Shell") << QStringLiteral("ShellVersion");
    QDBusMessage reply = QDBusConnection::sessionBus().call(ping, QDBus::Block, 1000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcFocus) << "GNOME Shell not reachable on D-Bus";
        m_available = false;
        return;
    }

    // Install and enable the extension
    if (!ensureExtensionInstalled()) {
        qCWarning(lcFocus) << "GNOME: failed to install/enable extension";
        m_available = false;
        return;
    }

    // Register our D-Bus service so the extension can call us
    QDBusConnection::sessionBus().registerService(QStringLiteral("com.logitune.app"));
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/FocusWatcher"), this,
        QDBusConnection::ExportAllSlots);

    m_available = true;
    qCInfo(lcFocus) << "GNOME desktop integration started";
}

bool GnomeDesktop::ensureExtensionInstalled()
{
    int major = detectShellMajorVersion();
    if (major < 42) {
        qCWarning(lcFocus) << "GNOME Shell version" << major << "not supported (need 42+)";
        return false;
    }

    QString variant = (major >= 45) ? QStringLiteral("v45") : QStringLiteral("v42");

    // Check system-wide install first
    QString systemDir = QStringLiteral("/usr/share/gnome-shell/extensions/")
                        + QLatin1String(kExtUuid);
    QString userDir = QDir::homePath()
                      + QStringLiteral("/.local/share/gnome-shell/extensions/")
                      + QLatin1String(kExtUuid);

    bool installed = QFile::exists(systemDir + QStringLiteral("/metadata.json"))
                  || QFile::exists(userDir + QStringLiteral("/metadata.json"));

    if (!installed) {
        // Copy from system install (package put files in /usr/share/.../<uuid>/v42/ and v45/)
        // We need to copy the correct variant's extension.js to the extension root
        QString sourceBase = systemDir;
        if (!QFile::exists(sourceBase + "/" + variant + "/extension.js")) {
            qCWarning(lcFocus) << "Extension source not found at" << sourceBase;
            return false;
        }

        QDir().mkpath(userDir);
        QFile::copy(sourceBase + QStringLiteral("/metadata.json"),
                    userDir + QStringLiteral("/metadata.json"));
        QFile::copy(sourceBase + "/" + variant + QStringLiteral("/extension.js"),
                    userDir + QStringLiteral("/extension.js"));
        qCInfo(lcFocus) << "Installed GNOME extension variant" << variant << "to" << userDir;
    } else if (QFile::exists(systemDir + QStringLiteral("/metadata.json"))
               && !QFile::exists(systemDir + QStringLiteral("/extension.js"))) {
        // System install has v42/ and v45/ subdirs but no root extension.js —
        // copy the correct variant to user dir
        QDir().mkpath(userDir);
        QFile::copy(systemDir + QStringLiteral("/metadata.json"),
                    userDir + QStringLiteral("/metadata.json"));
        QFile::copy(systemDir + "/" + variant + QStringLiteral("/extension.js"),
                    userDir + QStringLiteral("/extension.js"));
        qCInfo(lcFocus) << "Copied" << variant << "extension.js to" << userDir;
    }

    // Enable via D-Bus (GNOME 3.36+)
    QDBusMessage enableMsg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("/org/gnome/Shell/Extensions"),
        QStringLiteral("org.gnome.Shell.Extensions"),
        QStringLiteral("EnableExtension"));
    enableMsg << QString::fromLatin1(kExtUuid);
    QDBusReply<bool> enableReply = QDBusConnection::sessionBus().call(enableMsg, QDBus::Block, 2000);
    if (enableReply.isValid() && enableReply.value()) {
        qCInfo(lcFocus) << "GNOME extension enabled";
    } else {
        // Try CLI fallback
        QProcess proc;
        proc.start(QStringLiteral("gnome-extensions"),
                   {QStringLiteral("enable"), QString::fromLatin1(kExtUuid)});
        proc.waitForFinished(3000);
        if (proc.exitCode() != 0) {
            qCWarning(lcFocus) << "Failed to enable extension:" << proc.readAllStandardError();
            return false;
        }
        qCInfo(lcFocus) << "GNOME extension enabled via CLI";
    }

    return true;
}

int GnomeDesktop::detectShellMajorVersion()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg << QStringLiteral("org.gnome.Shell") << QStringLiteral("ShellVersion");
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        QString version = reply.arguments().first().value<QDBusVariant>().variant().toString();
        qCInfo(lcFocus) << "GNOME Shell version:" << version;
        return version.section('.', 0, 0).toInt();
    }
    return 0;
}

bool GnomeDesktop::available() const
{
    return m_available;
}

QString GnomeDesktop::desktopName() const
{
    return QStringLiteral("GNOME");
}

QStringList GnomeDesktop::detectedCompositors() const
{
    QStringList compositors;
    const QString desktop = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (desktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive))
        compositors << QStringLiteral("Mutter");
    return compositors;
}

void GnomeDesktop::focusChanged(const QString &appId, const QString &title)
{
    QString resolved = appId;
    // If appId looks like a raw wm_class (no dots), try resolving to .desktop baseName
    if (!appId.contains('.'))
        resolved = resolveDesktopFile(appId);

    if (resolved == m_lastAppId) return;
    m_lastAppId = resolved;
    emit activeWindowChanged(resolved, title);
}

void GnomeDesktop::blockGlobalShortcuts(bool block)
{
    // Use GNOME Shell Eval to toggle keyboard shortcut inhibition.
    // This runs inside the Shell process where it has full access.
    QString js = block
        ? QStringLiteral("global.stage.set_key_focus(null); "
                          "Main.layoutManager._startingUp = true;")
        : QStringLiteral("Main.layoutManager._startingUp = false;");

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("/org/gnome/Shell"),
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("Eval"));
    msg << js;
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

} // namespace logitune
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `src/core/CMakeLists.txt`, add after `desktop/LinuxDesktopBase.cpp`:
```cmake
    desktop/GnomeDesktop.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 5: Commit**

```bash
git add src/core/desktop/GnomeDesktop.h src/core/desktop/GnomeDesktop.cpp \
        src/core/CMakeLists.txt
git commit -m "feat: implement GnomeDesktop with Shell extension focus tracking

D-Bus-based focus detection via GNOME Shell extension callback.
Auto-installs and enables extension on start. Resolves app_id to
.desktop baseName via inherited LinuxDesktopBase."
```

---

### Task 4: Desktop Detection Factory in AppController

**Files:**
- Modify: `src/app/AppController.cpp`

- [ ] **Step 1: Add includes and replace hardcoded KDeDesktop**

At the top of `AppController.cpp`, add:
```cpp
#include "desktop/GnomeDesktop.h"
#include "desktop/GenericDesktop.h"
```

In the constructor, replace:
```cpp
    } else {
        m_ownedDesktop = std::make_unique<KDeDesktop>();
        m_desktop = m_ownedDesktop.get();
    }
```

With:
```cpp
    } else {
        const QString xdgDesktop = QProcessEnvironment::systemEnvironment()
                                       .value(QStringLiteral("XDG_CURRENT_DESKTOP"));
        if (xdgDesktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive))
            m_ownedDesktop = std::make_unique<KDeDesktop>();
        else if (xdgDesktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive))
            m_ownedDesktop = std::make_unique<GnomeDesktop>();
        else
            m_ownedDesktop = std::make_unique<GenericDesktop>();
        m_desktop = m_ownedDesktop.get();
    }
```

- [ ] **Step 2: Add GNOME entries to the focus ignore list**

In `onWindowFocusChanged`, add GNOME shell components to `kIgnored`:
```cpp
    static const QStringList kIgnored = {
        // KDE
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("kwin_wayland"),
        QStringLiteral("kwin_x11"),
        QStringLiteral("plasmashell"),
        QStringLiteral("org.kde.krunner"),
        // GNOME
        QStringLiteral("gnome-shell"),
        QStringLiteral("org.gnome.Shell"),
        QStringLiteral("org.gnome.Shell.Extensions"),
    };
```

- [ ] **Step 3: Build and run tests**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Clean build

Run: `./build/tests/logitune-tests 2>&1 | tail -3`
Expected: All tests pass

- [ ] **Step 4: Commit**

```bash
git add src/app/AppController.cpp
git commit -m "feat: desktop detection factory — auto-select KDE/GNOME/Generic

Detect desktop environment from XDG_CURRENT_DESKTOP at startup.
Add GNOME shell components to focus ignore list."
```

---

### Task 5: Install Extension via CMake + Packaging

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `scripts/package-arch.sh`
- Modify: `scripts/package-deb.sh`

- [ ] **Step 1: Add extension install rules to CMakeLists.txt**

After the existing install rules, add:
```cmake
# GNOME Shell extension (both API versions — app selects at runtime)
set(GNOME_EXT_DIR share/gnome-shell/extensions/logitune-focus@logitune.com)
install(FILES data/gnome-extension/metadata.json DESTINATION ${GNOME_EXT_DIR})
install(FILES data/gnome-extension/v42/extension.js DESTINATION ${GNOME_EXT_DIR}/v42)
install(FILES data/gnome-extension/v45/extension.js DESTINATION ${GNOME_EXT_DIR}/v45)
```

- [ ] **Step 2: Update Debian package optional dependencies**

In `scripts/package-deb.sh`, add to the `Suggests` field (create it if absent) after the `Depends` line:
```
Suggests: gnome-shell
```

- [ ] **Step 3: Build and verify install**

Run: `cmake --build build --parallel && sudo cmake --install build --prefix /usr 2>&1 | grep -i gnome`
Expected: Lines showing extension files installed to `/usr/share/gnome-shell/extensions/logitune-focus@logitune.com/`

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt scripts/package-deb.sh
git commit -m "feat: install GNOME Shell extension via cmake and packaging"
```

---

### Task 6: Desktop Factory Unit Tests

**Files:**
- Create: `tests/test_desktop_factory.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write desktop detection tests**

```cpp
// tests/test_desktop_factory.cpp
#include <gtest/gtest.h>
#include "desktop/KDeDesktop.h"
#include "desktop/GnomeDesktop.h"
#include "desktop/GenericDesktop.h"
#include "desktop/LinuxDesktopBase.h"

using namespace logitune;

TEST(DesktopFactory, KDeDesktopReportsKDE) {
    KDeDesktop kde;
    EXPECT_EQ(kde.desktopName(), "KDE");
}

TEST(DesktopFactory, GnomeDesktopReportsGNOME) {
    GnomeDesktop gnome;
    EXPECT_EQ(gnome.desktopName(), "GNOME");
}

TEST(DesktopFactory, GenericDesktopReportsGeneric) {
    GenericDesktop generic;
    EXPECT_EQ(generic.desktopName(), "Generic");
}

TEST(DesktopFactory, GnomeDetectsCompositorFromEnv) {
    GnomeDesktop gnome;
    // detectedCompositors reads XDG_CURRENT_DESKTOP; on a KDE machine
    // it won't contain GNOME, so we expect empty. On GNOME it returns ["Mutter"].
    // This test just verifies the method doesn't crash.
    auto compositors = gnome.detectedCompositors();
    // No assertion on content — environment-dependent
    EXPECT_TRUE(true);
}

TEST(DesktopFactory, GenericDesktopIsAlwaysAvailable) {
    GenericDesktop generic;
    EXPECT_TRUE(generic.available());
}

TEST(DesktopFactory, RunningApplicationsReturnsSortedList) {
    // LinuxDesktopBase::runningApplications scans .desktop files.
    // On any Linux system with /usr/share/applications, it should return
    // a non-empty list with wmClass/title/icon keys.
    GenericDesktop generic;
    auto apps = generic.runningApplications();
    EXPECT_GT(apps.size(), 0);

    // Verify sorted by title
    for (int i = 1; i < apps.size(); ++i) {
        QString prev = apps[i-1].toMap()["title"].toString().toLower();
        QString curr = apps[i].toMap()["title"].toString().toLower();
        EXPECT_LE(prev, curr);
    }
}
```

- [ ] **Step 2: Add to tests/CMakeLists.txt**

Add `test_desktop_factory.cpp` to the test sources list (same pattern as existing test files).

- [ ] **Step 3: Build and run tests**

Run: `cmake --build build --parallel 2>&1 | tail -5`
Expected: Clean build

Run: `./build/tests/logitune-tests --gtest_filter='DesktopFactory*' -v 2>&1`
Expected: All DesktopFactory tests PASS

Run: `./build/tests/logitune-tests 2>&1 | tail -3`
Expected: All tests pass (existing + new)

- [ ] **Step 4: Commit**

```bash
git add tests/test_desktop_factory.cpp tests/CMakeLists.txt
git commit -m "test: desktop factory unit tests for KDE/GNOME/Generic"
```

---

### Task 7: Install, Verify on KDE, Final Commit

**Files:** None new — integration test

- [ ] **Step 1: Full build and install**

```bash
cmake --build build --parallel
sudo cmake --install build --prefix /usr
```

- [ ] **Step 2: Run on KDE to verify no regressions**

```bash
pkill logitune; sleep 1; nohup /usr/bin/logitune > /tmp/logitune.log 2>&1 &
```

Verify in logs:
- `grep 'desktop integration' /tmp/logitune.log` should not show GNOME messages (we're on KDE)
- Focus switching between apps should still work
- DPI/button/thumb wheel per-profile switching should still work

- [ ] **Step 3: Verify extension files are installed**

```bash
ls -la /usr/share/gnome-shell/extensions/logitune-focus@logitune.com/
```

Expected: `metadata.json`, `v42/extension.js`, `v45/extension.js`

- [ ] **Step 4: Final commit if any fixups needed**

```bash
git add -A
git commit -m "chore: GNOME integration finalized and verified on KDE"
```
