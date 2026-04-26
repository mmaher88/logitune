"""Assemble the final descriptor dict with parser-compatible field names."""

from __future__ import annotations

from . import sources
from .sources import DeviceDbEntry, Depot
from .capabilities import features_from_capabilities, dpi_from_capabilities
from . import slots as slots_mod
from . import canonicalize


_DEFAULT_CONTROLS = [
    {
        "controlId": "0x0050",
        "buttonIndex": 0,
        "defaultName": "Left click",
        "defaultActionType": "default",
        "configurable": False,
    },
    {
        "controlId": "0x0051",
        "buttonIndex": 1,
        "defaultName": "Right click",
        "defaultActionType": "default",
        "configurable": False,
    },
]


def build(entry: DeviceDbEntry, depot: Depot) -> dict:
    """Assemble a descriptor dict for one device.

    Raises `slots.UnknownSlotName` if the metadata contains a slot name
    not in the mapping table.
    """
    back_image_aspect = None
    if depot.back_image is not None:
        dims = sources.read_png_dimensions(depot.back_image)
        if dims is not None and dims[0] > 0:
            back_image_aspect = dims[1] / dims[0]  # height / width

    parsed = slots_mod.parse(depot.metadata, back_image_aspect=back_image_aspect)

    ordered_buttons = canonicalize.sort_buttons(parsed.buttons)
    ordered_scroll = canonicalize.sort_scroll(parsed.scroll)
    ordered_easyswitch = canonicalize.sort_easyswitch(parsed.easyswitch)

    controls: list[dict] = list(_DEFAULT_CONTROLS)
    button_hotspots: list[dict] = []
    for idx_offset, b in enumerate(ordered_buttons):
        button_index = len(_DEFAULT_CONTROLS) + idx_offset
        controls.append({
            "controlId": f"0x{b.cid:04X}",
            "buttonIndex": button_index,
            "defaultName": b.name,
            "defaultActionType": b.action_type,
            "configurable": b.configurable,
        })
        button_hotspots.append({
            "buttonIndex": button_index,
            "xPct": b.x_pct,
            "yPct": b.y_pct,
            "side": "right" if b.x_pct > 0.5 else "left",
            "labelOffsetYPct": 0.0,
        })

    scroll_hotspots: list[dict] = []
    for slot_index, s in enumerate(ordered_scroll, start=1):
        scroll_hotspots.append({
            "kind": s.kind,
            "buttonIndex": -slot_index,
            "xPct": s.x_pct,
            "yPct": s.y_pct,
            "side": "right" if s.x_pct > 0.5 else "left",
            "labelOffsetYPct": 0.0,
        })

    easy_switch = [
        {"xPct": s.x_pct, "yPct": s.y_pct} for s in ordered_easyswitch
    ]

    images: dict[str, str] = {}
    if depot.front_image is not None:
        images["front"] = "front.png"
    if depot.side_image is not None:
        images["side"] = "side.png"
    if depot.back_image is not None:
        images["back"] = "back.png"

    has_buttons = len(button_hotspots) > 0
    status = "community-verified" if (has_buttons and "front" in images) else "placeholder"

    return {
        "name": entry.name,
        "status": status,
        "version": 1,
        "productIds": sorted(entry.product_ids),
        "features": features_from_capabilities(entry.capabilities),
        "dpi": dpi_from_capabilities(entry.capabilities),
        "controls": controls,
        "hotspots": {
            "buttons": button_hotspots,
            "scroll": scroll_hotspots,
        },
        "images": images,
        "easySwitchSlots": easy_switch,
        "defaultGestures": {},
    }
