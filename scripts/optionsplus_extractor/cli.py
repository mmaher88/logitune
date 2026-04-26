"""argparse wrapper — glues sources → capabilities → slots → canonicalize → descriptor → validate."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Optional

from . import sources, descriptor, validate
from .slots import UnknownSlotName


def _slugify(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def run(
    devices_dir: Path,
    main_dir: Path,
    output_dir: Path,
    dry_run: bool = False,
    skip_existing: bool = False,
) -> int:
    """Run the full extraction pipeline. Returns a process exit code."""
    devices_dir = Path(devices_dir)
    main_dir = Path(main_dir)
    output_dir = Path(output_dir)

    mice = sources.load_device_db(main_dir)
    print(f"Found {len(mice)} mice in Options+ database", file=sys.stderr)

    processed = 0
    skipped_no_images = 0
    unknown_slot_reports: list[dict] = []

    for depot_name, entry in sorted(mice.items(), key=lambda kv: kv[1].name):
        slug = _slugify(entry.name)
        depot_dir = devices_dir / depot_name
        out_dir = output_dir / slug

        if skip_existing and (out_dir / "descriptor.json").exists():
            continue

        depot = sources.load_depot(depot_dir)
        if depot.front_image is None:
            print(f"skip {slug}: no front image", file=sys.stderr)
            skipped_no_images += 1
            continue

        try:
            desc = descriptor.build(entry, depot)
        except UnknownSlotName as e:
            unknown_slot_reports.append({"device": slug, "error": str(e)})
            print(f"skip {slug}: unknown slot — {e}", file=sys.stderr)
            continue

        try:
            validate.check(desc, depot)
        except validate.SchemaError as e:
            print(f"skip {slug}: schema self-check failed — {e}", file=sys.stderr)
            continue

        if dry_run:
            print(f"  {entry.name:40s} -> {slug}/ ({len(desc['controls'])} controls)")
            processed += 1
            continue

        out_dir.mkdir(parents=True, exist_ok=True)
        with open(out_dir / "descriptor.json", "w") as f:
            json.dump(desc, f, indent=2)
            f.write("\n")
        shutil.copy2(depot.front_image, out_dir / "front.png")
        if depot.side_image is not None:
            shutil.copy2(depot.side_image, out_dir / "side.png")
        if depot.back_image is not None:
            shutil.copy2(depot.back_image, out_dir / "back.png")

        print(
            f"  {slug}: {len(desc['controls'])} controls, "
            f"{len(desc['hotspots']['buttons'])} hotspots, "
            f"{len(desc['hotspots']['scroll'])} scroll, "
            f"{len(desc['easySwitchSlots'])} easyswitch"
        )
        processed += 1

    report_path = output_dir / "extraction-report.json"
    output_dir.mkdir(parents=True, exist_ok=True)
    with open(report_path, "w") as f:
        json.dump({
            "processed": processed,
            "skipped_no_images": skipped_no_images,
            "unknown_slot_names": unknown_slot_reports,
        }, f, indent=2)

    print(
        f"Done: {processed} generated, {skipped_no_images} missing images, "
        f"{len(unknown_slot_reports)} unknown slots",
        file=sys.stderr,
    )
    return 1 if unknown_slot_reports else 0


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate logitune descriptors from extracted Options+ data",
    )
    parser.add_argument("--devices-dir", required=True,
                        help="extracted per-device depot directory")
    parser.add_argument("--main-dir", required=True,
                        help="extracted logioptionsplus main depot directory")
    parser.add_argument("--output-dir", required=True,
                        help="output directory for descriptors")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-existing", action="store_true", default=False)
    args = parser.parse_args(argv)
    return run(
        devices_dir=Path(args.devices_dir),
        main_dir=Path(args.main_dir),
        output_dir=Path(args.output_dir),
        dry_run=args.dry_run,
        skip_existing=args.skip_existing,
    )


if __name__ == "__main__":
    sys.exit(main())
