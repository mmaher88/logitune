# Tray Multi-Device Sections Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `TrayManager`'s single hardcoded battery row with per-device sections: one header action (device name) + one battery action (`"Battery: N%"`, `⚡` suffix when charging) per connected device, separated by `QMenu` separators. Tooltip surfaces every connected device. Closes #83.

**Architecture:** `TrayManager` stores a `QMap<PhysicalDevice *, DeviceEntry>` with three `QAction *`s per device (header, battery, separator). Reacts to `DeviceModel::countChanged` to add/remove entries and to each `PhysicalDevice::stateChanged` to refresh battery text. No changes outside `TrayManager`, `tests/test_tray_manager.cpp`, and — if needed — the `MockDevice` / `DeviceSession` test stubs.

**Tech Stack:** Qt 6 (`QMenu`, `QAction`, `QSystemTrayIcon`), C++20, GTest. Existing files: `src/app/TrayManager.{h,cpp}`, `tests/test_tray_manager.cpp`.

---

## File Structure

### Files modified

- `src/app/TrayManager.h` — drop `m_batteryAction` + `batteryAction()`; add `DeviceEntry` struct, `m_deviceModel`, `m_entries`, three helper methods.
- `src/app/TrayManager.cpp` — full rewrite of the constructor body + new helpers.
- `tests/test_tray_manager.cpp` — rewrite the test class fixture around a shared `attachMockDevice` helper; update/add test cases per the spec.

### Files potentially modified

- `tests/mocks/MockDevice.h` — already exposes `m_features`; staging for battery happens via `DeviceSession::m_batteryLevel` / `m_batteryCharging` (public members per `DeviceSession.h:153-154`). Do not change unless grep uncovers that a member is private.

---

## Task 1: TrayManager refactor + test suite overhaul (one commit per spec)

**Files:**
- Modify: `src/app/TrayManager.h`
- Modify: `src/app/TrayManager.cpp`
- Modify: `tests/test_tray_manager.cpp`

- [ ] **Step 1: Rewrite `src/app/TrayManager.h`**

Replace the entire file content with:

```cpp
#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMap>

namespace logitune {

class DeviceModel;
class PhysicalDevice;

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(DeviceModel *dm, QObject *parent = nullptr);
    ~TrayManager() override;

    QSystemTrayIcon *trayIcon() { return &m_trayIcon; }
    QMenu *menu() { return &m_menu; }
    QAction *showAction() { return m_showAction; }
    QAction *quitAction() { return m_quitAction; }

    void show();

signals:
    void showWindowRequested();

private:
    struct DeviceEntry {
        QAction *header    = nullptr;  // disabled, shows device name
        QAction *battery   = nullptr;  // disabled, shows "Battery: N%"
        QAction *separator = nullptr;  // separator after the battery row
        QMetaObject::Connection stateConn;
    };

    void rebuildEntries();
    void refreshEntry(PhysicalDevice *device);
    void refreshTooltip();

    QSystemTrayIcon m_trayIcon;
    QMenu m_menu;
    QAction *m_showAction = nullptr;
    QAction *m_quitAction = nullptr;

    DeviceModel *m_deviceModel = nullptr;
    QMap<PhysicalDevice *, DeviceEntry> m_entries;
};

} // namespace logitune
```

The public API change: `QAction *batteryAction()` is gone. That method is only referenced from `tests/test_tray_manager.cpp`, which Task 1 Step 3 rewrites.

- [ ] **Step 2: Rewrite `src/app/TrayManager.cpp`**

Replace the entire file content with:

```cpp
#include "TrayManager.h"
#include "PhysicalDevice.h"
#include "models/DeviceModel.h"
#include <QIcon>

namespace logitune {

TrayManager::TrayManager(DeviceModel *dm, QObject *parent)
    : QObject(parent)
    , m_deviceModel(dm)
{
    m_trayIcon.setIcon(QIcon(":/Logitune/qml/assets/logitune-tray.svg"));

    // Skeleton: Show / separator / (device sections inserted here) / Quit
    m_showAction = m_menu.addAction(QStringLiteral("Show Logitune"));
    m_menu.addSeparator();
    m_quitAction = m_menu.addAction(QStringLiteral("Quit"));
    m_trayIcon.setContextMenu(&m_menu);

    connect(m_showAction, &QAction::triggered, this,
            &TrayManager::showWindowRequested);
    connect(&m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit showWindowRequested();
    });

    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::countChanged,
                this, &TrayManager::rebuildEntries);
        rebuildEntries();
    } else {
        refreshTooltip();
    }
}

TrayManager::~TrayManager()
{
    for (auto &entry : m_entries)
        QObject::disconnect(entry.stateConn);
}

void TrayManager::show()
{
    m_trayIcon.show();
}

void TrayManager::rebuildEntries()
{
    if (!m_deviceModel) {
        refreshTooltip();
        return;
    }

    const auto &devices = m_deviceModel->devices();

    // Remove entries for devices no longer present
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        PhysicalDevice *d = it.key();
        if (!devices.contains(d)) {
            m_menu.removeAction(it.value().separator);
            m_menu.removeAction(it.value().battery);
            m_menu.removeAction(it.value().header);
            delete it.value().separator;
            delete it.value().battery;
            delete it.value().header;
            QObject::disconnect(it.value().stateConn);
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }

    // Add entries for new devices, inserted before the trailing Quit action
    for (PhysicalDevice *d : devices) {
        if (m_entries.contains(d))
            continue;

        DeviceEntry entry;
        entry.header = new QAction(d->deviceName(), &m_menu);
        entry.header->setEnabled(false);
        entry.battery = new QAction(QStringLiteral("Battery: ---%"), &m_menu);
        entry.battery->setEnabled(false);
        entry.separator = new QAction(&m_menu);
        entry.separator->setSeparator(true);

        m_menu.insertAction(m_quitAction, entry.header);
        m_menu.insertAction(m_quitAction, entry.battery);
        m_menu.insertAction(m_quitAction, entry.separator);

        entry.stateConn = connect(d, &PhysicalDevice::stateChanged, this,
            [this, d]() {
                refreshEntry(d);
                refreshTooltip();
            });

        m_entries.insert(d, entry);
        refreshEntry(d);
    }

    refreshTooltip();
}

void TrayManager::refreshEntry(PhysicalDevice *device)
{
    auto it = m_entries.find(device);
    if (it == m_entries.end())
        return;

    it.value().header->setText(device->deviceName());

    const int level = device->batteryLevel();
    const bool charging = device->batteryCharging();
    QString text = QStringLiteral("Battery: %1%").arg(level);
    if (charging)
        text.append(QStringLiteral(" \u26A1"));  // ⚡
    it.value().battery->setText(text);
}

void TrayManager::refreshTooltip()
{
    if (!m_deviceModel || m_deviceModel->devices().isEmpty()) {
        m_trayIcon.setToolTip(QStringLiteral("Logitune"));
        return;
    }

    const auto &devices = m_deviceModel->devices();
    if (devices.size() == 1) {
        PhysicalDevice *d = devices.first();
        QString line = QStringLiteral("Logitune \u2014 %1: %2%")
            .arg(d->deviceName()).arg(d->batteryLevel());
        if (d->batteryCharging())
            line.append(QStringLiteral(" \u26A1"));
        m_trayIcon.setToolTip(line);
        return;
    }

    QStringList parts;
    parts.reserve(devices.size());
    for (PhysicalDevice *d : devices) {
        QString part = QStringLiteral("%1: %2%")
            .arg(d->deviceName()).arg(d->batteryLevel());
        if (d->batteryCharging())
            part.append(QStringLiteral(" \u26A1"));
        parts << part;
    }
    m_trayIcon.setToolTip(QStringLiteral("Logitune\n%1")
                          .arg(parts.join(QStringLiteral(" \u2022 "))));
}

} // namespace logitune
```

Points to verify with `grep` before this step:
- `m_trayIcon.setToolTip("Logitune - MX Master 3S");` was the only tooltip setter — run `grep -n 'setToolTip' src/app/TrayManager.cpp` pre-edit to confirm.
- No consumer outside `tests/test_tray_manager.cpp` calls `batteryAction()` — run `grep -rn 'batteryAction' src/ tests/` pre-edit to confirm.

- [ ] **Step 3: Rewrite `tests/test_tray_manager.cpp`**

Replace the entire file content with:

```cpp
#include <gtest/gtest.h>
#include <memory>
#include <QApplication>
#include <QSignalSpy>
#include <QAction>
#include <QMenu>

#include "TrayManager.h"
#include "PhysicalDevice.h"
#include "DeviceSession.h"
#include "hidpp/HidrawDevice.h"
#include "helpers/TestFixtures.h"
#include "mocks/MockDevice.h"
#include "models/DeviceModel.h"

using namespace logitune;
using namespace logitune::test;

namespace {

// Count actions in the menu excluding separators.
int nonSeparatorCount(QMenu *menu) {
    int n = 0;
    for (auto *a : menu->actions())
        if (!a->isSeparator()) ++n;
    return n;
}

// Find the first action matching a predicate.
template <typename Pred>
QAction* findAction(QMenu *menu, Pred pred) {
    for (auto *a : menu->actions())
        if (pred(a)) return a;
    return nullptr;
}

// Build a PhysicalDevice + DeviceSession pair backed by a MockDevice
// with a staged battery level / charging state, attach to the DeviceModel.
// Returns the PhysicalDevice for later removal.
PhysicalDevice* attachMockDevice(DeviceModel &model,
                                 MockDevice &mock,
                                 const QString &name,
                                 int batteryLevel,
                                 bool charging,
                                 const QString &serial)
{
    auto mockHidraw = std::make_unique<hidpp::HidrawDevice>("/dev/null");
    auto *session = new DeviceSession(std::move(mockHidraw), 0xFF,
                                      QStringLiteral("Bluetooth"),
                                      nullptr, &model);
    session->applySimulation(&mock, serial);
    session->m_deviceName = name;
    session->m_batteryLevel = batteryLevel;
    session->m_batteryCharging = charging;

    auto *device = new PhysicalDevice(serial, &model);
    device->attachTransport(session);
    model.addPhysicalDevice(device);
    return device;
}

} // namespace

TEST(TrayManager, ZeroDevicesOnlyShowsShowAndQuit) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    EXPECT_EQ(nonSeparatorCount(tray.menu()), 2);
    EXPECT_EQ(tray.showAction()->text(), QStringLiteral("Show Logitune"));
    EXPECT_EQ(tray.quitAction()->text(), QStringLiteral("Quit"));
}

TEST(TrayManager, ShowActionEmitsShowWindowRequested) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    QSignalSpy spy(&tray, &TrayManager::showWindowRequested);
    tray.showAction()->trigger();
    EXPECT_EQ(spy.count(), 1);
}

TEST(TrayManager, QuitActionExists) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);
    EXPECT_EQ(tray.quitAction()->text(), QStringLiteral("Quit"));
}

TEST(TrayManager, OneDeviceAddsHeaderAndBatterySection) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mock;
    mock.setupMxControls();
    attachMockDevice(dm, mock, QStringLiteral("Mock Master"),
                     80, false, QStringLiteral("mock-A"));

    // Show + Mock Master + Battery: 80% + Quit = 4 non-separator actions
    EXPECT_EQ(nonSeparatorCount(tray.menu()), 4);

    auto *header = findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Mock Master");
    });
    ASSERT_NE(header, nullptr);
    EXPECT_FALSE(header->isEnabled());

    auto *battery = findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 80%");
    });
    ASSERT_NE(battery, nullptr);
    EXPECT_FALSE(battery->isEnabled());
}

TEST(TrayManager, SecondDeviceAppendsSection) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                     80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    // Show + Device A + 80% + Device B + 45% + Quit = 6
    EXPECT_EQ(nonSeparatorCount(tray.menu()), 6);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 45%"); }), nullptr);
}

TEST(TrayManager, DeviceRemovedStripsSection) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                                  80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));
    ASSERT_EQ(nonSeparatorCount(tray.menu()), 6);

    // Simulate disconnect by flipping the session's connected flag and
    // emitting stateChanged; DeviceModel's handler calls removeRow which
    // fires countChanged which triggers rebuildEntries.
    auto *sessionA = qobject_cast<DeviceSession *>(devA->primary());
    ASSERT_NE(sessionA, nullptr);
    sessionA->m_connected = false;
    emit devA->stateChanged();

    EXPECT_EQ(nonSeparatorCount(tray.menu()), 4);
    EXPECT_EQ(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 45%"); }), nullptr);
}

TEST(TrayManager, BatteryChangeUpdatesMatchingEntryOnly) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    auto *devA = attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                                  80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    // Mutate device A's battery, fire its stateChanged
    auto *sessionA = qobject_cast<DeviceSession *>(devA->primary());
    ASSERT_NE(sessionA, nullptr);
    sessionA->m_batteryLevel = 12;
    emit devA->stateChanged();

    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 12%"); }), nullptr);
    EXPECT_EQ(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 80%"); }), nullptr);
    EXPECT_NE(findAction(tray.menu(), [](QAction *a) {
        return a->text() == QStringLiteral("Battery: 45%"); }), nullptr);
}

TEST(TrayManager, ChargingSuffixAppearsWhenCharging) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mock;
    mock.setupMxControls();
    attachMockDevice(dm, mock, QStringLiteral("Charging Mouse"),
                     60, true, QStringLiteral("mock-C"));

    auto *battery = findAction(tray.menu(), [](QAction *a) {
        return a->text().startsWith(QStringLiteral("Battery: 60%"));
    });
    ASSERT_NE(battery, nullptr);
    EXPECT_TRUE(battery->text().contains(QStringLiteral("\u26A1")));
}

TEST(TrayManager, TooltipReflectsAllDevices) {
    ensureApp();
    DeviceModel dm;
    TrayManager tray(&dm);

    MockDevice mockA, mockB;
    mockA.setupMxControls();
    mockB.setupMxControls();
    attachMockDevice(dm, mockA, QStringLiteral("Device A"),
                     80, false, QStringLiteral("mock-A"));
    attachMockDevice(dm, mockB, QStringLiteral("Device B"),
                     45, false, QStringLiteral("mock-B"));

    const QString expected = QStringLiteral(
        "Logitune\nDevice A: 80% \u2022 Device B: 45%");
    EXPECT_EQ(tray.trayIcon()->toolTip(), expected);
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: clean build. Qt AUTOMOC handles the new members.

- [ ] **Step 5: Run the tray tests**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests --gtest_filter='TrayManager.*' 2>&1 | tail -15`
Expected: 9 `TrayManager.*` tests, all pass.

If any fail:

- **Compile error on `session->applySimulation(&mock, serial)`** — confirm signature with `grep -n "applySimulation" src/core/DeviceSession.h`. Existing action-filter tests use this pattern.
- **Compile error on `sessionA->m_batteryLevel = 12;`** — the `DeviceSession` member is public (`src/core/DeviceSession.h:153`). If the test class is not a friend, assignment might still compile because the member is public. If not, use a setter if one exists, else escalate.
- **`nonSeparatorCount` returns wrong count** — likely an off-by-one in the menu rebuild (extra separator or a stale Quit action insert). Check the produced menu with a temporary `qDebug() << menu->actions()` during the failing test and fix the rebuild.
- **Tooltip test fails on the bullet glyph** — `\u2022` is U+2022 BULLET, `\u2014` is U+2014 EM DASH, `\u26A1` is U+26A1 HIGH VOLTAGE SIGN. Confirm the source file was written as UTF-8 (the code blocks in this plan use escape sequences, which `QStringLiteral` decodes the same way in both the source and the test).

- [ ] **Step 6: Run the full test suite**

Run: `QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3`
Expected: baseline + 9 - (whatever tray tests previously existed). Run before the refactor too to capture baseline:

```bash
git stash
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3  # note baseline
git stash pop
```

Also run the QML tests: `QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3` — expect 72/72 pass (unrelated to this change).

- [ ] **Step 7: Commit**

```bash
git add src/app/TrayManager.h src/app/TrayManager.cpp tests/test_tray_manager.cpp
git commit -m "feat(tray): per-device sections in the menu

Replace the single hardcoded battery row with one header+battery
section per connected device, separated by QMenu separators.
Tooltip surfaces every device at a glance. Reactively tracks
DeviceModel::countChanged (add/remove) and each
PhysicalDevice::stateChanged (battery/name updates). Removes
the hardcoded 'Logitune - MX Master 3S' tooltip literal.

Closes #83."
```

---

## Task 2: Final verification

**Files:**
- None (verification only).

- [ ] **Step 1: Full build + test pass**

```bash
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests 2>&1 | tail -3
QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests 2>&1 | tail -3
```

Expected: both green; core test count up by +9 - (prior tray-test count, which was 6 at spec time; net +3).

- [ ] **Step 2: Smoke-launch (simulate-all)**

```bash
pkill -9 -f '/usr/bin/logitune\|build/src/app/logitune' 2>/dev/null
rm -f /tmp/logitune.lock /tmp/logitune-sim.log
nohup ./build/src/app/logitune --simulate-all > /tmp/logitune-sim.log 2>&1 & disown
sleep 4
ps aux | grep -v grep | grep 'build/src/app/logitune' || echo not-running
grep -iE "WARN|Error" /tmp/logitune-sim.log | head -5
```

Right-click the tray icon and confirm: the menu has a "Show Logitune" entry at top, one sub-section per simulated device (name + battery line), separators between each, and "Quit" at the bottom. Tooltip text matches the multi-device format.

Kill after: `pkill -9 -f 'build/src/app/logitune'`.

- [ ] **Step 3: Push the branch**

```bash
git push -u origin fix-tray-multi-device-sections
```

- [ ] **Step 4: STOP — do NOT open the PR directly**

The controller session drafts the PR body in conversation for user review. Return to the controller with:

- Commit SHA of the single code commit.
- Final test count.
- PR is NOT opened; branch is pushed and ready for the user-review step.

---

## Self-review summary

Every spec section maps to a task step:

- **Behaviour (per-device sections, connect/disconnect, battery update)** → Task 1 Step 2 (`rebuildEntries`, `refreshEntry`).
- **Tooltip (zero/one/multiple)** → Task 1 Step 2 (`refreshTooltip`).
- **Architecture (DeviceEntry, m_entries)** → Task 1 Step 1 + Step 2.
- **Tests (eight cases, removed one, renamed one)** → Task 1 Step 3.

No placeholders. All code blocks are complete. The helper `attachMockDevice` returns `PhysicalDevice *` and is used consistently across tests that need device removal. Method names (`rebuildEntries`, `refreshEntry`, `refreshTooltip`) match across the header and implementation. The `QAction *batteryAction()` removal is acknowledged in both the header rewrite and the test overhaul.
