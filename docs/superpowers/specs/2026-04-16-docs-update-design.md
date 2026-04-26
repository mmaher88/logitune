# Documentation Update Design

**Date:** 2026-04-16
**Status:** Approved
**Scope:** Bring README, wiki, and supporting docs into alignment with changes from PRs #37 (editor mode), #38 (carousel UX), #40 (verified/beta), and #42 (startup hardening). Assume #42 and #45 will land before this PR merges.

## Goals

1. Reflect the JSON descriptor workflow that replaced C++ `IDevice` subclasses in PR #37.
2. Document the in-app editor (`--edit` flag) as the preferred contribution path for visual polish.
3. Define the `verified` vs `beta` status badges and where the single source of truth lives.
4. Remove stale references to the old C++ descriptor pattern across wiki pages.
5. Produce a doc set where reviewers can read commit-by-commit and each state is internally consistent.

## Non-Goals

- No changes to `docs/superpowers/` (internal specs and plans).
- No CHANGELOG.md; GitHub releases remain the source of release notes.
- No API reference / doxygen generation.
- No changes to `Building.md` or `Adding-a-Desktop-Environment.md` (no drift detected).
- No "create a new descriptor" tooling in the editor; this doc update reflects the current scope.

## Audience

Two audiences, one doc set:

- **End users** (README, Getting-Started) want to know what the project is, how to install, and what the device badges mean.
- **Contributors** (Adding-a-Device, Editor-Mode, Architecture, Contributing, Testing) want to add a new device descriptor or understand internals.

The Editor Mode page bridges both: a short user-facing intro, then a "how to use this to contribute" section.

## Status Policy

Codifying what `verified` and `beta` mean in the descriptor `status` field:

- **`verified`**: The maintainer (or a trusted contributor) physically owns the device and confirmed end-to-end behavior works.
- **`beta`**: Everything else. Community-submitted, or shipped without hardware verification.

This is the only promotion criterion. Contributors submitting new descriptors should set `status: "beta"` unless they have hardware-verified and a maintainer agrees.

Single source of truth: `docs/wiki/Getting-Started.md#device-support-status`. All other doc mentions link here.

## File Plan

### Updated

| File | Change |
|------|--------|
| `README.md` | Add editor mode to Features list; add "Device support status" subsection linking to wiki; update Supported Devices table to include MX Master 2S, 3S, 4 with correct badges; add Editor Mode link |
| `docs/wiki/Home.md` | Add Editor Mode index entry; update Adding-a-Device blurb |
| `docs/wiki/Adding-a-Device.md` | Full rewrite for JSON workflow (see page outline below) |
| `docs/wiki/Architecture.md` | Replace C++ descriptor refs with JSON descriptor + `DeviceRegistry` + `JsonDevice` + `DescriptorWriter` + `EditorModel`; update diagrams where needed |
| `docs/wiki/Contributing.md` | Point device-adding workflow at Editor Mode as preferred path |
| `docs/wiki/Testing.md` | JSON fixtures instead of mock-device helpers; note crash dialog behavior change from #42 |
| `docs/wiki/Getting-Started.md` | Add "Device support status" section (source of truth for verified/beta) |
| `docs/wiki/HID++-Protocol.md` | Replace any inline descriptor code examples with JSON equivalents |

### Added

| File | Purpose |
|------|---------|
| `docs/wiki/Editor-Mode.md` | New. Covers `--edit` end-to-end with screenshots |

### Unchanged

- `docs/wiki/Building.md`
- `docs/wiki/Adding-a-Desktop-Environment.md`
- `docs/superpowers/**`

## Page Outline: Adding-a-Device.md (rewrite)

1. **Prerequisites**: PIDs, CIDs, features list, DPI range, images
2. **Step 1: Create the descriptor folder**: `devices/<slug>/descriptor.json` + placeholder `front.png`, `side.png`, `back.png`
3. **Step 2: Fill in the bootstrap JSON by hand**: fields the editor cannot help with:
   - `name`, `status` (default `beta`), `productIds`
   - `features` flags
   - `dpi` range
   - `controls` array (CIDs, `buttonIndex`, `defaultName`, `defaultActionType`, `configurable`)
   - Rough `buttonHotspots`, `scrollHotspots`, `easySwitchSlotPositions` with placeholder coordinates
4. **Step 3: Register with DeviceRegistry**: either ship in `devices/` (built-in) or drop into `$XDG_DATA_DIRS/logitune/devices/` for local testing
5. **Step 4: Polish with Editor Mode**: link to `Editor-Mode.md` for the visual pass (drag hotspots, drag slot circles, upload real images, rename controls)
6. **Step 5: Test**: `logitune --simulate-all` to eyeball without hardware; real hardware if available; smoke test checklist
7. **Step 6: Submit PR**: status field guidance, what the maintainer reviews
8. **When you need C++**: callout for new HID++ feature variants requiring a capability-table entry in `src/core/hidpp/capabilities/`; link to `Architecture.md#device-registry`
9. **Reference: MX Master 3S descriptor**: annotated worked example

## Page Outline: Editor-Mode.md (new)

1. **What it is**: visual editor for existing descriptors; prerequisite is an existing `descriptor.json`
2. **When to use it**: positioning hotspots, slot circles, uploading images, polishing text labels; the easy contribution path after bootstrapping the JSON by hand
3. **Launching**: `logitune --edit` (pairs well with `--simulate-all` when you don't have the hardware)
4. **UI walk-through with screenshots:**
   - Editor toolbar (amber stripe + Save / Reset / Undo / Redo / Diff buttons)
   - Buttons page drag handles
   - Point & Scroll page drag handles
   - Easy-Switch page slot-circle handles
   - Double-click rename for labels, control names, device name
   - Image upload via drag-drop or file picker
   - Conflict banner (when the file changed on disk)
   - Diff modal (on-disk vs in-memory)
5. **Save / reset semantics**: atomic write, preserve-unknown-fields, self-write suppression
6. **Contribution workflow**: fork → `--edit` → tweak → save → verify `git diff` → submit PR
7. **Limitations**: no "create new descriptor" flow; schema fields (CIDs, features flags) must be edited by hand in `descriptor.json`
8. **Links**: back to `Adding-a-Device.md#submitting-a-pr`, forward to `Getting-Started.md#device-support-status`

## Linking Structure

```
README.md
  -> Home.md
  -> Editor-Mode.md (new Features link)
  -> Getting-Started.md#device-support-status

Home.md
  -> Editor-Mode.md (new index entry)
  -> Adding-a-Device.md (updated blurb)

Getting-Started.md
  -> #device-support-status (new anchor, single source of truth)

Adding-a-Device.md
  -> Editor-Mode.md (visual-polish callout in Step 4)
  -> Architecture.md#device-registry (the "when you need C++" callout)
  -> Contributing.md (PR submission)

Editor-Mode.md
  -> Adding-a-Device.md#submitting-a-pr
  -> Getting-Started.md#device-support-status

Contributing.md
  -> Editor-Mode.md (preferred path)
  -> Adding-a-Device.md (full workflow)

Architecture.md
  -> (no new outbound links; internal refs updated)
```

## Screenshots

The Editor-Mode page needs screenshots. Split by capture method:

### Automated (agent captures during implementation)

Launched via `logitune --edit --simulate-all`; captured with `gnome-screenshot` or `spectacle`:

- Editor toolbar (amber stripe visible)
- Buttons page with hotspot drag handles visible
- Point & Scroll page with drag handles visible
- Easy-Switch page with slot-circle handles visible
- Empty image drop zone

### Manual (user captures)

Transient or interactive states where driving the UI from a script is brittle:

- Card mid-drag (line redrawing)
- Double-click rename in progress (text field active)
- Conflict banner visible
- Diff modal open with real diff content
- Save-in-progress / unsaved-changes indicator

Manual screenshots land in the final commit with explicit filenames in `docs/images/editor-mode-*.png`. The markdown uses placeholder paths during earlier commits.

## Commit Sequence

Each commit is independently coherent. Reviewer can read in order.

1. `docs: rewrite Adding-a-Device for JSON workflow`
2. `docs: add Editor-Mode wiki page`
3. `docs: explain verified/beta status in Getting-Started`
4. `docs: update Architecture for JSON descriptors + editor components`
5. `docs: update Testing for JSON fixtures + #42 crash dialog change`
6. `docs: update Contributing to point at Editor Mode`
7. `docs: update Home.md wiki index`
8. `docs: update README with editor mode, verified/beta, wiki links`
9. `docs(editor-mode): add interactive-state screenshots`

Commits 1-8 land without user-provided screenshots. Commit 9 follows once user supplies the interactive-state images.

## Dependencies and Assumptions

- PR #40 (verified/beta status) is merged to master. Confirmed at design time.
- PR #42 (startup hardening) will merge before this PR. The Testing.md wording about the on-launch crash dialog reflects the post-#42 behavior.
- PR #45 (CI caching) has no doc impact.

## Validation

- **Cross-link integrity**: every wiki-relative link resolves to a committed page with the referenced anchor.
- **No orphan pages**: every new page is reachable from Home.md.
- **Consistency**: verified/beta policy is stated in exactly one place; all other mentions link to it.
- **No stale code examples**: grep for `IDevice`, `MxMaster3sDescriptor`, `#include.*Descriptor.h` in wiki files and resolve any hits.

## Out of Scope (for a follow-up)

- Automated wiki publishing via GitHub Actions (currently wiki is manually synced from `docs/wiki/`).
- Schema JSON with `$schema` reference for IDE validation of `descriptor.json`.
- Contributor's guide to adding a new HID++ feature variant (requires C++ work; deeper than this pass).
