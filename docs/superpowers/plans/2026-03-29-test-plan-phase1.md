# Test Plan Phase 1: Foundation + Unit Tests

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set up testable architecture (DI refactor, mocks, helpers) and add ~145 unit tests covering every model, parser, and data type.

**Architecture:** Extract models + AppController into `logitune-app-lib` static library so tests can link against it. Add mock implementations of all interfaces. Refactor AppController to accept injected dependencies. Write comprehensive unit tests for all parsers, models, and data types.

**Tech Stack:** GTest, Qt6 Core/Test, QSignalSpy

---

### Task 1: Extract `logitune-app-lib` static library

The models and AppController live in the `logitune` executable target. Tests can't link against an executable. Extract them into a static library.

**Files:**
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create logitune-app-lib in CMakeLists.txt**

Replace `src/app/CMakeLists.txt` with:

```cmake
# Static library for models + AppController (testable)
add_library(logitune-app-lib STATIC
    AppController.cpp
    models/DeviceModel.cpp
    models/ButtonModel.cpp
    models/ActionModel.cpp
    models/ProfileModel.cpp
)
target_include_directories(logitune-app-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)
target_link_libraries(logitune-app-lib PUBLIC logitune-core Qt6::Quick)

# Main executable — just main.cpp + QML
qt_add_executable(logitune main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(logitune
    URI Logitune
    VERSION 1.0
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/HomeView.qml
        qml/DeviceView.qml
        qml/components/SideNav.qml
        qml/components/BatteryChip.qml
        qml/components/LogituneSlider.qml
        qml/components/LogituneToggle.qml
        qml/components/InfoCallout.qml
        qml/components/DetailPanel.qml
        qml/components/KeystrokeCapture.qml
        qml/components/ButtonCallout.qml
        qml/components/ActionsPanel.qml
        qml/components/DeviceRender.qml
        qml/components/AppProfileBar.qml
        qml/components/Toast.qml
        qml/pages/PointScrollPage.qml
        qml/pages/ButtonsPage.qml
        qml/pages/EasySwitchPage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        qml/assets/mx-master-3s.png
        qml/assets/mx-master-3s-back.png
        qml/assets/mx-master-3s-side.png
)

target_link_libraries(logitune PRIVATE logitune-app-lib Qt6::Quick Qt6::Svg Qt6::DBus Qt6::Widgets)

install(TARGETS logitune DESTINATION bin)
```

- [ ] **Step 2: Update tests/CMakeLists.txt to link app-lib**

Replace `tests/CMakeLists.txt` with:

```cmake
find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)
include(GoogleTest)

add_executable(logitune-tests
    # Existing tests
    test_smoke.cpp
    test_transport.cpp
    test_feature_dispatcher.cpp
    test_scroll_features.cpp
    test_features.cpp
    test_button_features.cpp
    test_profile_engine.cpp
    test_device_discovery.cpp
    test_action_executor.cpp
    test_device_registry.cpp
    # New unit tests (Phase 1)
    test_keystroke_parser.cpp
    test_button_action.cpp
    test_button_model.cpp
    test_profile_model.cpp
    test_action_model.cpp
    test_device_model.cpp
    test_wmclass_resolution.cpp
)

target_include_directories(logitune-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(logitune-tests PRIVATE
    logitune-app-lib
    logitune-core
    GTest::gtest_main
    Qt6::Core
    Qt6::Test
)

gtest_discover_tests(logitune-tests)
```

- [ ] **Step 3: Build to verify extraction works**

Run: `cmake --build build --parallel 2>&1 | tail -10`
Expected: Both `logitune` and `logitune-tests` link successfully, all 133 existing tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/app/CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: extract logitune-app-lib for testable models and AppController"
```

---

### Task 2: Mock library and test helpers

**Files:**
- Create: `tests/mocks/MockDesktop.h`
- Create: `tests/mocks/MockInjector.h`
- Create: `tests/mocks/MockTransport.h`
- Create: `tests/mocks/MockDevice.h`
- Create: `tests/helpers/TestFixtures.h`

- [ ] **Step 1: Create MockDesktop.h**

```cpp
#pragma once
#include "interfaces/IDesktopIntegration.h"

namespace logitune::test {

class MockDesktop : public IDesktopIntegration {
    Q_OBJECT
public:
    using IDesktopIntegration::IDesktopIntegration;

    void start() override {}
    bool available() const override { return true; }
    QString desktopName() const override { return QStringLiteral("Mock"); }
    QStringList detectedCompositors() const override { return {QStringLiteral("MockWM")}; }
    void blockGlobalShortcuts(bool) override { m_blocked++; }
    QVariantList runningApplications() const override { return m_apps; }

    // Test helpers
    void simulateFocus(const QString &wmClass, const QString &title = {}) {
        emit activeWindowChanged(wmClass, title);
    }
    void setRunningApps(const QVariantList &apps) { m_apps = apps; }

    int m_blocked = 0;
    QVariantList m_apps;
};

} // namespace logitune::test
```

- [ ] **Step 2: Create MockInjector.h**

```cpp
#pragma once
#include "interfaces/IInputInjector.h"
#include <QVector>

namespace logitune::test {

class MockInjector : public IInputInjector {
    Q_OBJECT
public:
    using IInputInjector::IInputInjector;

    bool init() override { return true; }

    void injectKeystroke(const QString &combo) override {
        m_calls.append({QStringLiteral("keystroke"), combo});
    }
    void injectCtrlScroll(int direction) override {
        m_calls.append({QStringLiteral("ctrlscroll"), QString::number(direction)});
    }
    void sendDBusCall(const QString &spec) override {
        m_calls.append({QStringLiteral("dbus"), spec});
    }
    void launchApp(const QString &command) override {
        m_calls.append({QStringLiteral("launch"), command});
    }

    struct Call { QString method; QString arg; };
    QVector<Call> m_calls;

    void clear() { m_calls.clear(); }
    bool hasCalled(const QString &method) const {
        for (const auto &c : m_calls)
            if (c.method == method) return true;
        return false;
    }
    QString lastArg(const QString &method) const {
        for (int i = m_calls.size() - 1; i >= 0; --i)
            if (m_calls[i].method == method) return m_calls[i].arg;
        return {};
    }
};

} // namespace logitune::test
```

- [ ] **Step 3: Create MockTransport.h**

```cpp
#pragma once
#include "interfaces/ITransport.h"
#include <QMap>

namespace logitune::test {

class MockTransport : public ITransport {
    Q_OBJECT
public:
    using ITransport::ITransport;

    std::optional<hidpp::Report> sendRequest(
        const hidpp::Report &req, int /*timeoutMs*/ = 2000) override
    {
        m_sentRequests.append(req);
        auto it = m_responses.find(req.featureIndex);
        if (it != m_responses.end()) return *it;
        return std::nullopt;
    }

    int notificationFd() const override { return -1; }
    std::vector<uint8_t> readRawReport(int /*timeoutMs*/ = 0) override { return {}; }

    // Test helpers
    void setResponse(uint8_t featureIndex, const hidpp::Report &resp) {
        m_responses[featureIndex] = resp;
    }
    void simulateNotification(const hidpp::Report &report) {
        emit notificationReceived(report);
    }

    QMap<uint8_t, hidpp::Report> m_responses;
    QVector<hidpp::Report> m_sentRequests;
};

} // namespace logitune::test
```

- [ ] **Step 4: Create MockDevice.h**

```cpp
#pragma once
#include "interfaces/IDevice.h"

namespace logitune::test {

class MockDevice : public IDevice {
public:
    QString deviceName() const override { return m_name; }
    std::vector<uint16_t> productIds() const override { return m_pids; }
    bool matchesPid(uint16_t pid) const override {
        return std::find(m_pids.begin(), m_pids.end(), pid) != m_pids.end();
    }
    QList<ControlDescriptor> controls() const override { return m_controls; }
    QList<HotspotDescriptor> buttonHotspots() const override { return {}; }
    QList<HotspotDescriptor> scrollHotspots() const override { return {}; }
    FeatureSupport features() const override { return m_features; }
    QString frontImagePath() const override { return {}; }
    QString sideImagePath() const override { return {}; }
    QString backImagePath() const override { return {}; }
    QMap<QString, ButtonAction> defaultGestures() const override { return m_gestures; }
    int minDpi() const override { return 200; }
    int maxDpi() const override { return 8000; }
    int dpiStep() const override { return 50; }
    int easySwitchSlots() const override { return 3; }

    // Configurable test data
    QString m_name = QStringLiteral("Mock Mouse");
    std::vector<uint16_t> m_pids = {0x4082};
    QList<ControlDescriptor> m_controls;
    FeatureSupport m_features;
    QMap<QString, ButtonAction> m_gestures;

    // Helper: build a standard 8-button MX-style control set
    void setupMxControls() {
        m_controls = {
            {0x0050, 0, QStringLiteral("Left click"),   QStringLiteral("default"), false},
            {0x0051, 1, QStringLiteral("Right click"),  QStringLiteral("default"), false},
            {0x0052, 2, QStringLiteral("Middle click"), QStringLiteral("default"), true},
            {0x0053, 3, QStringLiteral("Back"),         QStringLiteral("default"), true},
            {0x0056, 4, QStringLiteral("Forward"),      QStringLiteral("default"), true},
            {0x00C3, 5, QStringLiteral("Gesture"),      QStringLiteral("gesture-trigger"), true},
            {0x00C4, 6, QStringLiteral("Top"),          QStringLiteral("smartshift-toggle"), true},
            {0x0000, 7, QStringLiteral("Thumb wheel"),  QStringLiteral("default"), true},
        };
    }
};

} // namespace logitune::test
```

- [ ] **Step 5: Create TestFixtures.h**

```cpp
#pragma once
#include <QCoreApplication>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include "ProfileEngine.h"

namespace logitune::test {

// Ensures QCoreApplication exists for the test binary lifetime
inline QCoreApplication &ensureApp() {
    static int argc = 1;
    static char arg0[] = "logitune-tests";
    static char *argv[] = {arg0, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// Base fixture with a temp dir for profile files
class ProfileFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(m_tmpDir.isValid());
        m_profilesDir = m_tmpDir.path();
    }

    QString m_profilesDir;

    Profile makeDefaultProfile() {
        Profile p;
        p.name = QStringLiteral("default");
        p.dpi = 1000;
        p.smartShiftEnabled = true;
        p.smartShiftThreshold = 128;
        p.hiResScroll = true;
        p.scrollDirection = QStringLiteral("standard");
        p.thumbWheelMode = QStringLiteral("scroll");
        return p;
    }

private:
    QTemporaryDir m_tmpDir;
};

} // namespace logitune::test
```

- [ ] **Step 6: Build to verify mocks compile**

Create a minimal test file `tests/test_smoke_mocks.cpp` to verify:

```cpp
#include <gtest/gtest.h>
#include "MockDesktop.h"
#include "MockInjector.h"
#include "MockTransport.h"
#include "MockDevice.h"
#include "TestFixtures.h"

using namespace logitune::test;

TEST(Mocks, DesktopCompiles) {
    ensureApp();
    MockDesktop d;
    EXPECT_TRUE(d.available());
}

TEST(Mocks, InjectorRecordsCalls) {
    ensureApp();
    MockInjector inj;
    inj.injectKeystroke("Ctrl+C");
    EXPECT_EQ(inj.m_calls.size(), 1);
    EXPECT_EQ(inj.m_calls[0].arg, "Ctrl+C");
}

TEST(Mocks, DeviceSetupMxControls) {
    MockDevice dev;
    dev.setupMxControls();
    EXPECT_EQ(dev.controls().size(), 8);
}
```

Add `test_smoke_mocks.cpp` to tests/CMakeLists.txt. Build and run:

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="Mocks.*"`
Expected: 3 tests pass.

- [ ] **Step 7: Commit**

```bash
git add tests/mocks/ tests/helpers/ tests/test_smoke_mocks.cpp tests/CMakeLists.txt
git commit -m "test: add mock library and test helpers"
```

---

### Task 3: AppController dependency injection

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `src/app/main.cpp`

- [ ] **Step 1: Add DI constructor to AppController.h**

Change the constructor and add a member pointer for the injected desktop integration:

```cpp
// Replace the existing constructor and member declarations:
public:
    explicit AppController(QObject *parent = nullptr);
    AppController(IDesktopIntegration *desktop, IInputInjector *injector,
                  QObject *parent = nullptr);

// Replace subsystem members:
private:
    // Owned (created internally when not injected)
    std::unique_ptr<KDeDesktop> m_ownedDesktop;
    std::unique_ptr<UinputInjector> m_ownedInjector;

    // Active pointers (either injected or owned)
    IDesktopIntegration *m_desktop = nullptr;
    IInputInjector *m_injector = nullptr;

    // Non-injectable subsystems
    DeviceRegistry m_registry;
    DeviceManager  m_deviceManager;
    DeviceModel    m_deviceModel;
    ButtonModel    m_buttonModel;
    ActionModel    m_actionModel;
    ProfileModel   m_profileModel;
    ProfileEngine  m_profileEngine;
    ActionExecutor m_actionExecutor;
```

- [ ] **Step 2: Implement DI constructors in AppController.cpp**

```cpp
AppController::AppController(QObject *parent)
    : AppController(nullptr, nullptr, parent)
{
}

AppController::AppController(IDesktopIntegration *desktop, IInputInjector *injector,
                              QObject *parent)
    : QObject(parent)
    , m_deviceManager(&m_registry)
    , m_actionExecutor(nullptr)  // set below
{
    if (desktop) {
        m_desktop = desktop;
    } else {
        m_ownedDesktop = std::make_unique<KDeDesktop>(this);
        m_desktop = m_ownedDesktop.get();
    }

    if (injector) {
        m_injector = injector;
    } else {
        m_ownedInjector = std::make_unique<UinputInjector>(this);
        m_injector = m_ownedInjector.get();
    }

    m_actionExecutor.setInjector(m_injector);
}
```

- [ ] **Step 3: Update all references from `m_windowTracker`/`m_injector` to `m_desktop`/`m_injector`**

In `init()`:
- `m_deviceModel.setDesktopIntegration(m_desktop);` (was `&m_windowTracker`)
- `m_injector->init()` check (was `m_injector.init()`)

In `wireSignals()`:
- `connect(m_desktop, &IDesktopIntegration::activeWindowChanged, ...)` (was `&m_windowTracker`)

In `startMonitoring()`:
- `m_desktop->start()` (was `m_windowTracker.start()`)

In `onDeviceSetupComplete()`:
- `m_desktop->runningApplications()` (was `m_windowTracker.runningApplications()`)

- [ ] **Step 4: Update ActionExecutor to accept injected injector**

Add `void setInjector(IInputInjector *inj)` to ActionExecutor if not already present. The ActionExecutor should call `m_injector->injectKeystroke()` etc. instead of directly calling `UinputInjector`.

- [ ] **Step 5: Verify main.cpp still works (no changes needed if default constructor is used)**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests`
Expected: All 136 tests pass (133 existing + 3 mock smoke).

Run the app: `./build/src/app/logitune`
Expected: App launches and functions identically.

- [ ] **Step 6: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp src/app/main.cpp src/core/ActionExecutor.h src/core/ActionExecutor.cpp
git commit -m "refactor: AppController accepts injected dependencies for testability"
```

---

### Task 4: test_keystroke_parser.cpp

Tests `UinputInjector::parseKeystroke()` — static method, no I/O needed.

**Files:**
- Create: `tests/test_keystroke_parser.cpp`

- [ ] **Step 1: Write all keystroke parser tests**

```cpp
#include <gtest/gtest.h>
#include "input/UinputInjector.h"
#include <linux/input-event-codes.h>

using namespace logitune;
using KS = UinputInjector;

// --- Single letters ---
TEST(KeystrokeParser, SingleLetterA) { EXPECT_EQ(KS::parseKeystroke("A"), std::vector<int>{KEY_A}); }
TEST(KeystrokeParser, SingleLetterZ) { EXPECT_EQ(KS::parseKeystroke("Z"), std::vector<int>{KEY_Z}); }
TEST(KeystrokeParser, LowercaseLetter) { EXPECT_EQ(KS::parseKeystroke("a"), std::vector<int>{KEY_A}); }

// --- Digits ---
TEST(KeystrokeParser, Digit0) { EXPECT_EQ(KS::parseKeystroke("0"), std::vector<int>{KEY_0}); }
TEST(KeystrokeParser, Digit1) { EXPECT_EQ(KS::parseKeystroke("1"), std::vector<int>{KEY_1}); }
TEST(KeystrokeParser, Digit9) { EXPECT_EQ(KS::parseKeystroke("9"), std::vector<int>{KEY_9}); }

// --- Modifiers ---
TEST(KeystrokeParser, Ctrl) { EXPECT_EQ(KS::parseKeystroke("Ctrl"), std::vector<int>{KEY_LEFTCTRL}); }
TEST(KeystrokeParser, Shift) { EXPECT_EQ(KS::parseKeystroke("Shift"), std::vector<int>{KEY_LEFTSHIFT}); }
TEST(KeystrokeParser, Alt) { EXPECT_EQ(KS::parseKeystroke("Alt"), std::vector<int>{KEY_LEFTALT}); }
TEST(KeystrokeParser, Super) { EXPECT_EQ(KS::parseKeystroke("Super"), std::vector<int>{KEY_LEFTMETA}); }
TEST(KeystrokeParser, Meta) { EXPECT_EQ(KS::parseKeystroke("Meta"), std::vector<int>{KEY_LEFTMETA}); }

// --- Combos ---
TEST(KeystrokeParser, CtrlC) {
    auto keys = KS::parseKeystroke("Ctrl+C");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_C);
}
TEST(KeystrokeParser, CtrlShiftZ) {
    auto keys = KS::parseKeystroke("Ctrl+Shift+Z");
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_LEFTSHIFT);
    EXPECT_EQ(keys[2], KEY_Z);
}
TEST(KeystrokeParser, AltF4) {
    auto keys = KS::parseKeystroke("Alt+F4");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTALT);
    EXPECT_EQ(keys[1], KEY_F4);
}
TEST(KeystrokeParser, SuperD) {
    auto keys = KS::parseKeystroke("Super+D");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTMETA);
    EXPECT_EQ(keys[1], KEY_D);
}
TEST(KeystrokeParser, CtrlSuperLeft) {
    auto keys = KS::parseKeystroke("Ctrl+Super+Left");
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_LEFTMETA);
    EXPECT_EQ(keys[2], KEY_LEFT);
}

// --- Special keys ---
TEST(KeystrokeParser, Tab) { EXPECT_EQ(KS::parseKeystroke("Tab"), std::vector<int>{KEY_TAB}); }
TEST(KeystrokeParser, Space) { EXPECT_EQ(KS::parseKeystroke("Space"), std::vector<int>{KEY_SPACE}); }
TEST(KeystrokeParser, Enter) { EXPECT_EQ(KS::parseKeystroke("Enter"), std::vector<int>{KEY_ENTER}); }
TEST(KeystrokeParser, Escape) { EXPECT_EQ(KS::parseKeystroke("Escape"), std::vector<int>{KEY_ESC}); }
TEST(KeystrokeParser, Delete) { EXPECT_EQ(KS::parseKeystroke("Delete"), std::vector<int>{KEY_DELETE}); }
TEST(KeystrokeParser, Back) { EXPECT_EQ(KS::parseKeystroke("Back"), std::vector<int>{KEY_BACK}); }
TEST(KeystrokeParser, Forward) { EXPECT_EQ(KS::parseKeystroke("Forward"), std::vector<int>{KEY_FORWARD}); }
TEST(KeystrokeParser, Home) { EXPECT_EQ(KS::parseKeystroke("Home"), std::vector<int>{KEY_HOME}); }
TEST(KeystrokeParser, End) { EXPECT_EQ(KS::parseKeystroke("End"), std::vector<int>{KEY_END}); }
TEST(KeystrokeParser, PageUp) { EXPECT_EQ(KS::parseKeystroke("PageUp"), std::vector<int>{KEY_PAGEUP}); }
TEST(KeystrokeParser, PageDown) { EXPECT_EQ(KS::parseKeystroke("PageDown"), std::vector<int>{KEY_PAGEDOWN}); }
TEST(KeystrokeParser, Print) { EXPECT_EQ(KS::parseKeystroke("Print"), std::vector<int>{KEY_SYSRQ}); }

// --- Arrow keys ---
TEST(KeystrokeParser, Up) { EXPECT_EQ(KS::parseKeystroke("Up"), std::vector<int>{KEY_UP}); }
TEST(KeystrokeParser, Down) { EXPECT_EQ(KS::parseKeystroke("Down"), std::vector<int>{KEY_DOWN}); }
TEST(KeystrokeParser, Left) { EXPECT_EQ(KS::parseKeystroke("Left"), std::vector<int>{KEY_LEFT}); }
TEST(KeystrokeParser, Right) { EXPECT_EQ(KS::parseKeystroke("Right"), std::vector<int>{KEY_RIGHT}); }

// --- Media keys ---
TEST(KeystrokeParser, Mute) { EXPECT_EQ(KS::parseKeystroke("Mute"), std::vector<int>{KEY_MUTE}); }
TEST(KeystrokeParser, Play) { EXPECT_EQ(KS::parseKeystroke("Play"), std::vector<int>{KEY_PLAYPAUSE}); }
TEST(KeystrokeParser, VolumeUp) { EXPECT_EQ(KS::parseKeystroke("VolumeUp"), std::vector<int>{KEY_VOLUMEUP}); }
TEST(KeystrokeParser, VolumeDown) { EXPECT_EQ(KS::parseKeystroke("VolumeDown"), std::vector<int>{KEY_VOLUMEDOWN}); }
TEST(KeystrokeParser, BrightnessUp) { EXPECT_EQ(KS::parseKeystroke("BrightnessUp"), std::vector<int>{KEY_BRIGHTNESSUP}); }
TEST(KeystrokeParser, BrightnessDown) { EXPECT_EQ(KS::parseKeystroke("BrightnessDown"), std::vector<int>{KEY_BRIGHTNESSDOWN}); }

// --- F keys ---
TEST(KeystrokeParser, F1) { EXPECT_EQ(KS::parseKeystroke("F1"), std::vector<int>{KEY_F1}); }
TEST(KeystrokeParser, F5) { EXPECT_EQ(KS::parseKeystroke("F5"), std::vector<int>{KEY_F5}); }
TEST(KeystrokeParser, F11) { EXPECT_EQ(KS::parseKeystroke("F11"), std::vector<int>{KEY_F11}); }
TEST(KeystrokeParser, F12) { EXPECT_EQ(KS::parseKeystroke("F12"), std::vector<int>{KEY_F12}); }

// --- Symbols ---
TEST(KeystrokeParser, Minus) { EXPECT_EQ(KS::parseKeystroke("-"), std::vector<int>{KEY_MINUS}); }
TEST(KeystrokeParser, Equal) { EXPECT_EQ(KS::parseKeystroke("="), std::vector<int>{KEY_EQUAL}); }
TEST(KeystrokeParser, Comma) { EXPECT_EQ(KS::parseKeystroke(","), std::vector<int>{KEY_COMMA}); }
TEST(KeystrokeParser, Period) { EXPECT_EQ(KS::parseKeystroke("."), std::vector<int>{KEY_DOT}); }
TEST(KeystrokeParser, Slash) { EXPECT_EQ(KS::parseKeystroke("/"), std::vector<int>{KEY_SLASH}); }
TEST(KeystrokeParser, Backslash) { EXPECT_EQ(KS::parseKeystroke("\\"), std::vector<int>{KEY_BACKSLASH}); }
TEST(KeystrokeParser, Semicolon) { EXPECT_EQ(KS::parseKeystroke(";"), std::vector<int>{KEY_SEMICOLON}); }
TEST(KeystrokeParser, Apostrophe) { EXPECT_EQ(KS::parseKeystroke("'"), std::vector<int>{KEY_APOSTROPHE}); }
TEST(KeystrokeParser, Grave) { EXPECT_EQ(KS::parseKeystroke("`"), std::vector<int>{KEY_GRAVE}); }
TEST(KeystrokeParser, LeftBrace) { EXPECT_EQ(KS::parseKeystroke("["), std::vector<int>{KEY_LEFTBRACE}); }
TEST(KeystrokeParser, RightBrace) { EXPECT_EQ(KS::parseKeystroke("]"), std::vector<int>{KEY_RIGHTBRACE}); }

// --- Edge cases ---
TEST(KeystrokeParser, BarePlus) { EXPECT_EQ(KS::parseKeystroke("+"), std::vector<int>{KEY_KPPLUS}); }
TEST(KeystrokeParser, EmptyString) { EXPECT_TRUE(KS::parseKeystroke("").empty()); }
TEST(KeystrokeParser, UnknownKey) { EXPECT_TRUE(KS::parseKeystroke("FooBar").empty()); }
TEST(KeystrokeParser, WhitespaceInCombo) {
    auto keys = KS::parseKeystroke("Ctrl + C");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], KEY_LEFTCTRL);
    EXPECT_EQ(keys[1], KEY_C);
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="KeystrokeParser.*"`
Expected: All ~30 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_keystroke_parser.cpp tests/CMakeLists.txt
git commit -m "test: comprehensive keystroke parser tests — all key names, combos, edge cases"
```

---

### Task 5: test_button_action.cpp

Tests `ButtonAction::parse()` and `ButtonAction::serialize()` round-trips.

**Files:**
- Create: `tests/test_button_action.cpp`

- [ ] **Step 1: Write all ButtonAction tests**

```cpp
#include <gtest/gtest.h>
#include "ButtonAction.h"

using namespace logitune;

// --- parse ---
TEST(ButtonAction, ParseDefault) { auto a = ButtonAction::parse("default"); EXPECT_EQ(a.type, ButtonAction::Default); }
TEST(ButtonAction, ParseEmpty) { auto a = ButtonAction::parse(""); EXPECT_EQ(a.type, ButtonAction::Default); }
TEST(ButtonAction, ParseGestureTrigger) { auto a = ButtonAction::parse("gesture-trigger"); EXPECT_EQ(a.type, ButtonAction::GestureTrigger); }
TEST(ButtonAction, ParseSmartShiftToggle) {
    auto a = ButtonAction::parse("smartshift-toggle");
    EXPECT_EQ(a.type, ButtonAction::SmartShiftToggle);
    EXPECT_TRUE(a.payload.isEmpty());
}
TEST(ButtonAction, ParseKeystroke) {
    auto a = ButtonAction::parse("keystroke:Ctrl+C");
    EXPECT_EQ(a.type, ButtonAction::Keystroke);
    EXPECT_EQ(a.payload, "Ctrl+C");
}
TEST(ButtonAction, ParseMedia) {
    auto a = ButtonAction::parse("media:VolumeUp");
    EXPECT_EQ(a.type, ButtonAction::Media);
    EXPECT_EQ(a.payload, "VolumeUp");
}
TEST(ButtonAction, ParseDBus) {
    auto a = ButtonAction::parse("dbus:org.kde.kwin /KWin toggle");
    EXPECT_EQ(a.type, ButtonAction::DBus);
}
TEST(ButtonAction, ParseAppLaunch) {
    auto a = ButtonAction::parse("app-launch:kcalc");
    EXPECT_EQ(a.type, ButtonAction::AppLaunch);
    EXPECT_EQ(a.payload, "kcalc");
}
TEST(ButtonAction, ParseUnknownPrefix) {
    auto a = ButtonAction::parse("foobar:test");
    EXPECT_EQ(a.type, ButtonAction::Default);
}
TEST(ButtonAction, ParseNoColon) {
    auto a = ButtonAction::parse("something");
    EXPECT_EQ(a.type, ButtonAction::Default);
}

// --- Legacy migration ---
TEST(ButtonAction, ParseLegacySmartShift) {
    // Old profiles stored "keystroke:smartshift-toggle" — should migrate
    auto a = ButtonAction::parse("keystroke:smartshift-toggle");
    EXPECT_EQ(a.type, ButtonAction::SmartShiftToggle);
    EXPECT_TRUE(a.payload.isEmpty());
}

// --- serialize ---
TEST(ButtonAction, SerializeDefault) { EXPECT_EQ(ButtonAction{ButtonAction::Default, {}}.serialize(), "default"); }
TEST(ButtonAction, SerializeGestureTrigger) { EXPECT_EQ(ButtonAction{ButtonAction::GestureTrigger, {}}.serialize(), "gesture-trigger"); }
TEST(ButtonAction, SerializeSmartShiftToggle) { EXPECT_EQ(ButtonAction{ButtonAction::SmartShiftToggle, {}}.serialize(), "smartshift-toggle"); }
TEST(ButtonAction, SerializeKeystroke) { EXPECT_EQ(ButtonAction{ButtonAction::Keystroke, "Alt+F4"}.serialize(), "keystroke:Alt+F4"); }
TEST(ButtonAction, SerializeAppLaunch) { EXPECT_EQ(ButtonAction{ButtonAction::AppLaunch, "firefox"}.serialize(), "app-launch:firefox"); }

// --- Round-trip ---
TEST(ButtonAction, RoundTripKeystroke) {
    ButtonAction orig{ButtonAction::Keystroke, "Ctrl+Shift+Z"};
    EXPECT_EQ(ButtonAction::parse(orig.serialize()), orig);
}
TEST(ButtonAction, RoundTripSmartShift) {
    ButtonAction orig{ButtonAction::SmartShiftToggle, {}};
    EXPECT_EQ(ButtonAction::parse(orig.serialize()), orig);
}
TEST(ButtonAction, RoundTripGesture) {
    ButtonAction orig{ButtonAction::GestureTrigger, {}};
    EXPECT_EQ(ButtonAction::parse(orig.serialize()), orig);
}
TEST(ButtonAction, RoundTripAppLaunch) {
    ButtonAction orig{ButtonAction::AppLaunch, "kcalc"};
    EXPECT_EQ(ButtonAction::parse(orig.serialize()), orig);
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="ButtonAction.*"`
Expected: All ~20 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_button_action.cpp
git commit -m "test: ButtonAction parse/serialize round-trip tests for all types"
```

---

### Task 6: test_button_model.cpp

**Files:**
- Create: `tests/test_button_model.cpp`

- [ ] **Step 1: Write ButtonModel tests**

```cpp
#include <gtest/gtest.h>
#include <QSignalSpy>
#include "ButtonModel.h"
#include "TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class ButtonModelTest : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }
    ButtonModel model;
};

TEST_F(ButtonModelTest, InitialRowCount) { EXPECT_EQ(model.rowCount(), 8); }

TEST_F(ButtonModelTest, InitialDefaults) {
    EXPECT_EQ(model.actionNameForButton(0), "Left click");
    EXPECT_EQ(model.actionTypeForButton(0), "default");
    EXPECT_EQ(model.actionNameForButton(5), "Gestures");
    EXPECT_EQ(model.actionTypeForButton(5), "gesture-trigger");
}

TEST_F(ButtonModelTest, SetActionEmitsDataChanged) {
    QSignalSpy spy(&model, &ButtonModel::dataChanged);
    model.setAction(3, "Copy", "keystroke");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ButtonModelTest, SetActionEmitsUserActionChanged) {
    QSignalSpy spy(&model, &ButtonModel::userActionChanged);
    model.setAction(3, "Copy", "keystroke");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toInt(), 3);
    EXPECT_EQ(spy[0][1].toString(), "Copy");
    EXPECT_EQ(spy[0][2].toString(), "keystroke");
}

TEST_F(ButtonModelTest, SetActionUpdatesData) {
    model.setAction(3, "Copy", "keystroke");
    EXPECT_EQ(model.actionNameForButton(3), "Copy");
    EXPECT_EQ(model.actionTypeForButton(3), "keystroke");
}

TEST_F(ButtonModelTest, LoadFromProfileEmitsDataChanged) {
    QSignalSpy spy(&model, &ButtonModel::dataChanged);
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({"action" + QString::number(i), "keystroke"});
    model.loadFromProfile(buttons);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(ButtonModelTest, LoadFromProfileDoesNotEmitUserActionChanged) {
    QSignalSpy spy(&model, &ButtonModel::userActionChanged);
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({"action" + QString::number(i), "keystroke"});
    model.loadFromProfile(buttons);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ButtonModelTest, LoadFromProfileUpdatesData) {
    QList<QPair<QString, QString>> buttons;
    for (int i = 0; i < 8; ++i)
        buttons.append({"action" + QString::number(i), "type" + QString::number(i)});
    model.loadFromProfile(buttons);
    EXPECT_EQ(model.actionNameForButton(3), "action3");
    EXPECT_EQ(model.actionTypeForButton(3), "type3");
}

TEST_F(ButtonModelTest, LookupNonexistentButton) {
    EXPECT_TRUE(model.actionNameForButton(99).isEmpty());
    EXPECT_TRUE(model.actionTypeForButton(99).isEmpty());
}

TEST_F(ButtonModelTest, DataRoles) {
    auto idx = model.index(0);
    EXPECT_EQ(model.data(idx, ButtonModel::ButtonIdRole).toInt(), 0);
    EXPECT_EQ(model.data(idx, ButtonModel::ButtonNameRole).toString(), "Left click");
    EXPECT_EQ(model.data(idx, ButtonModel::ActionNameRole).toString(), "Left click");
    EXPECT_EQ(model.data(idx, ButtonModel::ActionTypeRole).toString(), "default");
}

TEST_F(ButtonModelTest, InvalidIndex) {
    auto idx = model.index(99);
    EXPECT_FALSE(model.data(idx, ButtonModel::ButtonIdRole).isValid());
}

TEST_F(ButtonModelTest, LoadFewerThanModelSize) {
    QList<QPair<QString, QString>> buttons = {{"only", "one"}};
    model.loadFromProfile(buttons);
    EXPECT_EQ(model.actionNameForButton(0), "only");
    // Rest unchanged
    EXPECT_EQ(model.actionNameForButton(1), "Right click");
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="ButtonModelTest.*"`
Expected: All ~15 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_button_model.cpp
git commit -m "test: ButtonModel tests — setAction signals, loadFromProfile isolation, data roles"
```

---

### Task 7: test_profile_model.cpp

**Files:**
- Create: `tests/test_profile_model.cpp`

- [ ] **Step 1: Write ProfileModel tests**

```cpp
#include <gtest/gtest.h>
#include <QSignalSpy>
#include "ProfileModel.h"
#include "TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class ProfileModelTest : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }
    ProfileModel model;
};

TEST_F(ProfileModelTest, InitialRowCount) { EXPECT_EQ(model.rowCount(), 1); } // "Defaults"

TEST_F(ProfileModelTest, DefaultEntry) {
    auto idx = model.index(0);
    EXPECT_EQ(model.data(idx, ProfileModel::NameRole).toString(), "Defaults");
    EXPECT_TRUE(model.data(idx, ProfileModel::IsActiveRole).toBool());
}

TEST_F(ProfileModelTest, AddProfile) {
    model.addProfile("google-chrome", "Google Chrome", "chrome-icon");
    EXPECT_EQ(model.rowCount(), 2);
}

TEST_F(ProfileModelTest, AddProfileEmitsProfileAdded) {
    QSignalSpy spy(&model, &ProfileModel::profileAdded);
    model.addProfile("google-chrome", "Google Chrome");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toString(), "google-chrome");
    EXPECT_EQ(spy[0][1].toString(), "Google Chrome");
}

TEST_F(ProfileModelTest, AddProfileAutoSelectsTab) {
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.addProfile("google-chrome", "Google Chrome");
    ASSERT_GE(spy.count(), 1);
    EXPECT_EQ(spy.last()[0].toString(), "Google Chrome");
}

TEST_F(ProfileModelTest, AddDuplicateWmClassIgnored) {
    model.addProfile("google-chrome", "Google Chrome");
    model.addProfile("Google-Chrome", "Chrome Again");
    EXPECT_EQ(model.rowCount(), 2); // still 2, not 3
}

TEST_F(ProfileModelTest, RestoreProfileSilent) {
    QSignalSpy addedSpy(&model, &ProfileModel::profileAdded);
    QSignalSpy switchedSpy(&model, &ProfileModel::profileSwitched);
    model.restoreProfile("google-chrome", "Google Chrome");
    EXPECT_EQ(addedSpy.count(), 0);
    EXPECT_EQ(switchedSpy.count(), 0);
    EXPECT_EQ(model.rowCount(), 2);
}

TEST_F(ProfileModelTest, RemoveProfileCantRemoveDefault) {
    model.removeProfile(0);
    EXPECT_EQ(model.rowCount(), 1); // unchanged
}

TEST_F(ProfileModelTest, RemoveProfile) {
    model.addProfile("google-chrome", "Google Chrome");
    EXPECT_EQ(model.rowCount(), 2);
    model.removeProfile(1);
    EXPECT_EQ(model.rowCount(), 1);
}

TEST_F(ProfileModelTest, RemoveDisplayedTabFallsBack) {
    model.addProfile("google-chrome", "Google Chrome");
    // Chrome tab is auto-selected (index 1)
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.removeProfile(1);
    EXPECT_EQ(model.displayIndex(), 0);
}

TEST_F(ProfileModelTest, SelectTabEmitsProfileSwitched) {
    model.addProfile("google-chrome", "Google Chrome");
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toString(), "default"); // index 0 always emits "default"
}

TEST_F(ProfileModelTest, SelectTabNoOpWhenAlreadySelected) {
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(0); // already at 0
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ProfileModelTest, SelectTabIndex0EmitsDefault) {
    model.addProfile("google-chrome", "Google Chrome");
    // Now on Chrome tab
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toString(), "default"); // not "Defaults"
}

TEST_F(ProfileModelTest, SelectTabOutOfBoundsIgnored) {
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.selectTab(99);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ProfileModelTest, SetHwActiveByProfileName) {
    model.addProfile("google-chrome", "Google Chrome");
    model.selectTab(0); // go back to default
    model.setHwActiveByProfileName("Google Chrome");
    auto idx = model.index(1);
    EXPECT_TRUE(model.data(idx, ProfileModel::IsHwActiveRole).toBool());
}

TEST_F(ProfileModelTest, SetHwActiveDoesNotEmitProfileSwitched) {
    model.addProfile("google-chrome", "Google Chrome");
    model.selectTab(0);
    QSignalSpy spy(&model, &ProfileModel::profileSwitched);
    model.setHwActiveByProfileName("Google Chrome");
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ProfileModelTest, SetHwActiveUnknownNameDefaultsToZero) {
    model.addProfile("google-chrome", "Google Chrome");
    model.setHwActiveByProfileName("NonExistent");
    auto idx = model.index(0);
    EXPECT_TRUE(model.data(idx, ProfileModel::IsHwActiveRole).toBool());
}

TEST_F(ProfileModelTest, RemoveShiftsDisplayIndex) {
    model.addProfile("app1", "App1");
    model.addProfile("app2", "App2");
    model.addProfile("app3", "App3");
    model.selectTab(1); // display on App1
    model.removeProfile(3); // remove App3 (after display)
    EXPECT_EQ(model.displayIndex(), 1); // unchanged
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="ProfileModelTest.*"`
Expected: All ~20 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_profile_model.cpp
git commit -m "test: ProfileModel tests — add/remove/select/restore, signal isolation, index management"
```

---

### Task 8: test_action_model.cpp

**Files:**
- Create: `tests/test_action_model.cpp`

- [ ] **Step 1: Write ActionModel tests**

```cpp
#include <gtest/gtest.h>
#include "ActionModel.h"
#include "TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class ActionModelTest : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }
    ActionModel model;
};

TEST_F(ActionModelTest, HasEntries) { EXPECT_GT(model.rowCount(), 0); }

TEST_F(ActionModelTest, AllRolesReturn) {
    auto idx = model.index(0);
    EXPECT_FALSE(model.data(idx, ActionModel::NameRole).toString().isEmpty());
    EXPECT_FALSE(model.data(idx, ActionModel::DescriptionRole).toString().isEmpty());
    EXPECT_FALSE(model.data(idx, ActionModel::ActionTypeRole).toString().isEmpty());
    // Payload can be empty (e.g. "Keyboard shortcut")
}

TEST_F(ActionModelTest, PayloadForNameFindsBack) {
    EXPECT_EQ(model.payloadForName("Back"), "Alt+Left");
}

TEST_F(ActionModelTest, PayloadForNameFindsForward) {
    EXPECT_EQ(model.payloadForName("Forward"), "Alt+Right");
}

TEST_F(ActionModelTest, PayloadForNameFindsCopy) {
    EXPECT_EQ(model.payloadForName("Copy"), "Ctrl+C");
}

TEST_F(ActionModelTest, PayloadForNameMissReturnsEmpty) {
    EXPECT_TRUE(model.payloadForName("NonExistent").isEmpty());
}

TEST_F(ActionModelTest, KeyboardShortcutHasEmptyPayload) {
    EXPECT_TRUE(model.payloadForName("Keyboard shortcut").isEmpty());
}

TEST_F(ActionModelTest, CalculatorIsAppLaunch) {
    for (int i = 0; i < model.rowCount(); ++i) {
        auto idx = model.index(i);
        if (model.data(idx, ActionModel::NameRole).toString() == "Calculator") {
            EXPECT_EQ(model.data(idx, ActionModel::ActionTypeRole).toString(), "app-launch");
            EXPECT_EQ(model.data(idx, ActionModel::PayloadRole).toString(), "kcalc");
            return;
        }
    }
    FAIL() << "Calculator action not found";
}

TEST_F(ActionModelTest, RoleNamesContainExpectedKeys) {
    auto roles = model.roleNames();
    EXPECT_TRUE(roles.values().contains("name"));
    EXPECT_TRUE(roles.values().contains("description"));
    EXPECT_TRUE(roles.values().contains("actionType"));
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="ActionModelTest.*"`
Expected: All ~10 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_action_model.cpp
git commit -m "test: ActionModel tests — payload lookup, roles, entry verification"
```

---

### Task 9: test_device_model.cpp

**Files:**
- Create: `tests/test_device_model.cpp`

- [ ] **Step 1: Write DeviceModel tests**

```cpp
#include <gtest/gtest.h>
#include <QSignalSpy>
#include "DeviceModel.h"
#include "TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class DeviceModelTest : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }
    DeviceModel model;
};

// --- Battery formatting ---
TEST_F(DeviceModelTest, BatteryStatusTextDefault) {
    // No DeviceManager set — batteryLevel returns 0
    EXPECT_EQ(model.batteryStatusText(), "Battery: 0%");
}

// --- Display values ---
TEST_F(DeviceModelTest, SetDisplayValuesEmitsSettingsReloaded) {
    QSignalSpy spy(&model, &DeviceModel::settingsReloaded);
    model.setDisplayValues(1200, true, 50, true, false, "zoom");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, DisplayValuesOverrideDefaults) {
    model.setDisplayValues(2000, false, 64, false, true, "volume");
    EXPECT_EQ(model.currentDPI(), 2000);
    EXPECT_FALSE(model.smartShiftEnabled());
    EXPECT_EQ(model.smartShiftThreshold(), 64);
    EXPECT_FALSE(model.scrollHiRes());
    EXPECT_TRUE(model.scrollInvert());
    EXPECT_EQ(model.thumbWheelMode(), "volume");
}

// --- Request signals ---
TEST_F(DeviceModelTest, SetDPIEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::dpiChangeRequested);
    model.setDPI(1600);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toInt(), 1600);
}

TEST_F(DeviceModelTest, SetSmartShiftEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::smartShiftChangeRequested);
    model.setSmartShift(false, 42);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toBool(), false);
    EXPECT_EQ(spy[0][1].toInt(), 42);
}

TEST_F(DeviceModelTest, SetScrollConfigEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::scrollConfigChangeRequested);
    model.setScrollConfig(true, true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toBool(), true);
    EXPECT_EQ(spy[0][1].toBool(), true);
}

TEST_F(DeviceModelTest, SetThumbWheelModeEmitsRequest) {
    QSignalSpy spy(&model, &DeviceModel::thumbWheelModeChangeRequested);
    model.setThumbWheelMode("zoom");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toString(), "zoom");
}

// --- Gesture user vs programmatic ---
TEST_F(DeviceModelTest, SetGestureActionEmitsUserGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::userGestureChanged);
    model.setGestureAction("up", "Show desktop", "Super+D");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].toString(), "up");
}

TEST_F(DeviceModelTest, LoadGesturesDoesNotEmitUserGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::userGestureChanged);
    QMap<QString, QPair<QString, QString>> gestures;
    gestures["up"] = qMakePair(QStringLiteral("Show desktop"), QStringLiteral("Super+D"));
    model.loadGesturesFromProfile(gestures);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DeviceModelTest, LoadGesturesEmitsGestureChanged) {
    QSignalSpy spy(&model, &DeviceModel::gestureChanged);
    QMap<QString, QPair<QString, QString>> gestures;
    gestures["up"] = qMakePair(QStringLiteral("Show desktop"), QStringLiteral("Super+D"));
    model.loadGesturesFromProfile(gestures);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DeviceModelTest, GestureLookup) {
    model.setGestureAction("left", "Switch left", "Ctrl+Super+Left");
    EXPECT_EQ(model.gestureActionName("left"), "Switch left");
    EXPECT_EQ(model.gestureKeystroke("left"), "Ctrl+Super+Left");
}

TEST_F(DeviceModelTest, GestureLookupMissReturnsEmpty) {
    EXPECT_TRUE(model.gestureActionName("nonexistent").isEmpty());
    EXPECT_TRUE(model.gestureKeystroke("nonexistent").isEmpty());
}

// --- Active profile name ---
TEST_F(DeviceModelTest, SetActiveProfileNameEmitsSignal) {
    QSignalSpy spy(&model, &DeviceModel::activeProfileNameChanged);
    model.setActiveProfileName("Chrome");
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(model.activeProfileName(), "Chrome");
}

TEST_F(DeviceModelTest, SetActiveProfileNameNoOpOnSame) {
    model.setActiveProfileName("Chrome");
    QSignalSpy spy(&model, &DeviceModel::activeProfileNameChanged);
    model.setActiveProfileName("Chrome");
    EXPECT_EQ(spy.count(), 0);
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="DeviceModelTest.*"`
Expected: All ~15 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_device_model.cpp
git commit -m "test: DeviceModel tests — display values, request signals, gesture isolation"
```

---

### Task 10: test_wmclass_resolution.cpp

**Files:**
- Create: `tests/test_wmclass_resolution.cpp`

- [ ] **Step 1: Write wmClass resolution tests**

```cpp
#include <gtest/gtest.h>
#include "ProfileEngine.h"
#include "TestFixtures.h"

using namespace logitune;
using namespace logitune::test;

class WmClassTest : public ProfileFixture {
protected:
    void SetUp() override {
        ProfileFixture::SetUp();
        m_engine.setDeviceConfigDir(m_profilesDir);

        // Create default profile on disk
        ProfileEngine::saveProfile(m_profilesDir + "/default.conf", makeDefaultProfile());
        m_engine.setDeviceConfigDir(m_profilesDir); // reload

        // Create app profiles
        m_engine.createProfileForApp("google-chrome", "Google Chrome");
        m_engine.createProfileForApp("dolphin", "Dolphin");
    }

    ProfileEngine m_engine;
};

TEST_F(WmClassTest, ExactMatch) {
    EXPECT_EQ(m_engine.profileForApp("google-chrome"), "Google Chrome");
}

TEST_F(WmClassTest, CaseInsensitive) {
    EXPECT_EQ(m_engine.profileForApp("Google-Chrome"), "Google Chrome");
}

TEST_F(WmClassTest, ShortClassFallback) {
    // "org.kde.dolphin" → last component "dolphin" matches binding key "dolphin"
    EXPECT_EQ(m_engine.profileForApp("org.kde.dolphin"), "Dolphin");
}

TEST_F(WmClassTest, NoMatchReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp("firefox"), "default");
}

TEST_F(WmClassTest, EmptyReturnsDefault) {
    EXPECT_EQ(m_engine.profileForApp(""), "default");
}

TEST_F(WmClassTest, ShortClassNoFalseMatch) {
    // "org.kde.something" — "something" doesn't match any binding
    EXPECT_EQ(m_engine.profileForApp("org.kde.something"), "default");
}

TEST_F(WmClassTest, MultipleBindingsCorrectOne) {
    EXPECT_EQ(m_engine.profileForApp("google-chrome"), "Google Chrome");
    EXPECT_EQ(m_engine.profileForApp("dolphin"), "Dolphin");
}

TEST_F(WmClassTest, ShortClassCaseInsensitive) {
    EXPECT_EQ(m_engine.profileForApp("org.KDE.Dolphin"), "Dolphin");
}

TEST_F(WmClassTest, CreateProfileForAppGuard) {
    // Modify the profile
    auto &p = m_engine.cachedProfile("Google Chrome");
    p.dpi = 2400;
    m_engine.saveProfileToDisk("Google Chrome");

    // Re-calling createProfileForApp should NOT overwrite
    m_engine.createProfileForApp("google-chrome", "Google Chrome");
    EXPECT_EQ(m_engine.cachedProfile("Google Chrome").dpi, 2400);
}

TEST_F(WmClassTest, DisplayHardwareProfileSeparation) {
    m_engine.setDisplayProfile("Google Chrome");
    m_engine.setHardwareProfile("Dolphin");
    EXPECT_EQ(m_engine.displayProfile(), "Google Chrome");
    EXPECT_EQ(m_engine.hardwareProfile(), "Dolphin");
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build --parallel && ./build/tests/logitune-tests --gtest_filter="WmClassTest.*"`
Expected: All ~10 tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_wmclass_resolution.cpp
git commit -m "test: wmClass resolution tests — exact, case-insensitive, short-class fallback, guard"
```

---

## Phase 1 Summary

After completing these 10 tasks:
- **Build infrastructure**: `logitune-app-lib` extracted, models testable
- **Mock library**: MockDesktop, MockInjector, MockTransport, MockDevice
- **AppController DI**: accepts injected dependencies for testing
- **~145 new unit tests** covering:
  - Every key name in parseKeystroke (30 tests)
  - ButtonAction parse/serialize for all 7 types (20 tests)
  - ButtonModel signal isolation (15 tests)
  - ProfileModel state management (20 tests)
  - ActionModel data/lookup (10 tests)
  - DeviceModel display values + request signals (15 tests)
  - wmClass resolution + profile engine guards (10 tests)
  - Mock smoke tests (3 tests)
  - Plus 22 from extending existing tests

**Total after Phase 1: ~278 tests** (133 existing + 145 new)

## What's Next

**Phase 2** (separate plan): Integration tests — AppController signal chains, profile switching, device reconnect, DeviceManager commands (~80 tests)

**Phase 3** (separate plan): System tests — real hidraw, real uinput, real KDE focus (~30 tests)

**Phase 4** (separate plan): QML tests — component behavior with mock models (~20 tests)
