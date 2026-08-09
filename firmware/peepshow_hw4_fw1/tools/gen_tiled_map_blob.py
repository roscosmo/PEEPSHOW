#!/usr/bin/env python3
"""
Convert a Tiled JSON map into PeepShow compact map blob (TMAP v2).
"""

from __future__ import annotations

import argparse
import json
import struct
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


MAGIC = 0x50414D54  # "TMAP"
VERSION = 2
MAX_BYTES = 32768

HEADER_FMT = "<IHHIIHHBBHIIIIIHH"
OBJECT_FMT = "<HHhhhhII"

HEADER_SIZE = struct.calcsize(HEADER_FMT)
OBJECT_SIZE = struct.calcsize(OBJECT_FMT)
CRC32_OFFSET = 12

FLIP_MASK = 0xE0000000
GID_MASK = 0x1FFFFFFF

OBJ_NONE = 0
OBJ_SPAWN = 1
OBJ_INTERACT = 2
OBJ_EXIT = 3
OBJ_CAMERA_ZONE = 4
OBJ_INDOOR_ZONE = 5
OBJ_LIGHT = 6

FLAG_SOLID = 1 << 0
FLAG_WATER = 1 << 1
FLAG_SLOW = 1 << 2
FLAG_OCCLUDER = 1 << 3
FLAG_ROOF = 1 << 4
FLAG_EMISSIVE = 1 << 5


def _align4(n: int) -> int:
    return (n + 3) & ~3


def _fnv1a32(text: str) -> int:
    h = 0x811C9DC5
    for b in text.encode("utf-8"):
        h ^= b
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


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


def _props_list_to_dict(props: Any) -> dict[str, Any]:
    out: dict[str, Any] = {}
    if not isinstance(props, list):
        return out
    for p in props:
        if isinstance(p, dict) and "name" in p and "value" in p:
            out[str(p["name"])] = p["value"]
    return out


def _boolish(v: Any) -> bool:
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return v.strip().lower() in {"1", "true", "yes", "on"}
    return False


def _intish(v: Any, default: int = 0) -> int:
    if isinstance(v, bool):
        return int(v)
    if isinstance(v, int):
        return v
    if isinstance(v, float):
        return int(round(v))
    if isinstance(v, str):
        try:
            return int(v, 10)
        except ValueError:
            return default
    return default


def _tile_flags_from_props(props: dict[str, Any]) -> int:
    flags = 0
    if _boolish(props.get("solid")):
        flags |= FLAG_SOLID
    if _boolish(props.get("water")):
        flags |= FLAG_WATER
    if _boolish(props.get("slow")):
        flags |= FLAG_SLOW
    if _boolish(props.get("occluder")):
        flags |= FLAG_OCCLUDER
    if _boolish(props.get("roof")):
        flags |= FLAG_ROOF
    if _boolish(props.get("emissive")):
        flags |= FLAG_EMISSIVE
    return flags


def _strip_gid(raw_gid: int) -> int:
    return raw_gid & GID_MASK


def _load_tileset_flags_from_tsx(tsx_path: Path) -> dict[int, int]:
    out: dict[int, int] = {}
    if not tsx_path.exists():
        return out

    tree = ET.parse(tsx_path)
    root = tree.getroot()
    for tile in root.findall("tile"):
        tid_attr = tile.get("id")
        if tid_attr is None:
            continue
        try:
            local_id = int(tid_attr, 10)
        except ValueError:
            continue

        props_node = tile.find("properties")
        props: dict[str, Any] = {}
        if props_node is not None:
            for p in props_node.findall("property"):
                name = p.get("name")
                if not name:
                    continue
                ptype = p.get("type", "string")
                val_attr = p.get("value")
                if val_attr is None:
                    val_attr = p.text if p.text is not None else ""
                if ptype == "bool":
                    props[name] = _boolish(val_attr)
                elif ptype in {"int", "float"}:
                    props[name] = _intish(val_attr, 0)
                else:
                    props[name] = str(val_attr)

        out[local_id] = _tile_flags_from_props(props)
    return out


def _load_tileset_local_flags(map_path: Path, map_obj: dict[str, Any]) -> list[tuple[int, dict[int, int]]]:
    result: list[tuple[int, dict[int, int]]] = []

    tilesets = map_obj.get("tilesets")
    if not isinstance(tilesets, list):
        return result

    for ts in tilesets:
        if not isinstance(ts, dict):
            continue
        first_gid = _intish(ts.get("firstgid"), 0)
        if first_gid <= 0:
            continue

        local_flags: dict[int, int] = {}

        tiles = ts.get("tiles")
        if isinstance(tiles, list):
            for t in tiles:
                if not isinstance(t, dict):
                    continue
                local_id = _intish(t.get("id"), -1)
                if local_id < 0:
                    continue
                local_flags[local_id] = _tile_flags_from_props(_props_list_to_dict(t.get("properties")))

        src = ts.get("source")
        if isinstance(src, str) and src.strip():
            tsx_path = (map_path.parent / src).resolve()
            tsx_flags = _load_tileset_flags_from_tsx(tsx_path)
            if tsx_flags:
                local_flags.update(tsx_flags)

        result.append((first_gid, local_flags))

    result.sort(key=lambda x: x[0])
    return result


def _resolve_tile_flags(gid: int, tilesets: list[tuple[int, dict[int, int]]]) -> int:
    if gid == 0:
        return 0
    chosen_first_gid = -1
    chosen_map: dict[int, int] | None = None
    for first_gid, flag_map in tilesets:
        if first_gid <= gid and first_gid > chosen_first_gid:
            chosen_first_gid = first_gid
            chosen_map = flag_map
    if chosen_map is None:
        return 0
    local_id = gid - chosen_first_gid
    return chosen_map.get(local_id, 0)


def _pick_tile_layers(map_obj: dict[str, Any], layer_name: str | None) -> list[dict[str, Any]]:
    layers = map_obj.get("layers")
    if not isinstance(layers, list):
        raise ValueError("map.layers must be a list")

    tile_layers = [l for l in layers if isinstance(l, dict) and l.get("type") == "tilelayer"]
    if not tile_layers:
        raise ValueError("map has no tilelayer")

    if layer_name:
        token = layer_name.strip().lower()
        if token in {"*", "all", "visible", "all_visible"}:
            visible_layers = [l for l in tile_layers if _boolish(l.get("visible", True))]
            if visible_layers:
                return visible_layers
            return [tile_layers[0]]

        for l in tile_layers:
            if str(l.get("name", "")) == layer_name:
                return [l]
        raise ValueError(f"tile layer '{layer_name}' not found")

    visible_layers = [l for l in tile_layers if _boolish(l.get("visible", True))]
    if visible_layers:
        return visible_layers
    return [tile_layers[0]]


def _pick_object_layer(map_obj: dict[str, Any], layer_name: str | None) -> dict[str, Any] | None:
    layers = map_obj.get("layers")
    if not isinstance(layers, list):
        return None

    object_layers = [l for l in layers if isinstance(l, dict) and l.get("type") == "objectgroup"]
    if not object_layers:
        return None

    if layer_name:
        for l in object_layers:
            if str(l.get("name", "")) == layer_name:
                return l
        return None

    for l in object_layers:
        if _boolish(l.get("visible", True)):
            return l
    return object_layers[0]


def _object_type(name: str, obj_type: str) -> int:
    key = (obj_type.strip() or name.strip()).lower()
    if "spawn" in key:
        return OBJ_SPAWN
    if key == "exit" or "exit" in key or "door" in key:
        return OBJ_EXIT
    if key == "camerazone" or "camera" in key:
        return OBJ_CAMERA_ZONE
    if key == "indoorzone" or "indoor" in key:
        return OBJ_INDOOR_ZONE
    if key == "light":
        return OBJ_LIGHT
    if key == "interact" or key == "npc" or "interact" in key or "npc" in key:
        return OBJ_INTERACT
    return OBJ_NONE


def _build_object_record(obj: dict[str, Any]) -> bytes | None:
    name = str(obj.get("name", ""))
    obj_type = str(obj.get("type", ""))
    props = _props_list_to_dict(obj.get("properties"))
    otype = _object_type(name, obj_type)
    if otype == OBJ_NONE:
        return None

    flags = 1 if _boolish(obj.get("visible", True)) else 0
    x_px = _intish(obj.get("x"), 0)
    y_px = _intish(obj.get("y"), 0)
    w_px = _intish(obj.get("width"), 0)
    h_px = _intish(obj.get("height"), 0)
    arg0 = 0
    arg1 = 0

    if otype == OBJ_SPAWN:
        arg0 = _fnv1a32(str(props.get("kind", name)))
        arg1 = _fnv1a32(str(props.get("spawn_id", props.get("id", name))))
    elif otype == OBJ_INTERACT:
        script_key = str(props.get("script_id", props.get("action", name)))
        arg0 = _fnv1a32(script_key)
        if "dialogue_id" in props:
            arg1 = _fnv1a32(str(props.get("dialogue_id", "")))
        elif "text_id" in props:
            arg1 = _intish(props.get("text_id"), 0) & 0xFFFFFFFF
        else:
            arg1 = _fnv1a32(str(props.get("dialog_id", props.get("text", ""))))
    elif otype == OBJ_EXIT:
        arg0 = _fnv1a32(str(props.get("target_map", props.get("to_map", ""))))
        arg1 = _fnv1a32(str(props.get("target_spawn", props.get("to_spawn", ""))))
    elif otype == OBJ_CAMERA_ZONE:
        arg0 = _intish(props.get("zoom"), 0) & 0xFFFFFFFF
        arg1 = _intish(props.get("priority"), 0) & 0xFFFFFFFF
    elif otype == OBJ_INDOOR_ZONE:
        arg0 = _intish(props.get("ambient"), 0) & 0xFFFFFFFF
        arg1 = _intish(props.get("ambient_night"), 0) & 0xFFFFFFFF
    elif otype == OBJ_LIGHT:
        arg0 = _intish(props.get("radius"), 0) & 0xFFFFFFFF
        arg1 = _intish(props.get("intensity"), 0) & 0xFFFFFFFF

    return struct.pack(
        OBJECT_FMT,
        otype & 0xFFFF,
        flags & 0xFFFF,
        x_px,
        y_px,
        w_px,
        h_px,
        arg0 & 0xFFFFFFFF,
        arg1 & 0xFFFFFFFF,
    )


def build_blob(map_path: Path, tile_layer_name: str | None, object_layer_name: str | None) -> bytes:
    map_obj = json.loads(map_path.read_text(encoding="utf-8"))
    if not isinstance(map_obj, dict):
        raise ValueError("map root must be object")

    map_w = _intish(map_obj.get("width"), 0)
    map_h = _intish(map_obj.get("height"), 0)
    tile_w = _intish(map_obj.get("tilewidth"), 0)
    tile_h = _intish(map_obj.get("tileheight"), 0)
    if map_w <= 0 or map_h <= 0:
        raise ValueError("invalid map dimensions")
    if tile_w <= 0 or tile_h <= 0:
        raise ValueError("invalid tile dimensions")

    tile_count = map_w * map_h
    tile_layers = _pick_tile_layers(map_obj, tile_layer_name)
    layer_count = len(tile_layers)
    if layer_count <= 0:
        raise ValueError("no tile layers selected")

    layer_data: list[list[int]] = []
    for layer in tile_layers:
        data = layer.get("data")
        if not isinstance(data, list):
            raise ValueError(f"tile layer '{layer.get('name', '')}' data must be array")
        if len(data) != tile_count:
            raise ValueError(
                f"tile layer '{layer.get('name', '')}' data size {len(data)} != width*height {tile_count}"
            )
        gids = [_strip_gid(_intish(raw, 0)) for raw in data]
        layer_data.append(gids)

    tilesets = _load_tileset_local_flags(map_path, map_obj)

    tile_flags = bytearray(tile_count)
    for i in range(tile_count):
        flags = 0
        for layer in layer_data:
            gid = layer[i]
            if gid != 0:
                flags |= _resolve_tile_flags(gid, tilesets)
        tile_flags[i] = flags & 0xFF

    tile_gids = bytearray()
    for layer in layer_data:
        for i, gid in enumerate(layer):
            if gid > 0xFFFF:
                raise ValueError(f"gid {gid} exceeds uint16 at index {i}")
            tile_gids.extend(struct.pack("<H", gid & 0xFFFF))

    object_bytes = bytearray()
    obj_layer = _pick_object_layer(map_obj, object_layer_name)
    if obj_layer is not None:
        objs = obj_layer.get("objects")
        if isinstance(objs, list):
            for obj in objs:
                if not isinstance(obj, dict):
                    continue
                rec = _build_object_record(obj)
                if rec is not None:
                    object_bytes.extend(rec)

    tile_flags_offset = HEADER_SIZE
    tile_gids_offset = _align4(tile_flags_offset + len(tile_flags))
    objects_offset = _align4(tile_gids_offset + len(tile_gids))
    total_size = objects_offset + len(object_bytes)
    if total_size > MAX_BYTES:
        raise ValueError(f"blob size {total_size} exceeds max {MAX_BYTES}")

    blob = bytearray(total_size)

    header = struct.pack(
        HEADER_FMT,
        MAGIC,
        VERSION,
        HEADER_SIZE,
        total_size,
        0,  # CRC placeholder
        map_w & 0xFFFF,
        map_h & 0xFFFF,
        tile_w & 0xFF,
        tile_h & 0xFF,
        layer_count & 0xFFFF,
        tile_count,
        len(object_bytes) // OBJECT_SIZE,
        tile_flags_offset,
        tile_gids_offset,
        objects_offset,
        OBJECT_SIZE,
        0,
    )

    blob[0:HEADER_SIZE] = header
    blob[tile_flags_offset : tile_flags_offset + len(tile_flags)] = tile_flags
    blob[tile_gids_offset : tile_gids_offset + len(tile_gids)] = tile_gids
    blob[objects_offset : objects_offset + len(object_bytes)] = object_bytes

    crc = _crc32_masked(bytes(blob))
    blob[CRC32_OFFSET : CRC32_OFFSET + 4] = struct.pack("<I", crc)
    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate compact TMAP blob from Tiled JSON.")
    parser.add_argument("--in-json", type=Path, required=True, help="Input Tiled map JSON path")
    parser.add_argument("--out-bin", type=Path, required=True, help="Output .bin path")
    parser.add_argument(
        "--tile-layer",
        default="",
        help="Tile layer name. Use '*'/all_visible to include all visible tile layers (default behavior).",
    )
    parser.add_argument(
        "--objects-layer",
        default="objects",
        help="Object layer name (default: objects). If missing, first visible objectgroup is used.",
    )
    args = parser.parse_args()

    tile_layer_name = args.tile_layer if args.tile_layer else None
    obj_layer_name = args.objects_layer if args.objects_layer else None
    blob = build_blob(args.in_json, tile_layer_name, obj_layer_name)

    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(blob)
    print(f"wrote {args.out_bin} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
