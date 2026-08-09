#!/usr/bin/env python3
"""Generate pet UI sprite assets for STOP-mode pet page.

Inputs:
  - Assets/pet/pet_assets.json   (menu icons + action rows, optional state fallback sprites)
  - Assets/pet/pet_sheet.json    (2bpp animation frames/clips)

Output:
  - Core/Inc/ui/ui_pet_assets_autogen.h
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
ASSET_ROOT = ROOT / "Assets" / "pet"
MANIFEST_PATH = ASSET_ROOT / "pet_assets.json"
SHEET_MANIFEST_PATH = ASSET_ROOT / "pet_sheet.json"
OUT_HEADER = ROOT / "Core" / "Inc" / "ui" / "ui_pet_assets_autogen.h"

SPR_W = 16
SPR_H = 16
SPR_STRIDE = 2

PET_STATE_NAME_TO_ID: dict[str, int] = {
    "sleep": 0,
    "idle": 1,
    "feed": 2,
    "play": 3,
    "rest": 4,
}

FALLBACK_ICON_ROWS: dict[str, tuple[int, ...]] = {
    "feed": (0x00, 0x18, 0x24, 0x7E, 0x42, 0x3C, 0x00, 0x00),
    "play": (0x10, 0x18, 0x1C, 0x1E, 0x1C, 0x18, 0x10, 0x00),
    "start_game": (0x3C, 0x42, 0x5A, 0x66, 0x66, 0x5A, 0x42, 0x3C),
    "options": (0x24, 0x18, 0x7E, 0x3C, 0x7E, 0x18, 0x24, 0x00),
}

FALLBACK_STATE_ROWS: dict[str, tuple[int, ...]] = {
    "sleep": (0x3C, 0x42, 0x81, 0xB1, 0x89, 0x85, 0x42, 0x3C),
    "idle": (0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C),
    "feed": (0x3C, 0x42, 0xA5, 0x81, 0xBD, 0x81, 0x42, 0x3C),
    "play": (0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C),
    "rest": (0x3C, 0x42, 0x81, 0xBD, 0xA5, 0x81, 0x42, 0x3C),
}

FALLBACK_SELECTION_BG_ROWS: tuple[int, ...] = (
    0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C
)

DEFAULT_MANIFEST: dict[str, Any] = {
    "icons": {
        "feed": "icons/feed.png",
        "play": "icons/play.png",
        "start_game": "icons/start.png",
        "options": "icons/options.png",
    },
    "selection_bg": "icons/selection_bg_16.png",
    "states": {
        "sleep": "states/sleep.png",
        "idle": "states/idle.png",
        "feed": "states/feed.png",
        "play": "states/play.png",
        "rest": "states/rest.png",
    },
    "rows": {
        "top": ["feed", "play"],
        "bottom": ["start_game", "options"],
    },
    "pet_state_map": {
        "0": "sleep",
        "1": "idle",
        "2": "feed",
        "3": "play",
        "4": "rest",
    },
}


def _c_ident(name: str) -> str:
    ident = re.sub(r"[^a-zA-Z0-9_]", "_", name)
    if not ident:
        return "id"
    if ident[0].isdigit():
        ident = "_" + ident
    return ident


def _c_ident_upper(name: str) -> str:
    return _c_ident(name).upper()


def _emit_u8_array(name: str, data: bytes) -> str:
    lines = [f"static const uint8_t {name}[] = {{"]
    if not data:
        lines.append("  0x00")
    else:
        row = []
        for i, b in enumerate(data):
            row.append(f"0x{b:02X}")
            if len(row) == 16 or i == (len(data) - 1):
                lines.append("  " + ", ".join(row) + ",")
                row = []
    lines.append("};")
    return "\n".join(lines)


def _resolve_repo_path(path_text: str) -> Path:
    p = Path(path_text)
    if p.is_absolute():
        return p
    return ROOT / p


def _fallback_mask(rows: tuple[int, ...]) -> bytes:
    src_h = len(rows)
    if src_h <= 0:
        raise ValueError("fallback rows must not be empty")

    src_w = 8
    for v in rows:
        if v > 0xFF:
            src_w = 16
            break

    out = bytearray(SPR_H * SPR_STRIDE)
    for y in range(SPR_H):
        sy = (y * src_h) // SPR_H
        row_bits = int(rows[sy])
        for x in range(SPR_W):
            sx = (x * src_w) // SPR_W
            src_bit = (row_bits >> (src_w - 1 - sx)) & 0x1
            if src_bit == 0:
                continue
            idx = y * SPR_STRIDE + (x >> 3)
            out[idx] |= (1 << (7 - (x & 7)))

    return bytes(out)


def _fallback_on() -> bytes:
    return bytes([0x00] * (SPR_H * SPR_STRIDE))


def _load_png_1bpp_or_none(path: Path) -> tuple[bytes, bytes] | None:
    if not path.exists():
        return None
    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError(
            f"Pillow is required to read {path}. Install with: pip install pillow"
        ) from exc

    img = Image.open(path).convert("RGBA")
    if img.size != (SPR_W, SPR_H):
        raise ValueError(f"{path} must be {SPR_W}x{SPR_H}, got {img.size[0]}x{img.size[1]}")

    on = bytearray(SPR_H * SPR_STRIDE)
    mask = bytearray(SPR_H * SPR_STRIDE)
    for y in range(SPR_H):
        for x in range(SPR_W):
            r, g, b, a = img.getpixel((x, y))
            if a == 0:
                continue
            bit = 1 << (7 - (x & 7))
            idx = y * SPR_STRIDE + (x >> 3)
            mask[idx] |= bit
            lum = (299 * r + 587 * g + 114 * b) // 1000
            if lum >= 128:
                on[idx] |= bit
    return bytes(on), bytes(mask)


def _pack_cell_1bpp(img_rgba, x0: int, y0: int, w: int, h: int) -> tuple[bytes, bytes]:
    if (w != SPR_W) or (h != SPR_H):
        raise ValueError(f"icon sheet cells must be {SPR_W}x{SPR_H}")

    on = bytearray(h * SPR_STRIDE)
    mask = bytearray(h * SPR_STRIDE)
    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x0 + x, y0 + y))
            if a == 0:
                continue
            bit = 1 << (7 - (x & 7))
            idx = y * SPR_STRIDE + (x >> 3)
            mask[idx] |= bit
            lum = (299 * r + 587 * g + 114 * b) // 1000
            if lum >= 128:
                on[idx] |= bit
    return bytes(on), bytes(mask)


def _sprite1_block(prefix: str, asset_name: str, on: bytes, mask: bytes) -> tuple[str, str]:
    token = _c_ident(asset_name).lower()
    on_name = f"{prefix}_{token}_on"
    mask_name = f"{prefix}_{token}_mask"
    spr_name = f"{prefix}_{token}_spr"
    block = "\n\n".join(
        (
            _emit_u8_array(on_name, on),
            _emit_u8_array(mask_name, mask),
            (
                f"static const sprite1_t {spr_name} = {{\n"
                f"  .w = {SPR_W}u,\n"
                f"  .h = {SPR_H}u,\n"
                f"  .stride = {SPR_STRIDE}u,\n"
                f"  .on = {on_name},\n"
                f"  .mask = {mask_name},\n"
                f"  .leftmost_is_msb = true\n"
                f"}};"
            ),
        )
    )
    return block, spr_name


def _load_manifest(path: Path) -> dict[str, Any]:
    if not path.exists():
        print(f"Manifest missing at {path}, using built-in defaults.")
        return DEFAULT_MANIFEST

    loaded = json.loads(path.read_text(encoding="utf-8"))
    for key in ("states", "rows", "pet_state_map"):
        if key not in loaded:
            raise ValueError(f"Manifest missing required key: {key}")
    if ("icons" not in loaded) and ("icon_sheet" not in loaded):
        raise ValueError("Manifest must contain either 'icons' or 'icon_sheet'")
    if "top" not in loaded["rows"] or "bottom" not in loaded["rows"]:
        raise ValueError("Manifest rows must contain 'top' and 'bottom' lists")
    return loaded


def _load_icon_sheet(manifest: dict[str, Any], top_row: list[str], bottom_row: list[str]) -> tuple[dict[str, tuple[bytes, bytes]], tuple[bytes, bytes] | None]:
    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError("Pillow is required. Install with: pip install pillow") from exc

    sheet = manifest["icon_sheet"]
    png_path = _resolve_repo_path(sheet["png"])
    cell_w = int(sheet["cell_w"])
    cell_h = int(sheet["cell_h"])
    cols = int(sheet.get("cols", 0))
    rows = int(sheet.get("rows", 0))
    icon_cells = sheet["icons"]

    if (cell_w <= 0) or (cell_h <= 0):
        raise ValueError("icon_sheet cell_w/cell_h must be > 0")

    img = Image.open(png_path).convert("RGBA")
    if cols > 0 and rows > 0:
        expected_w = cols * cell_w
        expected_h = rows * cell_h
        if img.size != (expected_w, expected_h):
            raise ValueError(
                f"Icon sheet size mismatch for {png_path}: got {img.size[0]}x{img.size[1]}, "
                f"expected {expected_w}x{expected_h}"
            )

    action_ids: list[str] = []
    for row_action in top_row + bottom_row:
        if row_action not in action_ids:
            action_ids.append(row_action)

    bitmaps: dict[str, tuple[bytes, bytes]] = {}
    for action_id in action_ids:
        if action_id not in icon_cells:
            raise ValueError(f"icon_sheet missing icon cell for action '{action_id}'")
        pos = icon_cells[action_id]
        gx = int(pos["x"])
        gy = int(pos["y"])
        x0 = gx * cell_w
        y0 = gy * cell_h
        if x0 < 0 or y0 < 0 or x0 + cell_w > img.size[0] or y0 + cell_h > img.size[1]:
            raise ValueError(f"icon_sheet cell out of bounds for action '{action_id}'")
        bitmaps[action_id] = _pack_cell_1bpp(img, x0, y0, cell_w, cell_h)

    selection_bg: tuple[bytes, bytes] | None = None
    sel_pos = sheet.get("selection_bg")
    if isinstance(sel_pos, dict):
        gx = int(sel_pos["x"])
        gy = int(sel_pos["y"])
        x0 = gx * cell_w
        y0 = gy * cell_h
        if x0 < 0 or y0 < 0 or x0 + cell_w > img.size[0] or y0 + cell_h > img.size[1]:
            raise ValueError("icon_sheet selection_bg cell out of bounds")
        selection_bg = _pack_cell_1bpp(img, x0, y0, cell_w, cell_h)

    return bitmaps, selection_bg


def _manifest_state_default(manifest: dict[str, Any], state_ids: list[str]) -> str:
    mapped_idle = manifest.get("pet_state_map", {}).get("1")
    if isinstance(mapped_idle, str) and mapped_idle in state_ids:
        return mapped_idle
    if "idle" in state_ids:
        return "idle"
    return state_ids[0]


def _load_sheet_manifest(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise ValueError(f"Sheet manifest missing: {path}")
    m = json.loads(path.read_text(encoding="utf-8"))
    for key in ("sheet", "frames", "clips", "state_to_clip"):
        if key not in m:
            raise ValueError(f"Sheet manifest missing required key: {key}")
    return m


def _clip_tick_domain_symbol(tick_domain_name: str) -> tuple[str, str]:
    name = str(tick_domain_name).strip().lower()
    if name == "rtc_1hz":
        name = "stop_wake_1hz"
    if name == "stop_wake_1hz":
        return "UI_PET_ANIM_TICK_STOP_WAKE_1HZ", name
    if name == "active":
        return "UI_PET_ANIM_TICK_ACTIVE", name
    if name == "stop_select_2hz":
        return "UI_PET_ANIM_TICK_STOP_SELECT_2HZ", name
    raise ValueError(f"Unsupported tick_domain '{tick_domain_name}'")


def _clip_frame_ms_list(clip_name: str, clip_def: dict[str, Any], frame_count: int, tick_domain_name: str) -> list[int]:
    def _to_pos_int(v: Any, what: str) -> int:
        i = int(v)
        if i <= 0:
            raise ValueError(f"Clip '{clip_name}' {what} must be > 0")
        return i

    if "frame_ms" in clip_def:
        frame_ms = clip_def["frame_ms"]
        if isinstance(frame_ms, list):
            vals = [_to_pos_int(v, "frame_ms[]") for v in frame_ms]
            if len(vals) == 1:
                return vals * frame_count
            if len(vals) != frame_count:
                raise ValueError(f"Clip '{clip_name}' frame_ms count mismatch")
            return vals
        return [_to_pos_int(frame_ms, "frame_ms")] * frame_count

    if "dur_ticks" in clip_def:
        # Legacy compatibility: convert clip ticks to milliseconds.
        # active historically advanced near 50Hz in this project (about 20ms per tick).
        legacy_tick_ms = 20
        if tick_domain_name == "stop_wake_1hz":
            legacy_tick_ms = 1000
        elif tick_domain_name == "stop_select_2hz":
            legacy_tick_ms = 500

        durs = list(clip_def["dur_ticks"])
        if len(durs) != frame_count:
            raise ValueError(f"Clip '{clip_name}' dur_ticks count mismatch")
        return [_to_pos_int(v, "dur_ticks[]") * legacy_tick_ms for v in durs]

    raise ValueError(f"Clip '{clip_name}' must define frame_ms (preferred) or dur_ticks (legacy)")


def _pack_frame_2bpp(img_rgba, x0: int, y0: int, w: int, h: int) -> tuple[bytes, bytes]:
    color_stride = (w + 3) // 4
    mask_stride = (w + 7) // 8
    color = bytearray(h * color_stride)
    mask = bytearray(h * mask_stride)

    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x0 + x, y0 + y))
            if a == 0:
                continue

            # mask bit (leftmost pixel -> bit 7)
            mask_idx = y * mask_stride + (x >> 3)
            mask_bit = 1 << (7 - (x & 7))
            mask[mask_idx] |= mask_bit

            # map luminance to 2bpp level: 0 white .. 3 black
            lum = (299 * r + 587 * g + 114 * b) // 1000
            if lum >= 192:
                level = 0
            elif lum >= 128:
                level = 1
            elif lum >= 64:
                level = 2
            else:
                level = 3

            color_idx = y * color_stride + (x >> 2)
            shift = 6 - ((x & 3) * 2)
            color[color_idx] |= (level & 0x03) << shift

    return bytes(color), bytes(mask)


def _sheet_sprite2_blocks(sheet_manifest: dict[str, Any]) -> tuple[str, dict[str, str], dict[str, str], int, int, int, int]:
    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError("Pillow is required. Install with: pip install pillow") from exc

    sheet = sheet_manifest["sheet"]
    frames = sheet_manifest["frames"]
    png_path = _resolve_repo_path(sheet["png"])
    cell_w = int(sheet["cell_w"])
    cell_h = int(sheet["cell_h"])
    cols = int(sheet.get("cols", 0))
    rows = int(sheet.get("rows", 0))
    if cell_w <= 0 or cell_h <= 0:
        raise ValueError("sheet cell_w/cell_h must be > 0")

    img = Image.open(png_path).convert("RGBA")
    if cols > 0 and rows > 0:
        expected_w = cols * cell_w
        expected_h = rows * cell_h
        if img.size != (expected_w, expected_h):
            raise ValueError(
                f"Sheet size mismatch for {png_path}: got {img.size[0]}x{img.size[1]}, "
                f"expected {expected_w}x{expected_h}"
            )

    blocks: list[str] = []
    sprite_symbol_by_frame: dict[str, str] = {}
    source_info: dict[str, str] = {}
    color_stride = (cell_w + 3) // 4
    mask_stride = (cell_w + 7) // 8

    for frame_name, frame_pos in frames.items():
        gx = int(frame_pos["x"])
        gy = int(frame_pos["y"])
        x0 = gx * cell_w
        y0 = gy * cell_h
        if x0 < 0 or y0 < 0 or x0 + cell_w > img.size[0] or y0 + cell_h > img.size[1]:
            raise ValueError(f"Frame '{frame_name}' cell out of sheet bounds")

        color_bytes, mask_bytes = _pack_frame_2bpp(img, x0, y0, cell_w, cell_h)
        token = _c_ident(frame_name).lower()
        color_name = f"ui_pet_frame_{token}_color2bpp"
        mask_name = f"ui_pet_frame_{token}_mask"
        spr_name = f"ui_pet_frame_{token}_spr"
        block = "\n\n".join(
            (
                _emit_u8_array(color_name, color_bytes),
                _emit_u8_array(mask_name, mask_bytes),
                (
                    f"static const sprite2_t {spr_name} = {{\n"
                    f"  .w = {cell_w}u,\n"
                    f"  .h = {cell_h}u,\n"
                    f"  .color_stride = {color_stride}u,\n"
                    f"  .mask_stride = {mask_stride}u,\n"
                    f"  .color2bpp = {color_name},\n"
                    f"  .mask = {mask_name},\n"
                    f"  .leftmost_is_msb = true\n"
                    f"}};"
                ),
            )
        )
        blocks.append(block)
        sprite_symbol_by_frame[frame_name] = spr_name
        source_info[frame_name] = f"sheet[{gx},{gy}]"

    return "\n\n".join(blocks), sprite_symbol_by_frame, source_info, cell_w, cell_h, color_stride, mask_stride


def generate(manifest_path: Path, sheet_manifest_path: Path) -> str:
    manifest = _load_manifest(manifest_path)
    sheet_manifest = _load_sheet_manifest(sheet_manifest_path)

    icons: dict[str, str] = manifest.get("icons", {})
    states: dict[str, str] = manifest["states"]
    rows: dict[str, list[str]] = manifest["rows"]
    pet_state_map: dict[str, str] = manifest["pet_state_map"]

    top_row = list(rows.get("top", []))
    bottom_row = list(rows.get("bottom", []))
    action_ids = list(icons.keys())
    if "icon_sheet" in manifest:
        action_ids = []
        for row_action in top_row + bottom_row:
            if row_action not in action_ids:
                action_ids.append(row_action)
    state_ids = list(states.keys())
    if not action_ids:
        raise ValueError("Manifest icons section is empty")
    if not state_ids:
        raise ValueError("Manifest states section is empty")

    if not top_row:
        raise ValueError("rows.top must contain at least one action id")
    if not bottom_row:
        raise ValueError("rows.bottom must contain at least one action id")

    if "icon_sheet" not in manifest:
        for row_action in top_row + bottom_row:
            if row_action not in icons:
                raise ValueError(f"Row references unknown action icon id: {row_action}")

    # Existing icon/state sprite generation
    sprite1_blocks: list[str] = []
    icon_sprite_symbol: dict[str, str] = {}
    state_sprite_symbol: dict[str, str] = {}
    icon_sources: list[str] = []
    state_sources: list[str] = []

    selection_bg_loaded: tuple[bytes, bytes] | None = None
    if "icon_sheet" in manifest:
        icon_sheet_bitmaps, selection_bg_loaded = _load_icon_sheet(manifest, top_row, bottom_row)
        for action_id in action_ids:
            if action_id in icon_sheet_bitmaps:
                on, mask = icon_sheet_bitmaps[action_id]
                icon_sources.append(f"{action_id}=sheet")
            else:
                fallback_rows = FALLBACK_ICON_ROWS.get(action_id, FALLBACK_ICON_ROWS["feed"])
                on = _fallback_on()
                mask = _fallback_mask(fallback_rows)
                icon_sources.append(f"{action_id}=fallback")
            block, spr_symbol = _sprite1_block("ui_pet_icon", action_id, on, mask)
            sprite1_blocks.append(block)
            icon_sprite_symbol[action_id] = spr_symbol
    else:
        for action_id, relpath in icons.items():
            fallback_rows = FALLBACK_ICON_ROWS.get(action_id, FALLBACK_ICON_ROWS["feed"])
            src_path = ASSET_ROOT / relpath
            loaded = _load_png_1bpp_or_none(src_path)
            if loaded is None:
                on = _fallback_on()
                mask = _fallback_mask(fallback_rows)
                icon_sources.append(f"{action_id}=fallback")
            else:
                on, mask = loaded
                icon_sources.append(f"{action_id}=png")
            block, spr_symbol = _sprite1_block("ui_pet_icon", action_id, on, mask)
            sprite1_blocks.append(block)
            icon_sprite_symbol[action_id] = spr_symbol

    selection_bg_path = manifest.get("selection_bg")
    if isinstance(selection_bg_path, str):
        src_path = ASSET_ROOT / selection_bg_path
        loaded = _load_png_1bpp_or_none(src_path)
        if loaded is not None:
            selection_bg_loaded = loaded

    if selection_bg_loaded is None:
        selection_bg_on = _fallback_on()
        selection_bg_mask = _fallback_mask(FALLBACK_SELECTION_BG_ROWS)
        icon_sources.append("selection_bg=fallback")
    else:
        selection_bg_on, selection_bg_mask = selection_bg_loaded
        icon_sources.append("selection_bg=asset")

    selection_bg_block, selection_bg_symbol = _sprite1_block("ui_pet", "selection_bg", selection_bg_on, selection_bg_mask)
    sprite1_blocks.append(selection_bg_block)

    for state_id, relpath in states.items():
        fallback_rows = FALLBACK_STATE_ROWS.get(state_id, FALLBACK_STATE_ROWS["idle"])
        src_path = ASSET_ROOT / relpath
        loaded = _load_png_1bpp_or_none(src_path)
        if loaded is None:
            on = _fallback_on()
            mask = _fallback_mask(fallback_rows)
            state_sources.append(f"{state_id}=fallback")
        else:
            on, mask = loaded
            state_sources.append(f"{state_id}=png")
        block, spr_symbol = _sprite1_block("ui_pet_state", state_id, on, mask)
        sprite1_blocks.append(block)
        state_sprite_symbol[state_id] = spr_symbol

    # New sheet-based animation generation
    sprite2_blocks, frame_sprite_symbol, frame_sources, frame_w, frame_h, frame_color_stride, frame_mask_stride = _sheet_sprite2_blocks(sheet_manifest)

    clips = sheet_manifest["clips"]
    state_to_clip = sheet_manifest["state_to_clip"]
    clip_names = list(clips.keys())
    if not clip_names:
        raise ValueError("Sheet clips section is empty")

    clip_enum_lines: list[str] = []
    clip_blocks: list[str] = []
    clip_table_entries: list[str] = []
    clip_sources: list[str] = []
    clip_name_to_id: dict[str, int] = {}

    for idx, clip_name in enumerate(clip_names):
        clip_name_to_id[clip_name] = idx
        clip_enum_lines.append(f"  UI_PET_CLIP_ID_{_c_ident_upper(clip_name)} = {idx}u,")

    clip_enum_lines.append(f"  UI_PET_CLIP_ID_COUNT = {len(clip_names)}u")

    for clip_name in clip_names:
        clip_def = clips[clip_name]
        frames = list(clip_def["frames"])
        if len(frames) == 0:
            raise ValueError(f"Clip '{clip_name}' has no frames")

        for frame_name in frames:
            if frame_name not in frame_sprite_symbol:
                raise ValueError(f"Clip '{clip_name}' references unknown frame '{frame_name}'")

        tick_domain, tick_domain_name = _clip_tick_domain_symbol(str(clip_def.get("tick_domain", "active")))
        frame_ms = _clip_frame_ms_list(clip_name, clip_def, len(frames), tick_domain_name)

        loop = 1 if bool(clip_def.get("loop", False)) else 0
        token = _c_ident(clip_name).lower()
        frame_ptrs_name = f"ui_pet_clip_{token}_frames"
        frame_ms_name = f"ui_pet_clip_{token}_frame_ms"

        frame_ptrs = ", ".join(f"&{frame_sprite_symbol[f]}" for f in frames)
        frame_ms_vals = ", ".join(f"{int(v)}u" for v in frame_ms)
        clip_block = (
            f"static const sprite2_t *const {frame_ptrs_name}[] = {{{frame_ptrs}}};\n"
            f"static const uint16_t {frame_ms_name}[] = {{{frame_ms_vals}}};"
        )
        clip_blocks.append(clip_block)
        clip_table_entries.append(
            "  { "
            f"{frame_ptrs_name}, {frame_ms_name}, "
            f"{len(frames)}u, {loop}u, {tick_domain}"
            " },"
        )
        clip_sources.append(f"{clip_name}={len(frames)}f/{tick_domain_name}")

    # Map states to clip IDs
    state_clip_cases: list[tuple[int, str]] = []
    for state_name, clip_name in state_to_clip.items():
        if state_name not in PET_STATE_NAME_TO_ID:
            raise ValueError(f"state_to_clip unknown state '{state_name}'")
        if clip_name not in clip_name_to_id:
            raise ValueError(f"state_to_clip references unknown clip '{clip_name}'")
        state_id = PET_STATE_NAME_TO_ID[state_name]
        state_clip_cases.append(
            (state_id, f"case {state_id}u: return UI_PET_CLIP_ID_{_c_ident_upper(clip_name)};")
        )
    state_clip_cases.sort(key=lambda x: x[0])
    default_clip_name = state_to_clip.get("idle", clip_names[0])
    if default_clip_name not in clip_name_to_id:
        default_clip_name = clip_names[0]

    # Existing action table/state sprite table for compatibility
    action_enum_lines = []
    action_icon_table_entries = []
    for idx, action_id in enumerate(action_ids):
        action_enum_lines.append(f"  UI_PET_ACTION_ID_{_c_ident_upper(action_id)} = {idx}u,")
        action_icon_table_entries.append(
            f"  &{icon_sprite_symbol[action_id]}, /* UI_PET_ACTION_ID_{_c_ident_upper(action_id)} */"
        )
    action_enum_lines.append(f"  UI_PET_ACTION_ID_COUNT = {len(action_ids)}u")

    top_ids = ", ".join(f"UI_PET_ACTION_ID_{_c_ident_upper(action_id)}" for action_id in top_row)
    bottom_ids = ", ".join(f"UI_PET_ACTION_ID_{_c_ident_upper(action_id)}" for action_id in bottom_row)

    state_cases = []
    for key, state_id in pet_state_map.items():
        if state_id not in state_sprite_symbol:
            raise ValueError(f"pet_state_map references unknown state id: {state_id}")
        state_num = int(key)
        state_cases.append((state_num, f"case {state_num}u: return &{state_sprite_symbol[state_id]};"))
    state_cases.sort(key=lambda item: item[0])
    state_default_id = _manifest_state_default(manifest, state_ids)

    header = f"""// Auto-generated by tools/gen_pet_sprites.py. Do not edit manually.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "display_renderer.h"

enum {{
  UI_PET_ASSET_W_PX = {SPR_W},
  UI_PET_ASSET_H_PX = {SPR_H},
  UI_PET_ASSET_ROW_BYTES = {SPR_STRIDE}
}};

enum {{
  UI_PET_FRAME_W_PX = {frame_w},
  UI_PET_FRAME_H_PX = {frame_h},
  UI_PET_FRAME_COLOR_STRIDE = {frame_color_stride},
  UI_PET_FRAME_MASK_STRIDE = {frame_mask_stride}
}};

enum {{
{chr(10).join(action_enum_lines)}
}};

enum {{
  UI_PET_ROW_TOP_COUNT = {len(top_row)}u,
  UI_PET_ROW_BOTTOM_COUNT = {len(bottom_row)}u
}};

enum {{
  UI_PET_ANIM_TICK_STOP_WAKE_1HZ = 0u,
  UI_PET_ANIM_TICK_ACTIVE = 1u,
  UI_PET_ANIM_TICK_STOP_SELECT_2HZ = 2u,
  UI_PET_ANIM_TICK_RTC_1HZ = UI_PET_ANIM_TICK_STOP_WAKE_1HZ
}};

enum {{
{chr(10).join(clip_enum_lines)}
}};

typedef struct
{{
  const sprite2_t *const *frames;
  const uint16_t *frame_ms;
  uint8_t frame_count;
  uint8_t loop;
  uint8_t tick_domain;
}} ui_pet_anim_clip_t;

static const uint8_t ui_pet_row_top_action_ids[UI_PET_ROW_TOP_COUNT] = {{{top_ids}}};
static const uint8_t ui_pet_row_bottom_action_ids[UI_PET_ROW_BOTTOM_COUNT] = {{{bottom_ids}}};

{chr(10).join(sprite1_blocks)}

{sprite2_blocks}

static const sprite1_t *const ui_pet_action_icon_table[UI_PET_ACTION_ID_COUNT] = {{
{chr(10).join(action_icon_table_entries)}
}};

{chr(10).join(clip_blocks)}

static const ui_pet_anim_clip_t ui_pet_anim_clip_table[UI_PET_CLIP_ID_COUNT] = {{
{chr(10).join(clip_table_entries)}
}};

static inline uint32_t UiPetAssets_RowCount(uint32_t row_idx)
{{
  if (row_idx == 0u)
  {{
    return UI_PET_ROW_TOP_COUNT;
  }}
  if (row_idx == 1u)
  {{
    return UI_PET_ROW_BOTTOM_COUNT;
  }}
  return 0u;
}}

static inline uint32_t UiPetAssets_ActionIdAt(uint32_t row_idx, uint32_t col_idx)
{{
  const uint8_t *row_ids = 0;
  uint32_t row_count = 0u;

  if (row_idx == 0u)
  {{
    row_ids = ui_pet_row_top_action_ids;
    row_count = UI_PET_ROW_TOP_COUNT;
  }}
  else if (row_idx == 1u)
  {{
    row_ids = ui_pet_row_bottom_action_ids;
    row_count = UI_PET_ROW_BOTTOM_COUNT;
  }}
  else
  {{
    return 0u;
  }}

  if (col_idx >= row_count)
  {{
    return row_ids[0];
  }}
  return row_ids[col_idx];
}}

static inline const sprite1_t *UiPetAssets_GetActionIcon(uint32_t action_id)
{{
  if (action_id >= UI_PET_ACTION_ID_COUNT)
  {{
    return ui_pet_action_icon_table[0];
  }}
  return ui_pet_action_icon_table[action_id];
}}

static inline const sprite1_t *UiPetAssets_GetSelectionBg(void)
{{
  return &{selection_bg_symbol};
}}

static inline const sprite1_t *UiPetAssets_GetStateSprite(uint32_t pet_state)
{{
  switch (pet_state)
  {{
{chr(10).join("    " + line for _, line in state_cases)}
    default: return &{state_sprite_symbol[state_default_id]};
  }}
}}

static inline const ui_pet_anim_clip_t *UiPetAssets_GetClip(uint32_t clip_id)
{{
  if (clip_id >= UI_PET_CLIP_ID_COUNT)
  {{
    return &ui_pet_anim_clip_table[0];
  }}
  return &ui_pet_anim_clip_table[clip_id];
}}

static inline uint32_t UiPetAssets_ClipIdForPetState(uint32_t pet_state)
{{
  switch (pet_state)
  {{
{chr(10).join("    " + line for _, line in state_clip_cases)}
    default: return UI_PET_CLIP_ID_{_c_ident_upper(default_clip_name)};
  }}
}}
"""

    print("Pet icon assets:", ", ".join(icon_sources))
    print("Pet state assets:", ", ".join(state_sources))
    print("Pet top row order:", ", ".join(top_row))
    print("Pet bottom row order:", ", ".join(bottom_row))
    print("Pet sheet frames:", ", ".join(f"{k}={v}" for k, v in frame_sources.items()))
    print("Pet clips:", ", ".join(clip_sources))
    return header


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=OUT_HEADER, help="Output header path.")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=MANIFEST_PATH,
        help="Pet icon/menu manifest path.",
    )
    parser.add_argument(
        "--sheet-manifest",
        type=Path,
        default=SHEET_MANIFEST_PATH,
        help="Pet sheet animation manifest path.",
    )
    args = parser.parse_args()

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(generate(args.manifest, args.sheet_manifest), encoding="utf-8")
    print(f"Generated {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
