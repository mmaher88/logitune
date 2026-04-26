# Documentation Update Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring README, wiki, and supporting docs into alignment with PRs #37 (editor mode), #38 (carousel UX), #40 (verified/beta), and #42 (startup hardening).

**Architecture:** Nine commits on branch `docs-sync-after-editor-mode`, each atomic and internally consistent. Single source of truth for verified/beta (Getting-Started.md). All pages cross-link instead of duplicating. Screenshot capture is split: automated stills during implementation, user-supplied interactive states in the final commit.

**Tech Stack:** Markdown (GitHub Flavored), mermaid diagrams in Architecture.md, images in `docs/images/`, screenshots captured with `gnome-screenshot` / `spectacle`, markdown link validation by grep.

**Design spec:** `docs/superpowers/specs/2026-04-16-docs-update-design.md`. Read it before starting Task 1.

---

## Global rules for every task

**No em-dashes (U+2014 "—") in any file you create or modify.** Replace with colon, comma, period, or parentheses as appropriate. Pre-existing em-dashes in files you are NOT modifying may stay; do not do a sweep. When rewriting a file, remove the em-dashes in the rewritten parts.

**Verify each commit:**

1. `git diff --check` to catch whitespace errors
2. `grep -c "—" <file>` → expect 0 for new files; for modifications, count must not increase vs base
3. For wiki pages, grep for stale code refs: `grep -En 'IDevice|MxMaster3sDescriptor|class.*Descriptor' <file>` → expect 0 hits after rewrites
4. `git commit` (never `git commit --amend` unless the previous commit was fully local and the user asked for it)

**Every code example lifted from the repo must match the repo.** When you quote a JSON snippet, open `devices/mx-master-3s/descriptor.json` and copy verbatim. Paraphrased schemas rot.

---

## File Structure

### Create

- `docs/wiki/Editor-Mode.md` (new wiki page)
- `docs/images/editor-toolbar.png` (automated screenshot)
- `docs/images/editor-buttons-page.png` (automated screenshot)
- `docs/images/editor-point-scroll-page.png` (automated screenshot)
- `docs/images/editor-easy-switch-page.png` (automated screenshot)
- `docs/images/editor-image-drop-zone.png` (automated screenshot)
- `docs/images/editor-card-drag.png` (user-supplied)
- `docs/images/editor-rename.png` (user-supplied)
- `docs/images/editor-conflict-banner.png` (user-supplied)
- `docs/images/editor-diff-modal.png` (user-supplied)
- `docs/images/editor-unsaved-indicator.png` (user-supplied)

### Modify

- `docs/wiki/Adding-a-Device.md` (full rewrite)
- `docs/wiki/Architecture.md` (sections touching device descriptors)
- `docs/wiki/Testing.md` (JSON fixtures + crash dialog note)
- `docs/wiki/Contributing.md` (device-adding section)
- `docs/wiki/Home.md` (index)
- `docs/wiki/Getting-Started.md` (add Device Support Status section)
- `README.md` (features list, supported devices table, wiki links)

### Unchanged

- `docs/wiki/Building.md`
- `docs/wiki/Adding-a-Desktop-Environment.md`
- `docs/wiki/HID++-Protocol.md` (no descriptor code examples present; confirm in Task 4 sub-step)

---

## Task 1: Rewrite Adding-a-Device for JSON workflow

**Files:**

- Rewrite: `docs/wiki/Adding-a-Device.md`

Section outline follows spec §"Page Outline: Adding-a-Device.md (rewrite)". Use the MX Master 3S descriptor as the worked example (not a hypothetical "MX Anywhere 3S" as the old page did). The JSON schema table must match the fields actually parsed by `src/core/devices/JsonDevice.cpp`.

- [ ] **Step 1: Read source-of-truth files before writing**

Run:

```bash
cat devices/mx-master-3s/descriptor.json | head -80
grep -E "parseStatus|fromJson|fromObject" src/core/devices/JsonDevice.cpp | head -30
```

Confirm field names and status values (`verified` / `beta`) before you start writing the schema table.

- [ ] **Step 2: Rewrite the page top-to-bottom**

Replace the entire file with content following the spec outline. Required sections in this order:

1. Intro paragraph: JSON-first workflow, editor mode polishes the visual layout, C++ only needed for new HID++ feature variants.
2. `## Prerequisites` (PIDs via lsusb/Solaar, CIDs via `--debug` log, features list, DPI range, device images).
3. `## Step 1: Create the descriptor folder`: bash commands for `mkdir devices/<slug>` and placing placeholder PNGs.
4. `## Step 2: Fill in the bootstrap JSON by hand`: full schema table with every field from `JsonDevice::fromJson`, types, whether required.
5. `## Step 3: Register with DeviceRegistry`: two paths: built-in (`devices/` in repo + CMake install) or user-local (`$XDG_DATA_DIRS/logitune/devices/`). Include the exact directory that `DeviceRegistry::loadFromXdg()` scans.
6. `## Step 4: Polish with Editor Mode`: short paragraph explaining the visual pass, link to `Editor-Mode.md` for details. Do NOT duplicate the editor walk-through here.
7. `## Step 5: Test`: `logitune --simulate-all` for eyeballing without hardware; smoke-test checklist; running `logitune-tests` against the new descriptor fixture.
8. `## Step 6: Submit a PR`: set `"status": "beta"` unless you've hardware-verified; link to Contributing.md for commit format.
9. `## When you need C++`: callout: new HID++ feature variant means adding a capability-table entry in `src/core/hidpp/capabilities/`; link to `Architecture.md#device-registry`.
10. `## Reference: MX Master 3S descriptor`: the full annotated JSON, pulled verbatim from `devices/mx-master-3s/descriptor.json`.

The schema table format (use this exact header, fill rows by reading `JsonDevice::fromJson`):

```markdown
| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `name` | string | yes | Display name, e.g. `"MX Master 3S"` |
| `status` | string | yes | `"verified"` or `"beta"`: see [Device Support Status](Getting-Started#device-support-status) |
| `productIds` | array of string | yes | Hex PIDs as strings, e.g. `["0xb034"]` |
| ... | ... | ... | ... |
```

Run this to confirm field names and catch any you missed:

```bash
grep -E 'contains\("|value\(QStringLiteral' src/core/devices/JsonDevice.cpp | head -40
```

- [ ] **Step 3: Verify the page**

```bash
grep -c "—" docs/wiki/Adding-a-Device.md
grep -En 'IDevice|MxMaster3sDescriptor|class.*Descriptor' docs/wiki/Adding-a-Device.md
```

Both must output `0` (the second grep prints nothing on match-free).

- [ ] **Step 4: Commit**

```bash
git add docs/wiki/Adding-a-Device.md
git commit -m "docs: rewrite Adding-a-Device for JSON workflow

Replace the old C++ IDevice subclass walk-through with the current
JSON descriptor workflow. Sections:

- Bootstrap JSON by hand (fields the editor cannot help with)
- Register with DeviceRegistry (built-in vs XDG override)
- Polish with Editor Mode (link, not duplicate)
- When you need C++ (new HID++ feature variants only)

Uses MX Master 3S as the worked example. Schema table derived from
JsonDevice::fromJson field parsing.

Refs: PR #37"
```

---

## Task 2: Add Editor-Mode wiki page + automated screenshots

**Files:**

- Create: `docs/wiki/Editor-Mode.md`
- Create: `docs/images/editor-toolbar.png`
- Create: `docs/images/editor-buttons-page.png`
- Create: `docs/images/editor-point-scroll-page.png`
- Create: `docs/images/editor-easy-switch-page.png`
- Create: `docs/images/editor-image-drop-zone.png`

- [ ] **Step 1: Launch the app in editor + simulate mode**

```bash
pkill -f "^logitune" 2>/dev/null
nohup logitune --edit --simulate-all > /tmp/logitune.log 2>&1 & disown
sleep 3
pgrep -a logitune
```

Confirm the process is running.

- [ ] **Step 2: Capture the editor toolbar**

Drive navigation and capture. MX Master 3S is device index 1 in the simulated carousel. Navigate using `xdotool` (mouse click on the device card) or manually by clicking. Then capture the full window:

```bash
WID=$(xdotool search --name "^Logitune$" | head -1)
xdotool windowactivate "$WID" && sleep 0.2
gnome-screenshot -w -f docs/images/editor-toolbar.png
```

Editor toolbar is visible on any device page (mainStack.depth > 1). Crop later in Step 7.

- [ ] **Step 3: Capture each editor page**

For Buttons page: click the selected device card (advances `mainStack`). BUTTONS is the default sidebar tab. Screenshot:

```bash
gnome-screenshot -w -f docs/images/editor-buttons-page.png
```

Click "POINT & SCROLL" tab, screenshot:

```bash
sleep 0.5
gnome-screenshot -w -f docs/images/editor-point-scroll-page.png
```

Click "EASY-SWITCH" tab, screenshot:

```bash
sleep 0.5
gnome-screenshot -w -f docs/images/editor-easy-switch-page.png
```

- [ ] **Step 4: Capture the empty image drop zone**

Still on Easy-Switch (it has the drop-zone region most clearly). If the device already has an image, the drop-zone state needs the "replace image" button hovered. Hover the image area by moving the cursor over it with xdotool, then capture:

```bash
# Hover over the image area (adjust coords to image centre of your window)
xdotool mousemove --window "$WID" 800 500
sleep 0.5
gnome-screenshot -w -f docs/images/editor-image-drop-zone.png
```

If the resulting capture does not show the drop-zone state clearly, note the issue and flag that frame as user-supplied in Task 9 instead.

- [ ] **Step 5: Quit the app cleanly**

```bash
pkill -f "^logitune" 2>/dev/null
```

- [ ] **Step 6: Crop / resize screenshots if needed**

If `gnome-screenshot -w` produced full-window shots with chrome you don't want, crop with ImageMagick. Target width ~1200px for wiki embedding:

```bash
for f in docs/images/editor-*.png; do
  dims=$(identify -format "%wx%h" "$f")
  echo "$f: $dims"
done
# If any is wider than 1400 px, resize:
# mogrify -resize 1200x docs/images/<file>.png
```

- [ ] **Step 7: Write Editor-Mode.md**

Per spec §"Page Outline: Editor-Mode.md (new)". Required sections:

1. `## What it is`: visual editor for existing descriptors; prerequisite is an existing `descriptor.json`; does not create descriptors from scratch.
2. `## When to use it`: positioning hotspots, slot circles, uploading real images, polishing labels.
3. `## Launching`: commands:

    ```bash
    # Against your connected hardware:
    logitune --edit

    # Without hardware (iterate on every bundled descriptor):
    logitune --edit --simulate-all
    ```

4. `## The editor toolbar`: insert `docs/images/editor-toolbar.png`, describe Save / Reset / Undo / Redo buttons, path indicator on the right, amber sidebar stripe indicator.
5. `## Buttons page`: insert `docs/images/editor-buttons-page.png`, describe marker drag, card drag (free-flow with column snap on release), double-click label rename.
6. `## Point & Scroll page`: insert `docs/images/editor-point-scroll-page.png`, describe marker drag for scroll/thumb/pointer hotspots.
7. `## Easy-Switch page`: insert `docs/images/editor-easy-switch-page.png`, describe slot circle drag.
8. `## Uploading device images`: insert `docs/images/editor-image-drop-zone.png`, describe drag-drop and file-picker paths, note the image is copied into `devices/<slug>/` and tracked in git.
9. `## Renaming labels`: placeholder reference to `docs/images/editor-rename.png` (user-supplied in Task 9).
10. `## Resolving file conflicts`: placeholder reference to `docs/images/editor-conflict-banner.png` (user-supplied).
11. `## Viewing the diff`: placeholder reference to `docs/images/editor-diff-modal.png` (user-supplied).
12. `## Save, reset, and unsaved changes`: placeholder reference to `docs/images/editor-unsaved-indicator.png` (user-supplied), describe atomic write (`DescriptorWriter`), preserve-unknown-fields guarantee, self-write suppression.
13. `## Contribution workflow`: fork, `logitune --edit`, make changes, save, `git diff` to review, submit PR (link to `Adding-a-Device.md#submitting-a-pr`).
14. `## Limitations`: no "create new descriptor" tooling; schema fields (CIDs, features, DPI range) must be hand-edited in `descriptor.json`.
15. `## See also`: `Adding-a-Device.md`, `Getting-Started.md#device-support-status`.

Every image reference: use wiki-relative paths like `![Editor toolbar](../images/editor-toolbar.png)` if that's how other wiki pages embed images; check with `grep -E '!\[.*\]\(' docs/wiki/*.md | head` first and match the pattern.

- [ ] **Step 8: Verify the page**

```bash
grep -c "—" docs/wiki/Editor-Mode.md
# Confirm every image file referenced exists:
grep -oE '!\[[^]]*\]\([^)]+\)' docs/wiki/Editor-Mode.md \
  | sed 's/.*(\(.*\))/\1/' \
  | while read p; do
      [ -f "docs/wiki/$p" ] || [ -f "docs/$p" ] || [ -f "$p" ] || echo "MISSING: $p"
    done
```

First grep must print `0`. Second must print no `MISSING` lines for the five automated images; user-supplied placeholders will be added in Task 9.

- [ ] **Step 9: Commit**

```bash
git add docs/wiki/Editor-Mode.md docs/images/editor-toolbar.png \
        docs/images/editor-buttons-page.png \
        docs/images/editor-point-scroll-page.png \
        docs/images/editor-easy-switch-page.png \
        docs/images/editor-image-drop-zone.png
git commit -m "docs: add Editor-Mode wiki page

Covers the --edit workflow end-to-end: launching, toolbar, drag
handles on each page, image upload, save/reset semantics, and the
fork-edit-PR contribution flow. Automated stills included; interactive
screenshots (drag, rename, conflict, diff) follow in a later commit.

Refs: PR #37"
```

---

## Task 3: Add Device Support Status section to Getting-Started

**Files:**

- Modify: `docs/wiki/Getting-Started.md`

- [ ] **Step 1: Find the right insertion point**

```bash
grep -n "^## " docs/wiki/Getting-Started.md
```

Identify where "Supported Devices" or similar sits. Insert a new subsection `## Device support status` immediately after it, or if no existing devices section, add after the installation section.

- [ ] **Step 2: Write the section**

Exact content to insert:

```markdown
## Device support status

Every device descriptor carries a `status` field with one of two values:

- **`verified`** (green check badge): the maintainer or a trusted
  contributor owns the device and has confirmed it works end-to-end
  on real hardware. All ship-bundled descriptors currently ship as
  verified.
- **`beta`** (amber badge): community-submitted or shipped without
  hardware verification. Core HID++ functionality almost always
  works, but hotspot positions, slot-circle positions, and device
  images may need polish. Report issues or open a PR via the
  [editor mode](Editor-Mode) workflow.

When you contribute a new descriptor, set `"status": "beta"` in
`descriptor.json` unless you have hardware-verified it and a
maintainer agrees to promote it to `verified`.
```

- [ ] **Step 3: Verify**

```bash
grep -A 14 "## Device support status" docs/wiki/Getting-Started.md | head -20
# The section should contain no em-dashes you added:
grep -c "—" docs/wiki/Getting-Started.md
# Record the count. If it equals the pre-edit count (19 per spec), you introduced none.
```

- [ ] **Step 4: Commit**

```bash
git add docs/wiki/Getting-Started.md
git commit -m "docs: explain verified/beta device support status

Adds a single source of truth for the badge meanings. README and other
wiki pages link here rather than duplicating the explanation.

Refs: PR #40"
```

---

## Task 4: Update Architecture for JSON descriptors + editor components

**Files:**

- Modify: `docs/wiki/Architecture.md`

- [ ] **Step 1: Find stale references**

```bash
grep -n "IDevice\|MxMaster3sDescriptor\|class.*Descriptor\|C++ descriptor" docs/wiki/Architecture.md
```

Note every hit.

- [ ] **Step 2: Replace the Device Registry section**

Find the existing "Device Registry" or equivalent section. Replace its content with:

```markdown
## Device Registry

`DeviceRegistry` loads device descriptors at startup from three sources
(highest precedence first):

1. `$XDG_DATA_DIRS/logitune/devices/<slug>/descriptor.json` (user
   overrides, useful when iterating on a community descriptor)
2. `/usr/share/logitune/devices/<slug>/descriptor.json` (installed
   descriptors, shipped with the package)
3. Built-in fallback compiled from `devices/` in the source tree

Each descriptor is wrapped in a `JsonDevice` instance that exposes the
`IDevice` interface consumed by the rest of the app. `JsonDevice` is
the only concrete `IDevice` subclass; there are no per-device C++
classes. A new device is a `descriptor.json` file plus three images.

For the contributor-facing workflow, see
[Adding a Device](Adding-a-Device). For the visual-editing tool, see
[Editor Mode](Editor-Mode).
```

- [ ] **Step 3: Add editor-mode component references**

Find the component/module section that lists the major classes. Add entries for:

- `JsonDevice` (`src/core/devices/JsonDevice.{h,cpp}`): parses `descriptor.json` and adapts to `IDevice`.
- `DescriptorWriter` (`src/core/devices/DescriptorWriter.{h,cpp}`): atomic writes preserving unknown fields.
- `EditorModel` (`src/app/models/EditorModel.{h,cpp}`): `--edit` mode state, undo/redo command stack, file-conflict detection.

If a mermaid component diagram exists, add nodes for these three classes with edges:
`EditorModel --> DescriptorWriter --> JsonDevice --> DeviceRegistry`.

Run:

```bash
grep -c "mermaid" docs/wiki/Architecture.md
```

If `0`, skip the diagram step.

- [ ] **Step 4: Scrub remaining stale refs**

Re-run the grep from Step 1. Every hit either:

- Is inside a historical changelog paragraph you chose to leave (annotate with a note saying "(pre-#37 pattern)"), OR
- Needs to be rewritten to describe the JSON workflow.

- [ ] **Step 5: Verify**

```bash
grep -En "IDevice|MxMaster3sDescriptor|class.*Descriptor" docs/wiki/Architecture.md
```

Only acceptable matches are inside the IDevice paragraph describing "JsonDevice is the only concrete IDevice subclass". Every other historical mention should have been rewritten or annotated.

- [ ] **Step 6: Check HID++-Protocol.md for inline descriptor code**

```bash
grep -En "class.*Descriptor|MxMaster3sDescriptor|IDevice\s*\*" docs/wiki/HID++-Protocol.md
```

If hits exist, fix them in this same commit (the spec says that page may need minor updates). If no hits, skip.

- [ ] **Step 7: Commit**

```bash
git add docs/wiki/Architecture.md
# Add HID++-Protocol.md only if Step 6 made changes:
git status docs/wiki/HID++-Protocol.md
# If modified:
# git add docs/wiki/HID++-Protocol.md

git commit -m "docs: update Architecture for JSON descriptors + editor

Device Registry section now describes JsonDevice as the only concrete
IDevice subclass and documents the three-tier load order (XDG,
/usr/share, built-in). Adds DescriptorWriter and EditorModel to the
component list.

Refs: PR #37"
```

---

## Task 5: Update Testing for JSON fixtures + crash dialog behavior

**Files:**

- Modify: `docs/wiki/Testing.md`

- [ ] **Step 1: Find descriptor-test references**

```bash
grep -nE "MockDevice|setupMx|setupMockMx|DescriptorRegistry.*register|registerDevice" docs/wiki/Testing.md
```

Note each hit. These will be rewritten to describe JSON fixture loading via `DeviceRegistry::findBySourcePath`.

- [ ] **Step 2: Replace the add-a-test section**

Find the section that walks through adding a device test. Replace with:

```markdown
### Adding a device test

Device tests load a JSON fixture through `DeviceRegistry`; there is no
per-device mock class to extend.

1. Drop a fixture at `tests/fixtures/<slug>/descriptor.json` (plus
   placeholder images if the test exercises image paths).
2. In your test, load the fixture:

    ```cpp
    DeviceRegistry registry;
    registry.loadFromDirectory(fixturePath);
    const IDevice *dev = registry.findBySourcePath(fixturePath + "/<slug>");
    ASSERT_NE(dev, nullptr);
    ```

3. Use the parameterized `DeviceSpec` pattern in
   `tests/test_device_registry.cpp` for smoke-testing every bundled
   descriptor. Add your device to the `kDevices` array with its
   expected field values.
```

- [ ] **Step 3: Add crash-dialog behavior note**

Find the section covering logging, crash handling, or stability. Add:

```markdown
### Crash dialog behavior

Catchable crashes (SIGSEGV, SIGABRT, SIGFPE, SIGBUS, and uncaught C++
exceptions) show the Crash Report dialog at the moment they happen,
via `CrashHandler` installed during startup. The app does not show a
recovery dialog on the next launch: uncatchable exits (SIGKILL, OOM,
power loss, reboot) leave a lock file behind, but the user already
knows those happened, so the lock is silently cleaned up.

When testing crash paths, expect the dialog to appear in the same
session; do not test for a recovery-on-startup dialog.
```

- [ ] **Step 4: Verify**

```bash
grep -c "—" docs/wiki/Testing.md
grep -En "MockDevice|setupMx" docs/wiki/Testing.md
```

The first should match the pre-edit count (14); the second should have zero hits from your new content (legacy hits elsewhere are OK if they describe unrelated mock machinery).

- [ ] **Step 5: Commit**

```bash
git add docs/wiki/Testing.md
git commit -m "docs: update Testing for JSON fixtures + crash dialog

Describe the JSON-fixture pattern with DeviceRegistry::findBySourcePath
instead of mock-device helper methods. Add a note that crash dialogs
fire at crash time, with no on-launch recovery prompt (post-#42).

Refs: PRs #37 and #42"
```

---

## Task 6: Update Contributing to point at Editor Mode

**Files:**

- Modify: `docs/wiki/Contributing.md`

- [ ] **Step 1: Find the device-adding guidance**

```bash
grep -n "device\|descriptor" docs/wiki/Contributing.md | head
```

Identify the section (if any) that mentions adding devices.

- [ ] **Step 2: Rewrite or add an "Adding a device" subsection**

Content:

```markdown
## Adding a device

The preferred path for new device descriptors is the in-app editor:

1. Fork the repo and bootstrap a `descriptor.json` for your device
   (see [Adding a Device](Adding-a-Device) for the JSON schema).
2. Run `logitune --edit` (pair with `--simulate-all` if you do not
   own the hardware) and use the editor to position hotspots, drop in
   device images, and polish labels.
3. Save. `git diff devices/<slug>/` should show the in-memory changes
   you made.
4. Submit a PR with `"status": "beta"` unless you have
   hardware-verified the descriptor.

For the full walkthrough, see [Editor Mode](Editor-Mode) and
[Adding a Device](Adding-a-Device).
```

- [ ] **Step 3: Verify and commit**

```bash
grep -c "—" docs/wiki/Contributing.md
```

Confirm no increase. Then:

```bash
git add docs/wiki/Contributing.md
git commit -m "docs: point Contributing at Editor Mode as preferred path

Short 'Adding a device' section now directs contributors to the
fork -> --edit -> save -> PR workflow and links to the full guides.

Refs: PR #37"
```

---

## Task 7: Update Home.md wiki index

**Files:**

- Modify: `docs/wiki/Home.md`

- [ ] **Step 1: Inspect current index**

```bash
cat docs/wiki/Home.md
```

- [ ] **Step 2: Add Editor Mode entry**

Insert an `Editor Mode` entry immediately below `Adding a Device`. Use the same pattern / emoji style the page already uses for other entries. Example if entries are bullet-style:

```markdown
- [Editor Mode](Editor-Mode): visual editor for existing device descriptors; positions hotspots, slot circles, image uploads.
```

If entries use a table, match the table. Never use em-dashes in your new line; use a colon instead.

- [ ] **Step 3: Update the Adding a Device blurb**

Rewrite the existing Adding-a-Device blurb to say "JSON descriptor workflow" instead of "C++ IDevice subclass" (if the current blurb mentions classes at all). Keep the tone.

- [ ] **Step 4: Verify and commit**

```bash
grep -c "—" docs/wiki/Home.md
# Ensure the new entries link to real pages:
grep -oE '\(([A-Za-z-]+)\)' docs/wiki/Home.md \
  | sed 's/[()]//g' \
  | while read p; do
      [ -f "docs/wiki/$p.md" ] || echo "MISSING wiki page: $p"
    done
```

Expect no `MISSING` lines.

```bash
git add docs/wiki/Home.md
git commit -m "docs: add Editor Mode to wiki index

Cross-references the new Editor Mode page and refreshes the
Adding a Device blurb to match the JSON workflow.

Refs: PR #37"
```

---

## Task 8: Update README with editor mode, verified/beta, wiki links

**Files:**

- Modify: `README.md`

- [ ] **Step 1: Update the Features list**

Open `README.md`. Find the `## ✨ Features` section. Add one bullet (match existing style including emoji):

```markdown
- 🛠️ **In-app descriptor editor**: launch with `--edit` to position hotspots, upload images, and tune labels without hand-editing JSON. [Learn more](https://github.com/mmaher88/logitune/wiki/Editor-Mode)
```

- [ ] **Step 2: Update the Supported Devices table**

Find the table. Replace with:

```markdown
| Device | Status | Connection |
|--------|--------|------------|
| MX Master 3S | ✅ Verified | Bolt / Bluetooth |
| MX Master 2S | ✅ Verified | Bolt / Bluetooth / Unifying |
| MX Master 4 | ✅ Verified | Bolt / Bluetooth |
| Other Logitech HID++ 2.0 | 🔧 Add via [device descriptor](https://github.com/mmaher88/logitune/wiki/Adding-a-Device) |: |
```

Verify status values against the descriptors before committing:

```bash
for f in devices/*/descriptor.json; do
  echo "$(dirname $f | xargs basename): $(jq -r .status $f)"
done
```

If any descriptor reports `"beta"` instead of `"verified"`, update the table row's badge to `🧪 Beta` and cross-reference the Getting-Started anchor. The design spec assumed all three ship as verified; verify before writing.

- [ ] **Step 3: Add the Device Support Status link**

Below the table, add a one-line note:

```markdown
See [Device Support Status](https://github.com/mmaher88/logitune/wiki/Getting-Started#device-support-status) for what the badges mean.
```

- [ ] **Step 4: Verify**

```bash
# Confirm no new em-dashes (pre-edit count is 12):
grep -c "—" README.md
# Confirm wiki links are all to existing pages:
grep -oE 'wiki/[A-Za-z#-]+' README.md | sort -u
```

Cross-check each wiki link has a corresponding `docs/wiki/<Page>.md` file.

- [ ] **Step 5: Commit**

```bash
git add README.md
git commit -m "docs: README mentions editor mode, verified/beta, MX Master lineup

- Features list includes the in-app descriptor editor
- Supported Devices lists MX Master 2S, 3S, and 4 with verified badges
- Links to Device Support Status for badge semantics
- Links to the new Editor Mode wiki page

Refs: PRs #37 and #40"
```

---

## Task 9: Add interactive-state screenshots (user-supplied)

**Files:**

- Create: `docs/images/editor-card-drag.png`
- Create: `docs/images/editor-rename.png`
- Create: `docs/images/editor-conflict-banner.png`
- Create: `docs/images/editor-diff-modal.png`
- Create: `docs/images/editor-unsaved-indicator.png`

This task depends on user-supplied captures; do not start until the user provides them.

- [ ] **Step 1: Request the captures from the user**

Ask the user to capture these five states in `logitune --edit --simulate-all` and save to the listed paths (or provide files and you will move them):

1. `editor-card-drag.png`: a button card mid-drag on MX Master 3S Buttons page, with the connector line visibly redrawing.
2. `editor-rename.png`: double-click a label to enter text-editing mode; capture with the field active and the caret visible.
3. `editor-conflict-banner.png`: touch `devices/mx-master-3s/descriptor.json` while the editor has unsaved changes (`touch devices/mx-master-3s/descriptor.json` in another terminal), so the conflict banner appears.
4. `editor-diff-modal.png`: make any change, click the Diff button, capture the modal with real diff content.
5. `editor-unsaved-indicator.png`: after any change, before save; capture the toolbar showing the unsaved-changes indicator.

- [ ] **Step 2: Confirm files are in place**

```bash
for f in editor-card-drag editor-rename editor-conflict-banner editor-diff-modal editor-unsaved-indicator; do
  [ -f "docs/images/$f.png" ] && echo "OK  $f" || echo "MISSING $f"
done
```

All five must print `OK`.

- [ ] **Step 3: Update Editor-Mode.md to reference the new images**

The placeholders added in Task 2 Steps 9-12 point at these files; confirm the `![alt](../images/...)` paths match the filenames now present and that every image renders when you preview the markdown.

- [ ] **Step 4: Verify**

```bash
grep -oE '!\[[^]]*\]\([^)]+\)' docs/wiki/Editor-Mode.md \
  | sed 's/.*(\(.*\))/\1/' \
  | while read p; do
      [ -f "docs/wiki/$p" ] || [ -f "docs/$p" ] || [ -f "$p" ] || echo "MISSING: $p"
    done
```

Expect zero `MISSING` lines.

- [ ] **Step 5: Commit**

```bash
git add docs/images/editor-card-drag.png \
        docs/images/editor-rename.png \
        docs/images/editor-conflict-banner.png \
        docs/images/editor-diff-modal.png \
        docs/images/editor-unsaved-indicator.png
# If Editor-Mode.md was touched in Step 3:
git add docs/wiki/Editor-Mode.md
git commit -m "docs(editor-mode): add interactive-state screenshots

Supplies the card-drag, rename, conflict-banner, diff-modal, and
unsaved-indicator captures that round out the Editor Mode guide.

Refs: PR #37"
```

---

## After all tasks

- [ ] **Final step: Push the branch and open the PR**

```bash
git push -u origin docs-sync-after-editor-mode
```

Draft the PR body in chat for user review before posting (per the
`feedback_draft_comments.md` rule). PR title:

```
docs: post-editor-mode documentation sync
```

The PR closes no specific issue. It references PRs #37, #40, and #42.

---

## Self-Review

Against `docs/superpowers/specs/2026-04-16-docs-update-design.md`:

| Spec requirement | Implemented in |
|------------------|----------------|
| README updates | Task 8 |
| Home.md index | Task 7 |
| Adding-a-Device rewrite | Task 1 |
| Architecture JSON refs | Task 4 |
| Contributing points at Editor Mode | Task 6 |
| Testing JSON fixtures + #42 crash note | Task 5 |
| Getting-Started verified/beta | Task 3 |
| HID++-Protocol scrub | Task 4 Step 6 |
| Editor-Mode.md new page | Task 2 |
| Linking structure (README → wiki, etc.) | Tasks 7, 8 |
| Automated screenshots | Task 2 |
| User-supplied screenshots | Task 9 |
| Status policy single source of truth | Task 3 (source) + Task 7, 8 (links) |
| Commit sequence matches spec §Commit Sequence | All tasks are atomic commits in the listed order |

No placeholders scanned: every step contains either the exact content to add, the exact command to run, or a specific section of the spec to expand. No TBD / TODO / "fill in" markers.

Type consistency: `verified` / `beta` used identically in every task; `descriptor.json` / `DeviceRegistry` / `JsonDevice` / `DescriptorWriter` / `EditorModel` spelled identically; no link target renamed between tasks.
