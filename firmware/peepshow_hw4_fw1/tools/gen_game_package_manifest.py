#!/usr/bin/env python3
"""
Generate a binary GAME_PACKAGE_MANIFEST (MGPK) blob from JSON.

Layout and limits are defined by Core/Inc/game_package_manifest.h.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any


MAGIC = 0x4B50474D  # "MGPK"
VERSION_V1 = 1
VERSION_V2 = 2
VERSION_V3 = 3
VERSION_V4 = 4
VERSION_V5 = 5
VERSION_DEFAULT = VERSION_V5
MAX_BYTES = 4096
MAX_MODE_COUNT = 16
MAX_PET_ROUTE_COUNT = 16
MAX_PET_MENU_ITEM_COUNT = 32

HEADER_V12_FMT = "<IHHIIIIHHII"
HEADER_V3_FMT = "<IHHIIIIHHIIHHI"
MODE_V1_FMT = "<IHHI"
MODE_V2_FMT = "<IHH" + ("I" * 17) + "ii"
MODE_V4_FMT = "<IHH" + ("I" * 17) + "ii" + ("I" * 6)
MODE_V5_FMT = "<IHH" + ("I" * 17) + "ii" + ("I" * 8)
PET_ROUTE_FMT = "<HHI"
PET_MENU_ITEM_FMT = "<BBBBHH"

GAME_PET_MENU_SLOT_COUNT = 10
GAME_PET_MENU_ACTION_COUNT = 10

PET_MENU_SELECT_KIND_BY_NAME = {
    "none": 0,
    "feed": 1,
    "play": 2,
    "start_game": 3,
    "options": 4,
    "launch_mode": 5,
    "open_page": 6,
    "sand_fx": 7,
}
PET_MENU_STATUS_KIND_BY_NAME = {
    "none": 0,
    "bool": 1,
    "level4": 2,
}
PET_MENU_STATUS_SOURCE_BY_NAME = {
    "none": 0,
    "battery": 1,
}

NATIVE_PAGE_SYMBOL_BY_KEY = {
    "home": "UI_PAGE_HOME_NATIVE",
    "pet": "UI_PAGE_PET_NATIVE",
    "battery_stats": "UI_PAGE_BATT_STATS_NATIVE",
    "audio_levels": "UI_PAGE_AUDIO_LEVELS_NATIVE",
    "lis2": "UI_PAGE_LIS2_NATIVE",
    "lis2_steps": "UI_PAGE_LIS2_STEPS_NATIVE",
    "joy_cal": "UI_PAGE_JOY_CAL_NATIVE",
    "joy_target": "UI_PAGE_JOY_TARGET_NATIVE",
}
NATIVE_TREE_META_BY_KEY = {
    "system_root": ("UI_TREE_ID_SYSTEM_ROOT", "UI_MENU_SYSTEM"),
    "pet_feed": ("UI_TREE_ID_PET_FEED", "UI_MENU_PET_FEED"),
}

HEADER_V12_SIZE = struct.calcsize(HEADER_V12_FMT)
HEADER_V3_SIZE = struct.calcsize(HEADER_V3_FMT)
MODE_V1_SIZE = struct.calcsize(MODE_V1_FMT)
MODE_V2_SIZE = struct.calcsize(MODE_V2_FMT)
MODE_V4_SIZE = struct.calcsize(MODE_V4_FMT)
MODE_V5_SIZE = struct.calcsize(MODE_V5_FMT)
PET_ROUTE_SIZE = struct.calcsize(PET_ROUTE_FMT)
PET_MENU_ITEM_SIZE = struct.calcsize(PET_MENU_ITEM_FMT)

CRC32_OFFSET_IN_HEADER = 12


def _u32(value: Any, field: str) -> int:
    if not isinstance(value, int):
        raise ValueError(f"{field} must be integer")
    if value < 0 or value > 0xFFFFFFFF:
        raise ValueError(f"{field} out of uint32 range")
    return value


def _u16(value: Any, field: str) -> int:
    if not isinstance(value, int):
        raise ValueError(f"{field} must be integer")
    if value < 0 or value > 0xFFFF:
        raise ValueError(f"{field} out of uint16 range")
    return value


def _u8(value: Any, field: str) -> int:
    if not isinstance(value, int):
        raise ValueError(f"{field} must be integer")
    if value < 0 or value > 0xFF:
        raise ValueError(f"{field} out of uint8 range")
    return value


def _s32(value: Any, field: str) -> int:
    if not isinstance(value, int):
        raise ValueError(f"{field} must be integer")
    if value < -0x80000000 or value > 0x7FFFFFFF:
        raise ValueError(f"{field} out of int32 range")
    return value


def _as_str(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def _parse_enum_u32(value: Any, mapping: dict[str, int], field: str) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        key = value.strip().lower()
        if key in mapping:
            return mapping[key]
        try:
            return int(key, 10)
        except ValueError:
            pass
    raise ValueError(f"{field} must be an integer or one of: {', '.join(sorted(mapping.keys()))}")


def _load_json_array(path: Path, field_name: str) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, list):
        raise ValueError(f"{path.as_posix()} must be a JSON array for {field_name}.")
    out: list[dict[str, Any]] = []
    for idx, item in enumerate(raw):
        if not isinstance(item, dict):
            raise ValueError(f"{path.as_posix()}[{idx}] must be an object.")
        out.append(item)
    return out


def _crc32_ieee_masked_crc_field(blob: bytes) -> int:
    crc = 0xFFFFFFFF
    for i, b in enumerate(blob):
        v = 0 if CRC32_OFFSET_IN_HEADER <= i < (CRC32_OFFSET_IN_HEADER + 4) else b
        crc ^= v
        for _ in range(8):
            if (crc & 1) != 0:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (~crc) & 0xFFFFFFFF


def _load_profile_index(path: Path, domain: str) -> dict[str, dict[str, Any]]:
    if not path.is_file():
        return {}
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, list):
        raise ValueError(f"{path.as_posix()} must be a JSON array.")
    out: dict[str, dict[str, Any]] = {}
    for ix, item in enumerate(raw):
        if not isinstance(item, dict):
            raise ValueError(f"{domain}[{ix}] must be an object.")
        profile_key = str(item.get("id", "")).strip()
        if profile_key == "":
            raise ValueError(f"{domain}[{ix}] missing required field 'id'.")
        if profile_key in out:
            raise ValueError(f"{domain} duplicate id '{profile_key}'.")
        out[profile_key] = item
    return out


def _find_repo_root_for_manifest(manifest_path: Path) -> Path | None:
    cur = manifest_path.resolve()
    if cur.is_file():
        cur = cur.parent
    for _ in range(24):
        if (cur / "Assets" / "game_project" / "project.json").is_file():
            return cur
        if cur.parent == cur:
            break
        cur = cur.parent
    return None


def _load_profile_indexes_for_manifest(manifest_path: Path) -> dict[str, dict[str, dict[str, Any]]]:
    repo_root = _find_repo_root_for_manifest(manifest_path)
    if repo_root is None:
        return {}
    gp_dir = repo_root / "Assets" / "game_project"
    return {
        "controller_profiles": _load_profile_index(gp_dir / "controller_profiles.json", "controller_profiles"),
        "camera_profiles": _load_profile_index(gp_dir / "camera_profiles.json", "camera_profiles"),
        "input_profiles": _load_profile_index(gp_dir / "input_profiles.json", "input_profiles"),
    }


def _load_game_project_domains_for_manifest(manifest_path: Path) -> dict[str, list[dict[str, Any]]]:
    repo_root = _find_repo_root_for_manifest(manifest_path)
    if repo_root is None:
        return {}
    gp_dir = repo_root / "Assets" / "game_project"
    return {
        "pages": _load_json_array(gp_dir / "pages.json", "pages"),
        "pet_menu_slots": _load_json_array(gp_dir / "pet_menu_slots.json", "pet_menu_slots"),
    }


def _compile_pet_menu_items_from_slots(
    data: dict[str, Any],
    pages: list[dict[str, Any]],
    pet_menu_slots: list[dict[str, Any]],
) -> list[dict[str, Any]] | None:
    if len(pet_menu_slots) == 0:
        return None

    modes_raw = data.get("modes", [])
    if not isinstance(modes_raw, list):
        raise ValueError("modes must be a list before compiling pet_menu_slots.")

    mode_id_by_key: dict[str, int] = {}
    for idx, mode in enumerate(modes_raw):
        if not isinstance(mode, dict):
            raise ValueError(f"modes[{idx}] must be an object")
        mode_key = _as_str(mode.get("id"))
        mode_id = mode.get("mode_id")
        if mode_key == "":
            raise ValueError(f"modes[{idx}] missing id required by pet_menu_slots compiler.")
        if not isinstance(mode_id, int) or mode_id <= 0:
            raise ValueError(f"modes[{idx}] missing/invalid mode_id required by pet_menu_slots compiler.")
        if mode_key in mode_id_by_key:
            raise ValueError(f"duplicate mode key '{mode_key}' in modes[].")
        mode_id_by_key[mode_key] = mode_id

    page_id_by_key: dict[str, int] = {}
    for idx, page in enumerate(pages):
        page_key = _as_str(page.get("id"))
        page_id = page.get("page_id")
        if page_key == "":
            raise ValueError(f"pages[{idx}] missing id.")
        if not isinstance(page_id, int) or page_id <= 0:
            raise ValueError(f"pages[{idx}] missing/invalid page_id.")
        if page_key in page_id_by_key:
            raise ValueError(f"duplicate page id '{page_key}' in pages.json.")
        page_id_by_key[page_key] = page_id

    compiled: list[dict[str, Any]] = []
    seen_slots: set[int] = set()
    for idx, slot in enumerate(pet_menu_slots):
        slot_key = _as_str(slot.get("id"))
        slot_index = _u8(slot.get("slot_index"), f"pet_menu_slots[{idx}].slot_index")
        icon_action_id = _u8(slot.get("icon_action_id"), f"pet_menu_slots[{idx}].icon_action_id")
        select_kind = _parse_enum_u32(
            slot.get("select_kind"), PET_MENU_SELECT_KIND_BY_NAME, f"pet_menu_slots[{idx}].select_kind"
        )
        status_kind = _parse_enum_u32(
            slot.get("status_kind", "none"),
            PET_MENU_STATUS_KIND_BY_NAME,
            f"pet_menu_slots[{idx}].status_kind",
        )
        status_source_id = _parse_enum_u32(
            slot.get("status_source", "none"),
            PET_MENU_STATUS_SOURCE_BY_NAME,
            f"pet_menu_slots[{idx}].status_source",
        )

        if slot_index >= GAME_PET_MENU_SLOT_COUNT:
            raise ValueError(
                f"pet_menu_slots[{idx}] slot_index {slot_index} out of range 0..{GAME_PET_MENU_SLOT_COUNT - 1}"
            )
        if slot_index in seen_slots:
            raise ValueError(f"duplicate pet_menu_slots slot_index {slot_index}")
        seen_slots.add(slot_index)
        if icon_action_id >= GAME_PET_MENU_ACTION_COUNT:
            raise ValueError(
                f"pet_menu_slots[{idx}] icon_action_id {icon_action_id} out of range 0..{GAME_PET_MENU_ACTION_COUNT - 1}"
            )

        arg0 = 0
        if select_kind == PET_MENU_SELECT_KIND_BY_NAME["launch_mode"]:
            mode_key = _as_str(slot.get("mode_key"))
            if mode_key == "":
                raise ValueError(f"pet_menu_slots[{idx}] launch_mode requires mode_key")
            if mode_key not in mode_id_by_key:
                raise ValueError(
                    f"pet_menu_slots[{idx}] mode_key '{mode_key}' not found in manifest.modes[]"
                )
            arg0 = mode_id_by_key[mode_key]
        elif select_kind == PET_MENU_SELECT_KIND_BY_NAME["open_page"]:
            page_key = _as_str(slot.get("page_key"))
            if page_key == "":
                raise ValueError(f"pet_menu_slots[{idx}] open_page requires page_key")
            if page_key not in page_id_by_key:
                raise ValueError(
                    f"pet_menu_slots[{idx}] page_key '{page_key}' not found in pages.json"
                )
            arg0 = page_id_by_key[page_key]
        elif status_kind != PET_MENU_STATUS_KIND_BY_NAME["none"]:
            arg0 = _u16(
                slot.get("status_base_icon_action_id"),
                f"pet_menu_slots[{idx}].status_base_icon_action_id",
            )
            if arg0 >= GAME_PET_MENU_ACTION_COUNT:
                raise ValueError(
                    f"pet_menu_slots[{idx}] status_base_icon_action_id {arg0} out of range"
                )

        if status_kind == PET_MENU_STATUS_KIND_BY_NAME["none"]:
            if status_source_id != PET_MENU_STATUS_SOURCE_BY_NAME["none"]:
                raise ValueError(
                    f"pet_menu_slots[{idx}] status_kind=none requires status_source=none"
                )
        else:
            if status_source_id != PET_MENU_STATUS_SOURCE_BY_NAME["battery"]:
                raise ValueError(
                    f"pet_menu_slots[{idx}] non-none status_kind requires status_source=battery"
                )

        compiled.append(
            {
                "id": slot_key if slot_key != "" else f"slot_{slot_index}",
                "slot_index": slot_index,
                "icon_action_id": icon_action_id,
                "select_kind": select_kind,
                "status_kind": status_kind,
                "arg0": arg0,
                "status_source_id": status_source_id,
            }
        )

    compiled.sort(key=lambda rec: int(rec["slot_index"]))
    return compiled


def _emit_ui_page_registry_autogen(manifest_path: Path, pages: list[dict[str, Any]]) -> None:
    repo_root = _find_repo_root_for_manifest(manifest_path)
    if repo_root is None:
        return

    out_path = repo_root / "Core" / "Inc" / "ui" / "ui_page_registry_autogen.h"
    lines: list[str] = []
    lines.append("// Auto-generated by tools/gen_game_package_manifest.py. Do not edit manually.")
    lines.append("#ifndef UI_PAGE_REGISTRY_AUTOGEN_H")
    lines.append("#define UI_PAGE_REGISTRY_AUTOGEN_H")
    lines.append("")
    lines.append('#include "ui/ui_page_registry.h"')
    lines.append('#include "ui/ui_page_native.h"')
    lines.append('#include "ui/ui_menu_tree.h"')
    lines.append("")

    entries: list[str] = []
    seen_page_ids: set[int] = set()
    for idx, page in enumerate(pages):
        page_id = _u16(page.get("page_id"), f"pages[{idx}].page_id")
        if page_id in seen_page_ids:
            raise ValueError(f"duplicate pages.page_id {page_id} for ui page registry.")
        seen_page_ids.add(page_id)

        route_kind = _as_str(page.get("route_kind")).lower()
        if route_kind == "native_page":
            page_key = _as_str(page.get("native_page_key")).lower()
            symbol = NATIVE_PAGE_SYMBOL_BY_KEY.get(page_key)
            if symbol is None:
                raise ValueError(
                    f"pages[{idx}] native_page_key '{page_key}' is not supported in firmware registry."
                )
            entries.append(
                "  { %uu, UI_PAGE_ROUTE_NATIVE_PAGE, &%s, 0u, (const ui_menu_t *)0 }," % (page_id, symbol)
            )
        elif route_kind == "native_tree":
            tree_key = _as_str(page.get("native_tree_key")).lower()
            tree_meta = NATIVE_TREE_META_BY_KEY.get(tree_key)
            if tree_meta is None:
                raise ValueError(
                    f"pages[{idx}] native_tree_key '{tree_key}' is not supported in firmware registry."
                )
            tree_id, menu_symbol = tree_meta
            entries.append(
                "  { %uu, UI_PAGE_ROUTE_NATIVE_TREE, (const ui_page_t *)0, %s, &%s },"
                % (page_id, tree_id, menu_symbol)
            )
        else:
            raise ValueError(
                f"pages[{idx}] route_kind '{route_kind}' is invalid. Expected native_page or native_tree."
            )

    lines.append(f"#define UI_PAGE_REGISTRY_ENTRY_COUNT ({len(entries)}u)")
    lines.append("")
    lines.append(
        "static const ui_page_registry_entry_t g_ui_page_registry_entries[UI_PAGE_REGISTRY_ENTRY_COUNT] = {"
    )
    lines.extend(entries)
    lines.append("};")
    lines.append("")
    lines.append("#endif /* UI_PAGE_REGISTRY_AUTOGEN_H */")
    lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")


def build_manifest(
    data: dict[str, Any], profile_indexes: dict[str, dict[str, dict[str, Any]]] | None = None
) -> bytes:
    profile_indexes = profile_indexes or {}
    controller_profiles = profile_indexes.get("controller_profiles", {})
    camera_profiles = profile_indexes.get("camera_profiles", {})
    input_profiles = profile_indexes.get("input_profiles", {})
    manifest_version = _u16(
        data.get("manifest_version", VERSION_DEFAULT), "manifest_version"
    )
    if manifest_version not in (VERSION_V1, VERSION_V2, VERSION_V3, VERSION_V4, VERSION_V5):
        raise ValueError(
            f"manifest_version must be {VERSION_V1}, {VERSION_V2}, {VERSION_V3}, {VERSION_V4}, or {VERSION_V5}"
        )

    pkg_id = _u32(data.get("package_id"), "package_id")
    pkg_ver = _u32(data.get("package_version"), "package_version")

    modes_raw = data.get("modes")
    if not isinstance(modes_raw, list):
        raise ValueError("modes must be a list")
    if len(modes_raw) == 0:
        raise ValueError("modes must not be empty")
    if len(modes_raw) > MAX_MODE_COUNT:
        raise ValueError(f"modes count exceeds {MAX_MODE_COUNT}")

    routes_raw = data.get("pet_routes", [])
    if not isinstance(routes_raw, list):
        raise ValueError("pet_routes must be a list")
    if len(routes_raw) > MAX_PET_ROUTE_COUNT:
        raise ValueError(f"pet_routes count exceeds {MAX_PET_ROUTE_COUNT}")

    pet_menu_items_raw = data.get("pet_menu_items", [])
    if not isinstance(pet_menu_items_raw, list):
        raise ValueError("pet_menu_items must be a list")
    if len(pet_menu_items_raw) > MAX_PET_MENU_ITEM_COUNT:
        raise ValueError(f"pet_menu_items count exceeds {MAX_PET_MENU_ITEM_COUNT}")
    if manifest_version not in (VERSION_V3, VERSION_V4, VERSION_V5) and len(pet_menu_items_raw) > 0:
        raise ValueError("pet_menu_items require manifest_version 3, 4, or 5")

    mode_ids: set[int] = set()
    mode_bytes = bytearray()
    for idx, m in enumerate(modes_raw):
        if not isinstance(m, dict):
            raise ValueError(f"modes[{idx}] must be an object")
        mode_id = _u32(m.get("mode_id"), f"modes[{idx}].mode_id")
        runtime_kind = _u16(m.get("runtime_kind"), f"modes[{idx}].runtime_kind")
        backend_id = _u16(m.get("backend_id"), f"modes[{idx}].backend_id")
        if mode_id in mode_ids:
            raise ValueError(f"duplicate mode_id {mode_id}")
        mode_ids.add(mode_id)
        if manifest_version == VERSION_V1:
            mode_bytes.extend(
                struct.pack(MODE_V1_FMT, mode_id, runtime_kind, backend_id, 0)
            )
        else:
            scene_map_addr = _u32(
                m.get("scene_map_addr", 0), f"modes[{idx}].scene_map_addr"
            )
            scene_map_size_bytes = _u32(
                m.get("scene_map_size_bytes", 0),
                f"modes[{idx}].scene_map_size_bytes",
            )
            scene_tileset_addr = _u32(
                m.get("scene_tileset_addr", 0), f"modes[{idx}].scene_tileset_addr"
            )
            scene_tileset_size_bytes = _u32(
                m.get("scene_tileset_size_bytes", 0),
                f"modes[{idx}].scene_tileset_size_bytes",
            )
            controller_profile_key = str(m.get("controller_profile_key", "")).strip()
            camera_profile_key = str(m.get("camera_profile_key", "")).strip()
            input_profile_key = str(m.get("input_profile_key", "")).strip()

            controller_profile = (
                controller_profiles.get(controller_profile_key) if controller_profile_key != "" else None
            )
            if controller_profile_key != "" and controller_profile is None:
                raise ValueError(
                    f"modes[{idx}].controller_profile_key '{controller_profile_key}' not found in controller_profiles.json"
                )
            camera_profile = camera_profiles.get(camera_profile_key) if camera_profile_key != "" else None
            if camera_profile_key != "" and camera_profile is None:
                raise ValueError(
                    f"modes[{idx}].camera_profile_key '{camera_profile_key}' not found in camera_profiles.json"
                )
            input_profile = input_profiles.get(input_profile_key) if input_profile_key != "" else None
            if input_profile_key != "" and input_profile is None:
                raise ValueError(
                    f"modes[{idx}].input_profile_key '{input_profile_key}' not found in input_profiles.json"
                )

            topdown_render_scale = _u32(
                (controller_profile or {}).get("topdown_render_scale", m.get("topdown_render_scale", 0)),
                f"modes[{idx}].topdown_render_scale",
            )
            topdown_tile_present_mode = _u32(
                (controller_profile or {}).get("topdown_tile_present_mode", m.get("topdown_tile_present_mode", 0)),
                f"modes[{idx}].topdown_tile_present_mode",
            )
            controller_profile_id = _u32(
                (controller_profile or {}).get("profile_id", m.get("controller_profile_id", 0)),
                f"modes[{idx}].controller_profile_id",
            )
            camera_profile_id = _u32(
                (camera_profile or {}).get("profile_id", m.get("camera_profile_id", 0)),
                f"modes[{idx}].camera_profile_id",
            )
            input_deadzone_permille = _u32(
                (input_profile or {}).get("input_deadzone_permille", m.get("input_deadzone_permille", 0)),
                f"modes[{idx}].input_deadzone_permille",
            )
            input_flags = _u32(
                (input_profile or {}).get("input_flags", m.get("input_flags", 0)),
                f"modes[{idx}].input_flags",
            )
            move_speed_px_s = _u32(
                (controller_profile or {}).get("move_speed_px_s", m.get("move_speed_px_s", 0)),
                f"modes[{idx}].move_speed_px_s",
            )
            move_accel_px_s2 = _u32(
                (controller_profile or {}).get("move_accel_px_s2", m.get("move_accel_px_s2", 0)),
                f"modes[{idx}].move_accel_px_s2",
            )
            move_decel_px_s2 = _u32(
                (controller_profile or {}).get("move_decel_px_s2", m.get("move_decel_px_s2", 0)),
                f"modes[{idx}].move_decel_px_s2",
            )
            camera_deadzone_w_px = _u32(
                (camera_profile or {}).get("camera_deadzone_w_px", m.get("camera_deadzone_w_px", 0)),
                f"modes[{idx}].camera_deadzone_w_px",
            )
            camera_deadzone_h_px = _u32(
                (camera_profile or {}).get("camera_deadzone_h_px", m.get("camera_deadzone_h_px", 0)),
                f"modes[{idx}].camera_deadzone_h_px",
            )
            camera_follow_permille = _u32(
                (camera_profile or {}).get("camera_follow_permille", m.get("camera_follow_permille", 0)),
                f"modes[{idx}].camera_follow_permille",
            )
            camera_max_speed_px_s = _u32(
                (camera_profile or {}).get("camera_max_speed_px_s", m.get("camera_max_speed_px_s", 0)),
                f"modes[{idx}].camera_max_speed_px_s",
            )
            camera_lookahead_x_px = _s32(
                (camera_profile or {}).get("camera_lookahead_x_px", m.get("camera_lookahead_x_px", 0)),
                f"modes[{idx}].camera_lookahead_x_px",
            )
            camera_lookahead_y_px = _s32(
                (camera_profile or {}).get("camera_lookahead_y_px", m.get("camera_lookahead_y_px", 0)),
                f"modes[{idx}].camera_lookahead_y_px",
            )
            if manifest_version in (VERSION_V4, VERSION_V5):
                scene_map_id = _u32(
                    m.get("scene_map_id", 0), f"modes[{idx}].scene_map_id"
                )
                scene_tileset_id = _u32(
                    m.get("scene_tileset_id", 0),
                    f"modes[{idx}].scene_tileset_id",
                )
                music_asset_id = _u32(
                    m.get("music_asset_id", 0), f"modes[{idx}].music_asset_id"
                )
                sfx_interact_asset_id = _u32(
                    m.get("sfx_interact_asset_id", 0),
                    f"modes[{idx}].sfx_interact_asset_id",
                )
                sfx_confirm_asset_id = _u32(
                    m.get("sfx_confirm_asset_id", 0),
                    f"modes[{idx}].sfx_confirm_asset_id",
                )
                sfx_error_asset_id = _u32(
                    m.get("sfx_error_asset_id", 0),
                    f"modes[{idx}].sfx_error_asset_id",
                )
                if manifest_version == VERSION_V5:
                    scene_lifecycle = _u32(
                        m.get("scene_lifecycle", 0), f"modes[{idx}].scene_lifecycle"
                    )
                    resume_domain_id = _u32(
                        m.get("resume_domain_id", 0), f"modes[{idx}].resume_domain_id"
                    )
                    if scene_lifecycle > 2:
                        raise ValueError(
                            f"modes[{idx}].scene_lifecycle must be 0, 1, or 2"
                        )
                    if scene_lifecycle == 1 and resume_domain_id == 0:
                        raise ValueError(
                            f"modes[{idx}] resumable lifecycle requires non-zero resume_domain_id"
                        )
                    if scene_lifecycle == 2 and resume_domain_id != 0:
                        raise ValueError(
                            f"modes[{idx}] transient lifecycle requires resume_domain_id = 0"
                        )
                    mode_bytes.extend(
                        struct.pack(
                            MODE_V5_FMT,
                            mode_id,
                            runtime_kind,
                            backend_id,
                            scene_map_addr,
                            scene_map_size_bytes,
                            scene_tileset_addr,
                            scene_tileset_size_bytes,
                            topdown_render_scale,
                            topdown_tile_present_mode,
                            controller_profile_id,
                            camera_profile_id,
                            input_deadzone_permille,
                            input_flags,
                            move_speed_px_s,
                            move_accel_px_s2,
                            move_decel_px_s2,
                            camera_deadzone_w_px,
                            camera_deadzone_h_px,
                            camera_follow_permille,
                            camera_max_speed_px_s,
                            camera_lookahead_x_px,
                            camera_lookahead_y_px,
                            scene_map_id,
                            scene_tileset_id,
                            music_asset_id,
                            sfx_interact_asset_id,
                            sfx_confirm_asset_id,
                            sfx_error_asset_id,
                            scene_lifecycle,
                            resume_domain_id,
                        )
                    )
                else:
                    mode_bytes.extend(
                        struct.pack(
                            MODE_V4_FMT,
                            mode_id,
                            runtime_kind,
                            backend_id,
                            scene_map_addr,
                            scene_map_size_bytes,
                            scene_tileset_addr,
                            scene_tileset_size_bytes,
                            topdown_render_scale,
                            topdown_tile_present_mode,
                            controller_profile_id,
                            camera_profile_id,
                            input_deadzone_permille,
                            input_flags,
                            move_speed_px_s,
                            move_accel_px_s2,
                            move_decel_px_s2,
                            camera_deadzone_w_px,
                            camera_deadzone_h_px,
                            camera_follow_permille,
                            camera_max_speed_px_s,
                            camera_lookahead_x_px,
                            camera_lookahead_y_px,
                            scene_map_id,
                            scene_tileset_id,
                            music_asset_id,
                            sfx_interact_asset_id,
                            sfx_confirm_asset_id,
                            sfx_error_asset_id,
                        )
                    )
            else:
                mode_bytes.extend(
                    struct.pack(
                        MODE_V2_FMT,
                        mode_id,
                        runtime_kind,
                        backend_id,
                        scene_map_addr,
                        scene_map_size_bytes,
                        scene_tileset_addr,
                        scene_tileset_size_bytes,
                        topdown_render_scale,
                        topdown_tile_present_mode,
                        controller_profile_id,
                        camera_profile_id,
                        input_deadzone_permille,
                        input_flags,
                        move_speed_px_s,
                        move_accel_px_s2,
                        move_decel_px_s2,
                        camera_deadzone_w_px,
                        camera_deadzone_h_px,
                        camera_follow_permille,
                        camera_max_speed_px_s,
                        camera_lookahead_x_px,
                        camera_lookahead_y_px,
                    )
                )

    route_bytes = bytearray()
    for idx, r in enumerate(routes_raw):
        if not isinstance(r, dict):
            raise ValueError(f"pet_routes[{idx}] must be an object")
        pet_entry_id = _u16(r.get("pet_entry_id"), f"pet_routes[{idx}].pet_entry_id")
        mode_id = _u32(r.get("mode_id"), f"pet_routes[{idx}].mode_id")
        if mode_id not in mode_ids:
            raise ValueError(
                f"pet_routes[{idx}].mode_id={mode_id} not present in modes[]"
            )
        route_bytes.extend(struct.pack(PET_ROUTE_FMT, pet_entry_id, 0, mode_id))

    pet_menu_bytes = bytearray()
    seen_slots: set[int] = set()
    for idx, rec in enumerate(pet_menu_items_raw):
        if not isinstance(rec, dict):
            raise ValueError(f"pet_menu_items[{idx}] must be an object")
        slot_index = _u8(rec.get("slot_index"), f"pet_menu_items[{idx}].slot_index")
        icon_action_id = _u8(
            rec.get("icon_action_id"), f"pet_menu_items[{idx}].icon_action_id"
        )
        select_kind = _u8(rec.get("select_kind"), f"pet_menu_items[{idx}].select_kind")
        status_kind = _u8(rec.get("status_kind", 0), f"pet_menu_items[{idx}].status_kind")
        arg0 = _u16(rec.get("arg0", 0), f"pet_menu_items[{idx}].arg0")
        status_source_id = _u16(
            rec.get("status_source_id", 0),
            f"pet_menu_items[{idx}].status_source_id",
        )
        if slot_index >= GAME_PET_MENU_SLOT_COUNT:
            raise ValueError(
                f"pet_menu_items[{idx}].slot_index {slot_index} out of range 0..{GAME_PET_MENU_SLOT_COUNT - 1}"
            )
        if icon_action_id >= GAME_PET_MENU_ACTION_COUNT:
            raise ValueError(
                f"pet_menu_items[{idx}].icon_action_id {icon_action_id} out of range 0..{GAME_PET_MENU_ACTION_COUNT - 1}"
            )
        if select_kind not in PET_MENU_SELECT_KIND_BY_NAME.values():
            raise ValueError(f"pet_menu_items[{idx}].select_kind {select_kind} is unsupported")
        if status_kind not in PET_MENU_STATUS_KIND_BY_NAME.values():
            raise ValueError(f"pet_menu_items[{idx}].status_kind {status_kind} is unsupported")
        if status_source_id not in PET_MENU_STATUS_SOURCE_BY_NAME.values():
            raise ValueError(
                f"pet_menu_items[{idx}].status_source_id {status_source_id} is unsupported"
            )
        if slot_index in seen_slots:
            raise ValueError(f"duplicate pet_menu_items slot_index {slot_index}")
        seen_slots.add(slot_index)
        if select_kind == PET_MENU_SELECT_KIND_BY_NAME["launch_mode"]:
            if int(arg0) not in mode_ids:
                raise ValueError(
                    f"pet_menu_items[{idx}] launch_mode arg0={arg0} is not present in modes[].mode_id"
                )
        if select_kind == PET_MENU_SELECT_KIND_BY_NAME["open_page"] and arg0 == 0:
            raise ValueError(f"pet_menu_items[{idx}] open_page requires non-zero arg0 page_id")
        if status_kind == PET_MENU_STATUS_KIND_BY_NAME["none"]:
            if status_source_id != PET_MENU_STATUS_SOURCE_BY_NAME["none"]:
                raise ValueError(
                    f"pet_menu_items[{idx}] status_kind=none requires status_source_id=0"
                )
        else:
            if status_source_id != PET_MENU_STATUS_SOURCE_BY_NAME["battery"]:
                raise ValueError(
                    f"pet_menu_items[{idx}] status_kind requires battery status_source_id=1"
                )
        pet_menu_bytes.extend(
            struct.pack(
                PET_MENU_ITEM_FMT,
                slot_index,
                icon_action_id,
                select_kind,
                status_kind,
                arg0,
                status_source_id,
            )
        )

    header_size = (
        HEADER_V3_SIZE
        if manifest_version in (VERSION_V3, VERSION_V4, VERSION_V5)
        else HEADER_V12_SIZE
    )
    modes_offset = header_size
    pet_routes_offset = modes_offset + len(mode_bytes)
    pet_menu_items_offset = pet_routes_offset + len(route_bytes)
    total_size = pet_menu_items_offset + len(pet_menu_bytes)

    if (
        (modes_offset & 0x3) != 0
        or (pet_routes_offset & 0x3) != 0
        or (pet_menu_items_offset & 0x3) != 0
    ):
        raise ValueError("internal alignment error: offsets are not 4-byte aligned")
    if total_size > MAX_BYTES:
        raise ValueError(f"manifest size {total_size} exceeds max {MAX_BYTES}")

    if manifest_version in (VERSION_V3, VERSION_V4, VERSION_V5):
        header = struct.pack(
            HEADER_V3_FMT,
            MAGIC,
            manifest_version,
            header_size,
            total_size,
            0,  # crc32 placeholder
            pkg_id,
            pkg_ver,
            len(modes_raw),
            len(routes_raw),
            modes_offset,
            pet_routes_offset,
            len(pet_menu_items_raw),
            0,
            pet_menu_items_offset,
        )
    else:
        header = struct.pack(
            HEADER_V12_FMT,
            MAGIC,
            manifest_version,
            header_size,
            total_size,
            0,  # crc32 placeholder
            pkg_id,
            pkg_ver,
            len(modes_raw),
            len(routes_raw),
            modes_offset,
            pet_routes_offset,
        )

    blob = bytearray()
    blob.extend(header)
    blob.extend(mode_bytes)
    blob.extend(route_bytes)
    blob.extend(pet_menu_bytes)

    crc32 = _crc32_ieee_masked_crc_field(bytes(blob))
    blob[CRC32_OFFSET_IN_HEADER : CRC32_OFFSET_IN_HEADER + 4] = struct.pack("<I", crc32)
    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate binary GAME_PACKAGE_MANIFEST (MGPK) from JSON."
    )
    parser.add_argument(
        "--in-json",
        type=Path,
        default=Path("Assets/game_package/manifest.example.json"),
        help="Input manifest JSON path",
    )
    parser.add_argument(
        "--out-bin",
        type=Path,
        default=Path("Assets/game_package/manifest.bin"),
        help="Output binary manifest path",
    )
    args = parser.parse_args()

    raw = args.in_json.read_text(encoding="utf-8")
    data = json.loads(raw)
    if not isinstance(data, dict):
        raise ValueError("top-level JSON must be an object")

    profile_indexes = _load_profile_indexes_for_manifest(args.in_json)
    project_domains = _load_game_project_domains_for_manifest(args.in_json)
    pages = project_domains.get("pages", [])
    pet_menu_slots = project_domains.get("pet_menu_slots", [])
    compiled_pet_menu = _compile_pet_menu_items_from_slots(data, pages, pet_menu_slots)
    if compiled_pet_menu is not None:
        data["pet_menu_items"] = compiled_pet_menu

    blob = build_manifest(data, profile_indexes=profile_indexes)
    manifest_version = struct.unpack_from("<H", blob, 4)[0]

    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(blob)
    _emit_ui_page_registry_autogen(args.in_json, pages)

    print(
        f"wrote {args.out_bin} ({len(blob)} bytes), "
        f"magic=0x{MAGIC:08x}, version={manifest_version}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
