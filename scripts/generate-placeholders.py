#!/usr/bin/env python3
"""Generate placeholder device descriptors from Solaar's device database.

Scans Solaar's installed descriptors.py for mouse devices, cross-references
CID names from special_keys.py, and outputs placeholder JSON descriptors
to the devices/ directory.

Usage:
    python3 scripts/generate-placeholders.py [--output-dir devices/]

Requires: Solaar installed (logitech_receiver package available on PYTHONPATH)
"""

import argparse
import json
import os
import re
import sys
import inspect

def slugify(name):
    """Convert device name to directory slug: 'MX Master 3S' -> 'mx-master-3s'"""
    return re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')

def parse_solaar_devices():
    """Extract mouse device entries from Solaar's descriptors.py."""
    try:
        from logitech_receiver import descriptors
    except ImportError:
        print("Error: Solaar not installed. Install with: pip install solaar", file=sys.stderr)
        sys.exit(1)

    src = inspect.getsource(descriptors)
    devices = []

    for match in re.finditer(r'_D\("([^"]*)",\s*(.*?)\)\s*$', src, re.MULTILINE):
        name = match.group(1)
        params = match.group(2)

        # Skip non-mice
        is_mouse = any(kw in name.lower() for kw in ['mouse', 'trackball', 'mx master', 'mx anywhere', 'pebble'])
        if not is_mouse:
            continue

        # Extract PIDs
        pids = []
        for pid_match in re.finditer(r'wpid="(\w+)"', params):
            pids.append(f"0x{pid_match.group(1)}")
        for pid_match in re.finditer(r'btid=0x(\w+)', params):
            pids.append(f"0x{pid_match.group(1)}")
        for pid_match in re.finditer(r'usbid=0x(\w+)', params):
            pids.append(f"0x{pid_match.group(1)}")

        if not pids:
            continue

        # Extract protocol version
        proto_match = re.search(r'protocol=(\d+\.\d+)', params)
        protocol = float(proto_match.group(1)) if proto_match else 0.0

        # Extract codename
        codename_match = re.search(r'codename="([^"]*)"', params)
        codename = codename_match.group(1) if codename_match else name

        devices.append({
            'name': name,
            'codename': codename,
            'pids': pids,
            'protocol': protocol,
            'hidpp2': protocol >= 2.0,
        })

    return devices

def get_common_mouse_cids():
    """Get standard mouse CID names from Solaar's special_keys.py."""
    try:
        from logitech_receiver import special_keys
    except ImportError:
        return {}

    cid_names = {}
    if hasattr(special_keys, 'CONTROL'):
        for entry in special_keys.CONTROL:
            cid_names[int(entry)] = str(entry)

    return cid_names

def build_placeholder(device, cid_names):
    """Build a placeholder JSON descriptor for a device."""
    # Standard mouse controls (most Logitech mice share these)
    standard_controls = [
        {"cid": "0x0050", "index": 0, "name": "Left click", "defaultAction": "default", "configurable": False},
        {"cid": "0x0051", "index": 1, "name": "Right click", "defaultAction": "default", "configurable": False},
        {"cid": "0x0052", "index": 2, "name": "Middle click", "defaultAction": "default", "configurable": True},
    ]

    # Common side buttons (most productivity mice have these)
    if device['hidpp2']:
        standard_controls.extend([
            {"cid": "0x0053", "index": 3, "name": "Back", "defaultAction": "default", "configurable": True},
            {"cid": "0x0056", "index": 4, "name": "Forward", "defaultAction": "default", "configurable": True},
        ])

    descriptor = {
        "name": device['name'],
        "status": "placeholder",
        "productIds": device['pids'],
        "features": {
            "battery": device['hidpp2'],
            "adjustableDpi": device['hidpp2'],
            "smartShift": False,
            "hiResWheel": device['hidpp2'],
            "thumbWheel": False,
            "reprogControls": device['hidpp2'],
            "gestureV2": False,
            "smoothScroll": True,
            "hapticFeedback": False,
        },
        "dpi": {"min": 200, "max": 4000, "step": 50},
        "controls": standard_controls,
        "hotspots": {"buttons": [], "scroll": []},
        "images": {"front": "front.png"},
        "easySwitchSlots": [],
        "defaultGestures": {},
    }

    return descriptor

def main():
    parser = argparse.ArgumentParser(description='Generate placeholder device descriptors from Solaar')
    parser.add_argument('--output-dir', default='devices', help='Output directory (default: devices/)')
    parser.add_argument('--dry-run', action='store_true', help='Print what would be created without writing')
    parser.add_argument('--skip-existing', action='store_true', default=True, help='Skip devices that already have descriptors')
    args = parser.parse_args()

    devices = parse_solaar_devices()
    cid_names = get_common_mouse_cids()

    print(f"Found {len(devices)} mice in Solaar database")

    created = 0
    skipped = 0

    for device in sorted(devices, key=lambda d: d['name']):
        slug = slugify(device['name'])
        dir_path = os.path.join(args.output_dir, slug)
        json_path = os.path.join(dir_path, 'descriptor.json')

        if args.skip_existing and os.path.exists(json_path):
            skipped += 1
            continue

        descriptor = build_placeholder(device, cid_names)

        if args.dry_run:
            print(f"  Would create: {dir_path}/")
            print(f"    {device['name']} PIDs={device['pids']} HID++2.0={device['hidpp2']}")
        else:
            os.makedirs(dir_path, exist_ok=True)
            with open(json_path, 'w') as f:
                json.dump(descriptor, f, indent=2)
                f.write('\n')
            print(f"  Created: {dir_path}/descriptor.json")
            created += 1

    print(f"\nDone: {created} created, {skipped} skipped (already exist)")

if __name__ == '__main__':
    main()
