# Editor Mode — Design Spec

**Date:** 2026-04-15
**Status:** Approved
**Related:** PR #34 (`debug-simulate-all`, the predecessor passive-viewer flag)

## Problem

Logitune ships JSON device descriptors that hand-curate the layout of buttons, scroll wheels, and easy-switch indicators on each Logitech mouse. Today these descriptors are produced by `scripts/optionsplus_extractor` from Options+ data, then iteratively patched by hand because Options+ doesn't have all the data we need (e.g. MX Vertical has no back image; LIFT uses `core_metadata_left.json`/`_right.json` instead of `core_metadata.json`; m720-triathlon has channel buttons on the top of the device, not the back).

The current iteration loop is:
1. Open the JSON in a text editor.
2. Edit `xPct`/`yPct` by guess.
3. Restart Logitune with `--simulate-all`.
4. Squint at whether the slot circle landed in the right place.
5. Repeat.

This is slow, error-prone, and impossible to do for 30+ community devices in any reasonable time. We need an in-app editor that lets us drag elements directly on the rendered device pages and save back to the source descriptor file.

## Goals

- Drag easy-switch slot circles, button hotspot markers, and overlay cards directly on the existing device pages, with the changes saved back to the descriptor JSON the device was loaded from.
- Replace device images (back/front/side) by drag-and-drop or file picker.
- Edit human-readable text fields (slot labels, button names, device name) in place.
- Support undo/redo per-device, with manual save (no autosave).
- Detect external file changes (e.g. `git pull` on the community-devices repo) and prompt before clobbering edits.
- Zero impact on the production binary path: editor code only runs when the `--edit` CLI flag is set.

## Non-goals

- Editing protocol-level fields (`controlId`, `buttonIndex`, DPI ranges, `capabilities` flags). These come from the hardware and shouldn't be hand-edited.
- Carousel ordering, custom themes, custom icons, capability overrides.
- Multi-window coordination.
- A "publish to community-devices repo" button (deferred to a future brainstorm).
- Cleaning up orphaned uploaded PNGs after undo (acceptable cost: a few stray files until `git clean`).
- Auto-save / drafts.

## Approach

**WYSIWYG in-place editor.** When `--edit` is set, the existing BUTTONS / POINT & SCROLL / EASY-SWITCH pages gain drag handles on their existing markers, circles, and cards. A toolbar at the top of the central content area shows save/undo/redo/reset and an unsaved-changes indicator. The user edits in the exact rendering they ship — no separate editor view, no tab-switching to verify positions.

The choice of in-place editing over a dedicated editor tab is deliberate: fine-tuning `labelOffsetYPct` matters because cards overlap text in the *real* page, not in an abstract editor canvas. Editing in production rendering eliminates the feedback delay.

The CLI flag `--edit` is **orthogonal** to `--simulate-all`. `--edit` alone edits the descriptor of whatever real device is plugged in. `--simulate-all --edit` is bulk-curation mode: every registered device loaded, all editable.

## Architecture

The design follows the existing MVVM split used throughout Logitune.

### Model layer (pure C++, no QML knowledge)

**`JsonDevice` extension**
Add source-path tracking so the editor knows where to write back:

```cpp
class JsonDevice : public IDevice {
public:
    JsonDevice(QJsonObject obj, QString sourcePath);
    QString sourcePath() const { return m_sourcePath; }
    qint64 loadedMtime() const { return m_loadedMtime; }
    // ... existing methods unchanged
private:
    QString m_sourcePath;     // canonicalized absolute path
    qint64 m_loadedMtime;     // captured at construction
};
```

`m_sourcePath` is set via `QFileInfo::canonicalFilePath()`. `m_loadedMtime` is captured via `QFileInfo::lastModified().toSecsSinceEpoch()` for the file watcher's stale-check.

`IDevice` is **not** changed. The editor casts `qobject_cast<const JsonDevice*>` to check editability; built-in devices (if any) silently aren't editable.

**`DescriptorWriter` (new C++ helper)**
Pure file I/O service. Takes a `QJsonObject` and a path, writes atomically via `QSaveFile`:

```cpp
class DescriptorWriter {
public:
    enum Result { Ok, IoError, JsonError };
    Result write(const QString &path, const QJsonObject &obj, QString *errorOut = nullptr);
};
```

Writes use `QJsonDocument::Indented` with 4-space indent (matching the community-devices repo convention). Atomic via `QSaveFile::commit()` (rename-into-place). Importantly, the writer applies edits to the **parsed-from-disk** JSON object, not to a re-derivation from `JsonDevice`, so any unknown future fields in the file are preserved verbatim.

**`EditCommand` struct**
Tagged-struct undo entries (no class hierarchy):

```cpp
struct EditCommand {
    enum Kind { SlotMove, HotspotMove, ImageReplace, TextEdit } kind;
    int index;                // slot idx / button idx (-1 for top-level)
    QString role;             // "back"/"front"/"side" for ImageReplace; field name for TextEdit
    QJsonValue before;        // full prior subobject or value
    QJsonValue after;         // new subobject or value
};
```

Undo = swap `before`↔`after` and re-apply. No diffing logic.

### ViewModel layer (Q_PROPERTY-rich, QML-registered)

**`EditorModel` (new QML singleton — "Model" naming matches existing repo convention; it's a ViewModel in MVVM terms)**

State:

```cpp
class EditorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing CONSTANT)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString activeDevicePath READ activeDevicePath NOTIFY activeDevicePathChanged)

public slots:
    void updateSlotPosition(int idx, double xPct, double yPct);
    void updateHotspot(int buttonIndex, double xPct, double yPct,
                       QString side, double labelOffsetYPct);
    void updateText(QString field, int index, QString value);
    void replaceImage(QString role, QString sourcePath);
    void save();
    void reset();
    void undo();
    void redo();

signals:
    void dirtyChanged();
    void undoStateChanged();
    void activeDevicePathChanged();
    void saved(QString path);
    void saveFailed(QString path, QString error);
    void externalChangeDetected(QString path);

private:
    bool m_editing = false;
    QString m_activeDevicePath;
    QHash<QString, QJsonObject> m_pendingEdits;       // path -> in-memory descriptor
    QHash<QString, QStack<EditCommand>> m_undoStacks; // per-device
    QHash<QString, QStack<EditCommand>> m_redoStacks;
    QSet<QString> m_dirty;
    QSet<QString> m_selfWrittenPaths;                 // suppress watcher self-fire
    QFileSystemWatcher *m_watcher;
    DeviceRegistry *m_registry;                       // for reload after save
    DescriptorWriter m_writer;
};
```

`EditorModel` is instantiated **only** when `--edit` is set. It is not a globally-registered singleton; it does not exist in the production binary path.

**State management rules**
- During a drag, the QML element binds its `x`/`y` directly to the drag handler's centroid so it visually tracks the cursor — `EditorModel` is **not** called on every position change. Only on drag *release* does the QML side call `EditorModel.updateSlotPosition(...)`, which mutates `m_pendingEdits[activeDevicePath]` and pushes a single `EditCommand` for the entire drag (so undo undoes the whole drag, not every pixel).
- Switching active device leaves the prior device's pending edits and undo stack intact; switching back restores them. Per-device state is keyed by source path.
- Save clears `m_pendingEdits[path]`, both stacks for that path, removes from `m_dirty`. Reset is the same except it doesn't write to disk; reset shows a confirm dialog if `hasUnsavedChanges`.
- Save failures emit `saveFailed` and leave state intact for retry.

**Existing `DeviceModel` integration**
When `EditorModel` mutates the in-memory descriptor for the active device, it asks `DeviceModel` to re-emit its position-changed signals so existing bindings repaint. `DeviceModel` itself gains no new properties — it just gets a "refresh from current source" entry point that the editor (and the file watcher) can call.

### View layer (QML)

**`EditorToolbar.qml` (new component)**
A 36px bar anchored to the top of the central content area in `MainWindow.qml`, visible only when `EditorModel.editing`. Shared across all pages. Layout, left to right:
- Truncated path label (e.g. `…/devices/mx-master-3s.json`)
- Unsaved-changes dot + "Unsaved changes" text (visible when `hasUnsavedChanges`)
- Undo / Redo icon buttons (enabled from `canUndo`/`canRedo`)
- Reset button (with confirm dialog when dirty)
- Save button (primary style, disabled when not dirty)

**Edit-mode visual indicator**
When `editing` is true, the sidebar gets a 4px amber stripe along its left edge so the user always knows at a glance they're in editor mode. Otherwise the UI is visually identical to production.

**Tier 1: Easy-switch slot drag (`EasySwitchPage.qml`)**
The existing slot-circle Repeater renders a small `Rectangle` per slot. In edit mode each circle gets:
- An invisible 24×24 hit-target wrapping the visible 9×9 circle (so it's grabbable)
- A `DragHandler { enabled: EditorModel.editing }`
- During drag, `x`/`y` bind to the drag handler's centroid converted back to percentages so the circle visually tracks the cursor
- On drag release, calls `EditorModel.updateSlotPosition(index, newXPct, newYPct)`

The conversion is the inverse of the existing rendering math: `xPct = (centroidX - imageContainer.imgX) / imageContainer.imgW`, clamped to `[0, 1]`. Note: the easyswitch x coordinate is already aspect-corrected at extraction time (the back-image rotation transform); the inverse must produce the *same* aspect-corrected value — this is tested explicitly.

**Tier 2: Button & point/scroll marker and card drag (`ButtonsPage.qml`, `PointScrollPage.qml`)**
Each hotspot has two draggable elements:
- *Marker drag* — updates `xPct`/`yPct` only. The connector line auto-tracks via existing rendering bindings.
- *Card drag* — updates `side` (snap-to-nearest-column based on which half of the screen the centroid is in) and `labelOffsetYPct` (free vertical drag).

When `editing` is on, both elements get a faint outline + cursor change to `Qt.SizeAllCursor` to signal they're draggable.

**Tier 2 text editing**
Double-click on slot labels, button card names, and the sidebar device-name header turns the text into an in-place `TextField`. Enter commits, Esc cancels. Each commit pushes a `TextEdit` `EditCommand` onto the undo stack.

**Tier 3: Image upload**
Each image area in EASY-SWITCH and BUTTONS pages becomes a `DropArea` accepting PNG file drops. A small "Replace image" button anchored top-right of the image opens `QFileDialog` as a fallback. Both paths funnel into `EditorModel.replaceImage(role, sourcePath)`.

The replace flow:
1. Compute destination path: same directory as the descriptor, filename derived from role (reuse existing image's filename if any, else generate `<device-id>-<role>.png`).
2. `QFile::copy()` the source to the destination (write to temp + atomic rename if destination exists).
3. Push `EditCommand{ImageReplace, role, beforeFilename, afterFilename}`.
4. Update in-memory descriptor's `assets.<role>Image` field to the new filename.
5. Do **not** save the JSON — user still has to hit Save. (The new PNG sits on disk unreferenced until then; harmless.)
6. On undo: revert the descriptor field. The PNG file stays on disk (orphaned, accepted cost).
7. On save: descriptor and PNG are both committed.

**Tier 4: Schema additions**
Two new optional fields, both backwards-compatible (no version bump):

1. **`label` on easy-switch slot objects** (string) — display name for the slot in the channel list at the bottom of EASY-SWITCH page. When absent, falls back to current behavior ("Available" / connection type).
2. **`displayName` on button hotspots** (string) — overrides `defaultName` for the card heading. When absent, `defaultName` (which the protocol/extractor sets) is used. This lets the editor fix wrong extractor names without overwriting the protocol-derived field.

Both are added to `JsonDevice`'s parsing in `parseEasySwitchSlots()` and `parseHotspots()`. Defaults (empty `QString`) preserve current rendering. Nothing else added to the schema in this scope.

### CLI plumbing

`main.cpp` adds an `--edit` `QCommandLineOption` alongside the existing `--simulate-all`. Both are orthogonal:

```cpp
const bool simulateAll = parser.isSet(simulateAllOption);
const bool editMode = parser.isSet(editOption);
controller.startMonitoring(simulateAll, editMode);
```

`AppController::startMonitoring(bool simulateAll, bool editMode)`:
- If `editMode`: instantiate `EditorModel`, hand it the `DeviceRegistry` reference, register as QML singleton via `qmlRegisterSingletonInstance`.
- Modes are independent: `--edit` alone (real device), `--simulate-all` alone (existing behavior), both (bulk curation), neither (production, unchanged).

## Data flow

### Drag → save round trip

```
User drags slot circle on EasySwitchPage.qml
        |
        v
DragHandler.onActiveChanged (release)
        |
        v
EditorModel.updateSlotPosition(idx, xPct, yPct)
        |
        |--> push EditCommand onto m_undoStacks[activeDevicePath]
        |--> mutate m_pendingEdits[activeDevicePath]
        |--> add path to m_dirty
        |--> emit dirtyChanged, undoStateChanged
        |--> ask DeviceModel to refresh-from-pending
        v
DeviceModel re-emits property change signals
        |
        v
QML bindings repaint (circle stays at new position)

[user clicks Save]
        |
        v
EditorModel::save()
        |
        |--> add activeDevicePath to m_selfWrittenPaths
        |--> DescriptorWriter::write(path, m_pendingEdits[path])
        |       atomic via QSaveFile
        |--> on success:
        |       clear m_pendingEdits[path], stacks, m_dirty
        |       call DeviceRegistry::reload(path)
        |       emit saved(path)
        |--> on failure:
        |       emit saveFailed(path, error)
        |       leave state intact
        v
DeviceRegistry::reload reads file, replaces cached JsonDevice,
emits deviceUpdated → DeviceModel re-reads → bindings repaint
        |
        v
QFileSystemWatcher fires for the path we just wrote
        |
        v
EditorModel sees path in m_selfWrittenPaths, removes it, suppresses prompt
```

### External file change while editing

```
User runs `git pull` on community-devices repo
        |
        v
QFileSystemWatcher fires fileChanged(path)
        |
        v
EditorModel::onFileChanged(path)
        |
        |--> if path in m_selfWrittenPaths: remove, return (own write)
        |--> compare disk mtime to JsonDevice::loadedMtime
        |--> if path in m_dirty:
        |       emit externalChangeDetected(path)
        |       (banner appears, user picks: keep/load/diff)
        |--> else:
        |       silently call DeviceRegistry::reload(path)
        |       (DeviceModel repaints)
```

## Schema additions

Both fields are optional; existing descriptors keep working unchanged. No version bump.

### `label` on easy-switch slot

```json
{
  "easySwitchSlotPositions": [
    { "xPct": 0.42, "yPct": 0.78, "label": "Mac" },
    { "xPct": 0.50, "yPct": 0.78, "label": "Linux" },
    { "xPct": 0.58, "yPct": 0.78 }
  ]
}
```

### `displayName` on button hotspot

```json
{
  "hotspots": [
    {
      "buttonIndex": 0,
      "controlId": 80,
      "defaultName": "Left Click",
      "defaultActionType": "click",
      "displayName": "Primary Button",
      "xPct": 0.30,
      "yPct": 0.20,
      "side": "left",
      "labelOffsetYPct": 0.05
    }
  ]
}
```

## Error handling

- **Save failure** (disk full, permission denied, atomic-rename failure): `EditorModel` emits `saveFailed(path, errorString)`; the toolbar surfaces a non-blocking error message; the in-memory pending state is preserved so the user can retry after fixing the cause.
- **Image copy failure**: same pattern — error surfaced, no descriptor mutation, no `EditCommand` pushed.
- **Reset on dirty descriptor**: confirm dialog ("Discard unsaved changes?") before reverting.
- **External file change during pending edit**: non-blocking banner with three options:
  - **Keep my edits** — leave in-memory state, dismiss banner. Next save will overwrite the disk version (and trigger another self-write watcher event, which is correctly suppressed).
  - **Load disk version** — discard pending edits, clear undo stack for this path, re-read file.
  - **View diff** — modal showing on-disk JSON vs in-memory JSON, line-diffed (Myers diff via small helper or `diff` shellout).
- **Editing a non-`JsonDevice`**: `EditorModel` silently disables editing for that device; toolbar shows "Read-only" instead of Save/Reset.
- **Symlink target replaced**: re-stat at every reload; if canonical path changed, re-add the watch.

## Testing

### Unit tests (gtest, C++)

- **`tst_DescriptorWriter`** — round-trip byte-identical; preserve unknown fields (`__future_field`); atomic write under failure; indent matches community convention.
- **`tst_EditorModel`** — drag updates pending state; undo restores prior; per-device stack isolation; save clears stack and dirty; save failure leaves state intact; reset with dirty; file watcher silent reload; file watcher conflict prompt; self-write suppression.
- **`tst_JsonDeviceSourcePath`** — `sourcePath()` returns canonicalized path; new optional fields parse correctly when present and default to empty when absent.
- **`tst_DeviceRegistry`** — `loadFromFile(path)` for single descriptor; `reload(path)` replaces cache and emits `deviceUpdated`.

### QML tests (qtest)

- **`tst_EditorToolbar`** — visibility gated on `editing`; save button enabled state tracks `hasUnsavedChanges`; undo/redo state tracks `canUndo`/`canRedo`; Save click invokes `EditorModel.save()` (SignalSpy).
- **`tst_EasySwitchPageEdit`** — no drag handlers when `editing` is false; drag handlers active when true; drag math test asserts the inverse-of-rendering produces correct `xPct`/`yPct` (including the aspect-correction case).
- **`tst_ButtonsPageEdit`** — marker drag updates xPct/yPct only; card drag updates side and labelOffsetYPct.
- **`tst_SideNav`** (extend existing) — amber stripe visible when `editing` is true.

### Integration smoke test

A checklist run before marking the PR ready (not automated CI):
1. Drag an easy-switch slot, save, restart, assert position persisted.
2. Drag a button card, undo, assert reverted.
3. Drop a PNG onto the back image area, save, restart, assert new image renders.
4. Externally `sed` a descriptor while editor is open, assert conflict banner appears with all three options working.

### Test fixtures

`tests/fixtures/editor/` — copy of `mx-master-3s.json` for mutation tests (so production descriptor isn't touched), plus a synthetic minimal descriptor with `__future_field` for round-trip preservation.

## Risks

1. **`QFileSystemWatcher` on symlinks.** The user keeps `/tmp/sim-live/logitune/devices` as a symlink; `QFileSystemWatcher` watches the resolved target on Linux. If the symlink is recreated pointing elsewhere, watching breaks silently. Mitigation: re-stat at every reload; if the canonical path changed, re-add the watch.

2. **Self-write loop.** Saving fires the watcher for the file we just wrote. Mitigated by `m_selfWrittenPaths`. The suppression must clear on the *first* watcher callback for that path after save, not on a timer, to avoid losing genuine external edits in a same-second window.

3. **`DeviceRegistry` ownership model.** If existing code holds raw `JsonDevice*` pointers across a reload, replacing the cache entry will dangle them. Need to verify the ownership model in `DeviceRegistry` before implementing. Possibly the reload mutates the existing `JsonDevice` object in place rather than replacing it. Flagged for the implementation plan to resolve in the foundation step.

4. **Drag math sign errors on aspect-corrected easyswitch coordinates.** The easyswitch `xPct` is already aspect-multiplied at extraction time (the back-image rotation transform: `x_out = (marker_x / 100) × (back_height / back_width)`). The drag inverse must produce the *same* aspect-multiplied value, not the raw rotated coordinate. Tested explicitly with the m720-triathlon "buttons on top" case.

5. **Schema additions colliding with extractor goldens.** `label` and `displayName` shouldn't appear in extractor output (extractor PR is separate). Verify the extractor's golden fixtures don't accidentally start emitting empty values.

## Build sequence

The writing-plans skill will turn this into TDD tasks. Rough order:

1. **Foundation** — `JsonDevice::sourcePath()`, `DeviceRegistry::loadFromFile`/`reload`, schema additions parsed by `JsonDevice`. No UI yet, fully tested.
2. **`DescriptorWriter`** — atomic round-trip writer with preserve-unknown-fields. Fully tested in isolation.
3. **`EditorModel`** (no QML yet) — state, undo stacks, dirty tracking, save flow, file watcher integration. Fully tested via gtest.
4. **CLI `--edit` flag + AppController plumbing** — instantiates `EditorModel` only when flag set, registers QML singleton.
5. **`EditorToolbar.qml`** — bar, indicator, save/undo/redo/reset buttons. Smoke test renders.
6. **Tier 1 — easy-switch slot drag** — drag handlers, drag math + inverse, QML test for the math. End-to-end first slice; fixes the original use case.
7. **Tier 2 — button & point/scroll markers, cards, text editing** — marker drag, card drag with side-snap, double-click text editing.
8. **Tier 3 — image upload** — DropArea + file picker, atomic copy into descriptor directory, undo support.
9. **Conflict banner UI** — for `externalChangeDetected`, with diff modal.
10. **Smoke-test pass** against a copy of the community-devices repo; document the manual checklist in the PR.

By step 6 the user can already fix easy-switch positions in-app — exactly the workflow that triggered this brainstorm.

## Open questions punted to implementation

- Exact `DeviceRegistry` ownership model (reload-in-place vs. replace) — verified during step 1.
- Whether `EditCommand` needs a per-command label for "Undo move slot 2" tooltips — defer until UX feels lacking.
- Whether to store editor toolbar position as a `QSettings` preference — defer; fixed at the top is fine.
