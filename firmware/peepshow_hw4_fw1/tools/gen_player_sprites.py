#!/usr/bin/env python3
"""Generate player sprite assets from a sprite sheet.

Default input:
  - Assets/sprites/player_sheet.png

Optional manifest:
  - Assets/sprites/player_sheet.json

Output:
  - Assets/sprites/player_sprites_autogen.h

Frame naming convention (auto-discovered):
  <clip>_<direction>_<index>

Examples:
  idle_down_1, walk_up_3, action_1_right_2, attack_down_right_4
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SHEET = ROOT / "Assets" / "sprites" / "player_sheet.png"
DEFAULT_MANIFEST = ROOT / "Assets" / "sprites" / "player_sheet.json"
DEFAULT_OUT = ROOT / "Assets" / "sprites" / "player_sprites_autogen.h"

# Fallback layout when no manifest exists.
DEFAULT_FRAME_MAP: dict[str, tuple[int, int]] = {
    "walk_up_1": (0, 0),
    "walk_right_1": (1, 0),
    "walk_left_1": (2, 0),
    "walk_down_1": (3, 0),
    "walk_up_2": (0, 1),
    "walk_right_2": (1, 1),
    "walk_left_2": (2, 1),
    "walk_down_2": (3, 1),
    "idle_up_1": (0, 0),
    "idle_up_2": (0, 1),
    "idle_right_1": (1, 0),
    "idle_right_2": (1, 1),
    "idle_left_1": (2, 0),
    "idle_left_2": (2, 1),
    "idle_down_1": (3, 0),
    "idle_down_2": (3, 1),
}

DEFAULT_CLIP_ORDER: tuple[str, ...] = ("idle", "walk")
DEFAULT_CLIP_FRAME_MS: dict[str, int] = {
    "idle": 260,
    "walk": 120,
}

DIR_ORDER: tuple[str, ...] = (
    "down",
    "up",
    "right",
    "left",
    "down_right",
    "down_left",
    "up_right",
    "up_left",
)

DIR_ENUM: dict[str, str] = {
    "down": "GAME_SPRITE_DIR_DOWN",
    "up": "GAME_SPRITE_DIR_UP",
    "right": "GAME_SPRITE_DIR_RIGHT",
    "left": "GAME_SPRITE_DIR_LEFT",
    "down_right": "GAME_SPRITE_DIR_DOWN_RIGHT",
    "down_left": "GAME_SPRITE_DIR_DOWN_LEFT",
    "up_right": "GAME_SPRITE_DIR_UP_RIGHT",
    "up_left": "GAME_SPRITE_DIR_UP_LEFT",
}

DIR_ALIASES: dict[str, str] = {
    "dr": "down_right",
    "dl": "down_left",
    "ur": "up_right",
    "ul": "up_left",
}

FRAME_NAME_RE = re.compile(
    r"^(?P<clip>[A-Za-z0-9_]+)_(?P<dir>down_right|down_left|up_right|up_left|down|up|right|left|dr|dl|ur|ul)_(?P<idx>\d+)$"
)
FRAME_NAME_RE_NODIR = re.compile(
    r"^(?P<clip>[A-Za-z0-9_]+)_(?P<idx>\d+)$"
)


@dataclass(frozen=True)
class FramePlanes:
    color2bpp: bytes
    mask1bpp: bytes


@dataclass(frozen=True)
class ClipDirDef:
    frame_names: tuple[str, ...]
    frame_ms: tuple[int, ...]


@dataclass(frozen=True)
class ClipDef:
    name: str
    loop: int
    fallback_dir: str
    dirs: dict[str, ClipDirDef]


def _warn(message: str) -> None:
    print(f"[gen_player_sprites] WARNING: {message}", file=sys.stderr)


def _to_repo_path(path_text: str) -> Path:
    p = Path(path_text)
    if p.is_absolute():
        return p
    return ROOT / p


def _sanitize_ident(token: str) -> str:
    out = "".join(ch if (ch.isalnum() or ch == "_") else "_" for ch in token)
    if not out:
        out = "unnamed"
    if out[0].isdigit():
        out = f"n_{out}"
    return out


def _emit_u8_array(name: str, data: bytes) -> str:
    lines: list[str] = [f"static const uint8_t {name}[] = {{"]
    if not data:
        lines.append("  0x00,")
    else:
        row: list[str] = []
        for i, b in enumerate(data):
            row.append(f"0x{b:02X}")
            if (len(row) == 16) or (i == (len(data) - 1)):
                lines.append("  " + ", ".join(row) + ",")
                row = []
    lines.append("};")
    return "\n".join(lines)


def _parse_frame_xy(value: Any, frame_name: str) -> tuple[int, int]:
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"frame '{frame_name}' must be [col,row]")
    col = int(value[0])
    row = int(value[1])
    if col < 0 or row < 0:
        raise ValueError(f"frame '{frame_name}' has negative coord")
    return col, row


def _normalize_dir_token(dir_token: str) -> str:
    d = dir_token.lower()
    if d in DIR_ALIASES:
        return DIR_ALIASES[d]
    return d


def _expand_frame_ms(value: Any, count: int, label: str) -> tuple[int, ...]:
    vals: list[int]
    if isinstance(value, list):
        vals = [int(v) for v in value]
    else:
        vals = [int(value)]

    if not vals:
        raise ValueError(f"{label} must not be empty")
    for v in vals:
        if v <= 0:
            raise ValueError(f"{label} values must be > 0")

    if len(vals) >= count:
        return tuple(vals[:count])
    return tuple(vals + ([vals[-1]] * (count - len(vals))))


def _load_manifest(path: Path) -> tuple[int, int, dict[str, tuple[int, int]], dict[str, Any]]:
    if not path.exists():
        return 16, 16, dict(DEFAULT_FRAME_MAP), {}

    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("manifest root must be an object")

    cell_w = int(data.get("cell_w", 16))
    cell_h = int(data.get("cell_h", 16))
    if (cell_w <= 0) or (cell_h <= 0):
        raise ValueError("cell_w/cell_h must be > 0")

    frames_raw = data.get("frames")
    if frames_raw is None:
        frame_map = dict(DEFAULT_FRAME_MAP)
    else:
        if not isinstance(frames_raw, dict):
            raise ValueError("manifest 'frames' must be an object")
        frame_map = {name: _parse_frame_xy(value, name) for name, value in frames_raw.items()}

    if not frame_map:
        raise ValueError("manifest has no frames")

    return cell_w, cell_h, frame_map, data


def _parse_clips_from_frames(frame_map: dict[str, tuple[int, int]]) -> dict[str, dict[str, tuple[str, ...]]]:
    parsed: dict[str, dict[str, list[tuple[int, str]]]] = {}
    unmatched = 0

    for frame_name in frame_map:
        m = FRAME_NAME_RE.match(frame_name)
        if m is not None:
            clip_name = m.group("clip").lower()
            dir_name = _normalize_dir_token(m.group("dir"))
            idx = int(m.group("idx"))
            if dir_name not in DIR_ORDER:
                unmatched += 1
                continue
        else:
            m_simple = FRAME_NAME_RE_NODIR.match(frame_name)
            if m_simple is None:
                unmatched += 1
                continue
            clip_name = m_simple.group("clip").lower()
            dir_name = "down"
            idx = int(m_simple.group("idx"))

        parsed.setdefault(clip_name, {}).setdefault(dir_name, []).append((idx, frame_name))

    if unmatched > 0:
        _warn(f"ignored {unmatched} frame names that do not match '<clip>_<dir>_<index>'")

    clips: dict[str, dict[str, tuple[str, ...]]] = {}
    for clip_name, dirs in parsed.items():
        clip_dirs: dict[str, tuple[str, ...]] = {}
        for dir_name, idx_names in dirs.items():
            ordered = tuple(name for _, name in sorted(idx_names, key=lambda item: item[0]))
            if ordered:
                clip_dirs[dir_name] = ordered
        if clip_dirs:
            clips[clip_name] = clip_dirs

    return clips


def _clip_order(clips: dict[str, dict[str, tuple[str, ...]]]) -> list[str]:
    order: list[str] = []
    for name in DEFAULT_CLIP_ORDER:
        if name in clips:
            order.append(name)
    for name in sorted(clips.keys()):
        if name not in order:
            order.append(name)
    return order


def _clip_dir_frame_ms(manifest: dict[str, Any], clip_name: str, dir_name: str, frame_count: int) -> tuple[int, ...]:
    by_dir_key = f"{clip_name}_frame_ms_by_dir"
    if isinstance(manifest.get(by_dir_key), dict):
        by_dir = manifest[by_dir_key]
        if dir_name in by_dir:
            return _expand_frame_ms(by_dir[dir_name], frame_count, f"{by_dir_key}.{dir_name}")

    clip_key = f"{clip_name}_frame_ms"
    if clip_key in manifest:
        return _expand_frame_ms(manifest[clip_key], frame_count, clip_key)

    default_ms = DEFAULT_CLIP_FRAME_MS.get(clip_name, 120)
    return tuple([default_ms] * frame_count)


def _build_clip_defs(
    manifest: dict[str, Any], parsed_clips: dict[str, dict[str, tuple[str, ...]]]
) -> list[ClipDef]:
    ordered_names = _clip_order(parsed_clips)
    clip_defs: list[ClipDef] = []

    for clip_name in ordered_names:
        src_dirs = parsed_clips[clip_name]
        dirs: dict[str, ClipDirDef] = {}
        for dir_name in DIR_ORDER:
            frame_names = src_dirs.get(dir_name)
            if not frame_names:
                continue
            frame_ms = _clip_dir_frame_ms(manifest, clip_name, dir_name, len(frame_names))
            dirs[dir_name] = ClipDirDef(frame_names=frame_names, frame_ms=frame_ms)

        if not dirs:
            continue

        fallback_dir = "down"
        if fallback_dir not in dirs:
            fallback_dir = next(iter(dirs.keys()))

        loop_key = f"{clip_name}_loop"
        loop = int(bool(manifest.get(loop_key, True)))
        clip_defs.append(ClipDef(name=clip_name, loop=loop, fallback_dir=fallback_dir, dirs=dirs))

    return clip_defs


def _quantize_tone5_from_lum(lum: int) -> int:
    # Nearest center in 5 visible shades: white, 1/4, 2/4, 3/4, black.
    centers = (255, 191, 127, 63, 0)
    best_ix = 0
    best_dist = abs(lum - centers[0])
    for ix in range(1, len(centers)):
        dist = abs(lum - centers[ix])
        if dist < best_dist:
            best_dist = dist
            best_ix = ix
    return best_ix


def _encode_tone5_to_level_mask(tone: int) -> tuple[int, int]:
    if tone <= 0:
        return (0, 1)  # white
    if tone == 1:
        return (1, 1)  # 1/4 black
    if tone == 2:
        return (2, 1)  # 2/4 black
    if tone == 3:
        return (1, 0)  # 3/4 black (extended shade via mask=0)
    return (3, 1)      # black


def _pack_2bpp_mask_planes(img_rgba, x0: int, y0: int, w: int, h: int) -> FramePlanes:
    color_stride = (w + 3) // 4
    mask_stride = (w + 7) // 8
    color2bpp = bytearray(h * color_stride)
    mask1bpp = bytearray(h * mask_stride)

    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x0 + x, y0 + y))
            if a == 0:
                level = 0
                mask_bit = 0
            else:
                lum = (299 * r + 587 * g + 114 * b) // 1000
                tone = _quantize_tone5_from_lum(lum)
                level, mask_bit = _encode_tone5_to_level_mask(tone)

            if mask_bit != 0:
                mask_idx = y * mask_stride + (x >> 3)
                mask1bpp[mask_idx] |= 1 << (7 - (x & 7))

            color_idx = y * color_stride + (x >> 2)
            shift = 6 - (2 * (x & 0x3))
            color2bpp[color_idx] |= level << shift

    return FramePlanes(color2bpp=bytes(color2bpp), mask1bpp=bytes(mask1bpp))


def _extract_frames_for_names(
    img_rgba,
    frame_map: dict[str, tuple[int, int]],
    frame_names: list[str],
    cell_w: int,
    cell_h: int,
) -> dict[str, FramePlanes]:
    sheet_w, sheet_h = img_rgba.size
    out: dict[str, FramePlanes] = {}

    for frame_name in frame_names:
        if frame_name not in frame_map:
            raise ValueError(f"clip references unknown frame '{frame_name}'")
        col, row = frame_map[frame_name]
        x0 = col * cell_w
        y0 = row * cell_h
        if (x0 + cell_w) > sheet_w or (y0 + cell_h) > sheet_h:
            raise ValueError(
                f"frame '{frame_name}' at [{col},{row}] out of bounds for sheet "
                f"{sheet_w}x{sheet_h} with cell {cell_w}x{cell_h}"
            )
        out[frame_name] = _pack_2bpp_mask_planes(img_rgba, x0, y0, cell_w, cell_h)

    return out


def _build_header(
    sheet_path: Path,
    cell_w: int,
    cell_h: int,
    frame_planes: dict[str, FramePlanes],
    clip_defs: list[ClipDef],
    symbol_prefix: str,
) -> str:
    try:
        sheet_label = sheet_path.relative_to(ROOT).as_posix()
    except ValueError:
        sheet_label = sheet_path.as_posix()

    color_stride = (cell_w + 3) // 4
    mask_stride = (cell_w + 7) // 8
    prefix = _sanitize_ident(symbol_prefix.lower())
    prefix_u = prefix.upper()

    frame_order: list[str] = []
    for clip in clip_defs:
        for dir_name in DIR_ORDER:
            dir_def = clip.dirs.get(dir_name)
            if dir_def is None:
                continue
            for frame_name in dir_def.frame_names:
                if frame_name not in frame_order:
                    frame_order.append(frame_name)

    blocks: list[str] = []
    for frame_name in frame_order:
        planes = frame_planes[frame_name]
        token = _sanitize_ident(frame_name)
        blocks.append(_emit_u8_array(f"{prefix}_{token}_color2bpp", planes.color2bpp))
        blocks.append("")
        blocks.append(_emit_u8_array(f"{prefix}_{token}_mask", planes.mask1bpp))
        blocks.append("")

    clip_blocks: list[str] = []
    clip_enum_lines: list[str] = []
    clip_table_lines: list[str] = []
    clip_lookup: dict[str, int] = {}

    for idx, clip in enumerate(clip_defs):
        clip_token = _sanitize_ident(clip.name.lower())
        clip_lookup[clip.name] = idx
        clip_enum_lines.append(f"  {prefix_u}_CLIP_INDEX_{clip_token.upper()} = {idx}u,")

        for dir_name in DIR_ORDER:
            dir_def = clip.dirs.get(dir_name)
            if dir_def is None:
                continue
            dir_token = _sanitize_ident(dir_name)
            arr_name = f"{prefix}_clip_{clip_token}_{dir_token}_frames"
            entries = []
            for frame_name, frame_ms in zip(dir_def.frame_names, dir_def.frame_ms):
                frame_token = _sanitize_ident(frame_name)
                entries.append(
                    f"  {{ {prefix}_{frame_token}_color2bpp, {prefix}_{frame_token}_mask, {int(frame_ms)}u }},"
                )
            clip_blocks.append(f"static const game_sprite_frame_t {arr_name}[] = {{")
            clip_blocks.extend(entries)
            clip_blocks.append("};")
            clip_blocks.append("")

        track_arr_name = f"{prefix}_clip_{clip_token}_dir_tracks"
        clip_blocks.append(f"static const game_sprite_dir_track_t {track_arr_name}[GAME_SPRITE_DIR_COUNT] = {{")
        for dir_name in DIR_ORDER:
            dir_def = clip.dirs.get(dir_name)
            if dir_def is None:
                continue
            dir_token = _sanitize_ident(dir_name)
            arr_name = f"{prefix}_clip_{clip_token}_{dir_token}_frames"
            dir_enum = DIR_ENUM[dir_name]
            clip_blocks.append(
                f"  [{dir_enum}] = {{ {arr_name}, {(len(dir_def.frame_names))}u }},"
            )
        clip_blocks.append("};")
        clip_blocks.append("")

        clip_table_lines.append(
            "  { "
            f"{track_arr_name}, "
            f"{clip.loop}u, "
            f"(uint8_t){DIR_ENUM[clip.fallback_dir]} "
            "},"
        )

    clip_count = len(clip_defs)
    idle_idx = clip_lookup.get("idle", 0xFF)
    walk_idx = clip_lookup.get("walk", 0xFF)

    mapping_lines = []
    for clip in clip_defs:
        for dir_name in DIR_ORDER:
            dir_def = clip.dirs.get(dir_name)
            if dir_def is None:
                continue
            mapping_lines.append(f"//   clip '{clip.name}' dir '{dir_name}' -> {', '.join(dir_def.frame_names)}")

    return (
        "// Auto-generated by tools/gen_player_sprites.py. Do not edit manually.\n"
        "#pragma once\n"
        "\n"
        "#include <stdint.h>\n"
        "#include \"game_sprite_anim.h\"\n"
        "\n"
        f"enum {{ {prefix_u}_SPRITE_W_PX = {cell_w}, {prefix_u}_SPRITE_H_PX = {cell_h} }};\n"
        f"enum {{ {prefix_u}_SPRITE_COLOR_ROW_BYTES = {color_stride}, {prefix_u}_SPRITE_MASK_ROW_BYTES = {mask_stride} }};\n"
        "\n"
        + "\n".join(blocks).rstrip()
        + "\n\n"
        + "\n".join(clip_blocks).rstrip()
        + "\n\n"
        "enum {\n"
        + ("\n".join(clip_enum_lines) if clip_enum_lines else f"  {prefix_u}_CLIP_INDEX_DEFAULT = 0u,")
        + f"\n  {prefix_u}_CLIP_COUNT = {clip_count}u\n"
        "};\n"
        f"#define {prefix_u}_CLIP_IDLE_INDEX ((uint8_t){idle_idx}u)\n"
        f"#define {prefix_u}_CLIP_WALK_INDEX ((uint8_t){walk_idx}u)\n"
        f"static const game_sprite_clip_t {prefix}_sprite_clips[{prefix_u}_CLIP_COUNT] = {{\n"
        + ("\n".join(clip_table_lines) if clip_table_lines else "  { 0, 0u, (uint8_t)GAME_SPRITE_DIR_DOWN },")
        + "\n};\n"
        f"static const game_sprite_set_t {prefix}_sprite_set = {{\n"
        f"  {prefix}_sprite_clips,\n"
        f"  (uint8_t){prefix_u}_CLIP_COUNT\n"
        "};\n"
        "\n"
        f"// Source sheet: {sheet_label}\n"
        "// Resolved clip mapping:\n"
        + ("\n".join(mapping_lines) if mapping_lines else "//   (none)")
        + "\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate player_sprites_autogen.h from player sheet.")
    parser.add_argument("--sheet", default=str(DEFAULT_SHEET), help="Path to player sheet PNG")
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST), help="Optional frame mapping manifest JSON")
    parser.add_argument("--out", default=str(DEFAULT_OUT), help="Output header path")
    parser.add_argument("--symbol-prefix", default="player", help="C symbol prefix for generated arrays/metadata")
    args = parser.parse_args()

    sheet_path = _to_repo_path(args.sheet)
    manifest_path = _to_repo_path(args.manifest)
    out_path = _to_repo_path(args.out)

    if not sheet_path.exists():
        raise FileNotFoundError(f"sheet not found: {sheet_path}")

    cell_w, cell_h, frame_map, manifest = _load_manifest(manifest_path)
    parsed_clips = _parse_clips_from_frames(frame_map)
    clip_defs = _build_clip_defs(manifest, parsed_clips)
    if not clip_defs:
        raise ValueError("no clips discovered from frame names; use '<clip>_<dir>_<index>' naming")

    try:
        from PIL import Image  # type: ignore
    except Exception as exc:
        raise RuntimeError("Pillow is required. Install with: pip install pillow") from exc

    used_frame_names: list[str] = []
    for clip in clip_defs:
        for dir_name in DIR_ORDER:
            dir_def = clip.dirs.get(dir_name)
            if dir_def is None:
                continue
            for frame_name in dir_def.frame_names:
                if frame_name not in used_frame_names:
                    used_frame_names.append(frame_name)

    img = Image.open(sheet_path).convert("RGBA")
    frame_planes = _extract_frames_for_names(img, frame_map, used_frame_names, cell_w, cell_h)

    header_text = _build_header(sheet_path, cell_w, cell_h, frame_planes, clip_defs, args.symbol_prefix)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(header_text, encoding="utf-8")

    print(f"wrote {out_path}")
    print(f"exported {len(used_frame_names)} frames across {len(clip_defs)} clips")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
