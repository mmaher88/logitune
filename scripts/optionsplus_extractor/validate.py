"""Layer 1 schema self-check: verify generated dict matches JsonDevice parser expectations."""

from __future__ import annotations

from typing import Optional

from .sources import Depot


class SchemaError(Exception):
    """Raised when a generated descriptor doesn't match the parser's field expectations."""


_ALLOWED_SCROLL_KINDS = {"", "scrollwheel", "thumbwheel", "pointer"}

_REQUIRED_CONTROL_KEYS = {"controlId", "buttonIndex", "defaultName", "defaultActionType"}
_REQUIRED_HOTSPOT_KEYS = {"buttonIndex", "xPct", "yPct", "side"}


def check(descriptor: dict, depot: Optional[Depot]) -> None:
    """Validate a generated descriptor dict against JsonDevice.cpp expectations.

    Raises SchemaError with a specific message on the first mismatch.
    `depot` is only used to verify referenced images exist; pass None
    to skip that check (used by tests that build descriptors without
    real image files).
    """
    _check_top_level(descriptor)
    _check_controls(descriptor.get("controls", []))
    _check_hotspots(descriptor.get("hotspots", {}))
    _check_images(descriptor.get("images", {}), depot)


def _check_top_level(d: dict) -> None:
    if not d.get("name"):
        raise SchemaError("missing or empty 'name'")
    pids = d.get("productIds") or []
    if not isinstance(pids, list) or not pids:
        raise SchemaError("productIds must be a non-empty list")
    for pid in pids:
        if not isinstance(pid, str) or not pid.startswith("0x"):
            raise SchemaError(f"productIds entry {pid!r} is not a hex string")
        try:
            int(pid, 16)
        except ValueError:
            raise SchemaError(f"productIds entry {pid!r} is not a valid hex int")


def _check_controls(controls: list) -> None:
    for i, c in enumerate(controls):
        if not isinstance(c, dict):
            raise SchemaError(f"controls[{i}] is not an object")
        missing = _REQUIRED_CONTROL_KEYS - set(c.keys())
        if missing:
            raise SchemaError(
                f"controls[{i}] missing required keys {sorted(missing)} "
                f"(got keys: {sorted(c.keys())})"
            )
        cid = c["controlId"]
        if not isinstance(cid, str) or not cid.startswith("0x"):
            raise SchemaError(f"controls[{i}].controlId {cid!r} is not a hex string")
        try:
            int(cid, 16)
        except ValueError:
            raise SchemaError(f"controls[{i}].controlId {cid!r} is not valid hex")
        if not isinstance(c["buttonIndex"], int):
            raise SchemaError(f"controls[{i}].buttonIndex must be int")


def _check_hotspots(hotspots: dict) -> None:
    for group in ("buttons", "scroll"):
        entries = hotspots.get(group, [])
        for i, h in enumerate(entries):
            if not isinstance(h, dict):
                raise SchemaError(f"hotspots.{group}[{i}] is not an object")
            missing = _REQUIRED_HOTSPOT_KEYS - set(h.keys())
            if missing:
                raise SchemaError(
                    f"hotspots.{group}[{i}] missing required keys {sorted(missing)}"
                )
            if not isinstance(h["xPct"], (int, float)):
                raise SchemaError(f"hotspots.{group}[{i}].xPct must be numeric")
            if not (0.0 <= h["xPct"] <= 1.0 and 0.0 <= h["yPct"] <= 1.0):
                raise SchemaError(
                    f"hotspots.{group}[{i}] coords out of range [0,1]: "
                    f"({h['xPct']}, {h['yPct']})"
                )
            if group == "scroll":
                kind = h.get("kind", "")
                if kind not in _ALLOWED_SCROLL_KINDS:
                    raise SchemaError(
                        f"hotspots.scroll[{i}].kind {kind!r} not in "
                        f"{sorted(_ALLOWED_SCROLL_KINDS - {''})}"
                    )


def _check_images(images: dict, depot: Optional[Depot]) -> None:
    if depot is None:
        return
    front = images.get("front")
    if front and depot.front_image is None:
        raise SchemaError("images.front set but depot has no front image")
