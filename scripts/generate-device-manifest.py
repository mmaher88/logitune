#!/usr/bin/env python3
"""Regenerate devices/manifest.json from bundled descriptors."""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DEVICES = REPO / "devices"
MANIFEST = DEVICES / "manifest.json"
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".svg", ".webp"}


def manifest_files(device_dir: pathlib.Path) -> list[str]:
    files = ["descriptor.json"]
    for path in sorted(device_dir.iterdir()):
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            files.append(path.name)
    return files


def build_manifest() -> dict:
    devices: dict[str, dict] = {}
    for descriptor in sorted(DEVICES.glob("*/descriptor.json")):
        slug = descriptor.parent.name
        data = json.loads(descriptor.read_text())
        devices[slug] = {
            "version": int(data.get("version", 1)),
            "pids": data["productIds"],
            "files": manifest_files(descriptor.parent),
        }
    return {"devices": devices}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    text = json.dumps(build_manifest(), indent=2) + "\n"
    if args.check:
        if not MANIFEST.exists() or MANIFEST.read_text() != text:
            sys.stderr.write("devices/manifest.json is out of sync. Run: scripts/generate-device-manifest.py\n")
            return 1
        return 0

    if not MANIFEST.exists() or MANIFEST.read_text() != text:
        MANIFEST.write_text(text)
        print(f"Updated {MANIFEST}")
    else:
        print(f"{MANIFEST} already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
