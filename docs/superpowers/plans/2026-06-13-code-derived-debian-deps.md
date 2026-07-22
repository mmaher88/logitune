# Code-derived Debian dependencies — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Derive the Debian `qml6-module-*` dependency list from the QML `import` statements in the code, shared by both Debian packaging definitions, with a `--check` gate in CI and pre-push.

**Architecture:** A single importable Python generator (`scripts/generate_package_deps.py`) scans `src/**/*.qml`, maps each Qt module to its Debian package name algorithmically (plus a tiny implied/base set), and rewrites only the `qml6-module-*` segment of the `Depends:` line in `scripts/package-deb.sh` and `pkg/obs/debian.control`. A `--check` mode (set comparison) fails when either file is stale, wired into `hooks/pre-push` and `.github/workflows/ci.yml` next to the existing README-devices check.

**Tech Stack:** Python 3 (stdlib only: `argparse`, `pathlib`, `re`), pytest (mirrors `tests/scripts/test_extractor.py`), POSIX sh hook, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-06-13-code-derived-debian-deps-design.md`

**Note on filename:** the sibling generator is `generate-readme-devices.py` (hyphen), but this script must be importable by pytest, so it uses underscores: `generate_package_deps.py`. The CLI command is `python3 scripts/generate_package_deps.py`.

---

## File Structure

- **Create** `scripts/generate_package_deps.py` — the generator: import scanning, package mapping, depends-line rewrite, `--check`/write CLI. Importable as `generate_package_deps` (pytest runs from `scripts/`, which puts it on `sys.path`).
- **Create** `tests/scripts/test_package_deps.py` — pytest unit + repo-state tests.
- **Modify** `scripts/package-deb.sh` — its `Depends:` line is regenerated (gains `qml6-module-qtqml`, drops unused `qml6-module-qt5compat-graphicaleffects`, sorted).
- **Modify** `pkg/obs/debian.control` — its `Depends:` line is regenerated (gains `qml6-module-qtquick-dialogs`, sorted).
- **Modify** `hooks/pre-push` — add a `--check`+regenerate block next to the README check.
- **Modify** `.github/workflows/ci.yml` — add a `--check` step in the "README devices table" job.

---

## Task 1: Generator — import scanning + package mapping

**Files:**
- Create: `scripts/generate_package_deps.py`
- Test: `tests/scripts/test_package_deps.py`

- [ ] **Step 1: Write the failing test**

Create `tests/scripts/test_package_deps.py`:

```python
"""Tests for scripts/generate_package_deps.py. See spec
docs/superpowers/specs/2026-06-13-code-derived-debian-deps-design.md.
Run from the scripts/ dir so the module is importable:
    (cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q)
"""
import generate_package_deps as g


def test_module_maps_to_debian_package():
    pkgs = g.debian_packages({"QtQuick.Dialogs"})
    assert "qml6-module-qtquick-dialogs" in pkgs


def test_controls_implies_templates():
    pkgs = g.debian_packages({"QtQuick.Controls"})
    assert "qml6-module-qtquick-templates" in pkgs


def test_base_qml_modules_always_present():
    pkgs = g.debian_packages({"QtQuick"})
    assert {"qml6-module-qtqml", "qml6-module-qtqml-workerscript"} <= pkgs


def test_qml_imports_excludes_app_and_relative(tmp_path):
    src = tmp_path / "qml"
    src.mkdir()
    (src / "A.qml").write_text(
        'import QtQuick\n'
        'import QtQuick.Dialogs\n'
        'import Logitune\n'
        'import "components"\n', encoding="utf-8")
    assert g.qml_imports(src) == {"QtQuick", "QtQuick.Dialogs"}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: FAIL — `ModuleNotFoundError: No module named 'generate_package_deps'`

- [ ] **Step 3: Write minimal implementation**

Create `scripts/generate_package_deps.py`:

```python
#!/usr/bin/env python3
"""Generate the qml6-module-* dependency list for the Debian packages from
the QML imports in src/**/*.qml, keeping scripts/package-deb.sh and
pkg/obs/debian.control in sync with the code.

Usage:
    scripts/generate_package_deps.py            # rewrite both files in place
    scripts/generate_package_deps.py --check    # exit 1 if either is stale
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC_DIR = REPO / "src"

# Files whose `Depends:` line qml6-module-* segment this script owns.
TARGETS = [
    REPO / "scripts" / "package-deb.sh",
    REPO / "pkg" / "obs" / "debian.control",
]

# Runtime QML modules not written as explicit imports but needed at load
# time: qtqml is the base QML engine, workerscript backs WorkerScript, and
# qtquick-controls is built on qtquick-templates.
ALWAYS = {"qml6-module-qtqml", "qml6-module-qtqml-workerscript"}
IMPLIES = {"qml6-module-qtquick-controls": "qml6-module-qtquick-templates"}

# Matches `import QtQuick`, `import QtQuick.Dialogs`, etc. Quoted/relative
# imports and the app's own `Logitune` module do not start with `Qt`.
IMPORT_RE = re.compile(r"^\s*import\s+(Qt[A-Za-z0-9.]*)")
DEPENDS_RE = re.compile(r"^Depends:\s*(.*)$")


def qml_imports(src_dir: pathlib.Path) -> set[str]:
    """Qt QML module paths imported by any .qml file under src_dir."""
    modules: set[str] = set()
    for qml in src_dir.rglob("*.qml"):
        for line in qml.read_text(encoding="utf-8").splitlines():
            m = IMPORT_RE.match(line)
            if m:
                modules.add(m.group(1))
    return modules


def debian_packages(modules: set[str]) -> set[str]:
    """Map Qt module paths to Debian package names, plus implied/base modules."""
    pkgs = {"qml6-module-" + m.lower().replace(".", "-") for m in modules}
    pkgs |= ALWAYS
    for trigger, implied in IMPLIES.items():
        if trigger in pkgs:
            pkgs.add(implied)
    return pkgs
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: PASS (4 passed)

- [ ] **Step 5: Commit**

```bash
cd /home/mina/repos/logitune-wt-pkg-deps
git add scripts/generate_package_deps.py tests/scripts/test_package_deps.py
git commit -m "feat(pkg-deps): scan QML imports and map to Debian packages"
```

---

## Task 2: Depends-line rewrite + token extraction

**Files:**
- Modify: `scripts/generate_package_deps.py` (append functions)
- Test: `tests/scripts/test_package_deps.py` (append tests)

- [ ] **Step 1: Write the failing test**

Append to `tests/scripts/test_package_deps.py`:

```python
def test_qml_tokens_extracts_only_qml():
    value = "libqt6core6 (>= 6.4), qml6-module-qtquick, libudev1, qml6-module-qtqml"
    assert g.qml_tokens(value) == {"qml6-module-qtquick", "qml6-module-qtqml"}


def test_rewrite_keeps_non_qml_in_order_then_sorted_qml():
    value = "libqt6core6 (>= 6.4), qml6-module-qtquick, libudev1, qml6-module-qtqml"
    out = g.rewrite_depends_value(value, {"qml6-module-qtqml", "qml6-module-qtquick"})
    assert out == ("libqt6core6 (>= 6.4), libudev1, "
                   "qml6-module-qtqml, qml6-module-qtquick")


def test_rewrite_preserves_dpkg_substvars():
    value = "${shlibs:Depends}, ${misc:Depends}, libudev1, qml6-module-qtquick"
    out = g.rewrite_depends_value(value, {"qml6-module-qtquick", "qml6-module-qtqml"})
    assert out.startswith("${shlibs:Depends}, ${misc:Depends}, libudev1, ")
    assert out.endswith("qml6-module-qtqml, qml6-module-qtquick")
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: FAIL — `AttributeError: module 'generate_package_deps' has no attribute 'qml_tokens'`

- [ ] **Step 3: Write minimal implementation**

Append to `scripts/generate_package_deps.py` (after `debian_packages`):

```python
def qml_tokens(depends_value: str) -> set[str]:
    """The set of qml6-module-* tokens in a Depends: field value."""
    return {t.strip() for t in depends_value.split(",")
            if t.strip().startswith("qml6-module-")}


def rewrite_depends_value(depends_value: str, packages: set[str]) -> str:
    """Keep every non-qml token in its original order, then append the
    qml packages sorted. Reproduces the existing libs-then-modules layout."""
    non_qml = [t.strip() for t in depends_value.split(",")
               if t.strip() and not t.strip().startswith("qml6-module-")]
    return ", ".join(non_qml + sorted(packages))
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: PASS (7 passed)

- [ ] **Step 5: Commit**

```bash
cd /home/mina/repos/logitune-wt-pkg-deps
git add scripts/generate_package_deps.py tests/scripts/test_package_deps.py
git commit -m "feat(pkg-deps): rewrite the Depends qml segment, preserve the rest"
```

---

## Task 3: File processing + CLI (`--check` / write)

**Files:**
- Modify: `scripts/generate_package_deps.py` (append `process_file`, `main`)
- Test: `tests/scripts/test_package_deps.py` (append tests)

- [ ] **Step 1: Write the failing test**

Append to `tests/scripts/test_package_deps.py`:

```python
def test_process_file_check_detects_stale_then_synced(tmp_path):
    f = tmp_path / "debian.control"
    f.write_text(
        "Package: logitune\n"
        "Depends: ${misc:Depends}, libudev1, qml6-module-qtquick\n"
        "Description: x\n", encoding="utf-8")
    want = {"qml6-module-qtquick", "qml6-module-qtqml"}

    # check mode: stale -> returns False, file unchanged
    assert g.process_file(f, want, check=True) is False
    assert "qml6-module-qtqml" not in f.read_text()

    # write mode: applies, file now contains both, sorted after non-qml
    assert g.process_file(f, want, check=False) is False
    text = f.read_text()
    assert ("Depends: ${misc:Depends}, libudev1, "
            "qml6-module-qtqml, qml6-module-qtquick\n") in text

    # check mode again: now in sync
    assert g.process_file(f, want, check=True) is True
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: FAIL — `AttributeError: module 'generate_package_deps' has no attribute 'process_file'`

- [ ] **Step 3: Write minimal implementation**

Append to `scripts/generate_package_deps.py` (after `rewrite_depends_value`):

```python
def process_file(path: pathlib.Path, packages: set[str], check: bool) -> bool:
    """Rewrite (or, in check mode, validate) the qml6-module-* segment of the
    `Depends:` line. Returns True if the file is already in sync."""
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    out: list[str] = []
    found = False
    in_sync = True
    for line in lines:
        m = DEPENDS_RE.match(line.rstrip("\n"))
        if m:
            found = True
            if qml_tokens(m.group(1)) != packages:
                in_sync = False
            newline = "Depends: " + rewrite_depends_value(m.group(1), packages)
            out.append(newline + ("\n" if line.endswith("\n") else ""))
        else:
            out.append(line)
    if not found:
        sys.stderr.write(f"error: no 'Depends:' line in {path}\n")
        sys.exit(2)
    if not check and not in_sync:
        path.write_text("".join(out), encoding="utf-8")
    return in_sync


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="Exit 1 if any packaging file is stale; do not modify.")
    args = ap.parse_args()

    packages = debian_packages(qml_imports(SRC_DIR))
    if not packages:
        sys.stderr.write("error: no QML imports found under src/\n")
        return 2

    stale = [p for p in TARGETS if not process_file(p, packages, args.check)]

    if args.check:
        if stale:
            sys.stderr.write(
                "Debian package deps are out of sync with QML imports:\n"
                + "".join(f"  {p}\n" for p in stale)
                + "Expected qml6-module-* set:\n  "
                + ", ".join(sorted(packages)) + "\n"
                + "Run: python3 scripts/generate_package_deps.py\n")
            return 1
        print("Debian package deps in sync.")
        return 0

    for p in stale:
        print(f"Updated {p}")
    if not stale:
        print("Debian package deps already up to date.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: PASS (8 passed)

- [ ] **Step 5: Commit**

```bash
cd /home/mina/repos/logitune-wt-pkg-deps
git add scripts/generate_package_deps.py tests/scripts/test_package_deps.py
git commit -m "feat(pkg-deps): process_file + --check/write CLI"
```

---

## Task 4: Regenerate the packaging files + repo-state guard test

**Files:**
- Modify: `scripts/package-deb.sh` (regenerated `Depends:` line)
- Modify: `pkg/obs/debian.control` (regenerated `Depends:` line)
- Test: `tests/scripts/test_package_deps.py` (append guard test)

- [ ] **Step 1: Run the generator to update both files**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps && python3 scripts/generate_package_deps.py`
Expected output (both updated, or one if PR #137 already merged dialogs into control):
```
Updated /home/mina/repos/logitune-wt-pkg-deps/scripts/package-deb.sh
Updated /home/mina/repos/logitune-wt-pkg-deps/pkg/obs/debian.control
```

- [ ] **Step 2: Verify the diff is exactly the qml segment**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps && git diff scripts/package-deb.sh pkg/obs/debian.control`
Expected: only the `Depends:` lines change. `package-deb.sh` gains `qml6-module-qtqml`, drops `qml6-module-qt5compat-graphicaleffects`, qml tokens sorted. `pkg/obs/debian.control` gains `qml6-module-qtquick-dialogs`, qml tokens sorted. Both qml segments are now identical:
`qml6-module-qtqml, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window`

- [ ] **Step 3: Write the guard test (real committed files stay in sync)**

Append to `tests/scripts/test_package_deps.py`:

```python
def test_real_packaging_files_in_sync():
    """The committed package-deb.sh and debian.control must match the
    QML imports — this is what the CI/pre-push --check enforces."""
    packages = g.debian_packages(g.qml_imports(g.SRC_DIR))
    for path in g.TARGETS:
        assert g.process_file(path, packages, check=True) is True, (
            f"{path} out of sync; run python3 scripts/generate_package_deps.py")
```

- [ ] **Step 4: Run the full test file to verify it passes**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps/scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q`
Expected: PASS (9 passed)

- [ ] **Step 5: Commit**

```bash
cd /home/mina/repos/logitune-wt-pkg-deps
git add scripts/package-deb.sh pkg/obs/debian.control tests/scripts/test_package_deps.py
git commit -m "packaging: regenerate Debian deps from QML imports

Adds qml6-module-qtquick-dialogs to the OBS control, drops the unused
qt5compat-graphicaleffects from package-deb.sh, and normalizes both qml
segments to the generated set."
```

---

## Task 5: Wire `--check` into pre-push and CI

**Files:**
- Modify: `hooks/pre-push` (add block after line 18, the README check)
- Modify: `.github/workflows/ci.yml` (add step after line 167, the README check)

- [ ] **Step 1: Add the pre-push check block**

In `hooks/pre-push`, immediately after the README check block (after the closing `fi` on line 18, before the blank line and `echo "🔍 Running tests before push..."`), insert:

```sh

# Debian package deps must match the QML imports in src/. Regenerate on
# drift so the developer just has to `git add` and retry.
echo "📦 Checking Debian package deps..."
if ! python3 "${REPO_ROOT}/scripts/generate_package_deps.py" --check > /dev/null 2>&1; then
    echo "⚠️  Debian package deps are out of sync with src/**/*.qml imports."
    python3 "${REPO_ROOT}/scripts/generate_package_deps.py" > /dev/null
    echo "   Regenerated package-deb.sh + debian.control. Review the diff, then:"
    echo "     git add scripts/package-deb.sh pkg/obs/debian.control && git commit --amend --no-edit"
    echo "   or make a fresh commit and re-push."
    exit 1
fi
```

- [ ] **Step 2: Add a pytest block for the new tests in pre-push**

In `hooks/pre-push`, after the existing "Extractor Python tests" block (after its closing `fi` on line 61, before `echo "✅ All tests passed. Pushing..."`), insert:

```sh
# Package-deps Python tests
(cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q > /dev/null 2>&1)
if [ $? -ne 0 ]; then
    echo "❌ Package-deps pytest failed. Push aborted."
    (cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q 2>&1 | tail -20)
    exit 1
fi
```

- [ ] **Step 3: Add the CI step**

In `.github/workflows/ci.yml`, in the "README devices table" job, after the existing step (lines 166-167):

```yaml
      - name: Verify README.md table matches descriptors
        run: python3 scripts/generate-readme-devices.py --check
```

add immediately below it:

```yaml
      - name: Verify Debian package deps match QML imports
        run: python3 scripts/generate_package_deps.py --check
      - name: Verify package-deps generator tests
        run: cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q
```

- [ ] **Step 4: Verify the check passes locally (files already regenerated in Task 4)**

Run: `cd /home/mina/repos/logitune-wt-pkg-deps && python3 scripts/generate_package_deps.py --check`
Expected: `Debian package deps in sync.` (exit 0)

- [ ] **Step 5: Commit**

```bash
cd /home/mina/repos/logitune-wt-pkg-deps
git add hooks/pre-push .github/workflows/ci.yml
git commit -m "ci(pkg-deps): --check Debian deps in pre-push and CI"
```

---

## Final verification (before PR)

- [ ] Configure + build so the pre-push gate can run (worktree has no `build/` yet):
  `cd /home/mina/repos/logitune-wt-pkg-deps && cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev && cmake --build build -j$(nproc)`
- [ ] Run the new tests: `cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q` → 9 passed
- [ ] Run `--check`: `python3 scripts/generate_package_deps.py --check` → in sync, exit 0
- [ ] Push (pre-push gate runs full suite) and open a PR to `master`.

---

## Self-review (done during authoring)

- **Spec coverage:** generator from QML imports (Tasks 1-3) ✓; both Debian files consume the same set (Task 4) ✓; `--check` in CI + pre-push (Task 5) ✓; drops unused graphicaleffects (Task 4 Step 2) ✓; Arch/Fedora untouched (no task touches them) ✓; set-based check (`process_file` compares `qml_tokens(...) != packages` as sets) ✓.
- **Placeholders:** none — every step has full code/commands and expected output.
- **Type consistency:** `qml_imports(Path)->set[str]`, `debian_packages(set)->set[str]`, `qml_tokens(str)->set[str]`, `rewrite_depends_value(str,set)->str`, `process_file(Path,set,bool)->bool` used consistently across Tasks 1-5; constants `SRC_DIR`, `TARGETS`, `ALWAYS`, `IMPLIES`, `DEPENDS_RE` defined in Task 1 and referenced later.
