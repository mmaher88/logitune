#!/usr/bin/env python3
"""Regenerate the Supported Devices table in README.md from
devices/*/descriptor.json.

Usage:
    scripts/generate-readme-devices.py              # write in place
    scripts/generate-readme-devices.py --check      # exit 1 if README differs

The table is bounded by the HTML markers
<!-- BEGIN DEVICES TABLE --> and <!-- END DEVICES TABLE --> in README.md.
Everything between them is replaced; content outside is untouched.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
README = REPO / "README.md"
DEVICES_DIR = REPO / "devices"
BEGIN = "<!-- BEGIN DEVICES TABLE -->"
END = "<!-- END DEVICES TABLE -->"

# Column label -> predicate taking the descriptor dict.
# Predicate returns True for ✅, False for —. Special strings can
# override: return a string and it is rendered verbatim.
COLUMNS: list[tuple[str, object]] = [
    ("Device",        lambda d: d["name"]),
    ("Status",        lambda d: "✅ Verified" if d.get("status") == "verified"
                                 else "🧪 Beta"),
    ("Battery",       lambda d: d["features"].get("battery", False)),
    ("DPI",           lambda d: d["features"].get("adjustableDpi", False)
                                 or d["features"].get("extendedDpi", False)),
    ("SmartShift",    lambda d: d["features"].get("smartShift", False)),
    ("Thumb wheel",   lambda d: d["features"].get("thumbWheel", False)
                                 or d["features"].get("thumbWheelGestureV2", False)),
    ("Button remap",  lambda d: d["features"].get("reprogControls", False)),
    ("Gestures",      lambda d: any(c.get("defaultActionType") == "gesture-trigger"
                                     for c in d.get("controls", []))),
    ("Smooth scroll", lambda d: d["features"].get("smoothScroll", False)),
    ("Easy-Switch",   lambda d: bool(d.get("easySwitchSlots"))),
]


def load_descriptors() -> list[dict]:
    out = []
    for p in sorted(DEVICES_DIR.glob("*/descriptor.json")):
        out.append(json.loads(p.read_text()))
    # Sort: verified first (preserving alpha order inside each group).
    out.sort(key=lambda d: (0 if d.get("status") == "verified" else 1, d["name"]))
    return out


def render_cell(value: object) -> str:
    if isinstance(value, str):
        return value
    return "✅" if value else "—"


def render_table(descriptors: list[dict]) -> str:
    header = "| " + " | ".join(c[0] for c in COLUMNS) + " |"
    align_cells = [":--:" if i > 1 else ("--------" if i == 0 else ":------:")
                   for i, _ in enumerate(COLUMNS)]
    sep = "|" + "|".join(align_cells) + "|"
    rows = [header, sep]
    for d in descriptors:
        row = "| " + " | ".join(render_cell(c[1](d)) for c in COLUMNS) + " |"
        rows.append(row)
    return "\n".join(rows)


def splice(readme_text: str, table: str) -> str:
    try:
        before, rest = readme_text.split(BEGIN, 1)
        _, after = rest.split(END, 1)
    except ValueError:
        sys.stderr.write(
            f"error: could not find both markers {BEGIN!r} and {END!r} in {README}\n")
        sys.exit(2)
    return f"{before}{BEGIN}\n{table}\n{END}{after}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="Exit 1 if README needs regeneration; do not modify.")
    args = ap.parse_args()

    descriptors = load_descriptors()
    if not descriptors:
        sys.stderr.write("error: no descriptors found under devices/*\n")
        return 2

    table = render_table(descriptors)
    current = README.read_text()
    updated = splice(current, table)

    if args.check:
        if current != updated:
            sys.stderr.write(
                "README.md device table is out of sync with devices/*/descriptor.json.\n"
                "Run: scripts/generate-readme-devices.py\n")
            return 1
        return 0

    if current != updated:
        README.write_text(updated)
        print(f"Updated {README}")
    else:
        print(f"{README} already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
