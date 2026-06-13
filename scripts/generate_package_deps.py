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


def qml_tokens(depends_value: str) -> set[str]:
    """The set of qml6-module-* package names in a Depends: field value,
    with any version-constraint suffix removed
    (e.g. 'qml6-module-x (>= 6.4)' -> 'qml6-module-x')."""
    names: set[str] = set()
    for raw in depends_value.split(","):
        token = raw.strip()
        if not token:
            continue
        name = token.split()[0]
        if name.startswith("qml6-module-"):
            names.add(name)
    return names


def rewrite_depends_value(depends_value: str, packages: set[str]) -> str:
    """Keep every non-qml token in its original order, then append the
    qml packages sorted. Reproduces the existing libs-then-modules layout."""
    non_qml = [t.strip() for t in depends_value.split(",")
               if t.strip() and not t.strip().startswith("qml6-module-")]
    return ", ".join(non_qml + sorted(packages))


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
