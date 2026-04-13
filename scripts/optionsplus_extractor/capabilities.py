"""Map Options+ capabilities dict to our features + dpi."""

from __future__ import annotations


def features_from_capabilities(caps: dict) -> dict:
    """Map an Options+ `capabilities` object to our 9-key feature subset.

    The parser defaults unset feature keys to false, except smoothScroll
    which defaults to true. We emit explicitly for the 9 keys we can
    determine from Options+ data.
    """
    swc = caps.get("scroll_wheel_capabilities", {}) or {}
    smooth = swc.get("smooth_scroll", {})
    if isinstance(smooth, bool):
        smooth_on = smooth
    else:
        smooth_on = bool((smooth or {}).get("win") or (smooth or {}).get("mac"))

    has_adjustable_dpi = (
        bool(caps.get("hasHighResolutionSensor"))
        or "highResolutionSensorInfo" in caps
        or bool(caps.get("pointerSpeed"))
    )
    has_programmable = bool((caps.get("specialKeys") or {}).get("programmable"))

    return {
        "battery": bool(
            caps.get("hasBatteryStatus") or caps.get("unified_battery")
        ),
        "adjustableDpi": has_adjustable_dpi,
        "smartShift": bool(swc.get("smartshift")),
        "hiResWheel": bool(swc.get("high_resolution")),
        "thumbWheel": "mouseThumbWheelOverride" in caps,
        "reprogControls": has_programmable,
        "smoothScroll": smooth_on if caps else True,
        "gestureV2": False,       # not represented in Options+ DB
        "hapticFeedback": False,  # not represented in Options+ DB
    }


def dpi_from_capabilities(caps: dict) -> dict:
    """Read DPI range from highResolutionSensorInfo; fall back to safe defaults."""
    info = caps.get("highResolutionSensorInfo")
    if info:
        return {
            "min": info.get("minDpiValueSensorOn", 200),
            "max": info.get("maxDpiValueSensorOn", 4000),
            "step": info.get("stepsSensorOn", 50),
        }
    return {"min": 200, "max": 4000, "step": 50}
