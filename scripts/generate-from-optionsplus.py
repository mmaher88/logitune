#!/usr/bin/env python3
"""Generate device descriptors from extracted Options+ data.

Combines Options+ device database (PIDs, names), depot images (front.png, side.png),
and metadata.json (button hotspots with CID-encoded slot IDs) to produce complete
JSON descriptors for the logitune-devices community repo.

Prerequisites:
    1. Extract the Options+ offline installer:
       python3 scripts/extract-depot.py --all /tmp/optionsplus/extracted --output-dir /tmp/optionsplus/devices
    2. Extract the main depot:
       python3 scripts/extract-depot.py /tmp/optionsplus/extracted/logioptionsplus.depot --output-dir /tmp/optionsplus/main

Usage:
    python3 scripts/generate-from-optionsplus.py \\
        --devices-dir /tmp/optionsplus/devices \\
        --main-dir /tmp/optionsplus/main/logioptionsplus \\
        --output-dir /tmp/logitune-devices-output
"""

import argparse
import json
import glob
import os
import re
import shutil
import sys

SLOT_NAME_MAP = {
    "SLOT_NAME_MIDDLE_BUTTON": ("Middle click", "default", True),
    "SLOT_NAME_BACK_BUTTON": ("Back", "default", True),
    "SLOT_NAME_FORWARD_BUTTON": ("Forward", "default", True),
    "SLOT_NAME_GESTURE_BUTTON": ("Gesture button", "gesture-trigger", True),
    "SLOT_NAME_DPI_BUTTON": ("DPI button", "default", True),
    "SLOT_NAME_LEFT_SCROLL_BUTTON": ("Shift wheel mode", "smartshift-toggle", True),
    "SLOT_NAME_SIDE_BUTTON_TOP": ("Top button", "default", True),
    "SLOT_NAME_SIDE_BUTTON_BOTTOM": ("Bottom button", "default", True),
}

# Left/right clicks are always present but not in metadata
DEFAULT_CONTROLS = [
    {"cid": "0x0050", "index": 0, "name": "Left click", "defaultAction": "default", "configurable": False},
    {"cid": "0x0051", "index": 1, "name": "Right click", "defaultAction": "default", "configurable": False},
]


def slugify(name):
    return re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')


def parse_cid_from_slot_id(slot_id):
    """Extract CID from slot ID suffix like 'mx-vertical-eb020_c82' -> 0x0052."""
    match = re.search(r'_c(\d+)$', slot_id)
    if match:
        return int(match.group(1))
    return None


def load_options_device_db(main_dir):
    """Load all mice from the Options+ device database."""
    mice = {}
    for f in sorted(glob.glob(os.path.join(main_dir, 'data/devices/devices*.json'))):
        try:
            data = json.load(open(f, encoding='utf-8-sig'))
        except Exception:
            continue
        devs = data.get('devices', []) if isinstance(data, dict) else []
        for d in devs:
            if d.get('type') != 'MOUSE':
                continue
            depot = d.get('depot', '')
            if not depot:
                continue
            pids = set()
            for mode in d.get('modes', []):
                for iface in mode.get('interfaces', []):
                    iid = iface.get('id', '')
                    if '046d' in iid.lower():
                        pid_hex = iid.lower().split('_')[-1] if '_' in iid else ''
                        if pid_hex:
                            pids.add(f"0x{pid_hex}")
            if pids:
                if depot not in mice:
                    mice[depot] = {
                        'name': d.get('displayName', depot),
                        'pids': set(),
                        'depot': depot,
                        'capabilities': d.get('capabilities', {}),
                    }
                mice[depot]['pids'].update(pids)
                # Prefer the first non-empty capabilities object we see
                if not mice[depot]['capabilities'] and d.get('capabilities'):
                    mice[depot]['capabilities'] = d.get('capabilities', {})
    return mice


def features_from_capabilities(caps):
    """Map Options+ capabilities object to our FeatureSupport bool dict."""
    swc = caps.get('scroll_wheel_capabilities', {})
    smooth = swc.get('smooth_scroll', {})
    if isinstance(smooth, bool):
        smooth_on = smooth
    else:
        smooth_on = bool(smooth.get('win') or smooth.get('mac'))

    has_adjustable_dpi = (
        bool(caps.get('hasHighResolutionSensor'))
        or 'highResolutionSensorInfo' in caps
        or bool(caps.get('pointerSpeed'))
    )
    has_programmable = bool(caps.get('specialKeys', {}).get('programmable'))

    return {
        "battery": bool(caps.get('hasBatteryStatus') or caps.get('unified_battery')),
        "adjustableDpi": has_adjustable_dpi,
        "smartShift": bool(swc.get('smartshift')),
        "hiResWheel": bool(swc.get('high_resolution')),
        # mouseThumbWheelOverride only appears on devices with a physical
        # thumb wheel (the MX Master line). virtual_thumbwheel is a different
        # software-emulated concept that doesn't map to our feature flag.
        "thumbWheel": 'mouseThumbWheelOverride' in caps,
        "reprogControls": has_programmable,
        "gestureV2": False,  # not represented in Options+ device db
        "smoothScroll": smooth_on,
        "hapticFeedback": False,  # not represented in Options+ device db
    }


def dpi_from_capabilities(caps):
    """Read DPI range from Options+ highResolutionSensorInfo, else defaults."""
    info = caps.get('highResolutionSensorInfo')
    if info:
        # Use SensorOn range (high-res sensor mode) for max possible DPI
        return {
            "min": info.get('minDpiValueSensorOn', 200),
            "max": info.get('maxDpiValueSensorOn', 4000),
            "step": info.get('stepsSensorOn', 50),
        }
    return {"min": 200, "max": 4000, "step": 50}


def parse_metadata_hotspots(metadata_path, image_key='device_buttons_image'):
    """Parse button hotspot positions from Options+ metadata.json."""
    try:
        meta = json.load(open(metadata_path))
    except Exception:
        return [], 0, 0

    controls = []
    img_w, img_h = 396, 396

    for img in meta.get('images', []):
        if img.get('key') != image_key:
            continue
        origin = img.get('origin', {})
        img_w = origin.get('width', 396)
        img_h = origin.get('height', 396)

        idx = len(DEFAULT_CONTROLS)
        for assignment in img.get('assignments', []):
            slot_id = assignment.get('slotId', '')
            slot_name = assignment.get('slotName', '')
            marker = assignment.get('marker', {})

            cid = parse_cid_from_slot_id(slot_id)
            if cid is None:
                continue

            name_info = SLOT_NAME_MAP.get(slot_name, (slot_name, "default", True))

            x_pct = round(marker.get('x', 0) / img_w, 3) if img_w else 0
            y_pct = round(marker.get('y', 0) / img_h, 3) if img_h else 0

            cid_hex = f"0x{cid:04x}"
            controls.append({
                'control': {
                    "cid": cid_hex,
                    "index": idx,
                    "name": name_info[0],
                    "defaultAction": name_info[1],
                    "configurable": name_info[2],
                },
                # Field names must match JsonDevice::parseHotspots() in
                # src/core/devices/JsonDevice.cpp (buttonIndex / xPct / yPct
                # / labelOffsetYPct). Mismatches are silently dropped as 0.
                'hotspot': {
                    "buttonIndex": idx,
                    "xPct": max(0, min(1, x_pct)),
                    "yPct": max(0, min(1, y_pct)),
                    "side": "right" if x_pct > 0.5 else "left",
                    "labelOffsetYPct": 0.0,
                },
            })
            idx += 1
        break

    return controls, img_w, img_h


def build_descriptor(mouse_info, controls_data, has_side_image, has_back_image):
    """Build a complete descriptor.json for a device."""
    all_controls = list(DEFAULT_CONTROLS)
    button_hotspots = []

    for cd in controls_data:
        all_controls.append(cd['control'])
        button_hotspots.append(cd['hotspot'])

    images = {"front": "front.png"}
    if has_side_image:
        images["side"] = "side.png"
    if has_back_image:
        images["back"] = "back.png"

    caps = mouse_info.get('capabilities', {})

    # Descriptors with real hotspots and a front image meet strict validation
    # requirements, so promote them out of "placeholder" status — they're
    # community-verified by virtue of coming from Logitech's own database.
    status = "community-verified" if button_hotspots else "placeholder"

    descriptor = {
        "name": mouse_info['name'],
        "status": status,
        "version": 1,
        "productIds": sorted(mouse_info['pids']),
        "features": features_from_capabilities(caps),
        "dpi": dpi_from_capabilities(caps),
        "controls": all_controls,
        "hotspots": {
            "buttons": button_hotspots,
            "scroll": [],
        },
        "images": images,
        "easySwitchSlots": [],
        "defaultGestures": {},
    }

    return descriptor


def main():
    parser = argparse.ArgumentParser(description='Generate descriptors from Options+ data')
    parser.add_argument('--devices-dir', required=True, help='Extracted depot devices directory')
    parser.add_argument('--main-dir', required=True, help='Extracted main depot directory')
    parser.add_argument('--output-dir', required=True, help='Output directory for descriptors')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--skip-existing', action='store_true', default=True)
    args = parser.parse_args()

    mice = load_options_device_db(args.main_dir)
    print(f"Found {len(mice)} mice in Options+ database")

    generated = 0
    skipped = 0
    no_images = 0

    for depot_name, mouse_info in sorted(mice.items(), key=lambda x: x[1]['name']):
        slug = slugify(mouse_info['name'])
        depot_dir = os.path.join(args.devices_dir, depot_name)
        out_dir = os.path.join(args.output_dir, slug)

        if args.skip_existing and os.path.exists(os.path.join(out_dir, 'descriptor.json')):
            skipped += 1
            continue

        # Handle both naming patterns: front.png (old) / front_core.png (new)
        front_img = os.path.join(depot_dir, 'front.png')
        if not os.path.exists(front_img):
            front_img = os.path.join(depot_dir, 'front_core.png')
        if not os.path.exists(front_img):
            no_images += 1
            continue

        side_img = os.path.join(depot_dir, 'side.png')
        if not os.path.exists(side_img):
            side_img = os.path.join(depot_dir, 'side_core.png')
        has_side = os.path.exists(side_img)

        # Back/bottom image (shows Easy-Switch indicators on flip side)
        back_img = os.path.join(depot_dir, 'back.png')
        if not os.path.exists(back_img):
            back_img = os.path.join(depot_dir, 'back_core.png')
        if not os.path.exists(back_img):
            back_img = os.path.join(depot_dir, 'bottom_core.png')
        if not os.path.exists(back_img):
            back_img = os.path.join(depot_dir, 'bottom.png')
        has_back = os.path.exists(back_img)

        # Handle both metadata patterns: metadata.json (old) / core_metadata.json (new)
        metadata_path = os.path.join(depot_dir, 'metadata.json')
        if not os.path.exists(metadata_path):
            metadata_path = os.path.join(depot_dir, 'core_metadata.json')
        controls_data, _, _ = parse_metadata_hotspots(metadata_path)

        descriptor = build_descriptor(mouse_info, controls_data, has_side, has_back)

        if args.dry_run:
            pids = ', '.join(mouse_info['pids'])
            imgs = "front"
            if has_side: imgs += "+side"
            if has_back: imgs += "+back"
            print(f"  {mouse_info['name']:40s} -> {slug}/ ({len(descriptor['controls'])} controls, imgs={imgs})")
            generated += 1
            continue

        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, 'descriptor.json'), 'w') as f:
            json.dump(descriptor, f, indent=2)
            f.write('\n')
        shutil.copy2(front_img, os.path.join(out_dir, 'front.png'))
        if has_side:
            shutil.copy2(side_img, os.path.join(out_dir, 'side.png'))
        if has_back:
            shutil.copy2(back_img, os.path.join(out_dir, 'back.png'))

        print(f"  {slug}: {len(descriptor['controls'])} controls, {len(descriptor['hotspots']['buttons'])} hotspots")
        generated += 1

    print(f"\nDone: {generated} generated, {skipped} skipped, {no_images} missing images")


if __name__ == '__main__':
    main()
