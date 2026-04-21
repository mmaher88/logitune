# ActionsPanel per-Device Filtering Design

**Status:** approved, ready for implementation plan
**Issue:** #63 (ActionModel: hide actions the selected device cannot execute)
**Target release:** next beta
**Author:** Mina Maher (brainstormed with Claude)
**Date:** 2026-04-21

## Summary

The ActionsPanel's action picker shows a static list of every assignable
action (Back, Forward, DPI cycle, Shift wheel mode, Gestures, Media
controls, ...) regardless of whether the currently-selected device can
actually execute each action. MX Vertical users, for example, see
"Shift wheel mode" in the picker even though the device has a
ratcheted-only wheel and the SmartShift toggle is a silent no-op at
dispatch time.

Add a `QSortFilterProxyModel` between `ActionModel` and the
`ActionsPanel` QML consumer. The proxy hides actions whose required
capability is not present on the selected device.

## Motivation

Three concrete gaps in the current unfiltered picker:

- **SmartShift toggle** appears for MX Vertical (`smartShift: false`
  in the descriptor). Selecting it divert-binds the button but
  `ActionExecutor.cpp:74` silently no-ops on dispatch. User-visible
  symptom: the button does nothing after remapping.
- **DPI cycle** appears for devices with `adjustableDpi: false`.
  Selecting it dispatches `DeviceSession::cycleDpi()` which bails
  early. Same silent no-op.
- **Gestures** (`gesture-trigger`) appears for devices with
  `reprogControls: false`. Divert fails, host never sees the press,
  nothing happens.

All three are failure modes where the UI offers something the device
can't deliver. Filtering the picker removes the trap.

## Approach

Three alternatives considered:

1. **Static `requiredCapability` on each `ActionEntry` + JS filter in
   ActionsPanel.qml.** Simplest diff but leaks capability domain into
   QML: adding a new capability means editing both `ActionModel.cpp`
   and the delegate's JavaScript switch.
2. **Q_INVOKABLE `DeviceModel::supportsAction(actionType)`.**
   Centralizes mapping in C++ but the delegate still branches on
   visibility, mixing filter concerns with rendering.
3. **`QSortFilterProxyModel` between ActionModel and the QML consumer**
   (chosen). Model layer owns filtering, QML consumer is unchanged,
   selection-change invalidation is one signal connection.

(3) is architecturally cleanest: Qt's standard Model/View/Delegate
separation, directly unit-testable, and leaves a natural hook for a
future "disable + tooltip" variant (replace filter with a `disabled`
role on the proxy rather than rewriting the delegate).

## Architecture

New class `ActionFilterModel` extends `QSortFilterProxyModel`. Owned
by `AppController`, constructed with a pointer to `DeviceModel`.
Connects to `DeviceModel::selectedChanged` and calls
`invalidateFilter()` when selection changes.

```cpp
class ActionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    ActionFilterModel(DeviceModel *deviceModel, QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    DeviceModel *m_deviceModel;
};
```

The raw `ActionModel` stays untouched. `AppController` wires the proxy
and registers it with the QML engine under the name `ActionModel`
(replacing the raw context property). QML consumers see the filtered
view transparently.

## Capability mapping

`filterAcceptsRow` reads the row's `ActionTypeRole` from the source
model and maps to a capability:

| `actionType`        | Required capability flag   | Rationale                                           |
|---------------------|----------------------------|-----------------------------------------------------|
| `default`           | none                       | Native button behavior, always available            |
| `keystroke`         | none                       | Host-side uinput injection                          |
| `app-launch`        | none                       | Host-side, device-independent                       |
| `media-controls`    | none                       | Keystroke-injected media keys                       |
| `none` (Do nothing) | none                       | Intentional no-op                                   |
| `dpi-cycle`         | `adjustableDpi`            | Requires DPI control on the device                  |
| `smartshift-toggle` | `smartShift`               | Requires free-spin wheel                            |
| `gesture-trigger`   | `reprogControls`           | Requires HID++ button diversion to receive presses  |
| `wheel-mode`        | `thumbWheel`               | Thumb wheel options only apply where one exists     |

Implementation:

```cpp
bool ActionFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    // When no device is selected (e.g. initial launch before udev scan
    // completes), show the full list. The ActionsPanel only opens in
    // response to a button-hotspot click on a selected device, so this
    // branch is mostly defensive.
    if (!m_deviceModel || m_deviceModel->selectedIndex() < 0)
        return true;

    const QString type = sourceModel()->data(
        sourceModel()->index(row, 0, parent),
        ActionModel::ActionTypeRole).toString();

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
```

## DeviceModel extensions

Two new `Q_PROPERTY`s on `DeviceModel`, matching the existing
`smartShiftSupported` / `smoothScrollSupported` / `thumbWheelSupported`
pattern:

- `adjustableDpiSupported` — reads `descriptor()->features().adjustableDpi`
- `reprogControlsSupported` — reads `descriptor()->features().reprogControls`

Both notify via `selectedChanged`. No model reorganization.

Fallback behavior: when no device is selected or the descriptor is
null, the getters return `false` (same pattern as existing
`smartShiftSupported`). The filter guards against this in
`filterAcceptsRow` by short-circuiting to `return true` when no
device is selected, so the picker is never empty before the first
device attaches. Once a device is selected, capability-gated rows
follow the getters.

## Wiring

`AppController`:

```cpp
// After m_actionModel construction:
m_actionFilterModel = std::make_unique<ActionFilterModel>(&m_deviceModel, this);
m_actionFilterModel->setSourceModel(&m_actionModel);
```

Where the QML engine is set up (previously
`setContextProperty("ActionModel", &m_actionModel)`):

```cpp
engine.rootContext()->setContextProperty("ActionModel",
    m_actionFilterModel.get());
```

Tests and any C++ consumers that need the raw list still go through
`actionModel()` (which returns the source, not the proxy).

## Code surface

### `src/app/models/ActionFilterModel.{h,cpp}` (new)

Contains the class. ~60 lines total.

### `src/app/models/DeviceModel.{h,cpp}` (modified)

- Add `Q_PROPERTY(bool adjustableDpiSupported READ ...)` and
  `Q_PROPERTY(bool reprogControlsSupported READ ...)`.
- Add `bool adjustableDpiSupported() const;` and
  `bool reprogControlsSupported() const;` getters.
- Both read `selectedDevice()->descriptor()->features().<flag>`
  with nullptr guards.

### `src/app/AppController.{h,cpp}` (modified)

- Add `std::unique_ptr<ActionFilterModel> m_actionFilterModel`.
- Construct + set source in the body of the constructor (or init()).
- `actionModel()` accessor keeps returning `&m_actionModel` so tests
  can directly exercise the source model without the filter.

### `src/app/main.cpp` (modified)

- Swap `setContextProperty("ActionModel", &m_actionModel)` to use
  `m_appController->actionFilterModel()` (new accessor).

### `src/app/CMakeLists.txt` (modified)

- Add `models/ActionFilterModel.cpp` to the target's sources.

### `tests/test_action_filter_model.cpp` (new)

Four tests exercising filter behavior. Uses the existing `MockDevice`
pattern, instantiating a minimal `DeviceModel` + `PhysicalDevice` +
`DeviceSession` stack to feed the filter.

## Tests

New file `tests/test_action_filter_model.cpp`:

1. **`EmptyDeviceModelShowsFullList`** — no device selected; filter
   returns `true` for every row; `proxy.rowCount() == source.rowCount()`.
2. **`FilterHidesUnsupportedActions`** — mock device with
   `smartShift=false`, `thumbWheel=false`, `adjustableDpi=true`,
   `reprogControls=true`; assert `"Shift wheel mode"` is not in the
   proxy's rows but `"DPI cycle"` and `"Gestures"` are.
3. **`SelectionChangeInvalidates`** — two mock devices: device A
   supports `adjustableDpi`, device B does not. Select A, observe
   `"DPI cycle"` present; select B via `DeviceModel::setSelectedIndex`,
   observe `"DPI cycle"` hidden.
4. **`UnrestrictedActionsAlwaysVisible`** — device with all capability
   flags false; assert `"Keyboard shortcut"`, `"Copy"`, `"Paste"`,
   `"Do nothing"` all still present.

Existing `test_action_model.cpp` stays as-is (source model unchanged).

## Rollout

Branch `fix-action-filter-per-device`. Single PR, four commits:

1. `feat(device-model): adjustableDpiSupported + reprogControlsSupported
   Q_PROPERTYs`.
2. `feat(app): ActionFilterModel proxy with capability-based
   filterAcceptsRow`.
3. `refactor(app): QML ActionModel context property points at filter
   proxy`.
4. `test(action-filter): capability-based filter tests`.

Ships in next beta.

## Known risks

- **Fresh-launch picker during device enumeration.** The first ~100ms
  after app start has no selected device; filter returns `true` for
  every row, user sees the full list. If a capability-less action is
  picked before enumeration completes, subsequent selection re-filters
  and may remove it from the picker — but the already-saved action
  stays on the button. Acceptable; matches current behavior where
  invalid selections are silent no-ops.
- **`QSortFilterProxyModel` re-invalidation cost.** Full re-scan of
  ~27 rows on every `selectedChanged`. Negligible.
- **Future capability additions.** Adding a new capability flag
  (e.g., `hapticFeedback`) means editing the C++ mapping in one place
  (`filterAcceptsRow`) and declaring the matching `*Supported`
  Q_PROPERTY on `DeviceModel`. Scales linearly; still better than the
  JS-switch-in-QML approach.

## Out of scope

- Disable-with-tooltip UX variant (we chose hide).
- Filtering gestures within the Gestures action submenu (already
  bounded by device-specific `defaultGestures`).
- Per-control action filtering (e.g., "DPI cycle" shown for wheel
  hotspots but not button hotspots). Controls-versus-actions is a
  different concern.
- Capability exposure for non-action purposes (status badges, help
  text, etc.).
