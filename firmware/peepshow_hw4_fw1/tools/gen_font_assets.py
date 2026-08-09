#!/usr/bin/env python3
"""Generate bitmap font assets from packed sheet PNGs.

Default source fallback:
  Assets/fonts/gb_font_default.png

Manifest mode:
  Assets/fonts/font_manifest.json

Output:
  Core/Inc/font_assets_autogen.h
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = ROOT / "Assets" / "fonts" / "gb_font_default.png"
DEFAULT_MANIFEST = ROOT / "Assets" / "fonts" / "font_manifest.json"
DEFAULT_OUT = ROOT / "Core" / "Inc" / "font_assets_autogen.h"


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


def _glyph_rows_from_sheet(img: Image.Image,
                           glyph_w: int, glyph_h: int,
                           cols: int, rows: int,
                           threshold: int) -> bytes:
    px = img.convert("L")
    out = bytearray()
    for gy in range(rows):
        for gx in range(cols):
            x0 = gx * glyph_w
            y0 = gy * glyph_h
            for y in range(glyph_h):
                byte = 0
                for x in range(glyph_w):
                    lum = px.getpixel((x0 + x, y0 + y))
                    is_black = 1 if lum < threshold else 0
                    byte |= (is_black << x)  # LSB-left to match font8x8_basic convention
                out.append(byte & 0xFF)
    return bytes(out)


def _c_ident(name: str) -> str:
    ident = re.sub(r"[^a-zA-Z0-9_]", "_", name)
    if not ident:
        return "id"
    if ident[0].isdigit():
        ident = "_" + ident
    return ident


def _resolve_path(path_text: str) -> Path:
    p = Path(path_text)
    if p.is_absolute():
        return p
    return ROOT / p


def _load_manifest(path: Path) -> dict | None:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def _build_config(args, manifest: dict | None) -> dict:
    cfg = {
        "glyph_w": args.glyph_w,
        "glyph_h": args.glyph_h,
        "cols": args.cols,
        "rows": args.rows,
        "first_char": args.first_char,
        "threshold": args.threshold,
        "fonts": {"regular": str(args.src)},
    }

    if manifest is None:
        return cfg

    cfg["glyph_w"] = int(manifest.get("glyph_w", cfg["glyph_w"]))
    cfg["glyph_h"] = int(manifest.get("glyph_h", cfg["glyph_h"]))
    cfg["cols"] = int(manifest.get("cols", cfg["cols"]))
    cfg["rows"] = int(manifest.get("rows", cfg["rows"]))
    cfg["first_char"] = int(manifest.get("first_char", cfg["first_char"]))
    cfg["threshold"] = int(manifest.get("threshold", cfg["threshold"]))

    manifest_fonts = manifest.get("fonts", {})
    if not isinstance(manifest_fonts, dict):
        raise ValueError("font manifest 'fonts' must be an object")
    if len(manifest_fonts) == 0:
        raise ValueError("font manifest 'fonts' is empty")
    if "regular" not in manifest_fonts:
        raise ValueError("font manifest must define 'fonts.regular'")

    cfg["fonts"] = dict(manifest_fonts)
    return cfg


def generate(out: Path, cfg: dict) -> None:
    glyph_w = int(cfg["glyph_w"])
    glyph_h = int(cfg["glyph_h"])
    cols = int(cfg["cols"])
    rows = int(cfg["rows"])
    first_char = int(cfg["first_char"])
    threshold = int(cfg["threshold"])
    fonts: dict[str, str] = cfg["fonts"]

    if glyph_w <= 0 or glyph_h <= 0 or cols <= 0 or rows <= 0:
        raise ValueError("glyph_w/glyph_h/cols/rows must be > 0")

    expected_w = cols * glyph_w
    expected_h = rows * glyph_h
    glyph_count = cols * rows
    last_char = first_char + glyph_count - 1
    glyph_row_stride = (glyph_w + 7) // 8

    preferred_order = ["regular", "bold", "italic_lower", "italic_upper", "tiny"]
    ordered_styles = [s for s in preferred_order if s in fonts]
    ordered_styles.extend(sorted([s for s in fonts.keys() if s not in ordered_styles]))

    font_blocks: list[str] = []
    style_to_symbol: dict[str, str] = {}

    for style in ordered_styles:
        src_path = _resolve_path(str(fonts[style]))
        if not src_path.exists():
            if style == "regular":
                raise FileNotFoundError(f"Required regular font sheet not found: {src_path}")
            print(f"Skipping optional font style '{style}' (missing file): {src_path}")
            continue
        img = Image.open(src_path)
        if img.size != (expected_w, expected_h):
            raise ValueError(
                f"Font sheet size mismatch for {src_path}: got {img.size[0]}x{img.size[1]}, "
                f"expected {expected_w}x{expected_h}"
            )

        glyph_data = _glyph_rows_from_sheet(img, glyph_w, glyph_h, cols, rows, threshold)
        token = _c_ident(style).lower()
        glyph_array_name = f"g_font_{token}_glyphs"
        font_symbol_name = f"g_render_font_{token}_asset"

        block = "\n\n".join(
            (
                _emit_u8_array(glyph_array_name, glyph_data),
                (
                    f"static const render_font_t {font_symbol_name} = {{\n"
                    f"  .glyph_w = {glyph_w},\n"
                    f"  .glyph_h = {glyph_h},\n"
                    f"  .first_char = 0x{first_char:02X},\n"
                    f"  .last_char = 0x{last_char:02X},\n"
                    f"  .glyph_row_stride_bytes = {glyph_row_stride},\n"
                    f"  .leftmost_is_msb = false,\n"
                    f"  .glyph_data = {glyph_array_name}\n"
                    f"}};"
                ),
            )
        )
        font_blocks.append(block)
        style_to_symbol[style] = font_symbol_name

    if "regular" not in style_to_symbol:
        raise ValueError("No regular font asset was generated")
    regular_symbol = style_to_symbol["regular"]
    bold_symbol = style_to_symbol.get("bold", "0")
    italic_lower_symbol = style_to_symbol.get("italic_lower", "0")
    italic_upper_symbol = style_to_symbol.get("italic_upper", "0")
    tiny_symbol = style_to_symbol.get("tiny", "0")

    header = f"""// Auto-generated by tools/gen_font_assets.py. Do not edit manually.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "render_font.h"

enum {{
  FONT_ASSET_GLYPH_W = {glyph_w},
  FONT_ASSET_GLYPH_H = {glyph_h},
  FONT_ASSET_FIRST_CHAR = 0x{first_char:02X},
  FONT_ASSET_LAST_CHAR = 0x{last_char:02X},
  FONT_ASSET_GLYPH_ROW_STRIDE = {glyph_row_stride}
}};

{chr(10).join(font_blocks)}

static const render_font_t g_render_font_default_asset = {{
  .glyph_w = FONT_ASSET_GLYPH_W,
  .glyph_h = FONT_ASSET_GLYPH_H,
  .first_char = FONT_ASSET_FIRST_CHAR,
  .last_char = FONT_ASSET_LAST_CHAR,
  .glyph_row_stride_bytes = FONT_ASSET_GLYPH_ROW_STRIDE,
  .leftmost_is_msb = false,
  .glyph_data = {regular_symbol}.glyph_data
}};

static const render_font_family_t g_render_font_family_default_asset = {{
  .regular = &{regular_symbol},
  .bold = {("&" + bold_symbol) if bold_symbol != "0" else "0"},
  .italic_lower = {("&" + italic_lower_symbol) if italic_lower_symbol != "0" else "0"},
  .italic_upper = {("&" + italic_upper_symbol) if italic_upper_symbol != "0" else "0"},
  .tiny = {("&" + tiny_symbol) if tiny_symbol != "0" else "0"}
}};
"""

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(header, encoding="utf-8")
    print(f"Generated {out}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--glyph-w", type=int, default=8)
    parser.add_argument("--glyph-h", type=int, default=8)
    parser.add_argument("--cols", type=int, default=16)
    parser.add_argument("--rows", type=int, default=14)
    parser.add_argument("--first-char", type=lambda s: int(s, 0), default=0x20)
    parser.add_argument("--threshold", type=int, default=128)
    args = parser.parse_args()

    manifest = _load_manifest(args.manifest)
    cfg = _build_config(args, manifest)
    generate(args.out, cfg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
