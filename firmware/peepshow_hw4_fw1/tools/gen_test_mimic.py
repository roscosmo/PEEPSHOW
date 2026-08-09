#!/usr/bin/env python3
"""Generate autogen sprite data for the realtime animation test page.

Inputs:
  Assets/test/*.png

Output:
  Core/Inc/ui/ui_test_mimic_autogen.h
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
INPUT_DIR = ROOT / "Assets" / "test"
INPUT_GLOB = "*.png"
OUT_HEADER = ROOT / "Core" / "Inc" / "ui" / "ui_test_mimic_autogen.h"

SPRITE_W = 48
SPRITE_H = 48
WHITE_LUMA_THRESH = 128


def _fit_to_canvas(img_rgba, canvas_w: int, canvas_h: int, image_module):
    """Center image inside canvas, scaling down to fit if necessary."""
    src_w, src_h = img_rgba.size
    if src_w <= 0 or src_h <= 0:
        raise ValueError("image has invalid dimensions")

    scale_num = min(canvas_w, src_w)
    scale_den = src_w
    alt_num = min(canvas_h, src_h)
    alt_den = src_h
    # min(canvas_w/src_w, canvas_h/src_h, 1.0) without floats
    if scale_num * alt_den > alt_num * scale_den:
        scale_num, scale_den = alt_num, alt_den
    if scale_num > scale_den:
        scale_num, scale_den = scale_den, scale_den

    out_w = max(1, (src_w * scale_num) // scale_den)
    out_h = max(1, (src_h * scale_num) // scale_den)

    if out_w != src_w or out_h != src_h:
        resampling = getattr(image_module, "Resampling", None)
        if resampling is not None:
            img_rgba = img_rgba.resize((out_w, out_h), resampling.NEAREST)
        else:
            img_rgba = img_rgba.resize((out_w, out_h), image_module.NEAREST)

    canvas = image_module.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    off_x = (canvas_w - out_w) // 2
    off_y = (canvas_h - out_h) // 2
    canvas.paste(img_rgba, (off_x, off_y))
    return canvas


def _pack_1bpp_planes_rgba(img_rgba, w: int, h: int) -> tuple[bytes, bytes, bytes]:
    stride = (w + 7) // 8
    on = bytearray(stride * h)
    on_inv = bytearray(stride * h)
    mask = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x, y))
            if a < 16:
                continue
            idx = y * stride + (x >> 3)
            bit = 1 << (7 - (x & 7))
            mask[idx] |= bit
            lum = (299 * r + 587 * g + 114 * b) // 1000
            if lum >= WHITE_LUMA_THRESH:
                on[idx] |= bit
            else:
                on_inv[idx] |= bit
    return bytes(on), bytes(on_inv), bytes(mask)


def _quantize_luma_to_2bpp(lum: int) -> int:
    if lum >= 224:
        return 0
    if lum >= 160:
        return 1
    if lum >= 96:
        return 2
    return 3


def _pack_2bpp_planes_rgba(img_rgba, w: int, h: int) -> tuple[bytes, bytes, bytes]:
    color_stride = (w + 3) // 4
    mask_stride = (w + 7) // 8
    color = bytearray(color_stride * h)
    color_inv = bytearray(color_stride * h)
    mask = bytearray(mask_stride * h)

    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x, y))
            if a < 16:
                continue

            lum = (299 * r + 587 * g + 114 * b) // 1000
            level = _quantize_luma_to_2bpp(lum) & 0x3
            level_inv = (3 - level) & 0x3

            cidx = y * color_stride + (x >> 2)
            shift = 6 - ((x & 3) * 2)
            color[cidx] |= (level << shift) & 0xFF
            color_inv[cidx] |= (level_inv << shift) & 0xFF

            midx = y * mask_stride + (x >> 3)
            mbit = 1 << (7 - (x & 7))
            mask[midx] |= mbit

    return bytes(color), bytes(color_inv), bytes(mask)


def _emit_u8_array(name: str, data: bytes) -> str:
    lines = [f"static const uint8_t {name}[] = {{"]
    row: list[str] = []
    for i, b in enumerate(data):
        row.append(f"0x{b:02X}")
        if len(row) == 16 or i == (len(data) - 1):
            lines.append("  " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    return "\n".join(lines)


def _symbolize(stem: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", stem.lower()).strip("_")


def _display_name(stem: str) -> str:
    name = stem
    if name.lower().startswith("48x48_"):
        name = name[6:]
    return name.replace("_", " ")


def _collect_inputs() -> list[Path]:
    return sorted(path for path in INPUT_DIR.glob(INPUT_GLOB) if path.is_file())


def generate() -> str:
    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError("Pillow is required: pip install pillow") from exc

    input_pngs = _collect_inputs()
    if not input_pngs:
        raise FileNotFoundError(f"no input sprites found: {INPUT_DIR / INPUT_GLOB}")

    stride_1bpp = (SPRITE_W + 7) // 8
    stride_2bpp = (SPRITE_W + 3) // 4
    arrays: list[str] = []
    entry_lines: list[str] = []

    for png in input_pngs:
        img = Image.open(png).convert("RGBA")
        img = _fit_to_canvas(img, SPRITE_W, SPRITE_H, Image)

        sym = _symbolize(png.stem)
        label = _display_name(png.stem)

        on, on_inv, mask_1bpp = _pack_1bpp_planes_rgba(img, SPRITE_W, SPRITE_H)
        color_2bpp, color_2bpp_inv, mask_2bpp = _pack_2bpp_planes_rgba(img, SPRITE_W, SPRITE_H)

        on_name = f"ui_test_mimic_{sym}_1bpp_on"
        on_inv_name = f"ui_test_mimic_{sym}_1bpp_on_inv"
        mask_1_name = f"ui_test_mimic_{sym}_mask_1bpp"
        color_name = f"ui_test_mimic_{sym}_2bpp_color"
        color_inv_name = f"ui_test_mimic_{sym}_2bpp_color_inv"
        mask_2_name = f"ui_test_mimic_{sym}_mask_2bpp"

        arrays.extend(
            [
                _emit_u8_array(on_name, on),
                _emit_u8_array(on_inv_name, on_inv),
                _emit_u8_array(mask_1_name, mask_1bpp),
                _emit_u8_array(color_name, color_2bpp),
                _emit_u8_array(color_inv_name, color_2bpp_inv),
                _emit_u8_array(mask_2_name, mask_2bpp),
            ]
        )

        entry_lines.append(
            "  {"
            f"{on_name}, {on_inv_name}, {mask_1_name}, "
            f"{color_name}, {color_inv_name}, {mask_2_name}, "
            f"\"{label}\""
            "},"
        )

    sprite_count = len(input_pngs)

    return f"""// Auto-generated by tools/gen_test_mimic.py. Do not edit manually.
#pragma once

#include <stdint.h>

enum {{
  UI_TEST_MIMIC_W = {SPRITE_W}u,
  UI_TEST_MIMIC_H = {SPRITE_H}u,
  UI_TEST_MIMIC_1BPP_STRIDE = {stride_1bpp}u,
  UI_TEST_MIMIC_2BPP_COLOR_STRIDE = {stride_2bpp}u,
  UI_TEST_MIMIC_2BPP_MASK_STRIDE = {stride_1bpp}u,
  UI_TEST_MIMIC_SPRITE_COUNT = {sprite_count}u
}};

typedef struct
{{
  const uint8_t *on_1bpp;
  const uint8_t *on_1bpp_inv;
  const uint8_t *mask_1bpp;
  const uint8_t *color_2bpp;
  const uint8_t *color_2bpp_inv;
  const uint8_t *mask_2bpp;
  const char *name;
}} ui_test_mimic_sprite_entry_t;

{chr(10).join(arrays)}

static const ui_test_mimic_sprite_entry_t ui_test_mimic_sprites[UI_TEST_MIMIC_SPRITE_COUNT] = {{
{chr(10).join(entry_lines)}
}};
"""


def main() -> int:
    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUT_HEADER.write_text(generate(), encoding="utf-8")
    print(f"generated {OUT_HEADER}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
