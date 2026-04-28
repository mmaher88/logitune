# Semantic Action Presets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a semantic action preset layer so "Show Desktop" / "Task Switcher" / "Switch Desktop L/R" / "Screenshot" / "Close Window" / "Calculator" actions work correctly on the active DE regardless of the user's shortcut rebinds, with profiles portable across DEs.

**Architecture:** `ActionPreset` + `ActionPresetRegistry` in Core/Domain load a shipped `actions.json` (Qt resource). `IDesktopIntegration` grows a `variantKey()` and a `resolveNamedAction(id) -> std::optional<ButtonAction>`, one resolution strategy per DE impl. `ButtonAction` gains a `PresetRef` type; `ButtonActionDispatcher` resolves `PresetRef` via the active desktop and hands the concrete `ButtonAction` to the unchanged `ActionExecutor`. The picker filters by `supportedBy(id, variantKey)` so unsupported presets are hidden rather than silently broken.

**Tech Stack:** Qt 6 (QtCore, QtDBus, QtQml), C++20, GoogleTest, Qt resource system (`qt_add_resources`), `gsettings` CLI (GNOME DE impl), kglobalaccel DBus service (KDE DE impl).

**Spec:** `docs/superpowers/specs/2026-04-24-semantic-action-presets-design.md`

**Issue:** [#110](https://github.com/mmaher88/logitune/issues/110)

---

## File Structure

### New files

| Path | Responsibility |
|---|---|
| `src/core/actions/ActionPreset.h`/`.cpp` | `ActionPreset` struct + JSON deserialization |
| `src/core/actions/ActionPresetRegistry.h`/`.cpp` | Loads `actions.json` from Qt resource, indexes by id, exposes `preset(id)`, `supportedBy(id, variantKey)`, `variantData(id, variantKey)`, `allPresets()` |
| `src/core/actions/actions.json` | Shipped data: seven presets with KDE and GNOME variants |
| `src/core/actions/actions.qrc` | Qt resource manifest bundling `actions.json` under `:/logitune/actions.json` |
| `tests/core/actions/test_action_preset.cpp` | `ActionPreset` struct and JSON parser unit tests |
| `tests/core/actions/test_action_preset_registry.cpp` | Registry lookup, `supportedBy`, `variantData`, resource loading tests |
| `tests/core/desktop/test_kde_desktop_resolve.cpp` | `KdeDesktop::resolveNamedAction` unit tests (injected DBus-caller stub) |
| `tests/core/desktop/test_gnome_desktop_resolve.cpp` | `GnomeDesktop::resolveNamedAction` unit tests (injected gsettings reader stub) |

### Modified files

| Path | Change |
|---|---|
| `src/core/ButtonAction.h` | Add `PresetRef` to `Type` enum |
| `src/core/ProfileEngine.cpp` | Extend `ButtonAction::parse`/`serialize` for `PresetRef` |
| `src/core/interfaces/IDesktopIntegration.h` | Add `virtual QString variantKey() const = 0;` and `virtual std::optional<ButtonAction> resolveNamedAction(const QString &id) const = 0;` |
| `src/core/desktop/GenericDesktop.h`/`.cpp` | Implement both new methods (returns `"generic"` / `nullopt`) |
| `src/core/desktop/KDeDesktop.h`/`.cpp` | Implement both new methods (kglobalaccel DBus); accept optional DBus-caller for tests |
| `src/core/desktop/GnomeDesktop.h`/`.cpp` | Implement both new methods (gsettings CLI); accept optional gsettings-reader for tests |
| `src/core/CMakeLists.txt` | Add new `actions/` sources + Qt resource |
| `src/app/services/ButtonActionDispatcher.h`/`.cpp` | Accept `IDesktopIntegration*`, resolve `PresetRef` before dispatch |
| `src/app/models/ActionModel.cpp` | Replace 7 hardcoded-keystroke rows with `actionType="preset"`; update `buttonActionToName` and `buttonEntryToAction` |
| `src/app/models/ActionFilterModel.h`/`.cpp` | Accept `IDesktopIntegration*` + `ActionPresetRegistry*`; hide preset rows where `!registry->supportedBy(id, desktop->variantKey())` or resolution returns `nullopt` |
| `src/app/AppRoot.h`/`.cpp` | Instantiate registry, pass to dispatcher + filter model |
| `tests/mocks/MockDesktop.h` | Implement the two new interface methods (configurable variant key + scripted resolve) |
| `tests/CMakeLists.txt` | Add new test files under `tests/core/actions/` and `tests/core/desktop/` |

---

## Ground Rules (followed in every task)

- **TDD:** write a failing test first, make it fail for the right reason, implement, re-run, commit.
- **Commands from repo root** (`/home/mina/repos/logitune-wt-semantic-actions`) unless noted.
- **Build:** `cmake --build build`
- **Run all tests:** `./build/tests/logitune-tests`
- **Run one test filter:** `./build/tests/logitune-tests --gtest_filter="PatternHere*"`
- **Commit messages:** no co-author signatures, no em-dashes, no test URLs or site names (CLAUDE.md rules).
- **Never push to master.** This branch is `semantic-action-presets`; PR opens at the end.
- **After code change:** per CLAUDE.md, kill + relaunch the app if you touched runtime code. Tests alone don't count as validation for UI-affecting changes.

---

## Task 1: Add `PresetRef` to `ButtonAction::Type` + parse/serialize

**Files:**
- Modify: `src/core/ButtonAction.h`
- Modify: `src/core/ProfileEngine.cpp:15-62` (the `ButtonAction::parse` / `serialize` functions)
- Test: `tests/test_button_action.cpp` (existing file)

Tiny and self-contained. No other class cares about the new enum value yet — later tasks add the consumers.

- [ ] **Step 1: Add three failing tests for `PresetRef`**

Append to `tests/test_button_action.cpp`:

```cpp
// ---------------------------------------------------------------------------
// PresetRef tests
// ---------------------------------------------------------------------------

TEST(ButtonAction, ParsePresetRefShowDesktop) {
    auto a = ButtonAction::parse("preset:show-desktop");
    EXPECT_EQ(a.type, ButtonAction::PresetRef);
    EXPECT_EQ(a.payload, "show-desktop");
}

TEST(ButtonAction, SerializePresetRefTaskSwitcher) {
    ButtonAction a{ButtonAction::PresetRef, "task-switcher"};
    EXPECT_EQ(a.serialize(), "preset:task-switcher");
}

TEST(ButtonAction, RoundTripPresetRefSwitchDesktopLeft) {
    ButtonAction orig{ButtonAction::PresetRef, "switch-desktop-left"};
    ButtonAction result = ButtonAction::parse(orig.serialize());
    EXPECT_EQ(result, orig);
}
```

- [ ] **Step 2: Build tests to confirm they fail to compile**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -20
```

Expected: compilation error, `'PresetRef' is not a member of 'logitune::ButtonAction'`.

- [ ] **Step 3: Add `PresetRef` to the enum**

Edit `src/core/ButtonAction.h`, replace the `Type` enum with:

```cpp
struct ButtonAction {
    enum Type {
        Default,
        Keystroke,
        GestureTrigger,
        SmartShiftToggle,
        DpiCycle,
        AppLaunch,
        DBus,
        Media,
        PresetRef,
    };
```

- [ ] **Step 4: Teach `parse` and `serialize` about it**

Edit `src/core/ProfileEngine.cpp`. In `ButtonAction::parse` (around line 43), add one more prefix handler before the fall-through return:

```cpp
    if (prefix == "app-launch") return {AppLaunch,   payload};
    if (prefix == "preset")     return {PresetRef,   payload};
```

In `ButtonAction::serialize` (the switch around line 49), add one case:

```cpp
    case Default:       return "default";
    case GestureTrigger:  return "gesture-trigger";
    case SmartShiftToggle: return "smartshift-toggle";
    case DpiCycle:      return "dpi-cycle";
    case Keystroke:     return "keystroke:" + payload;
    case Media:         return "media:" + payload;
    case DBus:          return "dbus:" + payload;
    case AppLaunch:     return "app-launch:" + payload;
    case PresetRef:     return "preset:" + payload;
```

- [ ] **Step 5: Rebuild and verify the three new tests pass**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ButtonAction.*PresetRef*"
```

Expected: `3 tests from 1 test suite ran. PASSED 3.`

- [ ] **Step 6: Run the full suite to verify no regression**

```bash
./build/tests/logitune-tests
```

Expected: `646 tests from 51 test suites ran. PASSED 646.`

- [ ] **Step 7: Commit**

```bash
git add src/core/ButtonAction.h src/core/ProfileEngine.cpp tests/test_button_action.cpp
git commit -m "feat(button-action): add PresetRef type variant

New ButtonAction::PresetRef type carries a preset id (e.g. show-desktop)
that DE impls will resolve to a concrete ButtonAction at fire-time. Enum
value added; parse/serialize gain a 'preset:' prefix. No consumers yet,
all existing behavior unchanged."
```

---

## Task 2: `ActionPreset` struct + JSON parser

**Files:**
- Create: `src/core/actions/ActionPreset.h`
- Create: `src/core/actions/ActionPreset.cpp`
- Create: `tests/core/actions/test_action_preset.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Pure data struct + parser. No Qt resource wiring yet — that's Task 4. The parser eats a `QJsonObject` and returns `ActionPreset` (or fails).

- [ ] **Step 1: Create the header**

`src/core/actions/ActionPreset.h`:

```cpp
#pragma once
#include <QHash>
#include <QJsonObject>
#include <QString>

namespace logitune {

/// A semantic action (e.g. "show-desktop") whose concrete implementation
/// differs per desktop environment. Static data; variants is a DE-native hint
/// the DE impl interprets at resolve-time, not a raw keystroke.
struct ActionPreset {
    QString id;                             // "show-desktop"
    QString label;                          // "Show Desktop"
    QString icon;                           // "desktop"
    QString category;                       // "workspace"
    QHash<QString, QJsonObject> variants;   // variantKey ("kde", "gnome") -> hint object

    /// Parse one preset from a JSON object. Returns an empty preset
    /// (id.isEmpty()) on malformed input.
    static ActionPreset fromJson(const QJsonObject &obj);

    bool isValid() const { return !id.isEmpty(); }
};

} // namespace logitune
```

- [ ] **Step 2: Create the CMake sources list entry**

Edit `src/core/CMakeLists.txt`. Inside the `target_sources(logitune-core PRIVATE ...)` block, add after the existing `devices/JsonDevice.cpp` line:

```cmake
    devices/DescriptorWriter.cpp
    devices/JsonDevice.cpp
    actions/ActionPreset.cpp
    actions/ActionPresetRegistry.cpp
```

(Yes, `ActionPresetRegistry.cpp` is referenced here even though Task 3 creates it — the build will fail on this task if you add it too early. Only add `actions/ActionPreset.cpp` in this task. Task 3 adds the registry line.)

Exact replacement for this step:

```cmake
    devices/DescriptorWriter.cpp
    devices/JsonDevice.cpp
    actions/ActionPreset.cpp
```

- [ ] **Step 3: Create the test file**

`tests/core/actions/test_action_preset.cpp`:

```cpp
#include <gtest/gtest.h>

#include "actions/ActionPreset.h"
#include <QJsonDocument>
#include <QJsonObject>

using logitune::ActionPreset;

namespace {
QJsonObject parseObj(const char *json) {
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    return doc.object();
}
} // namespace

TEST(ActionPreset, ParsesFullyFormedEntry) {
    auto obj = parseObj(R"({
        "id": "show-desktop",
        "label": "Show Desktop",
        "icon": "desktop",
        "category": "workspace",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    })");

    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_TRUE(p.isValid());
    EXPECT_EQ(p.id, "show-desktop");
    EXPECT_EQ(p.label, "Show Desktop");
    EXPECT_EQ(p.icon, "desktop");
    EXPECT_EQ(p.category, "workspace");
    EXPECT_EQ(p.variants.size(), 2);
    EXPECT_TRUE(p.variants.contains("kde"));
    EXPECT_TRUE(p.variants.contains("gnome"));
}

TEST(ActionPreset, VariantDataPreservesNestedStructure) {
    auto obj = parseObj(R"({
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } }
        }
    })");

    ActionPreset p = ActionPreset::fromJson(obj);
    ASSERT_TRUE(p.variants.contains("kde"));
    QJsonObject kde = p.variants.value("kde");
    ASSERT_TRUE(kde.contains("kglobalaccel"));
    QJsonObject spec = kde.value("kglobalaccel").toObject();
    EXPECT_EQ(spec.value("component").toString(), "kwin");
    EXPECT_EQ(spec.value("name").toString(), "Show Desktop");
}

TEST(ActionPreset, MissingIdReturnsInvalid) {
    auto obj = parseObj(R"({"label": "x"})");
    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_FALSE(p.isValid());
}

TEST(ActionPreset, EmptyObjectReturnsInvalid) {
    ActionPreset p = ActionPreset::fromJson(QJsonObject{});
    EXPECT_FALSE(p.isValid());
}

TEST(ActionPreset, OptionalFieldsDefaultEmpty) {
    auto obj = parseObj(R"({"id": "x", "label": "X"})");
    ActionPreset p = ActionPreset::fromJson(obj);
    EXPECT_TRUE(p.isValid());
    EXPECT_EQ(p.id, "x");
    EXPECT_TRUE(p.icon.isEmpty());
    EXPECT_TRUE(p.category.isEmpty());
    EXPECT_TRUE(p.variants.isEmpty());
}
```

- [ ] **Step 4: Register the test in CMake**

Edit `tests/CMakeLists.txt`. In the `add_executable(logitune-tests ...)` sources list, add after `test_distro_detector.cpp`:

```cmake
    test_distro_detector.cpp
    core/actions/test_action_preset.cpp
    services/test_active_device_resolver.cpp
```

- [ ] **Step 5: Try to build and confirm the test compiles but fails at link time**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -10
```

Expected: link error, `undefined reference to 'logitune::ActionPreset::fromJson'`.

- [ ] **Step 6: Implement the parser**

`src/core/actions/ActionPreset.cpp`:

```cpp
#include "actions/ActionPreset.h"

#include <QJsonObject>
#include <QJsonValue>

namespace logitune {

ActionPreset ActionPreset::fromJson(const QJsonObject &obj)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return {};  // invalid

    ActionPreset p;
    p.id       = id;
    p.label    = obj.value(QStringLiteral("label")).toString();
    p.icon     = obj.value(QStringLiteral("icon")).toString();
    p.category = obj.value(QStringLiteral("category")).toString();

    const QJsonValue variantsVal = obj.value(QStringLiteral("variants"));
    if (variantsVal.isObject()) {
        const QJsonObject variants = variantsVal.toObject();
        for (auto it = variants.constBegin(); it != variants.constEnd(); ++it) {
            if (it.value().isObject())
                p.variants.insert(it.key(), it.value().toObject());
        }
    }

    return p;
}

} // namespace logitune
```

- [ ] **Step 7: Reconfigure (new subdirectory) and build**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -5
cmake --build build --target logitune-tests 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 8: Run the new tests and the full suite**

```bash
./build/tests/logitune-tests --gtest_filter="ActionPreset.*"
./build/tests/logitune-tests
```

Expected: `5 tests` for the filter, `651 tests PASSED 651` overall.

- [ ] **Step 9: Commit**

```bash
git add src/core/actions/ActionPreset.h src/core/actions/ActionPreset.cpp \
        src/core/CMakeLists.txt \
        tests/core/actions/test_action_preset.cpp tests/CMakeLists.txt
git commit -m "feat(actions): add ActionPreset struct + JSON parser

Core/Domain data type for semantic action presets. id + label + icon +
category + a map of variantKey to DE-native hint object. Parser returns
an invalid preset (empty id) on malformed input. No registry or
resource wiring yet."
```

---

## Task 3: `ActionPresetRegistry`

**Files:**
- Create: `src/core/actions/ActionPresetRegistry.h`
- Create: `src/core/actions/ActionPresetRegistry.cpp`
- Create: `tests/core/actions/test_action_preset_registry.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Registry indexes presets by id. Exposes `loadFromJson(QByteArray)` for tests; Task 4 adds the Qt resource loader constructor.

- [ ] **Step 1: Create the header**

`src/core/actions/ActionPresetRegistry.h`:

```cpp
#pragma once
#include "actions/ActionPreset.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <vector>

namespace logitune {

/// Static data registry for semantic action presets. Parses JSON, indexes
/// presets by id, answers per-DE "is this supported" queries.
///
/// Load from raw JSON via loadFromJson (tests) or from the bundled Qt
/// resource via loadFromResource (production).
class ActionPresetRegistry {
public:
    ActionPresetRegistry() = default;

    /// Parse JSON and replace the registry contents. Returns the count
    /// of successfully-loaded presets. Malformed entries are skipped.
    int loadFromJson(const QByteArray &json);

    /// Load the bundled resource at :/logitune/actions.json.
    /// Returns the count of successfully-loaded presets.
    int loadFromResource();

    /// Lookup by id. Returns nullptr for unknown ids.
    const ActionPreset *preset(const QString &id) const;

    /// True if the preset exists AND has a variant entry for variantKey.
    bool supportedBy(const QString &id, const QString &variantKey) const;

    /// Returns the variant hint object for id/variantKey, or empty
    /// object if either is absent.
    QJsonObject variantData(const QString &id, const QString &variantKey) const;

    /// All presets, order of insertion. For UI listing.
    const std::vector<ActionPreset> &all() const { return m_presets; }

private:
    std::vector<ActionPreset> m_presets;
    QHash<QString, size_t>    m_index;  // id -> position in m_presets
};

} // namespace logitune
```

- [ ] **Step 2: Register the new source in CMake**

Edit `src/core/CMakeLists.txt` to add the registry source alongside the preset one:

```cmake
    actions/ActionPreset.cpp
    actions/ActionPresetRegistry.cpp
```

- [ ] **Step 3: Create the failing test file**

`tests/core/actions/test_action_preset_registry.cpp`:

```cpp
#include <gtest/gtest.h>

#include "actions/ActionPresetRegistry.h"

using logitune::ActionPresetRegistry;

namespace {
const QByteArray kMiniCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "category": "workspace",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    },
    {
        "id": "task-switcher",
        "label": "Task Switcher",
        "category": "workspace",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Expose" } }
        }
    }
])";
} // namespace

TEST(ActionPresetRegistry, LoadsTwoPresets) {
    ActionPresetRegistry r;
    EXPECT_EQ(r.loadFromJson(kMiniCatalog), 2);
    EXPECT_EQ(r.all().size(), 2u);
}

TEST(ActionPresetRegistry, PresetByIdReturnsNullForUnknown) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_EQ(r.preset("nonexistent"), nullptr);
}

TEST(ActionPresetRegistry, PresetByIdReturnsMatch) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    const auto *p = r.preset("show-desktop");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->label, "Show Desktop");
}

TEST(ActionPresetRegistry, SupportedByReturnsTrueForPresentVariant) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_TRUE(r.supportedBy("show-desktop", "kde"));
    EXPECT_TRUE(r.supportedBy("show-desktop", "gnome"));
}

TEST(ActionPresetRegistry, SupportedByReturnsFalseForAbsentVariant) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_FALSE(r.supportedBy("show-desktop", "hyprland"));
    EXPECT_FALSE(r.supportedBy("task-switcher", "gnome"));
    EXPECT_FALSE(r.supportedBy("nonexistent", "kde"));
}

TEST(ActionPresetRegistry, VariantDataReturnsNestedObject) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    QJsonObject data = r.variantData("show-desktop", "kde");
    ASSERT_TRUE(data.contains("kglobalaccel"));
    EXPECT_EQ(data.value("kglobalaccel").toObject().value("name").toString(),
              "Show Desktop");
}

TEST(ActionPresetRegistry, VariantDataReturnsEmptyForMissing) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_TRUE(r.variantData("nonexistent", "kde").isEmpty());
    EXPECT_TRUE(r.variantData("show-desktop", "hyprland").isEmpty());
}

TEST(ActionPresetRegistry, SkipsMalformedEntries) {
    ActionPresetRegistry r;
    const QByteArray mixed = R"([
        { "id": "valid", "label": "Valid", "variants": {} },
        { "label": "missing-id" },
        { "id": "valid2", "label": "Valid 2", "variants": {} }
    ])";
    EXPECT_EQ(r.loadFromJson(mixed), 2);
    EXPECT_NE(r.preset("valid"), nullptr);
    EXPECT_NE(r.preset("valid2"), nullptr);
}

TEST(ActionPresetRegistry, LoadFromJsonReplacesPreviousContents) {
    ActionPresetRegistry r;
    r.loadFromJson(kMiniCatalog);
    EXPECT_EQ(r.all().size(), 2u);

    const QByteArray replacement = R"([
        { "id": "only-one", "label": "Only", "variants": {} }
    ])";
    EXPECT_EQ(r.loadFromJson(replacement), 1);
    EXPECT_EQ(r.all().size(), 1u);
    EXPECT_EQ(r.preset("show-desktop"), nullptr);
    EXPECT_NE(r.preset("only-one"), nullptr);
}
```

- [ ] **Step 4: Register the test**

Edit `tests/CMakeLists.txt` to add after the `test_action_preset.cpp` line:

```cmake
    core/actions/test_action_preset.cpp
    core/actions/test_action_preset_registry.cpp
    services/test_active_device_resolver.cpp
```

- [ ] **Step 5: Build and confirm the link fails (loadFromJson unimplemented)**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -3
cmake --build build --target logitune-tests 2>&1 | tail -10
```

Expected: link error, undefined reference to `loadFromJson`.

- [ ] **Step 6: Implement the registry**

`src/core/actions/ActionPresetRegistry.cpp`:

```cpp
#include "actions/ActionPresetRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace logitune {

int ActionPresetRegistry::loadFromJson(const QByteArray &json)
{
    m_presets.clear();
    m_index.clear();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return 0;

    const QJsonArray arr = doc.array();
    m_presets.reserve(static_cast<size_t>(arr.size()));

    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        ActionPreset p = ActionPreset::fromJson(v.toObject());
        if (!p.isValid()) continue;
        m_index.insert(p.id, m_presets.size());
        m_presets.push_back(std::move(p));
    }

    return static_cast<int>(m_presets.size());
}

int ActionPresetRegistry::loadFromResource()
{
    QFile f(QStringLiteral(":/logitune/actions.json"));
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return loadFromJson(f.readAll());
}

const ActionPreset *ActionPresetRegistry::preset(const QString &id) const
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return nullptr;
    return &m_presets.at(*it);
}

bool ActionPresetRegistry::supportedBy(const QString &id,
                                       const QString &variantKey) const
{
    const ActionPreset *p = preset(id);
    if (!p) return false;
    return p->variants.contains(variantKey);
}

QJsonObject ActionPresetRegistry::variantData(const QString &id,
                                              const QString &variantKey) const
{
    const ActionPreset *p = preset(id);
    if (!p) return {};
    return p->variants.value(variantKey);
}

} // namespace logitune
```

- [ ] **Step 7: Build and run new tests**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionPresetRegistry.*"
./build/tests/logitune-tests
```

Expected: `8 tests from 1 test suite ran. PASSED 8.` Full suite: `659 PASSED 659`.

- [ ] **Step 8: Commit**

```bash
git add src/core/actions/ActionPresetRegistry.h \
        src/core/actions/ActionPresetRegistry.cpp \
        src/core/CMakeLists.txt \
        tests/core/actions/test_action_preset_registry.cpp tests/CMakeLists.txt
git commit -m "feat(actions): add ActionPresetRegistry

Indexes presets by id. Exposes preset(id) lookup, supportedBy(id, key)
for picker filtering, and variantData(id, key) for DE impls to read
their variant hint. loadFromJson accepts a bytearray (for tests);
loadFromResource will read the bundled :/logitune/actions.json
(populated in the next task)."
```

---

## Task 4: Ship `actions.json` as a Qt resource

**Files:**
- Create: `src/core/actions/actions.json`
- Create: `src/core/actions/actions.qrc`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/core/actions/test_action_preset_registry.cpp`

Qt resource is compiled into the static lib. Tests exercise the resource loader with the real shipped catalog; any malformed preset will fail there.

- [ ] **Step 1: Write the data**

`src/core/actions/actions.json`:

```json
[
  {
    "id": "show-desktop",
    "label": "Show desktop",
    "icon": "desktop",
    "category": "workspace",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
    }
  },
  {
    "id": "task-switcher",
    "label": "Task switcher",
    "icon": "windows",
    "category": "workspace",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "kwin", "name": "ExposeAll" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "switch-applications" } }
    }
  },
  {
    "id": "switch-desktop-left",
    "label": "Switch desktop left",
    "icon": "arrow-left",
    "category": "workspace",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "kwin", "name": "Switch to Previous Desktop" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "switch-to-workspace-left" } }
    }
  },
  {
    "id": "switch-desktop-right",
    "label": "Switch desktop right",
    "icon": "arrow-right",
    "category": "workspace",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "kwin", "name": "Switch to Next Desktop" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "switch-to-workspace-right" } }
    }
  },
  {
    "id": "screenshot",
    "label": "Screenshot",
    "icon": "camera",
    "category": "system",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "org_kde_spectacle_desktop", "name": "RectangularRegionScreenShot" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.shell.keybindings", "key": "show-screenshot-ui" } }
    }
  },
  {
    "id": "close-window",
    "label": "Close window",
    "icon": "close",
    "category": "window",
    "variants": {
      "kde":   { "kglobalaccel": { "component": "kwin", "name": "Window Close" } },
      "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "close" } }
    }
  },
  {
    "id": "calculator",
    "label": "Calculator",
    "icon": "calculator",
    "category": "app",
    "variants": {
      "kde":   { "app-launch": { "binary": "kcalc" } },
      "gnome": { "app-launch": { "binary": "gnome-calculator" } }
    }
  }
]
```

- [ ] **Step 2: Write the `.qrc` manifest**

`src/core/actions/actions.qrc`:

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
  <qresource prefix="/logitune">
    <file>actions.json</file>
  </qresource>
</RCC>
```

- [ ] **Step 3: Wire the resource into the core lib**

Edit `src/core/CMakeLists.txt`. Just before `target_include_directories(logitune-core PUBLIC ...)` at the bottom, add:

```cmake
qt_add_resources(logitune-core "action-presets"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/actions"
    FILES actions/actions.json
)
```

(Note: we don't need the `.qrc` file if we use `qt_add_resources` with FILES, but we keep the `.qrc` in the tree for documentation purposes and in case anyone wants to invoke `rcc` manually. Remove the `.qrc` generation from the CMake — it's reference material only.)

Actually simpler: just the CMake call does the job:

```cmake
qt_add_resources(logitune-core "action-presets"
    PREFIX "/logitune"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/actions"
    FILES actions/actions.json
)
```

Delete the `.qrc` file creation from Step 2 (don't ship it, Qt's CMake integration doesn't need it).

(Cleaner plan: skip Step 2 entirely. The `.qrc` is not needed.)

**Step 2 revised: do nothing.** Proceed to Step 3 directly with just the CMake call.

- [ ] **Step 4: Add a resource-load test**

Append to `tests/core/actions/test_action_preset_registry.cpp`:

```cpp
TEST(ActionPresetRegistry, LoadsFromBundledResource) {
    ActionPresetRegistry r;
    int n = r.loadFromResource();
    EXPECT_GE(n, 7);   // at least the seven we shipped
    EXPECT_NE(r.preset("show-desktop"), nullptr);
    EXPECT_NE(r.preset("task-switcher"), nullptr);
    EXPECT_NE(r.preset("switch-desktop-left"), nullptr);
    EXPECT_NE(r.preset("switch-desktop-right"), nullptr);
    EXPECT_NE(r.preset("screenshot"), nullptr);
    EXPECT_NE(r.preset("close-window"), nullptr);
    EXPECT_NE(r.preset("calculator"), nullptr);
}

TEST(ActionPresetRegistry, ShippedShowDesktopHasKdeAndGnomeVariants) {
    ActionPresetRegistry r;
    r.loadFromResource();
    EXPECT_TRUE(r.supportedBy("show-desktop", "kde"));
    EXPECT_TRUE(r.supportedBy("show-desktop", "gnome"));
}

TEST(ActionPresetRegistry, ShippedCalculatorUsesAppLaunch) {
    ActionPresetRegistry r;
    r.loadFromResource();
    QJsonObject gnome = r.variantData("calculator", "gnome");
    ASSERT_TRUE(gnome.contains("app-launch"));
    EXPECT_EQ(gnome.value("app-launch").toObject().value("binary").toString(),
              "gnome-calculator");
}
```

- [ ] **Step 5: Reconfigure, build, run**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -3
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionPresetRegistry.*"
./build/tests/logitune-tests
```

Expected: filter shows 11 passing tests. Full suite: `662 PASSED 662`.

- [ ] **Step 6: Commit**

```bash
git add src/core/actions/actions.json \
        src/core/CMakeLists.txt \
        tests/core/actions/test_action_preset_registry.cpp
git commit -m "feat(actions): ship actions.json as Qt resource

Bundles the seven v1 presets (show-desktop, task-switcher,
switch-desktop-left/right, screenshot, close-window, calculator) with
KDE (kglobalaccel) and GNOME (gsettings/app-launch) variants.
ActionPresetRegistry::loadFromResource reads it from
:/logitune/actions.json at runtime."
```

---

## Task 5: Extend `IDesktopIntegration`, update `GenericDesktop` + `MockDesktop`

**Files:**
- Modify: `src/core/interfaces/IDesktopIntegration.h`
- Modify: `src/core/desktop/GenericDesktop.h`
- Modify: `src/core/desktop/GenericDesktop.cpp`
- Modify: `tests/mocks/MockDesktop.h`
- Modify: `src/core/desktop/KDeDesktop.h`, `KDeDesktop.cpp` (stub overrides so class stays compilable)
- Modify: `src/core/desktop/GnomeDesktop.h`, `GnomeDesktop.cpp` (stub overrides so class stays compilable)
- Test: `tests/test_desktop_factory.cpp` (existing) — add one `variantKey` check per impl
- Test: new `tests/core/desktop/test_generic_desktop_resolve.cpp`

After this task, every `IDesktopIntegration` impl compiles with the two new methods. KDE and GNOME return stubs that the next two tasks replace with real implementations.

- [ ] **Step 1: Extend the interface**

Edit `src/core/interfaces/IDesktopIntegration.h` to add two new pure virtuals and the needed include:

```cpp
#pragma once
#include "ButtonAction.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <optional>

namespace logitune {

class IDesktopIntegration : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IDesktopIntegration() = default;

    virtual void start() = 0;
    virtual bool available() const = 0;
    virtual QString desktopName() const = 0;
    virtual QStringList detectedCompositors() const = 0;

    /// Short key identifying this DE in the action preset variants map
    /// (e.g. "kde", "gnome", "generic"). Matches a top-level key under
    /// "variants" in actions.json.
    virtual QString variantKey() const = 0;

    /// Resolve a semantic preset id (e.g. "show-desktop") to a concrete
    /// ButtonAction the ActionExecutor can fire. Returns nullopt when
    /// the preset is not supported on this DE or the user's live binding
    /// is empty/unreachable.
    virtual std::optional<ButtonAction> resolveNamedAction(const QString &id) const = 0;

    /// Block/unblock global shortcuts during keystroke capture.
    virtual void blockGlobalShortcuts(bool block) = 0;

    /// Return list of running graphical applications.
    /// Each entry is a QVariantMap with keys "wmClass" and "title".
    virtual QVariantList runningApplications() const = 0;

signals:
    void activeWindowChanged(const QString &wmClass, const QString &title);
};

} // namespace logitune
```

- [ ] **Step 2: Stub `GenericDesktop`**

Edit `src/core/desktop/GenericDesktop.h`. Add to the public method list after `blockGlobalShortcuts`:

```cpp
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;
```

Edit `src/core/desktop/GenericDesktop.cpp`. Append at the end of the file (inside the namespace):

```cpp
QString GenericDesktop::variantKey() const
{
    return QStringLiteral("generic");
}

std::optional<ButtonAction> GenericDesktop::resolveNamedAction(const QString &id) const
{
    Q_UNUSED(id);
    return std::nullopt;   // no portable bindings exposed in v1
}
```

(If `#include <optional>` is missing in the `.cpp`, add it near the top.)

- [ ] **Step 3: Stub `KDeDesktop` and `GnomeDesktop` to keep the build green**

Edit `src/core/desktop/KDeDesktop.h`. Add to the public method list (near `blockGlobalShortcuts`):

```cpp
    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;
```

Edit `src/core/desktop/KDeDesktop.cpp`. Append:

```cpp
QString KDeDesktop::variantKey() const
{
    return QStringLiteral("kde");
}

std::optional<ButtonAction> KDeDesktop::resolveNamedAction(const QString &id) const
{
    Q_UNUSED(id);
    return std::nullopt;   // TODO implemented in task 6
}
```

Make the same two changes to `GnomeDesktop.h`/`.cpp`, returning `"gnome"` from `variantKey()` and `std::nullopt` from `resolveNamedAction`. (Task 7 fills in the real GNOME impl.)

Add `#include <optional>` to each `.cpp` if not already present.

- [ ] **Step 4: Update `MockDesktop`**

Edit `tests/mocks/MockDesktop.h` to match the new interface:

```cpp
#pragma once
#include "interfaces/IDesktopIntegration.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

namespace logitune::test {

class MockDesktop : public logitune::IDesktopIntegration {
    Q_OBJECT
public:
    explicit MockDesktop(QObject *parent = nullptr)
        : IDesktopIntegration(parent)
    {}

    void start() override {}

    bool available() const override { return true; }

    QString desktopName() const override { return QStringLiteral("mock"); }

    QStringList detectedCompositors() const override { return {}; }

    QString variantKey() const override { return m_variantKey; }

    std::optional<logitune::ButtonAction>
    resolveNamedAction(const QString &id) const override {
        auto it = m_scripted.constFind(id);
        if (it == m_scripted.constEnd())
            return std::nullopt;
        return *it;
    }

    void blockGlobalShortcuts(bool /*block*/) override {
        ++m_blockCount;
    }

    QVariantList runningApplications() const override {
        return m_runningApps;
    }

    // --- Test helpers ---

    void simulateFocus(const QString &wmClass, const QString &title) {
        emit activeWindowChanged(wmClass, title);
    }

    void setRunningApps(const QVariantList &apps) {
        m_runningApps = apps;
    }

    void setVariantKey(const QString &key) { m_variantKey = key; }

    /// Pre-program resolve results by id. Any id not in the map returns nullopt.
    void scriptResolve(const QString &id, logitune::ButtonAction action) {
        m_scripted.insert(id, action);
    }

    void clearScriptedResolves() { m_scripted.clear(); }

    int blockCount() const { return m_blockCount; }

private:
    QVariantList m_runningApps;
    int m_blockCount = 0;
    QString m_variantKey = QStringLiteral("mock");
    QHash<QString, logitune::ButtonAction> m_scripted;
};

} // namespace logitune::test
```

- [ ] **Step 5: Add a resolve test for `GenericDesktop`**

Create `tests/core/desktop/test_generic_desktop_resolve.cpp`:

```cpp
#include <gtest/gtest.h>

#include "desktop/GenericDesktop.h"

using logitune::GenericDesktop;

TEST(GenericDesktopResolve, VariantKeyIsGeneric) {
    GenericDesktop d;
    EXPECT_EQ(d.variantKey(), "generic");
}

TEST(GenericDesktopResolve, ResolveReturnsNulloptForEverything) {
    GenericDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
    EXPECT_FALSE(d.resolveNamedAction("task-switcher").has_value());
    EXPECT_FALSE(d.resolveNamedAction("arbitrary-unknown-id").has_value());
}
```

- [ ] **Step 6: Register the new test**

Edit `tests/CMakeLists.txt` to add after `test_action_preset_registry.cpp`:

```cmake
    core/actions/test_action_preset_registry.cpp
    core/desktop/test_generic_desktop_resolve.cpp
    services/test_active_device_resolver.cpp
```

- [ ] **Step 7: Reconfigure, build, run**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -3
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="GenericDesktopResolve.*"
./build/tests/logitune-tests
```

Expected: 2 new tests pass; full suite `664 PASSED 664`.

- [ ] **Step 8: Commit**

```bash
git add src/core/interfaces/IDesktopIntegration.h \
        src/core/desktop/GenericDesktop.h src/core/desktop/GenericDesktop.cpp \
        src/core/desktop/KDeDesktop.h src/core/desktop/KDeDesktop.cpp \
        src/core/desktop/GnomeDesktop.h src/core/desktop/GnomeDesktop.cpp \
        tests/mocks/MockDesktop.h \
        tests/core/desktop/test_generic_desktop_resolve.cpp \
        tests/CMakeLists.txt
git commit -m "feat(desktop): add variantKey() and resolveNamedAction() to IDesktopIntegration

Generic returns 'generic' + nullopt for every id (no portable bindings
in v1). KDE and GNOME are stubbed to return their variant keys + nullopt
for now; real resolvers land in the next two commits. MockDesktop gains
setVariantKey and scriptResolve test helpers."
```

---

## Task 6: `KdeDesktop::resolveNamedAction` via kglobalaccel DBus

**Files:**
- Modify: `src/core/desktop/KDeDesktop.h`
- Modify: `src/core/desktop/KDeDesktop.cpp`
- Create: `tests/core/desktop/test_kde_desktop_resolve.cpp`
- Modify: `tests/CMakeLists.txt`

KDE resolution reads the preset registry, finds the `kglobalaccel` variant, and returns a `ButtonAction{DBus, payload}` where `payload` is the four-comma spec `ActionExecutor::parseDBusAction` already understands:

```
service,path,interface,method
```

For kglobalaccel action invocation the spec is:

```
org.kde.kglobalaccel, /component/<component>, org.kde.kglobalaccel.Component, invokeShortcut
```

The `name` argument that identifies the action is passed as the method's string argument. Since `ActionExecutor::executeDBusCall` / `UinputInjector::sendDBusCall` currently takes a spec without args, we extend the payload encoding: a **fifth comma-separated field** holds the string argument. The executor already splits on commas and currently rejects anything that isn't exactly four fields; extending to optionally accept a fifth is a minimal change made in Task 8.

For Task 6, we produce payloads with five fields. Tests check only the returned `ButtonAction` structure; Task 8 wires the new five-field format through to `UinputInjector`.

To make this testable without a real DBus, the `KdeDesktop` reads the `kglobalaccel` variant data directly from an `ActionPresetRegistry` pointer (setter-injected so tests can pass a registry with scripted contents). The real production wiring happens in Task 11 (AppRoot).

- [ ] **Step 1: Extend the header**

Edit `src/core/desktop/KDeDesktop.h`:

```cpp
#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QDBusInterface>
#include <QTimer>
#include <optional>

namespace logitune {

class ActionPresetRegistry;

class KDeDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    explicit KDeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;

    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;

    /// Test seam: point to a registry the resolver should query.
    /// Takes non-owning pointer; the registry must outlive this object.
    void setPresetRegistry(const ActionPresetRegistry *registry) {
        m_registry = registry;
    }

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
    bool m_available = false;
    const ActionPresetRegistry *m_registry = nullptr;
};

} // namespace logitune
```

- [ ] **Step 2: Create the failing test file**

`tests/core/desktop/test_kde_desktop_resolve.cpp`:

```cpp
#include <gtest/gtest.h>

#include "desktop/KDeDesktop.h"
#include "actions/ActionPresetRegistry.h"

using logitune::KDeDesktop;
using logitune::ActionPresetRegistry;
using logitune::ButtonAction;

namespace {
const QByteArray kCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } },
            "gnome": { "gsettings":    { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } }
        }
    },
    {
        "id": "task-switcher",
        "label": "Task Switcher",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "ExposeAll" } }
        }
    },
    {
        "id": "calculator",
        "label": "Calculator",
        "variants": {
            "kde": { "app-launch": { "binary": "kcalc" } }
        }
    },
    {
        "id": "gnome-only",
        "label": "Gnome Only",
        "variants": {
            "gnome": { "gsettings": { "schema": "x", "key": "y" } }
        }
    }
])";
} // namespace

TEST(KdeDesktopResolve, VariantKeyIsKde) {
    KDeDesktop d;
    EXPECT_EQ(d.variantKey(), "kde");
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptWithoutRegistry) {
    KDeDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}

TEST(KdeDesktopResolve, ResolveKglobalaccelReturnsDBusPayload) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("show-desktop");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::DBus);
    EXPECT_EQ(result->payload,
              "org.kde.kglobalaccel,/component/kwin,"
              "org.kde.kglobalaccel.Component,invokeShortcut,Show Desktop");
}

TEST(KdeDesktopResolve, ResolveKglobalaccelSecondPreset) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("task-switcher");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::DBus);
    EXPECT_EQ(result->payload,
              "org.kde.kglobalaccel,/component/kwin,"
              "org.kde.kglobalaccel.Component,invokeShortcut,ExposeAll");
}

TEST(KdeDesktopResolve, ResolveAppLaunchReturnsAppLaunchPayload) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("calculator");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::AppLaunch);
    EXPECT_EQ(result->payload, "kcalc");
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptForGnomeOnlyPreset) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("gnome-only").has_value());
}

TEST(KdeDesktopResolve, ResolveReturnsNulloptForUnknownId) {
    KDeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("not-a-real-preset").has_value());
}
```

- [ ] **Step 3: Register the test**

Edit `tests/CMakeLists.txt` to add after the generic resolve test:

```cmake
    core/desktop/test_generic_desktop_resolve.cpp
    core/desktop/test_kde_desktop_resolve.cpp
    services/test_active_device_resolver.cpp
```

- [ ] **Step 4: Build the test (should fail)**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -3
cmake --build build --target logitune-tests 2>&1 | tail -10
./build/tests/logitune-tests --gtest_filter="KdeDesktopResolve.*"
```

Expected: 5 failures (kglobalaccel ones + app-launch — all return nullopt from the stub).

- [ ] **Step 5: Implement the resolver**

Edit `src/core/desktop/KDeDesktop.cpp`. Add the include at the top (after existing includes):

```cpp
#include "actions/ActionPresetRegistry.h"
#include <QJsonObject>
```

Replace the stub `resolveNamedAction` with:

```cpp
std::optional<ButtonAction> KDeDesktop::resolveNamedAction(const QString &id) const
{
    if (!m_registry)
        return std::nullopt;

    const QJsonObject variant = m_registry->variantData(id, QStringLiteral("kde"));
    if (variant.isEmpty())
        return std::nullopt;

    // kglobalaccel: invoke a named action by DBus. Encoded as a five-field
    // payload: service,path,interface,method,arg
    if (variant.contains(QStringLiteral("kglobalaccel"))) {
        const QJsonObject spec = variant.value(QStringLiteral("kglobalaccel")).toObject();
        const QString component = spec.value(QStringLiteral("component")).toString();
        const QString name = spec.value(QStringLiteral("name")).toString();
        if (component.isEmpty() || name.isEmpty())
            return std::nullopt;

        const QString payload =
            QStringLiteral("org.kde.kglobalaccel,/component/") + component +
            QStringLiteral(",org.kde.kglobalaccel.Component,invokeShortcut,") + name;
        return ButtonAction{ButtonAction::DBus, payload};
    }

    // app-launch: simpler, same transport as the existing AppLaunch type.
    if (variant.contains(QStringLiteral("app-launch"))) {
        const QJsonObject spec = variant.value(QStringLiteral("app-launch")).toObject();
        const QString binary = spec.value(QStringLiteral("binary")).toString();
        if (binary.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::AppLaunch, binary};
    }

    return std::nullopt;
}
```

- [ ] **Step 6: Build and run new tests**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="KdeDesktopResolve.*"
./build/tests/logitune-tests
```

Expected: `7 tests from 1 test suite ran. PASSED 7.` Full suite: `671 PASSED 671`.

- [ ] **Step 7: Commit**

```bash
git add src/core/desktop/KDeDesktop.h src/core/desktop/KDeDesktop.cpp \
        tests/core/desktop/test_kde_desktop_resolve.cpp tests/CMakeLists.txt
git commit -m "feat(desktop): implement KdeDesktop::resolveNamedAction via kglobalaccel

Looks up the 'kde' variant in the preset registry. kglobalaccel hints
produce a DBus ButtonAction targeting org.kde.kglobalaccel's
invokeShortcut method with the action name as its argument. app-launch
hints produce an AppLaunch ButtonAction. Binding-independent: kglobalaccel
invokes actions by name regardless of what keystroke the user has bound."
```

---

## Task 7: `GnomeDesktop::resolveNamedAction` via gsettings

**Files:**
- Modify: `src/core/desktop/GnomeDesktop.h`
- Modify: `src/core/desktop/GnomeDesktop.cpp`
- Create: `tests/core/desktop/test_gnome_desktop_resolve.cpp`
- Modify: `tests/CMakeLists.txt`

GNOME has no binding-independent invoker for most actions. We shell out to `gsettings get <schema> <key>` to read the user's current binding, then translate the returned string into a keystroke ButtonAction.

`gsettings` returns GLib variant strings:
- `['<Super>d']` — one binding
- `['<Super>d', '<Primary><Super>d']` — multiple bindings
- `['']` — explicitly empty (user cleared)
- `[]` — no default set

We take the first non-empty entry, strip the `<...>` modifier blocks, and rewrite them as `Super+d` style using the existing `UinputInjector` keystroke format (Super, Ctrl, Alt, Shift, Meta).

For testability, we inject the gsettings runner as a `std::function` so tests script the raw gsettings output without actually spawning a process. Production uses the default runner (QProcess).

- [ ] **Step 1: Extend the header**

Edit `src/core/desktop/GnomeDesktop.h`. Open and view the current contents first, then add overrides, a setter-injected registry, and a setter-injected gsettings reader:

```cpp
#pragma once
#include "desktop/LinuxDesktopBase.h"
#include <QDBusInterface>
#include <QTimer>
#include <functional>
#include <optional>

namespace logitune {

class ActionPresetRegistry;

class GnomeDesktop : public LinuxDesktopBase {
    Q_OBJECT
public:
    /// Reader takes (schema, key) and returns the raw gsettings output
    /// (e.g. "['<Super>d']" or "" on failure).
    using GsettingsReader = std::function<QString(const QString &, const QString &)>;

    explicit GnomeDesktop(QObject *parent = nullptr);

    void start() override;
    bool available() const override;
    QString desktopName() const override;
    QStringList detectedCompositors() const override;
    void blockGlobalShortcuts(bool block) override;

    QString variantKey() const override;
    std::optional<ButtonAction> resolveNamedAction(const QString &id) const override;

    void setPresetRegistry(const ActionPresetRegistry *registry) { m_registry = registry; }
    void setGsettingsReader(GsettingsReader reader) { m_reader = std::move(reader); }

    /// Static helper: parse gsettings output ("['<Super>d']") into a
    /// keystroke payload ("Super+d"). Returns empty string on failure.
    static QString gsettingsToKeystroke(const QString &gsettingsOutput);

// ... (keep the existing private members and methods — only add the
// members below as private fields near the end of the class)

private:
    const ActionPresetRegistry *m_registry = nullptr;
    GsettingsReader m_reader;
    // ... existing private members (m_kwin, m_pollTimer, ...) stay
};

} // namespace logitune
```

(Read the current header first to preserve its private section verbatim; only add the three new fields.)

- [ ] **Step 2: Create the failing test file**

`tests/core/desktop/test_gnome_desktop_resolve.cpp`:

```cpp
#include <gtest/gtest.h>

#include "desktop/GnomeDesktop.h"
#include "actions/ActionPresetRegistry.h"

using logitune::GnomeDesktop;
using logitune::ActionPresetRegistry;
using logitune::ButtonAction;

namespace {
const QByteArray kCatalog = R"([
    {
        "id": "show-desktop",
        "label": "Show Desktop",
        "variants": {
            "gnome": { "gsettings": { "schema": "org.gnome.desktop.wm.keybindings", "key": "show-desktop" } },
            "kde":   { "kglobalaccel": { "component": "kwin", "name": "Show Desktop" } }
        }
    },
    {
        "id": "calculator",
        "label": "Calculator",
        "variants": {
            "gnome": { "app-launch": { "binary": "gnome-calculator" } }
        }
    },
    {
        "id": "kde-only",
        "label": "Kde Only",
        "variants": {
            "kde": { "kglobalaccel": { "component": "kwin", "name": "Something" } }
        }
    }
])";
} // namespace

// ---------------------------------------------------------------------------
// Static transform: gsettings output to keystroke
// ---------------------------------------------------------------------------

TEST(GnomeDesktopResolve, TransformSingleModifier) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['<Super>d']"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformCtrlAlt) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['<Primary><Alt>Left']"),
              "Ctrl+Alt+Left");
}

TEST(GnomeDesktopResolve, TransformPicksFirstNonEmpty) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['', '<Super>d']"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformReturnsEmptyForEmptyList) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("[]"), "");
}

TEST(GnomeDesktopResolve, TransformReturnsEmptyForAllEmpty) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['']"), "");
}

TEST(GnomeDesktopResolve, TransformHandlesWhitespace) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("[ '<Super>d' ]"), "Super+d");
}

TEST(GnomeDesktopResolve, TransformBareKeyNoModifiers) {
    EXPECT_EQ(GnomeDesktop::gsettingsToKeystroke("['Print']"), "Print");
}

// ---------------------------------------------------------------------------
// End-to-end resolve
// ---------------------------------------------------------------------------

TEST(GnomeDesktopResolve, VariantKeyIsGnome) {
    GnomeDesktop d;
    EXPECT_EQ(d.variantKey(), "gnome");
}

TEST(GnomeDesktopResolve, ResolveGsettingsReturnsKeystroke) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);
    d.setGsettingsReader([](const QString &schema, const QString &key) {
        if (schema == "org.gnome.desktop.wm.keybindings" && key == "show-desktop")
            return QStringLiteral("['<Super>d']");
        return QString();
    });

    auto result = d.resolveNamedAction("show-desktop");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::Keystroke);
    EXPECT_EQ(result->payload, "Super+d");
}

TEST(GnomeDesktopResolve, ResolveAppLaunchReturnsAppLaunchPayload) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    auto result = d.resolveNamedAction("calculator");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ButtonAction::AppLaunch);
    EXPECT_EQ(result->payload, "gnome-calculator");
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptWhenBindingEmpty) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);
    d.setGsettingsReader([](const QString &, const QString &) {
        return QStringLiteral("['']");
    });

    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptForKdeOnlyPreset) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("kde-only").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptForUnknownId) {
    GnomeDesktop d;
    ActionPresetRegistry reg;
    reg.loadFromJson(kCatalog);
    d.setPresetRegistry(&reg);

    EXPECT_FALSE(d.resolveNamedAction("not-real").has_value());
}

TEST(GnomeDesktopResolve, ResolveReturnsNulloptWithoutRegistry) {
    GnomeDesktop d;
    EXPECT_FALSE(d.resolveNamedAction("show-desktop").has_value());
}
```

- [ ] **Step 3: Register the test**

Edit `tests/CMakeLists.txt` to add after the kde resolve test:

```cmake
    core/desktop/test_kde_desktop_resolve.cpp
    core/desktop/test_gnome_desktop_resolve.cpp
    services/test_active_device_resolver.cpp
```

- [ ] **Step 4: Build and run (fails: stub returns nullopt)**

```bash
cmake -S . -B build -G Ninja 2>&1 | tail -3
cmake --build build --target logitune-tests 2>&1 | tail -10
./build/tests/logitune-tests --gtest_filter="GnomeDesktopResolve.*" 2>&1 | tail -5
```

Expected: the static-transform tests already fail (function unimplemented) + resolve-path tests fail.

- [ ] **Step 5: Implement the transform and the resolver**

Edit `src/core/desktop/GnomeDesktop.cpp`. Add includes at the top:

```cpp
#include "actions/ActionPresetRegistry.h"
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
```

Append to the end of the file (inside namespace `logitune`):

```cpp
// ---------------------------------------------------------------------------
// gsettings output to keystroke transform
// ---------------------------------------------------------------------------

QString GnomeDesktop::gsettingsToKeystroke(const QString &gsettingsOutput)
{
    // gsettings returns GLib variant strings like "['<Super>d']" or
    // "['<Primary><Alt>Left', '<Super>D']". Find the first non-empty
    // single-quoted binding.
    static const QRegularExpression kBindingRe(QStringLiteral("'([^']*)'"));
    QRegularExpressionMatchIterator it = kBindingRe.globalMatch(gsettingsOutput);

    QString binding;
    while (it.hasNext()) {
        const QString candidate = it.next().captured(1);
        if (!candidate.isEmpty()) {
            binding = candidate;
            break;
        }
    }
    if (binding.isEmpty())
        return {};

    // Rewrite <Super>, <Primary>, <Control>, <Ctrl>, <Alt>, <Shift>, <Meta>
    // into the Logitune format: Super+, Ctrl+, Alt+, Shift+, Meta+.
    static const QRegularExpression kModifierRe(QStringLiteral("<([^>]+)>"));
    QString out;
    int pos = 0;
    auto mit = kModifierRe.globalMatch(binding);
    while (mit.hasNext()) {
        auto m = mit.next();
        const QString mod = m.captured(1);
        QString norm;
        if (mod.compare(QStringLiteral("Primary"), Qt::CaseInsensitive) == 0 ||
            mod.compare(QStringLiteral("Control"), Qt::CaseInsensitive) == 0 ||
            mod.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Ctrl");
        } else if (mod.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Alt");
        } else if (mod.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Shift");
        } else if (mod.compare(QStringLiteral("Super"), Qt::CaseInsensitive) == 0 ||
                   mod.compare(QStringLiteral("Meta"), Qt::CaseInsensitive) == 0) {
            norm = QStringLiteral("Super");
        } else {
            norm = mod;
        }
        if (!out.isEmpty()) out += QLatin1Char('+');
        out += norm;
        pos = m.capturedEnd();
    }

    const QString tail = binding.mid(pos).trimmed();
    if (!tail.isEmpty()) {
        if (!out.isEmpty()) out += QLatin1Char('+');
        out += tail;
    }
    return out;
}

// ---------------------------------------------------------------------------
// resolveNamedAction
// ---------------------------------------------------------------------------

std::optional<ButtonAction> GnomeDesktop::resolveNamedAction(const QString &id) const
{
    if (!m_registry)
        return std::nullopt;

    const QJsonObject variant = m_registry->variantData(id, QStringLiteral("gnome"));
    if (variant.isEmpty())
        return std::nullopt;

    // gsettings: look up the live binding and translate.
    if (variant.contains(QStringLiteral("gsettings"))) {
        const QJsonObject spec = variant.value(QStringLiteral("gsettings")).toObject();
        const QString schema = spec.value(QStringLiteral("schema")).toString();
        const QString key = spec.value(QStringLiteral("key")).toString();
        if (schema.isEmpty() || key.isEmpty())
            return std::nullopt;

        QString raw;
        if (m_reader) {
            raw = m_reader(schema, key);
        } else {
            QProcess p;
            p.start(QStringLiteral("gsettings"),
                    {QStringLiteral("get"), schema, key});
            if (!p.waitForFinished(1000))
                return std::nullopt;
            raw = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }

        const QString combo = gsettingsToKeystroke(raw);
        if (combo.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::Keystroke, combo};
    }

    // app-launch: direct passthrough.
    if (variant.contains(QStringLiteral("app-launch"))) {
        const QJsonObject spec = variant.value(QStringLiteral("app-launch")).toObject();
        const QString binary = spec.value(QStringLiteral("binary")).toString();
        if (binary.isEmpty())
            return std::nullopt;
        return ButtonAction{ButtonAction::AppLaunch, binary};
    }

    return std::nullopt;
}
```

Also ensure the existing stub `variantKey` returns `"gnome"` (added in Task 5).

- [ ] **Step 6: Build and run new tests**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="GnomeDesktopResolve.*"
./build/tests/logitune-tests
```

Expected: `13 tests from 1 test suite ran. PASSED 13.` Full suite `684 PASSED 684`.

- [ ] **Step 7: Commit**

```bash
git add src/core/desktop/GnomeDesktop.h src/core/desktop/GnomeDesktop.cpp \
        tests/core/desktop/test_gnome_desktop_resolve.cpp tests/CMakeLists.txt
git commit -m "feat(desktop): implement GnomeDesktop::resolveNamedAction via gsettings

gsettings variants: read the user's current binding (injectable reader
for tests, QProcess in production), parse the GLib variant output,
rewrite modifier tokens ('<Primary>', '<Super>', ...) into Logitune
keystroke format ('Ctrl+', 'Super+', ...). Empty bindings return nullopt
so the UI can grey out the action.

app-launch variants: direct passthrough to ButtonAction::AppLaunch."
```

---

## Task 8: `ButtonActionDispatcher` resolves `PresetRef` and `ActionExecutor` handles five-field DBus

**Files:**
- Modify: `src/app/services/ButtonActionDispatcher.h`
- Modify: `src/app/services/ButtonActionDispatcher.cpp`
- Modify: `src/core/ActionExecutor.h` (no changes to `.h` needed if impl-only)
- Modify: `src/core/ActionExecutor.cpp` (extend `parseDBusAction` to accept 4 or 5 fields)
- Modify: `src/core/input/UinputInjector.cpp` (pass arg to DBus call)
- Modify: `tests/test_action_executor.cpp` (cover 5-field DBus)
- Modify: `tests/services/test_button_action_dispatcher.cpp`
- Modify: `tests/services/ButtonActionDispatcherFixture.h`
- Modify: `src/app/AppRoot.cpp` (constructor only: pass desktop to dispatcher)

This task wires resolution in; AppRoot then hands the real `IDesktopIntegration*` to the dispatcher (already done — `m_desktop` is already available in AppRoot).

- [ ] **Step 1: Read the existing dispatcher fixture so you know its shape**

```bash
cat tests/services/ButtonActionDispatcherFixture.h
```

(The fixture already stubs dependencies; you'll extend it with a `MockDesktop`.)

- [ ] **Step 2: Extend `ButtonActionDispatcher` to accept `IDesktopIntegration*`**

Edit `src/app/services/ButtonActionDispatcher.h`. Add a forward declaration and constructor arg:

```cpp
namespace logitune {

class ActionExecutor;
class ActiveDeviceResolver;
class ProfileEngine;
class IDevice;
class IDesktopIntegration;

class ButtonActionDispatcher : public QObject {
    Q_OBJECT
public:
    ButtonActionDispatcher(ProfileEngine *profileEngine,
                           ActionExecutor *actionExecutor,
                           ActiveDeviceResolver *selection,
                           IDesktopIntegration *desktop = nullptr,
                           QObject *parent = nullptr);
    // ... rest unchanged
private:
    // ... existing
    IDesktopIntegration *m_desktop;
    // ... existing
```

Edit `src/app/services/ButtonActionDispatcher.cpp`. Extend the constructor signature and add the `#include`:

```cpp
#include "interfaces/IDesktopIntegration.h"
// ...

ButtonActionDispatcher::ButtonActionDispatcher(ProfileEngine *profileEngine,
                                               ActionExecutor *actionExecutor,
                                               ActiveDeviceResolver *selection,
                                               IDesktopIntegration *desktop,
                                               QObject *parent)
    : QObject(parent)
    , m_profileEngine(profileEngine)
    , m_actionExecutor(actionExecutor)
    , m_selection(selection)
    , m_desktop(desktop)
{}
```

- [ ] **Step 3: Add preset resolution to `onDivertedButtonPressed`**

In `src/app/services/ButtonActionDispatcher.cpp`, find the existing dispatch block (around line 102-120 where it handles `ba.type`). After the final `AppLaunch` branch, add a new `PresetRef` branch that resolves and recurses:

Replace:

```cpp
    } else if (ba.type == ButtonAction::AppLaunch && !ba.payload.isEmpty()) {
        m_actionExecutor->launchApp(ba.payload);
    }
}
```

with:

```cpp
    } else if (ba.type == ButtonAction::AppLaunch && !ba.payload.isEmpty()) {
        m_actionExecutor->launchApp(ba.payload);
    } else if (ba.type == ButtonAction::PresetRef && !ba.payload.isEmpty()) {
        if (!m_desktop) {
            qCWarning(lcApp) << "preset action requested but desktop integration is null"
                             << ba.payload;
            return;
        }
        auto resolved = m_desktop->resolveNamedAction(ba.payload);
        if (!resolved.has_value()) {
            qCWarning(lcApp) << "preset" << ba.payload
                             << "not resolvable on" << m_desktop->variantKey();
            return;
        }
        if (resolved->type == ButtonAction::Keystroke || resolved->type == ButtonAction::Media) {
            m_actionExecutor->injectKeystroke(resolved->payload);
        } else if (resolved->type == ButtonAction::DBus) {
            m_actionExecutor->executeDBusCall(resolved->payload);
        } else if (resolved->type == ButtonAction::AppLaunch) {
            m_actionExecutor->launchApp(resolved->payload);
        } else {
            qCWarning(lcApp) << "preset" << ba.payload
                             << "resolved to unsupported type" << resolved->type;
        }
    }
}
```

Leave `onGestureRaw` unchanged — gestures already dispatch keystrokes; supporting `PresetRef` inside gestures is a follow-up (documented in the Deferred section at the bottom).

- [ ] **Step 4: Write a failing dispatcher test**

Append to `tests/services/test_button_action_dispatcher.cpp`:

```cpp
// ---------------------------------------------------------------------------
// PresetRef resolution
// ---------------------------------------------------------------------------

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvesViaDesktopAndFiresKeystroke) {
    mockDesktop().setVariantKey("mock");
    mockDesktop().scriptResolve("show-desktop",
                                ButtonAction{ButtonAction::Keystroke, "Super+D"});

    primeButtonAction(0, ButtonAction{ButtonAction::PresetRef, "show-desktop"});
    pressButton(0);

    EXPECT_EQ(mockInjector().lastKeystroke(), "Super+D");
}

TEST_F(ButtonActionDispatcherFixture, PresetRefUnresolvedLogsAndFiresNothing) {
    mockDesktop().setVariantKey("mock");
    // No scriptResolve -> resolveNamedAction returns nullopt

    primeButtonAction(0, ButtonAction{ButtonAction::PresetRef, "show-desktop"});
    pressButton(0);

    EXPECT_EQ(mockInjector().lastKeystroke(), QString());
    EXPECT_EQ(mockInjector().lastDBusCall(), QString());
    EXPECT_EQ(mockInjector().lastLaunch(), QString());
}

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvedToDBusFiresDBus) {
    mockDesktop().scriptResolve("calculator",
                                ButtonAction{ButtonAction::DBus,
                                             "org.x,/path,org.x.Iface,method"});
    primeButtonAction(0, ButtonAction{ButtonAction::PresetRef, "calculator"});
    pressButton(0);

    EXPECT_EQ(mockInjector().lastDBusCall(),
              "org.x,/path,org.x.Iface,method");
}

TEST_F(ButtonActionDispatcherFixture, PresetRefResolvedToAppLaunchFiresLaunch) {
    mockDesktop().scriptResolve("calculator",
                                ButtonAction{ButtonAction::AppLaunch, "gnome-calculator"});
    primeButtonAction(0, ButtonAction{ButtonAction::PresetRef, "calculator"});
    pressButton(0);

    EXPECT_EQ(mockInjector().lastLaunch(), "gnome-calculator");
}
```

- [ ] **Step 5: Extend the fixture to expose the mock desktop + helpers**

Read the existing fixture to locate its construction of the dispatcher:

```bash
cat tests/services/ButtonActionDispatcherFixture.h
```

Add a `MockDesktop` member, construct the dispatcher with it, and expose `mockDesktop()`. Also add `primeButtonAction(int index, ButtonAction action)` and `pressButton(int index)` helpers if they don't already exist (if they do, keep using them). The MockInjector likely tracks `lastKeystroke`, `lastDBusCall`, `lastLaunch` — verify those getters exist and add them if missing (adding to `tests/mocks/MockInjector.h`+`.cpp`).

(Read MockInjector to know what's there; the implementer extends only what's actually missing.)

```bash
cat tests/mocks/MockInjector.h tests/mocks/MockInjector.cpp
```

If `lastDBusCall` and `lastLaunch` don't already exist, add them as simple string accumulators.

- [ ] **Step 6: Build and run the dispatcher tests**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ButtonActionDispatcherFixture.Preset*"
./build/tests/logitune-tests
```

Expected: 4 preset tests pass; full suite still passes.

- [ ] **Step 7: Update AppRoot construction**

Edit `src/app/AppRoot.cpp` where `m_buttonDispatcher` is constructed (around line 48). Pass `m_desktop`:

```cpp
    , m_buttonDispatcher(&m_profileEngine, &m_actionExecutor, &m_deviceResolver,
                         m_desktop, this)
```

- [ ] **Step 8: Extend `ActionExecutor::parseDBusAction` to accept 4 or 5 fields + pass arg to injector**

Since KDE's resolver produces 5-field payloads (`service,path,interface,method,arg`), extend the parser. But the existing `DBusCall` struct has only 4 fields. We extend `DBusCall` minimally:

Edit `src/core/ActionExecutor.h`:

```cpp
struct DBusCall {
    QString service;
    QString path;
    QString interface;
    QString method;
    QString arg;      // optional; empty means "call with no args"
};
```

Edit `src/core/ActionExecutor.cpp`, replace `parseDBusAction` with:

```cpp
DBusCall ActionExecutor::parseDBusAction(const QString &spec)
{
    const QStringList parts = spec.split(QLatin1Char(','));
    if (parts.size() < 4 || parts.size() > 5)
        return {};

    DBusCall call{
        parts[0].trimmed(),
        parts[1].trimmed(),
        parts[2].trimmed(),
        parts[3].trimmed(),
        parts.size() == 5 ? parts[4].trimmed() : QString(),
    };
    return call;
}
```

Edit `src/core/input/UinputInjector.cpp` where `sendDBusCall` is implemented. It currently parses the 4-field spec and invokes with no args. Extend to call with the optional string arg:

```bash
grep -n "sendDBusCall\|parseDBusAction\|QDBusInterface" src/core/input/UinputInjector.cpp | head -20
```

Find the existing `sendDBusCall` body and, where it does the DBus call, change:

```cpp
iface.call(call.method);
```

to:

```cpp
if (call.arg.isEmpty())
    iface.call(call.method);
else
    iface.call(call.method, call.arg);
```

(Exact call site varies — the implementer reads first and edits conservatively.)

- [ ] **Step 9: Add DBus parser tests (4-field and 5-field)**

Append to `tests/test_action_executor.cpp`:

```cpp
TEST(ActionExecutor, ParseDBus4FieldAcceptedWithEmptyArg) {
    auto c = ActionExecutor::parseDBusAction("a,b,c,d");
    EXPECT_EQ(c.service, "a");
    EXPECT_EQ(c.method, "d");
    EXPECT_TRUE(c.arg.isEmpty());
}

TEST(ActionExecutor, ParseDBus5FieldPopulatesArg) {
    auto c = ActionExecutor::parseDBusAction(
        "org.kde.kglobalaccel,/component/kwin,"
        "org.kde.kglobalaccel.Component,invokeShortcut,Show Desktop");
    EXPECT_EQ(c.service, "org.kde.kglobalaccel");
    EXPECT_EQ(c.path, "/component/kwin");
    EXPECT_EQ(c.interface, "org.kde.kglobalaccel.Component");
    EXPECT_EQ(c.method, "invokeShortcut");
    EXPECT_EQ(c.arg, "Show Desktop");
}

TEST(ActionExecutor, ParseDBus6FieldRejected) {
    auto c = ActionExecutor::parseDBusAction("a,b,c,d,e,f");
    EXPECT_TRUE(c.method.isEmpty());
}

TEST(ActionExecutor, ParseDBus3FieldRejected) {
    auto c = ActionExecutor::parseDBusAction("a,b,c");
    EXPECT_TRUE(c.method.isEmpty());
}
```

- [ ] **Step 10: Full build + full test suite**

```bash
cmake --build build 2>&1 | tail -5
./build/tests/logitune-tests
```

Expected: 688+ tests pass (existing + 4 preset dispatcher + 4 DBus parser).

- [ ] **Step 11: Commit**

```bash
git add src/app/services/ButtonActionDispatcher.h \
        src/app/services/ButtonActionDispatcher.cpp \
        src/core/ActionExecutor.h src/core/ActionExecutor.cpp \
        src/core/input/UinputInjector.cpp \
        src/app/AppRoot.cpp \
        tests/services/test_button_action_dispatcher.cpp \
        tests/services/ButtonActionDispatcherFixture.h \
        tests/mocks/MockInjector.h tests/mocks/MockInjector.cpp \
        tests/test_action_executor.cpp
git commit -m "feat(dispatcher): resolve PresetRef via IDesktopIntegration at fire time

ButtonActionDispatcher accepts an IDesktopIntegration and, when a
PresetRef button action fires, calls desktop->resolveNamedAction(id) and
recurses the resolved ButtonAction into the existing keystroke/DBus/
app-launch paths. Null desktop or nullopt resolution logs and fires
nothing.

ActionExecutor::parseDBusAction now accepts an optional fifth field for
DBus methods that need a string argument (needed by kglobalaccel's
invokeShortcut). DBusCall struct gains an 'arg' field. UinputInjector
passes the arg through when non-empty."
```

---

## Task 9: `ActionModel` migration — seven entries use `PresetRef`

**Files:**
- Modify: `src/app/models/ActionModel.cpp`
- Modify: `tests/test_action_model.cpp`

The visible name strings in the UI stay the same. The `actionType` changes from `keystroke`/`app-launch` to `preset`, and `payload` changes from the raw keystroke/binary to the preset id. `buttonActionToName` and `buttonEntryToAction` are updated accordingly.

- [ ] **Step 1: Add failing tests for the new entries**

Append to `tests/test_action_model.cpp`:

```cpp
TEST(ActionModel, ShowDesktopEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Show desktop");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "show-desktop");
}

TEST(ActionModel, TaskSwitcherEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Task switcher");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "task-switcher");
}

TEST(ActionModel, CalculatorEntryIsPresetRef) {
    ActionModel m;
    int idx = m.indexForName("Calculator");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "preset");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "calculator");
}

TEST(ActionModel, BackEntryStaysRawKeystroke) {
    ActionModel m;
    int idx = m.indexForName("Back");
    ASSERT_GE(idx, 0);
    auto i = m.index(idx, 0);
    EXPECT_EQ(m.data(i, ActionModel::ActionTypeRole).toString(), "keystroke");
    EXPECT_EQ(m.data(i, ActionModel::PayloadRole).toString(), "Alt+Left");
}

TEST(ActionModel, buttonEntryToActionPresetReturnsPresetRef) {
    ActionModel m;
    ButtonAction ba = m.buttonEntryToAction("preset", "Show desktop");
    EXPECT_EQ(ba.type, ButtonAction::PresetRef);
    EXPECT_EQ(ba.payload, "show-desktop");
}

TEST(ActionModel, buttonActionToNamePresetLooksUpLabel) {
    ActionModel m;
    EXPECT_EQ(m.buttonActionToName(ButtonAction{ButtonAction::PresetRef, "show-desktop"}),
              "Show desktop");
}
```

- [ ] **Step 2: Run to confirm failures**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionModel.*"
```

Expected: the six new tests fail.

- [ ] **Step 3: Update `ActionModel.cpp` entries**

Edit `src/app/models/ActionModel.cpp`. Replace the relevant rows in the `m_actions = { ... }` initializer:

Replace:
```cpp
        { "Calculator",           "Open the system calculator",                 "app-launch",      "kcalc"       },
        { "Close window",         "Close the active window",                    "keystroke",       "Alt+F4"      },
```
with:
```cpp
        { "Calculator",           "Open the system calculator",                 "preset",          "calculator"  },
        { "Close window",         "Close the active window",                    "preset",          "close-window"},
```

Replace:
```cpp
        { "Screenshot",           "Capture a screenshot",                       "keystroke",       "Print"       },
```
with:
```cpp
        { "Screenshot",           "Capture a screenshot",                       "preset",          "screenshot"  },
```

Replace:
```cpp
        { "Show desktop",         "Minimize all windows to show desktop",       "keystroke",       "Super+D"     },
        { "Switch desktop left",  "Switch to the virtual desktop on the left",  "keystroke",       "Ctrl+Super+Left" },
        { "Switch desktop right", "Switch to the virtual desktop on the right", "keystroke",       "Ctrl+Super+Right" },
        { "Task switcher",        "Open the window/task switcher",              "keystroke",       "Super+W"     },
```
with:
```cpp
        { "Show desktop",         "Minimize all windows to show desktop",       "preset",          "show-desktop" },
        { "Switch desktop left",  "Switch to the virtual desktop on the left",  "preset",          "switch-desktop-left" },
        { "Switch desktop right", "Switch to the virtual desktop on the right", "preset",          "switch-desktop-right" },
        { "Task switcher",        "Open the window/task switcher",              "preset",          "task-switcher" },
```

- [ ] **Step 4: Update `buttonActionToName` to handle `PresetRef`**

In `src/app/models/ActionModel.cpp`, replace `buttonActionToName` with:

```cpp
QString ActionModel::buttonActionToName(const ButtonAction &ba) const
{
    if (ba.type == ButtonAction::Default)
        return QString();
    if (ba.type == ButtonAction::GestureTrigger)
        return QStringLiteral("Gestures");
    if (ba.type == ButtonAction::PresetRef) {
        for (const auto &a : m_actions) {
            if (a.actionType == QStringLiteral("preset") && a.payload == ba.payload)
                return a.name;
        }
        return ba.payload;
    }
    if (ba.type == ButtonAction::Keystroke) {
        for (const auto &a : m_actions) {
            if (a.actionType == QStringLiteral("keystroke") && a.payload == ba.payload)
                return a.name;
        }
        return ba.payload;
    }
    return ba.payload;
}
```

- [ ] **Step 5: Update `buttonEntryToAction` to handle `preset`**

Append to the if-chain before the final `return {ButtonAction::Default, {}};`:

```cpp
    if (actionType == QStringLiteral("preset")) {
        QString payload = payloadForName(actionName);
        if (payload.isEmpty()) payload = actionName;
        return {ButtonAction::PresetRef, payload};
    }
```

- [ ] **Step 6: Rebuild and run**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionModel.*"
./build/tests/logitune-tests
```

Expected: all ActionModel tests pass, including the 6 new ones; full suite still passes.

- [ ] **Step 7: Commit**

```bash
git add src/app/models/ActionModel.cpp tests/test_action_model.cpp
git commit -m "feat(action-model): migrate 7 entries to PresetRef

Show desktop, Task switcher, Switch desktop left/right, Screenshot,
Close window, and Calculator now carry actionType=preset with the
preset id as payload (e.g. 'show-desktop'). The visible names stay the
same. buttonActionToName and buttonEntryToAction gain a PresetRef
branch that maps names to ids and back. All other action entries
remain raw keystrokes; profiles with raw keystroke payloads keep
working unchanged."
```

---

## Task 10: `ActionFilterModel` hides unsupported presets

**Files:**
- Modify: `src/app/models/ActionFilterModel.h`
- Modify: `src/app/models/ActionFilterModel.cpp`
- Modify: `src/app/AppRoot.cpp`
- Modify: `tests/test_action_filter_model.cpp`

Filter rule: for rows with `actionType == "preset"`, read the payload as the preset id, call `registry->supportedBy(id, desktop->variantKey())`, hide rows that return false. Rows with other types are unaffected.

- [ ] **Step 1: Extend the header**

Edit `src/app/models/ActionFilterModel.h`:

```cpp
#pragma once
#include <QSortFilterProxyModel>

namespace logitune {

class DeviceModel;
class IDesktopIntegration;
class ActionPresetRegistry;

class ActionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ActionFilterModel(DeviceModel *deviceModel,
                               IDesktopIntegration *desktop = nullptr,
                               const ActionPresetRegistry *registry = nullptr,
                               QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
    IDesktopIntegration *m_desktop;
    const ActionPresetRegistry *m_registry;
};

} // namespace logitune
```

- [ ] **Step 2: Add failing tests**

Append to `tests/test_action_filter_model.cpp`:

```cpp
#include "interfaces/IDesktopIntegration.h"
#include "actions/ActionPresetRegistry.h"
#include "tests/mocks/MockDesktop.h"

TEST(ActionFilterModel, HidesPresetUnsupportedByVariantKey) {
    // Action model has "Show desktop" as a preset row
    ActionModel src;
    DeviceModel dev;

    ActionPresetRegistry reg;
    reg.loadFromJson(R"([
        { "id": "show-desktop", "label": "Show Desktop",
          "variants": { "kde": {"kglobalaccel": {"component": "kwin", "name": "X"}} } }
    ])");

    logitune::test::MockDesktop desktop;
    desktop.setVariantKey("gnome");  // preset has no gnome variant

    ActionFilterModel filter(&dev, &desktop, &reg);
    filter.setSourceModel(&src);

    // Verify "Show desktop" is filtered out
    bool found = false;
    for (int r = 0; r < filter.rowCount(); ++r) {
        if (filter.data(filter.index(r, 0), ActionModel::NameRole).toString() == "Show desktop") {
            found = true; break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ActionFilterModel, ShowsPresetSupportedByVariantKey) {
    ActionModel src;
    DeviceModel dev;

    ActionPresetRegistry reg;
    reg.loadFromJson(R"([
        { "id": "show-desktop", "label": "Show Desktop",
          "variants": { "kde": {"kglobalaccel": {"component": "kwin", "name": "X"}} } }
    ])");

    logitune::test::MockDesktop desktop;
    desktop.setVariantKey("kde");

    ActionFilterModel filter(&dev, &desktop, &reg);
    filter.setSourceModel(&src);

    bool found = false;
    for (int r = 0; r < filter.rowCount(); ++r) {
        if (filter.data(filter.index(r, 0), ActionModel::NameRole).toString() == "Show desktop") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(ActionFilterModel, ShowsPresetWhenDesktopOrRegistryIsNull) {
    // Back-compat: null desktop or registry means "accept all" (startup race)
    ActionModel src;
    DeviceModel dev;
    ActionFilterModel filter(&dev, nullptr, nullptr);
    filter.setSourceModel(&src);

    bool found = false;
    for (int r = 0; r < filter.rowCount(); ++r) {
        if (filter.data(filter.index(r, 0), ActionModel::NameRole).toString() == "Show desktop") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}
```

- [ ] **Step 3: Run test to confirm compile error (old constructor signature)**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -10
```

Expected: build errors in existing callers of the 2-arg constructor (AppRoot). The new test file also fails.

- [ ] **Step 4: Update the implementation**

Edit `src/app/models/ActionFilterModel.cpp`:

```cpp
#include "ActionFilterModel.h"
#include "ActionModel.h"
#include "DeviceModel.h"
#include "actions/ActionPresetRegistry.h"
#include "interfaces/IDesktopIntegration.h"

namespace logitune {

ActionFilterModel::ActionFilterModel(DeviceModel *deviceModel,
                                     IDesktopIntegration *desktop,
                                     const ActionPresetRegistry *registry,
                                     QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_deviceModel(deviceModel)
    , m_desktop(desktop)
    , m_registry(registry)
{
    if (m_deviceModel) {
        connect(m_deviceModel, &DeviceModel::selectedChanged,
                this, [this]() { invalidateFilter(); });
    }
}

bool ActionFilterModel::filterAcceptsRow(int sourceRow,
                                         const QModelIndex &sourceParent) const
{
    const QString type = sourceModel()->data(
        sourceModel()->index(sourceRow, 0, sourceParent),
        ActionModel::ActionTypeRole).toString();

    // Preset filtering: hide if not supported on the current DE variant.
    if (type == QLatin1String("preset")) {
        if (!m_desktop || !m_registry)
            return true;   // startup race: show, dispatcher will no-op if unresolvable
        const QString id = sourceModel()->data(
            sourceModel()->index(sourceRow, 0, sourceParent),
            ActionModel::PayloadRole).toString();
        return m_registry->supportedBy(id, m_desktop->variantKey());
    }

    // Capability filtering (unchanged): hide actions whose required device
    // capability is absent on the current device.
    if (!m_deviceModel || m_deviceModel->selectedIndex() < 0)
        return true;

    if (type == QLatin1String("dpi-cycle"))
        return m_deviceModel->adjustableDpiSupported();
    if (type == QLatin1String("smartshift-toggle"))
        return m_deviceModel->smartShiftSupported();
    if (type == QLatin1String("gesture-trigger"))
        return m_deviceModel->reprogControlsSupported();
    if (type == QLatin1String("wheel-mode"))
        return m_deviceModel->thumbWheelSupported();
    return true;
}

} // namespace logitune
```

- [ ] **Step 5: Update `AppRoot` — instantiate the registry and pass it**

Edit `src/app/AppRoot.h` (search for `m_actionFilterModel`) to add a registry member and a forward declaration:

```cpp
// near other forward decls or includes:
#include "actions/ActionPresetRegistry.h"

// in the private member block:
    ActionPresetRegistry m_presetRegistry;
```

Edit `src/app/AppRoot.cpp` constructor body. Before `m_actionFilterModel` is assigned:

Replace:
```cpp
    m_actionFilterModel = std::make_unique<ActionFilterModel>(&m_deviceModel, this);
    m_actionFilterModel->setSourceModel(&m_actionModel);
```
with:
```cpp
    m_presetRegistry.loadFromResource();
    m_actionFilterModel = std::make_unique<ActionFilterModel>(
        &m_deviceModel, m_desktop, &m_presetRegistry, this);
    m_actionFilterModel->setSourceModel(&m_actionModel);
```

Also wire the registry into the DE impls so they can resolve. After the constructor body (or in `init()`), add:

```cpp
    if (auto *kde = dynamic_cast<KDeDesktop *>(m_desktop))
        kde->setPresetRegistry(&m_presetRegistry);
    if (auto *gnome = dynamic_cast<GnomeDesktop *>(m_desktop))
        gnome->setPresetRegistry(&m_presetRegistry);
```

(Place in the constructor body after `m_desktop` is finalized, or at the top of `init()` — either works. Constructor is cleaner.)

- [ ] **Step 6: Register the new test include path**

The test adds `#include "actions/ActionPresetRegistry.h"` and `#include "interfaces/IDesktopIntegration.h"` and `#include "tests/mocks/MockDesktop.h"`. Verify paths resolve. If `MockDesktop.h` isn't reachable from `tests/test_action_filter_model.cpp` adjust to `#include "mocks/MockDesktop.h"` (the tests directory is already on the include path — verify via `grep "target_include_directories" tests/CMakeLists.txt`).

- [ ] **Step 7: Build and run**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionFilterModel.*"
./build/tests/logitune-tests
```

Expected: filter model tests + full suite pass.

- [ ] **Step 8: Commit**

```bash
git add src/app/models/ActionFilterModel.h src/app/models/ActionFilterModel.cpp \
        src/app/AppRoot.h src/app/AppRoot.cpp \
        tests/test_action_filter_model.cpp
git commit -m "feat(action-filter): hide preset rows unsupported on active DE

ActionFilterModel accepts an IDesktopIntegration and an
ActionPresetRegistry. Rows with actionType=preset are filtered by
registry.supportedBy(id, desktop.variantKey()); capability-based
filtering (dpi-cycle, smartshift, gesture-trigger, wheel-mode) is
unchanged. AppRoot owns the registry, loads it from the bundled
resource at construction, and hands it to the filter model and
both concrete DE impls."
```

---

## Task 11: Live-binding grey-out in the picker (UI behavior polish)

**Files:**
- Modify: `src/app/models/ActionFilterModel.cpp`
- Modify: `tests/test_action_filter_model.cpp`

Earlier steps hide presets whose variant isn't in the JSON at all. This task adds the second gate: if the DE's resolver returns `nullopt` at list-time (user cleared the binding), hide the row too.

- [ ] **Step 1: Add failing test**

Append to `tests/test_action_filter_model.cpp`:

```cpp
TEST(ActionFilterModel, HidesPresetWhenLiveResolutionReturnsNullopt) {
    ActionModel src;
    DeviceModel dev;

    ActionPresetRegistry reg;
    reg.loadFromJson(R"([
        { "id": "show-desktop", "label": "Show Desktop",
          "variants": { "mock": {"anything": {}} } }
    ])");

    logitune::test::MockDesktop desktop;
    desktop.setVariantKey("mock");
    // No scriptResolve -> resolveNamedAction returns nullopt

    ActionFilterModel filter(&dev, &desktop, &reg);
    filter.setSourceModel(&src);

    bool found = false;
    for (int r = 0; r < filter.rowCount(); ++r) {
        if (filter.data(filter.index(r, 0), ActionModel::NameRole).toString() == "Show desktop") {
            found = true; break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ActionFilterModel, ShowsPresetWhenLiveResolutionSucceeds) {
    ActionModel src;
    DeviceModel dev;

    ActionPresetRegistry reg;
    reg.loadFromJson(R"([
        { "id": "show-desktop", "label": "Show Desktop",
          "variants": { "mock": {"anything": {}} } }
    ])");

    logitune::test::MockDesktop desktop;
    desktop.setVariantKey("mock");
    desktop.scriptResolve("show-desktop",
                          logitune::ButtonAction{logitune::ButtonAction::Keystroke, "Super+D"});

    ActionFilterModel filter(&dev, &desktop, &reg);
    filter.setSourceModel(&src);

    bool found = false;
    for (int r = 0; r < filter.rowCount(); ++r) {
        if (filter.data(filter.index(r, 0), ActionModel::NameRole).toString() == "Show desktop") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}
```

- [ ] **Step 2: Run to confirm failure**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionFilterModel.HidesPresetWhenLive*"
./build/tests/logitune-tests --gtest_filter="ActionFilterModel.ShowsPresetWhenLive*"
```

Expected: the `HidesPresetWhenLive*` test fails (row currently shown because `supportedBy` passes).

- [ ] **Step 3: Extend the filter's preset branch**

In `src/app/models/ActionFilterModel.cpp`, update the preset block inside `filterAcceptsRow`:

```cpp
    if (type == QLatin1String("preset")) {
        if (!m_desktop || !m_registry)
            return true;
        const QString id = sourceModel()->data(
            sourceModel()->index(sourceRow, 0, sourceParent),
            ActionModel::PayloadRole).toString();
        if (!m_registry->supportedBy(id, m_desktop->variantKey()))
            return false;
        // Second gate: live resolution (catches empty user bindings on GNOME etc.)
        return m_desktop->resolveNamedAction(id).has_value();
    }
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build --target logitune-tests 2>&1 | tail -5
./build/tests/logitune-tests --gtest_filter="ActionFilterModel.*"
./build/tests/logitune-tests
```

Expected: both new tests pass; full suite passes.

- [ ] **Step 5: Commit**

```bash
git add src/app/models/ActionFilterModel.cpp tests/test_action_filter_model.cpp
git commit -m "feat(action-filter): hide preset rows whose live binding is empty

Second gate after variantKey support: call desktop.resolveNamedAction(id)
at filter time and hide the row when it returns nullopt. Covers the
case where a GNOME user has cleared the binding in System Settings for
a preset that nominally has a gnome variant. Prevents the picker from
offering actions that would no-op at fire time."
```

---

## Task 12: App-level smoke test and PR

**Files:**
- Smoke test on real hardware (no code changes)
- Modify: `docs/wiki/Architecture.md` (one line reference to the new module, optional)

TDD did its job — each unit is tested. This last task confirms end-to-end behavior on the maintainer's real device.

- [ ] **Step 1: Full test run (must be clean)**

```bash
./build/tests/logitune-tests
```

Expected: the six `MX_Master_3` tests pass (per-machine install is fixed), all new tests pass, no regressions. Target: `696 tests from 52 test suites ran. PASSED 696.` (exact count may vary by one or two).

- [ ] **Step 2: Rebuild the whole app**

```bash
cmake --build build 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Kill any running logitune and relaunch (per CLAUDE.md)**

```bash
pkill -f logitune 2>/dev/null
sleep 1
nohup ./build/src/app/logitune > /tmp/logitune-preset-smoke.log 2>&1 & disown
sleep 2
tail -20 /tmp/logitune-preset-smoke.log
```

Expected: no crashes, "DeviceRegistry loaded 10 devices" line, no preset-registry errors.

- [ ] **Step 4: Smoke test — assign "Show desktop" preset to a button**

Manual:

1. Click gesture button on the MX Master 3S in Logitune's UI.
2. In the Actions panel, select "Show desktop".
3. Verify the profile file on disk contains `preset:show-desktop` for that binding (check `~/.config/logitune/profiles/*.ini`).
4. Press the button on the mouse. On KDE, KWin should toggle Show Desktop.
5. Repeat with "Switch desktop left", "Switch desktop right", "Task switcher", "Calculator".

Expected: every preset fires its DE-native action regardless of what keystroke the user has bound to Show Desktop in System Settings.

- [ ] **Step 5: Regression smoke — existing raw keystrokes still work**

Manual:

1. Assign "Copy" (raw keystroke `Ctrl+C`) to a spare button.
2. Focus a text area, press the button.
3. Verify it copies.

Expected: legacy raw-payload actions fire identically to before. No regression.

- [ ] **Step 6: Open the PR**

```bash
git log origin/master..HEAD --oneline
git push -u origin semantic-action-presets
gh pr create --title "Semantic action presets with per-DE invocation (closes #110)" \
    --body-file - <<'EOF'
## Summary

Adds a semantic action preset layer so Show Desktop / Task Switcher / Switch Desktop L/R / Screenshot / Close Window / Calculator actions work correctly on the active DE, with profiles portable across DEs.

## Architecture

- `ActionPreset` + `ActionPresetRegistry` in Core/Domain, static data loaded from a bundled `actions.json` Qt resource
- `IDesktopIntegration::variantKey()` and `IDesktopIntegration::resolveNamedAction()` added, each DE impl owns its resolution strategy
- KDE: `kglobalaccel` DBus, binding-independent
- GNOME: `gsettings` lookup of the user's live binding, then keystroke inject
- Generic fallback: returns `nullopt` for everything (no portable bindings in v1)
- `ButtonAction::PresetRef` new type variant; old raw keystroke/DBus/app-launch profiles keep working unchanged
- `ButtonActionDispatcher` resolves `PresetRef` at fire time and delegates to `ActionExecutor`
- `ActionFilterModel` hides preset rows on DEs that don't support them, and hides rows whose live binding is empty on GNOME-family

## What changed

- Seven entries in `ActionModel` migrated from hardcoded keystrokes to `PresetRef`
- `ActionExecutor::parseDBusAction` extended to optionally accept a fifth field (string arg, needed by kglobalaccel's invokeShortcut)

## Test plan

- [x] `./build/tests/logitune-tests` all pass on master-with-fix baseline
- [x] New unit tests: `ActionPreset` parse, `ActionPresetRegistry` load / lookup / supportedBy / variantData, `GenericDesktop::resolveNamedAction`, `KdeDesktop::resolveNamedAction` (kglobalaccel + app-launch), `GnomeDesktop::resolveNamedAction` (gsettings + app-launch + keystroke transform), `ButtonActionDispatcher` preset resolution, `ActionModel` entries, `ActionFilterModel` variantKey + live-binding gates
- [x] Manual smoke on MX Master 3S / KDE Plasma for all seven presets
- [x] Manual regression on raw keystroke / raw DBus / raw app-launch actions

## Follow-ups (separate issues, out of scope here)

- Hyprland / Sway / i3 / Wayfire / Cinnamon / MATE / XFCE / Deepin / LXQt variants
- Gesture-direction PresetRef resolution (same pattern as button dispatch)
- User-overlay presets under `~/.config/logitune/actions.d/`

Closes #110
EOF
```

- [ ] **Step 7: Mark complete**

At this point the plan is done. The PR is up for review.

---

## Deferred / Out of Scope

Intentionally not handled in this plan (documented in issue #110 follow-ups):

- Gesture directions (`up`/`down`/`left`/`right`/`click` in `Profile::gestures`) cannot store `PresetRef` payloads yet — gesture branch in `ButtonActionDispatcher` still uses `Keystroke`-only. Trivial extension but adds scope; left for a follow-up PR.
- New DE impls beyond KDE + GNOME + Generic (Hyprland, Sway, i3, XFCE, Cinnamon, MATE, Wayfire, Deepin, LXQt, Budgie, Pantheon).
- User preset overlay at `~/.config/logitune/actions.d/*.json`.
- Icon and category driven grouping in the QML picker (the data is in the preset, but UI doesn't use it yet).
- Action-picker tooltip on greyed-out presets explaining why ("no binding set in GNOME Settings").

---

## Self-Review (done, issues fixed inline)

**Spec coverage.** Every spec section maps to tasks:
- Core pieces §1-§5 -> Tasks 2-7
- `ButtonAction::PresetRef` §6 -> Task 1
- Execution path for PresetRef §7 -> Task 8
- Escape hatch §8 -> no change needed (unchanged behavior)
- Live-binding validation -> Task 11
- Support matrix, not generic defaults -> Task 10
- Per-DE strategy (KDE+GNOME+Generic) -> Tasks 5, 6, 7
- Seven presets -> Task 4 data, Task 9 `ActionModel` rows
- File structure listed in spec -> mapped 1:1 here
- Testing strategy (1-10 in spec) -> Tasks 2, 3, 4, 6, 7, 8, 9, 10, 11 cover units; Task 12 handles smoke test
- Open Questions 1-5 -> decided: resolution in dispatcher (Task 8), QDBus interface (Task 6 notes, UinputInjector pass-through in Task 8 step 8), gsettings transform specified (Task 7), Qt resource used (Task 4), actionType="preset" (Task 9)

**Placeholder scan.** Skimmed every "write the code" step; every code block is complete. No TBD, TODO-except-deferred-section, no "similar to Task N". One intentional `TODO implemented in task 6` comment exists in a stub that Task 6 replaces — that's appropriate because it's a stub.

**Type consistency.** Verified across tasks:
- `ActionPreset::fromJson` (Task 2) = signature used in Task 3 registry
- `ActionPresetRegistry::loadFromJson`/`loadFromResource`/`preset`/`supportedBy`/`variantData`/`all` — all defined in Task 3 header, used unchanged later
- `IDesktopIntegration::variantKey`/`resolveNamedAction` — signature introduced in Task 5, used unchanged in Tasks 6, 7, 8, 10, 11
- `MockDesktop::setVariantKey`/`scriptResolve` — introduced Task 5, used in Tasks 8, 10, 11
- `ButtonAction::PresetRef` — enum value added Task 1, consumers in Tasks 8, 9
- `ButtonActionDispatcher` constructor gains `IDesktopIntegration*` param in Task 8; AppRoot wiring in Task 8 step 7 and Task 10 step 5 (both pass `m_desktop`)
- `DBusCall::arg` added Task 8; consumers only in Task 8
- `ActionFilterModel` constructor gains two params in Task 10; unchanged in Task 11

Ready for execution.
