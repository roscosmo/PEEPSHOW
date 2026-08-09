#!/usr/bin/env python3
"""
Convert a tileset sheet PNG into PeepShow compact tileset blob (TSET v1).
"""

from __future__ import annotations

import argparse
import struct
import xml.etree.ElementTree as ET
from pathlib import Path


MAGIC = 0x54455354  # "TSET"
VERSION = 1
MAX_BYTES = 65536

HEADER_FMT = "<IHHIIHHHHIIIIII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
CRC32_OFFSET = 12


def _align4(n: int) -> int:
    return (n + 3) & ~3


def _crc32_masked(blob: bytes) -> int:
    crc = 0xFFFFFFFF
    for i, b in enumerate(blob):
        v = 0 if CRC32_OFFSET <= i < (CRC32_OFFSET + 4) else b
        crc ^= v
        for _ in range(8):
            if (crc & 1) != 0:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (~crc) & 0xFFFFFFFF


def _int_attr(node: ET.Element, key: str, default: int = 0) -> int:
    value = node.get(key)
    if value is None:
        return default
    try:
        return int(value, 10)
    except ValueError:
        return default


def _resolve_from_tsx(tsx_path: Path) -> tuple[Path, int, int, int, int]:
    root = ET.parse(tsx_path).getroot()
    tile_w = _int_attr(root, "tilewidth", 0)
    tile_h = _int_attr(root, "tileheight", 0)
    columns = _int_attr(root, "columns", 0)
    tile_count = _int_attr(root, "tilecount", 0)

    img = root.find("image")
    if img is None:
        raise ValueError(f"{tsx_path}: missing <image> node")
    src = img.get("source")
    if not src:
        raise ValueError(f"{tsx_path}: image source is missing")

    png_path = (tsx_path.parent / src).resolve()
    if tile_w <= 0 or tile_h <= 0:
        raise ValueError(f"{tsx_path}: invalid tile size {tile_w}x{tile_h}")
    if columns <= 0:
        width = _int_attr(img, "width", 0)
        if width <= 0 or (width % tile_w) != 0:
            raise ValueError(f"{tsx_path}: cannot derive columns from image width")
        columns = width // tile_w
    if tile_count <= 0:
        width = _int_attr(img, "width", 0)
        height = _int_attr(img, "height", 0)
        if width <= 0 or height <= 0 or (width % tile_w) != 0 or (height % tile_h) != 0:
            raise ValueError(f"{tsx_path}: cannot derive tilecount from image size")
        tile_count = (width // tile_w) * (height // tile_h)
    rows = (tile_count + columns - 1) // columns
    return png_path, tile_w, tile_h, columns, rows


def _quantize_tone5(r: int, g: int, b: int) -> int:
    lum = (299 * r + 587 * g + 114 * b) // 1000
    centers = (255, 191, 127, 63, 0)
    best_idx = 0
    best_dist = abs(lum - centers[0])
    for idx in range(1, len(centers)):
        dist = abs(lum - centers[idx])
        if dist < best_dist:
            best_dist = dist
            best_idx = idx
    return best_idx


def _encode_tone5_to_level_mask(tone: int) -> tuple[int, int]:
    """
    Encode 5 visible shades + transparent using existing 2bpp+mask storage.

    tone 0: white      -> level 0, mask 1
    tone 1: 1/4 black  -> level 1, mask 1
    tone 2: 2/4 black  -> level 2, mask 1
    tone 3: 3/4 black  -> level 1, mask 0 (extended shade)
    tone 4: black      -> level 3, mask 1
    """
    if tone <= 0:
        return (0, 1)
    if tone == 1:
        return (1, 1)
    if tone == 2:
        return (2, 1)
    if tone == 3:
        return (1, 0)
    return (3, 1)


def build_blob(
    png_path: Path,
    tile_w: int,
    tile_h: int,
    columns: int,
    rows: int,
    base_gid: int,
    tile_indices: list[int] | None = None,
) -> bytes:
    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError("Pillow is required. Install with: pip install pillow") from exc

    if tile_w <= 0 or tile_h <= 0:
        raise ValueError("tile dimensions must be > 0")
    if columns <= 0 or rows <= 0:
        raise ValueError("columns/rows must be > 0")
    if base_gid < 0:
        raise ValueError("base_gid must be >= 0")

    img = Image.open(png_path).convert("RGBA")
    sheet_w, sheet_h = img.size
    expected_w = columns * tile_w
    expected_h = rows * tile_h
    if sheet_w < expected_w or sheet_h < expected_h:
        raise ValueError(
            f"sheet too small: got {sheet_w}x{sheet_h}, need at least {expected_w}x{expected_h}"
        )

    source_tile_count = columns * rows
    if tile_indices is None:
        source_indices = list(range(source_tile_count))
    else:
        source_indices = [int(idx) for idx in tile_indices]
        if len(source_indices) == 0:
            raise ValueError("tile_indices must contain at least one tile")
        for idx in source_indices:
            if idx < 0 or idx >= source_tile_count:
                raise ValueError(
                    f"tile_indices contains out-of-range tile {idx} for source tile count {source_tile_count}"
                )

    tile_count = len(source_indices)
    color_stride = (tile_w + 3) // 4
    mask_stride = (tile_w + 7) // 8
    tile_color_bytes = color_stride * tile_h
    tile_mask_bytes = mask_stride * tile_h

    color_plane = bytearray(tile_count * tile_color_bytes)
    mask_plane = bytearray(tile_count * tile_mask_bytes)

    for out_idx, source_idx in enumerate(source_indices):
        cell_x = (source_idx % columns) * tile_w
        cell_y = (source_idx // columns) * tile_h

        color_base = out_idx * tile_color_bytes
        mask_base = out_idx * tile_mask_bytes

        for y in range(tile_h):
            for x in range(tile_w):
                r, g, b, a = img.getpixel((cell_x + x, cell_y + y))
                color_idx = color_base + y * color_stride + (x >> 2)
                color_shift = 6 - ((x & 3) * 2)

                if a == 0:
                    level = 0
                    mask_bit = 0
                else:
                    tone = _quantize_tone5(r, g, b)
                    level, mask_bit = _encode_tone5_to_level_mask(tone)

                if mask_bit != 0:
                    mask_idx = mask_base + y * mask_stride + (x >> 3)
                    mask_plane[mask_idx] |= 1 << (7 - (x & 7))

                color_plane[color_idx] &= ~(0x3 << color_shift)
                color_plane[color_idx] |= (level & 0x3) << color_shift

    color_offset = _align4(HEADER_SIZE)
    mask_offset = _align4(color_offset + len(color_plane))
    total_size = mask_offset + len(mask_plane)
    if total_size > MAX_BYTES:
        raise ValueError(f"TSET blob size {total_size} exceeds max {MAX_BYTES}")

    blob = bytearray(total_size)
    header = struct.pack(
        HEADER_FMT,
        MAGIC,
        VERSION,
        HEADER_SIZE,
        total_size,
        0,  # crc placeholder
        tile_w & 0xFFFF,
        tile_h & 0xFFFF,
        0,
        0,
        tile_count & 0xFFFFFFFF,
        base_gid & 0xFFFFFFFF,
        color_stride & 0xFFFFFFFF,
        mask_stride & 0xFFFFFFFF,
        color_offset & 0xFFFFFFFF,
        mask_offset & 0xFFFFFFFF,
    )

    blob[0:HEADER_SIZE] = header
    blob[color_offset : color_offset + len(color_plane)] = color_plane
    blob[mask_offset : mask_offset + len(mask_plane)] = mask_plane
    crc = _crc32_masked(bytes(blob))
    blob[CRC32_OFFSET : CRC32_OFFSET + 4] = struct.pack("<I", crc)
    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate compact TSET blob from tileset PNG/TSX.")
    src_group = parser.add_mutually_exclusive_group(required=True)
    src_group.add_argument("--in-png", type=Path, help="Input tileset PNG path")
    src_group.add_argument("--in-tsx", type=Path, help="Input Tiled TSX path")
    parser.add_argument("--tile-width", type=int, default=0, help="Tile width in pixels (required with --in-png)")
    parser.add_argument("--tile-height", type=int, default=0, help="Tile height in pixels (required with --in-png)")
    parser.add_argument("--columns", type=int, default=0, help="Tiles per row (optional with --in-png)")
    parser.add_argument("--rows", type=int, default=0, help="Tile rows (optional with --in-png)")
    parser.add_argument("--base-gid", type=int, default=1, help="Base gid for tile 0 (default: 1)")
    parser.add_argument("--out-bin", type=Path, required=True, help="Output .tset.bin path")
    args = parser.parse_args()

    if args.in_tsx is not None:
        png_path, tile_w, tile_h, columns, rows = _resolve_from_tsx(args.in_tsx)
    else:
        if args.in_png is None:
            raise ValueError("missing --in-png or --in-tsx")
        png_path = args.in_png
        tile_w = args.tile_width
        tile_h = args.tile_height
        if tile_w <= 0 or tile_h <= 0:
            raise ValueError("--tile-width and --tile-height are required with --in-png")

        try:
            from PIL import Image  # type: ignore
        except Exception as exc:
            raise RuntimeError("Pillow is required. Install with: pip install pillow") from exc

        with Image.open(png_path) as img:
            sheet_w, sheet_h = img.size
        columns = args.columns if args.columns > 0 else (sheet_w // tile_w)
        rows = args.rows if args.rows > 0 else (sheet_h // tile_h)
        if columns <= 0 or rows <= 0:
            raise ValueError("could not derive columns/rows from PNG and tile size")
        if (sheet_w < (columns * tile_w)) or (sheet_h < (rows * tile_h)):
            raise ValueError("specified columns/rows exceed PNG dimensions")

    blob = build_blob(png_path, tile_w, tile_h, columns, rows, args.base_gid)
    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(blob)
    print(f"wrote {args.out_bin} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
