#!/usr/bin/env python3
"""Extract files from Logitech Options+ .depot container format.

Format: 4-byte magic (0x20170110) | 4-byte JSON header length | JSON header | files...
Each file: 4-byte little-endian size | raw file data

Usage:
    python3 scripts/extract-depot.py <depot-file> [--output-dir <dir>]
    python3 scripts/extract-depot.py --all <depot-dir> [--output-dir <dir>]
"""

import argparse
import json
import os
import struct
import sys

def extract_depot(depot_path, output_dir):
    """Extract all files from a .depot container."""
    with open(depot_path, 'rb') as f:
        magic = struct.unpack('<I', f.read(4))[0]
        if magic != 0x20170110:
            print(f"  Skip: not a depot file (magic 0x{magic:08x})", file=sys.stderr)
            return []

        json_len = struct.unpack('<I', f.read(4))[0]
        header = json.loads(f.read(json_len))
        files = header.get('files', [])

        os.makedirs(output_dir, exist_ok=True)
        extracted = []

        for entry in files:
            name = entry['name']
            size = struct.unpack('<I', f.read(4))[0]
            data = f.read(size)

            out_path = os.path.join(output_dir, name)
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            with open(out_path, 'wb') as out:
                out.write(data)
            extracted.append(name)

    return extracted

def main():
    parser = argparse.ArgumentParser(description='Extract Logitech Options+ .depot files')
    parser.add_argument('input', help='Single .depot file or directory (with --all)')
    parser.add_argument('--all', action='store_true', help='Extract all .depot files in directory')
    parser.add_argument('--output-dir', default='extracted', help='Output directory')
    parser.add_argument('--images-only', action='store_true', help='Only extract PNG/JPG/GIF files')
    parser.add_argument('--list', action='store_true', help='List contents without extracting')
    args = parser.parse_args()

    if args.all:
        depot_dir = args.input
        depot_files = [os.path.join(depot_dir, f) for f in os.listdir(depot_dir) if f.endswith('.depot')]
    else:
        depot_files = [args.input]

    total_files = 0
    total_images = 0

    for depot_path in sorted(depot_files):
        depot_name = os.path.basename(depot_path).replace('.depot', '')

        try:
            with open(depot_path, 'rb') as f:
                magic = struct.unpack('<I', f.read(4))[0]
                if magic != 0x20170110:
                    continue
                json_len = struct.unpack('<I', f.read(4))[0]
                header = json.loads(f.read(json_len))
        except Exception:
            continue

        files = header.get('files', [])
        has_front = any(f['name'] == 'front.png' for f in files)
        has_metadata = any(f['name'] == 'metadata.json' for f in files)

        if args.list:
            if has_front or has_metadata:
                file_names = [f['name'] for f in files]
                img_count = sum(1 for n in file_names if n.endswith(('.png', '.jpg', '.gif')))
                print(f"{depot_name}: {len(files)} files ({img_count} images)")
                if has_metadata:
                    print(f"  has metadata.json")
            continue

        if args.images_only and not has_front:
            continue

        out_dir = os.path.join(args.output_dir, depot_name)
        extracted = extract_depot(depot_path, out_dir)

        if args.images_only:
            for name in extracted:
                if not name.endswith(('.png', '.jpg', '.gif')):
                    os.remove(os.path.join(out_dir, name))

        img_count = sum(1 for n in extracted if n.endswith(('.png', '.jpg', '.gif')))
        total_files += len(extracted)
        total_images += img_count

        if extracted:
            print(f"  {depot_name}: {len(extracted)} files ({img_count} images)")

    print(f"\nTotal: {total_files} files, {total_images} images")

if __name__ == '__main__':
    main()
