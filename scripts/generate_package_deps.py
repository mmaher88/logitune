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
