# Code-derived Debian package dependencies

## Problem

Logitune ships through several packaging paths. The runtime dependency on
QML modules is declared by hand in **two** Debian definitions:

- `scripts/package-deb.sh` — the GitHub `.deb`
- `pkg/obs/debian.control` — the OBS-built `.deb` (what Ubuntu users `apt install`)

QML modules are loaded by the Qt engine at runtime, not linked as shared
objects, so `dpkg-shlibdeps` cannot auto-detect them. The two hand-maintained
lists drifted from each other and from the code: `pkg/obs/debian.control` was
missing `qml6-module-qtquick-dialogs`, which `DeviceRender.qml` imports. A clean
Ubuntu 24.04 install therefore started, talked to the device, and showed no
window (`QML failed to load — no root objects`).

This is Debian-specific. **Arch** (`qt6-declarative`) and **Fedora**
(`qt6-qtdeclarative` + rpm automatic dependency generation) bundle all QML
modules into one package, so they cannot drift on a missing QML module.

## Goals

- The Debian `qml6-module-*` dependency list is **derived from the code** (the
  `import` statements in `src/**/*.qml`), so it cannot drift from what the app
  actually loads.
- Both Debian definitions consume the **same generated list**, so they cannot
  disagree with each other.
- Drift is caught automatically in CI and pre-push, matching the existing
  `generate-readme-devices.py` / `--check` pattern.

## Non-goals (YAGNI)

- Arch and Fedora declarations. They bundle QML modules and Fedora auto-generates
  requires; there is nothing to drift. Left untouched.
- The non-QML library dependencies (`libqt6core6`, `libqt6svg6`, `libudev1`, …).
  These are stable and, on the OBS `.deb`, already covered by `${shlibs:Depends}`.
  The generator does **not** manage them.

## Design

### New tool: `scripts/generate-package-deps.py`

Mirrors `scripts/generate-readme-devices.py`: it can **write** the dependency
segment into the packaging files, and run with **`--check`** to fail (non-zero,
printing the expected list) when a file is stale.

**Computing the `qml6-module-*` set:**

1. Scan `src/**/*.qml` for `import <Module.Path>` statements. Ignore version
   suffixes, relative/string imports (`import "..."`), and the app's own module
   `Logitune`.
2. Map each remaining module to its Debian package **algorithmically**:
   `qml6-module-` + `module.lower().replace('.', '-')`.
   - `QtQuick` → `qml6-module-qtquick`
   - `QtQuick.Controls` → `qml6-module-qtquick-controls`
   - `QtQuick.Dialogs` → `qml6-module-qtquick-dialogs`
   - `QtQuick.Layouts` → `qml6-module-qtquick-layouts`
   - `QtQuick.Window` → `qml6-module-qtquick-window`

   A new `import` automatically pulls its package — no table to maintain.
3. Add a small, documented **implied set** for runtime modules that are not
   written as explicit imports:
   - `qml6-module-qtquick-controls` present ⇒ also `qml6-module-qtquick-templates`
     (Controls is built on Templates).
   - Always: `qml6-module-qtqml`, `qml6-module-qtqml-workerscript` (base QML
     engine underpinning any QtQuick app).
   This set lives as a commented constant in the script (~3 entries).
4. Sort and de-duplicate. This sorted list is the canonical segment.

**Writing / checking the files (token replacement, no moved boilerplate):**

The generator operates on each file's existing `Depends:` line. It preserves
every non-`qml6-module-*` token verbatim (the `${shlibs:Depends}, ${misc:Depends}`
prefix in `debian.control`, the `libqt6*`/`libudev1` libs in both) and replaces
only the `qml6-module-*` tokens with the generated set, inserted at the position
of the first existing `qml6-module-*` token. The library declarations stay in the
packaging files where they belong; the script owns only the QML segment.

- Write mode: rewrites the `Depends:` line in both files.
- `--check` mode: extracts the `qml6-module-*` tokens from each file and compares
  them to the generated set as a **set** (order-insensitive, so a harmless manual
  reorder does not fail); on a missing/extra package it prints the expected
  segment and exits 1. Write mode emits the tokens **sorted** for a deterministic
  committed diff.

### Integration

- `hooks/pre-push`: add `generate-package-deps.py --check` next to the existing
  README-devices check.
- `.github/workflows/ci.yml`: same `--check` step.
- Regenerate command: `python3 scripts/generate-package-deps.py`.

### Tests

`tests/scripts/test_package_deps.py` (pytest, alongside the existing
`test_extractor.py`):

- Mapping is algorithmic: `QtQuick.Dialogs` → `qml6-module-qtquick-dialogs`.
- Implied set applied: a fixture importing `QtQuick.Controls` yields
  `qml6-module-qtquick-templates`; base `qtqml`/`qtqml-workerscript` always present.
- Exclusions: `import Logitune` and `import "..."` are ignored.
- `--check` returns 0 on a synced file and non-zero on a stale one.
- An end-to-end check that the real `package-deb.sh` and `debian.control` are in
  sync after generation (guards the committed state).

## Behavior change to note

The shipped code does not `import Qt5Compat.GraphicalEffects`, so the generator
**drops** the stray `qml6-module-qt5compat-graphicaleffects` currently
over-declared in `scripts/package-deb.sh`. Correct (unused dependency), but a real
diff.

## Relationship to PR #137

PR #137 is the immediate one-line fix adding `qml6-module-qtquick-dialogs` to
`pkg/obs/debian.control`. This generator is the systemic prevention. If #137
lands first, the generator validates it (same output). If this lands, it
supersedes #137. Either order is fine.

## Rollout

No version/runtime behavior change in the app. The corrected Debian dependencies
reach Ubuntu users when the OBS spec syncs and rebuilds (next release tag, or a
manual `trigger_obs` dispatch). Existing `0.3.5-0` installs are unblocked with
`sudo apt install qml6-module-qtquick-dialogs`.
