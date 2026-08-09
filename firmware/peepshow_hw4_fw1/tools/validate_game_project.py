#!/usr/bin/env python3
"""
Validate game authoring project data and Tiled map references.

Checks:
- Required domain files exist and parse.
- IDs are unique within each domain.
- Cross-domain references in project data are valid.
- Tiled object properties reference valid IDs:
  entity_id/dialogue_id/script_id/item_id/target_map/target_spawn.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


DOMAIN_FILES = {
    "maps": "maps.json",
    "pages": "pages.json",
    "pet_menu_slots": "pet_menu_slots.json",
    "controller_profiles": "controller_profiles.json",
    "camera_profiles": "camera_profiles.json",
    "input_profiles": "input_profiles.json",
    "entities": "entities.json",
    "dialogues": "dialogues.json",
    "scripts": "scripts.json",
    "items": "items.json",
}
PROFILE_DOMAINS = ("controller_profiles", "camera_profiles", "input_profiles")
PROFILE_REQUIRED_INT_FIELDS: dict[str, tuple[str, ...]] = {
    "controller_profiles": (
        "topdown_render_scale",
        "topdown_tile_present_mode",
        "move_speed_px_s",
        "move_accel_px_s2",
        "move_decel_px_s2",
    ),
    "camera_profiles": (
        "camera_deadzone_w_px",
        "camera_deadzone_h_px",
        "camera_follow_permille",
        "camera_max_speed_px_s",
        "camera_lookahead_x_px",
        "camera_lookahead_y_px",
    ),
    "input_profiles": (
        "input_deadzone_permille",
        "input_flags",
    ),
}
LEGACY_MODE_TUNING_FIELDS = (
    "topdown_render_scale",
    "topdown_tile_present_mode",
    "controller_profile_id",
    "camera_profile_id",
    "input_deadzone_permille",
    "input_flags",
    "move_speed_px_s",
    "move_accel_px_s2",
    "move_decel_px_s2",
    "camera_deadzone_w_px",
    "camera_deadzone_h_px",
    "camera_follow_permille",
    "camera_max_speed_px_s",
    "camera_lookahead_x_px",
    "camera_lookahead_y_px",
)

TILED_REF_KEYS = {
    "entity_id": "entities",
    "dialogue_id": "dialogues",
    "script_id": "scripts",
    "item_id": "items",
}

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

PAGE_ROUTE_KIND_NATIVE_PAGE = "native_page"
PAGE_ROUTE_KIND_NATIVE_TREE = "native_tree"
PAGE_ROUTE_KINDS = {
    PAGE_ROUTE_KIND_NATIVE_PAGE,
    PAGE_ROUTE_KIND_NATIVE_TREE,
}

NATIVE_PAGE_KEYS = {
    "home",
    "pet",
    "battery_stats",
    "audio_levels",
    "lis2",
    "lis2_steps",
    "joy_cal",
    "joy_target",
}

NATIVE_TREE_KEYS = {
    "system_root",
    "pet_feed",
}


def find_repo_root(start: Path) -> Path:
    cur = start.resolve()
    for _ in range(60):
        if (cur / "Assets" / "game_project" / "project.json").is_file():
            return cur
        if cur.parent == cur:
            break
        cur = cur.parent
    raise FileNotFoundError("Could not locate repo root (missing Assets/game_project/project.json).")


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def as_str_id(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def parse_enum_u32(value: Any, mapping: dict[str, int]) -> int | None:
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
            return None
    return None


def object_properties(obj: dict[str, Any]) -> dict[str, Any]:
    props = obj.get("properties")
    out: dict[str, Any] = {}
    if isinstance(props, list):
        for item in props:
            if not isinstance(item, dict):
                continue
            key = as_str_id(item.get("name"))
            if key == "":
                continue
            out[key] = item.get("value")
    elif isinstance(props, dict):
        out = dict(props)
    return out


def iter_object_layers(layers: list[Any]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for layer in layers:
        if not isinstance(layer, dict):
            continue
        layer_type = as_str_id(layer.get("type")).lower()
        if layer_type == "objectgroup":
            out.append(layer)
            continue
        if layer_type == "group":
            nested = layer.get("layers")
            if isinstance(nested, list):
                out.extend(iter_object_layers(nested))
    return out


def collect_map_objects(tiled_doc: dict[str, Any]) -> list[tuple[str, dict[str, Any]]]:
    layers = tiled_doc.get("layers")
    if not isinstance(layers, list):
        return []
    out: list[tuple[str, dict[str, Any]]] = []
    for layer in iter_object_layers(layers):
        layer_name = as_str_id(layer.get("name")) or "<unnamed_layer>"
        objects = layer.get("objects")
        if not isinstance(objects, list):
            continue
        for obj in objects:
            if isinstance(obj, dict):
                out.append((layer_name, obj))
    return out


def collect_spawn_ids(tiled_doc: dict[str, Any]) -> set[str]:
    spawn_ids: set[str] = set()
    for _layer_name, obj in collect_map_objects(tiled_doc):
        cls = as_str_id(obj.get("class")).lower()
        typ = as_str_id(obj.get("type")).lower()
        props = object_properties(obj)
        kind = as_str_id(props.get("kind")).lower()
        spawn_id = as_str_id(props.get("spawn_id"))
        if spawn_id == "":
            spawn_id = as_str_id(obj.get("name"))
        if spawn_id == "":
            continue
        if ("spawn" in cls) or ("spawn" in typ) or (kind in {"player", "npc", "enemy", "pickup"}):
            spawn_ids.add(spawn_id)
    return spawn_ids


def build_id_index(domain: str, records: list[Any], errors: list[str]) -> dict[str, dict[str, Any]]:
    index: dict[str, dict[str, Any]] = {}
    for ix, item in enumerate(records):
        if not isinstance(item, dict):
            errors.append(f"{domain}[{ix}] is not an object.")
            continue
        item_id = as_str_id(item.get("id"))
        if item_id == "":
            errors.append(f"{domain}[{ix}] missing required field 'id'.")
            continue
        if item_id in index:
            errors.append(f"{domain} duplicate id: '{item_id}'.")
            continue
        index[item_id] = item
    return index


def validate_project(repo_root: Path) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    gp_dir = repo_root / "Assets" / "game_project"

    domain_data: dict[str, list[Any]] = {}
    domain_index: dict[str, dict[str, dict[str, Any]]] = {}

    for domain, filename in DOMAIN_FILES.items():
        path = gp_dir / filename
        if not path.is_file():
            errors.append(f"Missing domain file: {path.as_posix()}")
            domain_data[domain] = []
            domain_index[domain] = {}
            continue
        try:
            loaded = load_json(path)
        except Exception as exc:
            errors.append(f"Failed to parse {path.as_posix()}: {exc}")
            loaded = []
        if not isinstance(loaded, list):
            errors.append(f"{path.as_posix()} must be a JSON array.")
            loaded = []
        domain_data[domain] = loaded
        domain_index[domain] = build_id_index(domain, loaded, errors)

    for profile_domain in PROFILE_DOMAINS:
        seen_profile_ids: set[int] = set()
        for rec_id, rec in domain_index[profile_domain].items():
            profile_id = rec.get("profile_id")
            if not isinstance(profile_id, int) or profile_id <= 0:
                errors.append(f"{profile_domain}:{rec_id} missing or invalid profile_id (must be integer > 0).")
                continue
            if profile_id in seen_profile_ids:
                errors.append(f"{profile_domain} duplicate profile_id: '{profile_id}'.")
                continue
            seen_profile_ids.add(profile_id)
            for field in PROFILE_REQUIRED_INT_FIELDS.get(profile_domain, ()):
                value = rec.get(field)
                if not isinstance(value, int):
                    errors.append(
                        f"{profile_domain}:{rec_id} missing or invalid {field} (must be integer)."
                    )

    seen_page_ids: set[int] = set()
    for page_id, page in domain_index["pages"].items():
        runtime_page_id = page.get("page_id")
        if not isinstance(runtime_page_id, int) or runtime_page_id <= 0:
            errors.append(f"pages:{page_id} missing or invalid page_id (must be integer > 0).")
            continue
        if runtime_page_id in seen_page_ids:
            errors.append(f"pages duplicate page_id: '{runtime_page_id}'.")
        else:
            seen_page_ids.add(runtime_page_id)

        route_kind = as_str_id(page.get("route_kind")).lower()
        if route_kind not in PAGE_ROUTE_KINDS:
            errors.append(
                f"pages:{page_id} route_kind must be one of: {', '.join(sorted(PAGE_ROUTE_KINDS))}."
            )
            continue

        if route_kind == PAGE_ROUTE_KIND_NATIVE_PAGE:
            native_page_key = as_str_id(page.get("native_page_key")).lower()
            if native_page_key == "":
                errors.append(f"pages:{page_id} missing native_page_key for route_kind=native_page.")
            elif native_page_key not in NATIVE_PAGE_KEYS:
                errors.append(
                    f"pages:{page_id} native_page_key '{native_page_key}' is not supported by firmware registry."
                )
        elif route_kind == PAGE_ROUTE_KIND_NATIVE_TREE:
            native_tree_key = as_str_id(page.get("native_tree_key")).lower()
            if native_tree_key == "":
                errors.append(f"pages:{page_id} missing native_tree_key for route_kind=native_tree.")
            elif native_tree_key not in NATIVE_TREE_KEYS:
                errors.append(
                    f"pages:{page_id} native_tree_key '{native_tree_key}' is not supported by firmware registry."
                )

    # Validate direct project-domain links.
    for ent_id, ent in domain_index["entities"].items():
        dialogue_id = as_str_id(ent.get("dialogue_id"))
        if dialogue_id and dialogue_id not in domain_index["dialogues"]:
            errors.append(f"entities:{ent_id} references missing dialogue_id '{dialogue_id}'.")
        hooks = ent.get("event_hooks")
        if isinstance(hooks, list):
            for hook_ix, hook in enumerate(hooks):
                if not isinstance(hook, dict):
                    errors.append(f"entities:{ent_id} event_hooks[{hook_ix}] is not an object.")
                    continue
                script_id = as_str_id(hook.get("script_id"))
                if script_id and script_id not in domain_index["scripts"]:
                    errors.append(
                        f"entities:{ent_id} event_hooks[{hook_ix}] references missing script_id '{script_id}'."
                    )

    for script_id, script in domain_index["scripts"].items():
        actions = script.get("actions")
        if not isinstance(actions, list):
            errors.append(f"scripts:{script_id} field 'actions' must be an array.")
            continue
        for act_ix, action in enumerate(actions):
            if not isinstance(action, dict):
                errors.append(f"scripts:{script_id} actions[{act_ix}] is not an object.")
                continue
            op = as_str_id(action.get("op")).lower()
            if op == "show_dialogue":
                dialogue_id = as_str_id(action.get("dialogue_id"))
                if dialogue_id == "":
                    errors.append(f"scripts:{script_id} actions[{act_ix}] missing dialogue_id.")
                elif dialogue_id not in domain_index["dialogues"]:
                    errors.append(
                        f"scripts:{script_id} actions[{act_ix}] references missing dialogue_id '{dialogue_id}'."
                    )
            elif op == "call_script":
                nested_script = as_str_id(action.get("script_id"))
                if nested_script == "":
                    errors.append(f"scripts:{script_id} actions[{act_ix}] missing script_id.")
                elif nested_script not in domain_index["scripts"]:
                    errors.append(
                        f"scripts:{script_id} actions[{act_ix}] references missing script_id '{nested_script}'."
                    )
            elif op in {"add_item", "remove_item", "give_item"}:
                item_id = as_str_id(action.get("item_id"))
                if item_id == "":
                    errors.append(f"scripts:{script_id} actions[{act_ix}] missing item_id.")
                elif item_id not in domain_index["items"]:
                    errors.append(
                        f"scripts:{script_id} actions[{act_ix}] references missing item_id '{item_id}'."
                    )

    manifest_mode_ids: set[int] = set()
    manifest_modes_by_key: dict[str, dict[str, Any]] = {}
    manifest_path = repo_root / "Assets" / "game_package" / "manifest.example.json"
    if not manifest_path.is_file():
        errors.append(f"Missing package manifest: {manifest_path.as_posix()}")
    else:
        try:
            manifest_doc = load_json(manifest_path)
        except Exception as exc:
            errors.append(f"Failed to parse {manifest_path.as_posix()}: {exc}")
            manifest_doc = {}
        if not isinstance(manifest_doc, dict):
            errors.append(f"{manifest_path.as_posix()} must be a JSON object.")
            manifest_doc = {}
        modes = manifest_doc.get("modes", [])
        if not isinstance(modes, list):
            errors.append(f"{manifest_path.as_posix()} field 'modes' must be a JSON array.")
            modes = []
        pet_menu_items = manifest_doc.get("pet_menu_items", [])
        if not isinstance(pet_menu_items, list):
            errors.append(f"{manifest_path.as_posix()} field 'pet_menu_items' must be a JSON array.")
            pet_menu_items = []
        if len(domain_index["pet_menu_slots"]) > 0 and len(pet_menu_items) > 0:
            warnings.append(
                "manifest.pet_menu_items is non-empty while game_project/pet_menu_slots.json exists; "
                "generator will prioritize pet_menu_slots."
            )
        map_asset_index: dict[int, dict[str, Any]] = {}
        for map_key, map_rec in domain_index["maps"].items():
            map_asset_id = map_rec.get("map_asset_id")
            if isinstance(map_asset_id, int) and map_asset_id > 0:
                map_asset_index[map_asset_id] = map_rec
            else:
                warnings.append(f"maps:{map_key} missing or invalid map_asset_id.")
        for mode_ix, mode_rec in enumerate(modes):
            if not isinstance(mode_rec, dict):
                errors.append(f"manifest.modes[{mode_ix}] must be an object.")
                continue
            mode_key = as_str_id(mode_rec.get("id"))
            if mode_key == "":
                errors.append(f"manifest.modes[{mode_ix}] missing id.")
            elif mode_key in manifest_modes_by_key:
                errors.append(f"manifest.modes duplicate id: '{mode_key}'.")
            else:
                manifest_modes_by_key[mode_key] = mode_rec
            mode_id = mode_rec.get("mode_id")
            if isinstance(mode_id, int):
                if mode_id <= 0:
                    errors.append(f"manifest.modes[{mode_ix}] mode_id must be integer > 0.")
                elif mode_id in manifest_mode_ids:
                    errors.append(f"manifest.modes duplicate mode_id: '{mode_id}'.")
                else:
                    manifest_mode_ids.add(mode_id)
            else:
                errors.append(f"manifest.modes[{mode_ix}] missing mode_id.")
            for key_field, profile_domain in (
                ("controller_profile_key", "controller_profiles"),
                ("camera_profile_key", "camera_profiles"),
                ("input_profile_key", "input_profiles"),
            ):
                profile_key = as_str_id(mode_rec.get(key_field))
                if profile_key == "":
                    errors.append(f"manifest.modes[{mode_ix}] missing {key_field}.")
                    continue
                if profile_key not in domain_index[profile_domain]:
                    errors.append(
                        f"manifest.modes[{mode_ix}] {key_field} '{profile_key}' not found in {profile_domain}."
                    )
            scene_map_id = mode_rec.get("scene_map_id")
            if isinstance(scene_map_id, int) and scene_map_id > 0:
                map_rec = map_asset_index.get(scene_map_id)
                if map_rec is None:
                    errors.append(
                        f"manifest.modes[{mode_ix}] scene_map_id '{scene_map_id}' not found in maps.map_asset_id."
                    )
                else:
                    scene_tileset_id = mode_rec.get("scene_tileset_id")
                    expected_tileset_id = map_rec.get("tileset_asset_id")
                    if (
                        isinstance(scene_tileset_id, int)
                        and isinstance(expected_tileset_id, int)
                        and scene_tileset_id > 0
                        and expected_tileset_id > 0
                        and scene_tileset_id != expected_tileset_id
                    ):
                        warnings.append(
                            f"manifest.modes[{mode_ix}] scene_tileset_id {scene_tileset_id} "
                            f"does not match maps.tileset_asset_id {expected_tileset_id} for scene_map_id {scene_map_id}."
                        )
            for legacy_field in LEGACY_MODE_TUNING_FIELDS:
                if legacy_field in mode_rec:
                    errors.append(
                        f"manifest.modes[{mode_ix}] includes legacy field '{legacy_field}' "
                        "(profile data should own runtime tuning)."
                    )

    seen_slots: set[int] = set()
    for slot_id, slot in domain_index["pet_menu_slots"].items():
        slot_index = slot.get("slot_index")
        if not isinstance(slot_index, int):
            errors.append(f"pet_menu_slots:{slot_id} missing slot_index (must be integer).")
            continue
        if slot_index < 0 or slot_index >= GAME_PET_MENU_SLOT_COUNT:
            errors.append(
                f"pet_menu_slots:{slot_id} slot_index {slot_index} out of range 0..{GAME_PET_MENU_SLOT_COUNT - 1}."
            )
        if slot_index in seen_slots:
            errors.append(f"pet_menu_slots duplicate slot_index: '{slot_index}'.")
        else:
            seen_slots.add(slot_index)

        icon_action_id = slot.get("icon_action_id")
        if not isinstance(icon_action_id, int):
            errors.append(f"pet_menu_slots:{slot_id} missing icon_action_id (must be integer).")
        elif icon_action_id < 0 or icon_action_id >= GAME_PET_MENU_ACTION_COUNT:
            errors.append(
                f"pet_menu_slots:{slot_id} icon_action_id {icon_action_id} out of range 0..{GAME_PET_MENU_ACTION_COUNT - 1}."
            )

        select_kind = parse_enum_u32(slot.get("select_kind"), PET_MENU_SELECT_KIND_BY_NAME)
        if select_kind is None:
            errors.append(
                f"pet_menu_slots:{slot_id} select_kind '{slot.get('select_kind')}' is invalid."
            )
            continue
        if select_kind not in PET_MENU_SELECT_KIND_BY_NAME.values():
            errors.append(f"pet_menu_slots:{slot_id} select_kind {select_kind} is unsupported.")
            continue
        if select_kind == PET_MENU_SELECT_KIND_BY_NAME["launch_mode"]:
            mode_key = as_str_id(slot.get("mode_key"))
            if mode_key == "":
                errors.append(f"pet_menu_slots:{slot_id} launch_mode requires mode_key.")
            elif mode_key not in manifest_modes_by_key:
                errors.append(
                    f"pet_menu_slots:{slot_id} mode_key '{mode_key}' not found in manifest.modes[]."
                )
        if select_kind == PET_MENU_SELECT_KIND_BY_NAME["open_page"]:
            page_key = as_str_id(slot.get("page_key"))
            if page_key == "":
                errors.append(f"pet_menu_slots:{slot_id} open_page requires page_key.")
            elif page_key not in domain_index["pages"]:
                errors.append(
                    f"pet_menu_slots:{slot_id} page_key '{page_key}' not found in pages domain."
                )

        status_kind = parse_enum_u32(slot.get("status_kind", "none"), PET_MENU_STATUS_KIND_BY_NAME)
        if status_kind is None:
            errors.append(
                f"pet_menu_slots:{slot_id} status_kind '{slot.get('status_kind')}' is invalid."
            )
            continue
        if status_kind not in PET_MENU_STATUS_KIND_BY_NAME.values():
            errors.append(f"pet_menu_slots:{slot_id} status_kind {status_kind} is unsupported.")
            continue

        status_source = parse_enum_u32(
            slot.get("status_source", "none"), PET_MENU_STATUS_SOURCE_BY_NAME
        )
        if status_source is None:
            errors.append(
                f"pet_menu_slots:{slot_id} status_source '{slot.get('status_source')}' is invalid."
            )
            continue
        if status_source not in PET_MENU_STATUS_SOURCE_BY_NAME.values():
            errors.append(f"pet_menu_slots:{slot_id} status_source {status_source} is unsupported.")
            continue

        if status_kind == PET_MENU_STATUS_KIND_BY_NAME["none"]:
            if status_source != PET_MENU_STATUS_SOURCE_BY_NAME["none"]:
                errors.append(
                    f"pet_menu_slots:{slot_id} status_kind=none requires status_source=none."
                )
        else:
            if status_source != PET_MENU_STATUS_SOURCE_BY_NAME["battery"]:
                errors.append(
                    f"pet_menu_slots:{slot_id} status_kind requires status_source=battery."
                )
            status_base = slot.get("status_base_icon_action_id")
            if not isinstance(status_base, int):
                errors.append(
                    f"pet_menu_slots:{slot_id} status_kind requires integer status_base_icon_action_id."
                )
            elif status_base < 0 or status_base >= GAME_PET_MENU_ACTION_COUNT:
                errors.append(
                    f"pet_menu_slots:{slot_id} status_base_icon_action_id {status_base} out of range."
                )

    map_docs: dict[str, dict[str, Any]] = {}
    map_spawns: dict[str, set[str]] = {}
    pending_target_spawns: list[tuple[str, str, str, str]] = []

    for map_id, map_rec in domain_index["maps"].items():
        tiled_map = as_str_id(map_rec.get("tiled_map"))
        if tiled_map == "":
            errors.append(f"maps:{map_id} missing tiled_map.")
            continue
        tiled_path = repo_root / tiled_map
        if not tiled_path.is_file():
            errors.append(f"maps:{map_id} tiled_map missing file '{tiled_map}'.")
            continue
        try:
            tiled_doc = load_json(tiled_path)
        except Exception as exc:
            errors.append(f"maps:{map_id} failed to parse '{tiled_map}': {exc}")
            continue
        if not isinstance(tiled_doc, dict):
            errors.append(f"maps:{map_id} invalid tiled document root in '{tiled_map}'.")
            continue

        map_docs[map_id] = tiled_doc
        map_spawns[map_id] = collect_spawn_ids(tiled_doc)

        for layer_name, obj in collect_map_objects(tiled_doc):
            props = object_properties(obj)
            obj_name = as_str_id(obj.get("name")) or "<unnamed_object>"

            for prop_key, ref_domain in TILED_REF_KEYS.items():
                ref_id = as_str_id(props.get(prop_key))
                if ref_id == "":
                    continue
                if ref_id not in domain_index[ref_domain]:
                    errors.append(
                        f"maps:{map_id} object '{obj_name}' layer '{layer_name}' "
                        f"references missing {prop_key} '{ref_id}'."
                    )

            target_map = as_str_id(props.get("target_map"))
            target_spawn = as_str_id(props.get("target_spawn"))
            if target_map:
                if target_map not in domain_index["maps"]:
                    errors.append(
                        f"maps:{map_id} object '{obj_name}' layer '{layer_name}' "
                        f"references missing target_map '{target_map}'."
                    )
                elif target_spawn:
                    pending_target_spawns.append((map_id, obj_name, target_map, target_spawn))

    for src_map, obj_name, target_map, target_spawn in pending_target_spawns:
        spawns = map_spawns.get(target_map, set())
        if not spawns:
            warnings.append(
                f"maps:{src_map} object '{obj_name}' targets map '{target_map}', "
                "but no spawn objects were discovered in that map."
            )
            continue
        if target_spawn not in spawns:
            errors.append(
                f"maps:{src_map} object '{obj_name}' references target_spawn '{target_spawn}' "
                f"that is not present in map '{target_map}'."
            )

    return errors, warnings


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate game authoring project data.")
    parser.add_argument(
        "--repo-root",
        default="",
        help="Optional explicit repo root path (defaults to auto-detect from script location).",
    )
    args = parser.parse_args(argv)

    try:
        if args.repo_root:
            repo_root = Path(args.repo_root).resolve()
        else:
            repo_root = find_repo_root(Path(__file__).resolve())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    errors, warnings = validate_project(repo_root)

    if warnings:
        print("Warnings:")
        for warning in warnings:
            print(f"  - {warning}")

    if errors:
        print("Errors:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("PASS: project data and Tiled references are valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
