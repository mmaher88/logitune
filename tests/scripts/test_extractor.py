"""Tests for scripts/optionsplus_extractor/. See spec
docs/superpowers/specs/2026-04-13-optionsplus-extractor-design.md."""

import importlib


def test_package_imports():
    mod = importlib.import_module("optionsplus_extractor")
    assert mod.__doc__ is not None


def test_all_submodules_import():
    for name in ("sources", "capabilities", "slots", "canonicalize",
                 "descriptor", "validate", "cli"):
        importlib.import_module(f"optionsplus_extractor.{name}")


from pathlib import Path
from optionsplus_extractor import sources

FIXTURE_ROOT = Path(__file__).parent / "fixtures" / "optionsplus"


def test_load_device_db_returns_two_mice():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    assert set(mice.keys()) == {"mx_master_2s", "mx_master_3s"}


def test_device_db_entry_has_expected_fields():
    mice = sources.load_device_db(FIXTURE_ROOT / "main" / "logioptionsplus")
    m3s = mice["mx_master_3s"]
    assert m3s.name == "MX Master 3S"
    # 3S has two hardware revisions with PIDs b034 and b043
    assert "0xb034" in m3s.product_ids
    assert isinstance(m3s.capabilities, dict)
    assert m3s.capabilities.get("hasHighResolutionSensor") is True


def test_load_depot_finds_metadata_and_images():
    depot = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    assert depot.metadata is not None
    assert "images" in depot.metadata
    assert depot.front_image is not None
    assert depot.front_image.exists()


def test_load_depot_handles_2s_bottom_image_as_back():
    # 2S has bottom_core.png as its back image fallback; 3S has back_core.png.
    # Both should resolve via the back_image field.
    depot_2s = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_2s")
    depot_3s = sources.load_depot(FIXTURE_ROOT / "devices" / "mx_master_3s")
    assert depot_2s.back_image is not None and depot_2s.back_image.name == "bottom_core.png"
    assert depot_3s.back_image is not None and depot_3s.back_image.name == "back_core.png"
