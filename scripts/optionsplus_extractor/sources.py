"""Locate and load Options+ device DB and per-device depot files."""

from __future__ import annotations

import glob
import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class DeviceDbEntry:
    depot: str                  # e.g. "mx_master_3s"
    name: str                   # displayName from Options+
    product_ids: list[str] = field(default_factory=list)  # hex strings like "0xb019"
    capabilities: dict = field(default_factory=dict)


@dataclass
class Depot:
    path: Path
    metadata: Optional[dict]     # core_metadata.json or metadata.json contents
    front_image: Optional[Path]  # front_core.png | front.png
    side_image: Optional[Path]
    back_image: Optional[Path]


def load_device_db(main_dir: Path) -> dict[str, DeviceDbEntry]:
    """Load all mice from Options+ device database.

    `main_dir` is the extracted logioptionsplus depot root. The devices
    JSON files live at `<main_dir>/data/devices/devices*.json`.
    """
    result: dict[str, DeviceDbEntry] = {}
    pattern = str(main_dir / "data" / "devices" / "devices*.json")
    for path in sorted(glob.glob(pattern)):
        try:
            data = json.load(open(path, encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            continue
        for d in data.get("devices", []):
            if d.get("type") != "MOUSE":
                continue
            depot = d.get("depot", "")
            if not depot:
                continue
            pids = _extract_pids(d)
            entry = result.get(depot)
            if entry is None:
                entry = DeviceDbEntry(
                    depot=depot,
                    name=d.get("displayName", depot),
                    product_ids=sorted(pids),
                    capabilities=d.get("capabilities", {}) or {},
                )
                result[depot] = entry
            else:
                merged = sorted(set(entry.product_ids) | pids)
                entry.product_ids = merged
                if not entry.capabilities and d.get("capabilities"):
                    entry.capabilities = d["capabilities"]
    return result


def _extract_pids(device_entry: dict) -> set[str]:
    pids: set[str] = set()
    for mode in device_entry.get("modes", []) or []:
        for iface in mode.get("interfaces", []) or []:
            iid = iface.get("id", "") or ""
            if "046d" not in iid.lower():
                continue
            pid_hex = iid.lower().split("_")[-1] if "_" in iid else ""
            if pid_hex:
                pids.add(f"0x{pid_hex}")
    return pids


def load_depot(depot_dir: Path) -> Depot:
    """Load a per-device depot directory.

    Handles both modern (`core_metadata.json` + `*_core.png`) and legacy
    (`metadata.json` + unprefixed PNGs) naming conventions.
    """
    metadata = _load_first(depot_dir, ["core_metadata.json", "metadata.json"])
    front = _find_first(depot_dir, ["front_core.png", "front.png"])
    side  = _find_first(depot_dir, ["side_core.png",  "side.png"])
    back  = _find_first(
        depot_dir,
        ["back_core.png", "bottom_core.png", "back.png", "bottom.png"],
    )
    return Depot(
        path=depot_dir,
        metadata=metadata,
        front_image=front,
        side_image=side,
        back_image=back,
    )


def _load_first(base: Path, names: list[str]) -> Optional[dict]:
    for n in names:
        p = base / n
        if p.exists():
            try:
                return json.load(open(p))
            except (OSError, json.JSONDecodeError):
                return None
    return None


def _find_first(base: Path, names: list[str]) -> Optional[Path]:
    for n in names:
        p = base / n
        if p.exists():
            return p
    return None
