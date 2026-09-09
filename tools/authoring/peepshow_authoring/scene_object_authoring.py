"""Version-aware scene-object authoring integration; never an executable exporter."""

from copy import deepcopy

from .project import (
    SCENE_KEYS, SCENE_OPTIONAL_KEYS, ProjectCommandError, ValidationIssue,
    _check_keys, _check_scene, _require_command_fields,
)
from .scene_objects import (
    PROPERTY_KEYS, SceneObjectError, apply_object_actions, clear_object_override,
    initialize_objects, validate_object_model,
)


OBJECT_COMMANDS = (
    "object.add", "object.delete", "object.set_defaults", "object.bind_animation",
    "object.clear_animation", "object_override.set", "object_override.clear",
    "object_actions.set",
)
# Other legacy commands are not implicitly safe for a different scene representation.
STATE_MANAGEMENT_COMMANDS = (
    "state.add", "state.create", "state.delete", "state.rename", "state.set_entry",
    "editor.state_graph.set_node_position", "editor.state_graph.set_entry_layout",
)
COMMON_SCENE_COMMANDS = ("scene.rename", "project.set_entry_scene", *STATE_MANAGEMENT_COMMANDS)


def execution_model(scene):
    return "scene_objects" if scene.get("schema_version") == 2 else "legacy_state_bindings"


def graph_validation_view(scene):
    """Adapt only shared graph/render primitives for existing validation/host compilation.

    Object operations have placeholders at the same positions. Host preview
    restores their symbolic operations and owns live objects; this is not a
    lowering of object semantics to legacy executable semantics.
    """
    view = deepcopy(scene)
    view["schema_version"] = 1
    elements = []
    for obj in view.pop("objects"):
        element = {key: value for key, value in obj.items() if key not in {"object_id", "defaults", "animation_ref"}}
        element.update(obj["defaults"])
        element["element_id"] = obj["object_id"]
        element["focus_role"] = "none"
        elements.append(element)
    view["render_models"] = [{"visual_id": "object_preview", "focus_index": 0, "elements": elements}]
    view["waiting_visuals"] = [{
        "waiting_visual_id": "object_preview_wait", "presentation_id": "object_preview_wait",
        "phase_quantum_ms": 250, "combined_step_count": 1, "settled_step": 0,
        "cycle_policy": "loop", "elements": [],
    }]
    for state in view["states"]:
        state.pop("object_overrides")
        state["waiting_visual_ref"] = "object_preview_wait"
    if isinstance(view.get("reactive_wait_default"), dict):
        view["reactive_wait_default"]["waiting_visual_ref"] = "object_preview_wait"
    for route in [*view.get("routes", []), *view.get("event_handlers", [])]:
        if isinstance(route, dict) and isinstance(route.get("actions"), list):
            route["actions"] = [
                {"kind": "request_render"} if isinstance(action, dict)
                and isinstance(action.get("kind"), str) and action["kind"].startswith("object.")
                else action for action in route["actions"]
            ]
    return view


def check_object_scene(scene, source, frame_lookup, animations, audio_cue_ids, issues):
    base = f"scene[{source}]"
    start = len(issues)
    required = (SCENE_KEYS - {"render_models", "waiting_visuals"}) | {"objects"}
    _check_keys(scene, required, base, issues, required | SCENE_OPTIONAL_KEYS)
    sizes = {key: (frame.width, frame.height) for key, frame in frame_lookup.items()}
    issues.extend(ValidationIssue(item.code, f"{base}.{item.path}", item.message) for item in
                  validate_object_model({"objects": scene.get("objects"), "states": scene.get("states")}, sizes, animations))
    for collection in ("routes", "event_handlers"):
        if not isinstance(scene.get(collection, []), list):
            issues.append(ValidationIssue("PROJECT_TYPE_INVALID", f"{base}.{collection}", "must be an array"))
    wait = scene.get("reactive_wait_default")
    if isinstance(wait, dict) and "waiting_visual_ref" in wait:
        issues.append(ValidationIssue("OBJECT_LEGACY_FIELD", f"{base}.reactive_wait_default.waiting_visual_ref", "version 2 does not select state waiting records"))
    if len(issues) != start:
        return
    live = initialize_objects(scene["objects"])
    for collection in ("routes", "event_handlers"):
        for index, route in enumerate(scene.get(collection, [])):
            if not isinstance(route, dict) or not isinstance(route.get("actions"), list):
                continue
            for action_index, action in enumerate(route["actions"]):
                if not isinstance(action, dict) or not isinstance(action.get("kind"), str):
                    continue
                path = f"{base}.{collection}[{index}].actions[{action_index}]"
                if action["kind"].startswith("set_element_"):
                    issues.append(ValidationIssue("OBJECT_LEGACY_ACTION", path, "legacy destination-binding mutations cannot target scene objects"))
                elif action["kind"].startswith("object."):
                    if "target_scene" in route:
                        issues.append(ValidationIssue("SCENE_TRANSITION_ACTION_UNSUPPORTED", path, "scene replacement currently supports only play_sfx"))
                    try:
                        apply_object_actions(scene["objects"], live, [action], sizes)
                    except SceneObjectError as exc:
                        issues.append(ValidationIssue(exc.issue.code, path, exc.issue.message))
    if len(issues) != start:
        return
    _check_scene(graph_validation_view(scene), source, frame_lookup, set(animations), audio_cue_ids, issues)


def check_command_model(scenes, command):
    ref = command.get("scene_id")
    scene = next((item for item in scenes if item["scene_id"] == ref), None)
    kind = command.get("kind")
    if not isinstance(kind, str):
        raise ProjectCommandError("COMMAND_SHAPE_INVALID", "command kind must be text")
    if kind in OBJECT_COMMANDS:
        if scene is None or scene.get("schema_version") != 2:
            raise ProjectCommandError("COMMAND_EXECUTION_MODEL_MISMATCH", "object commands require a version-2 scene")
    elif scene is not None and scene.get("schema_version") == 2 and kind not in COMMON_SCENE_COMMANDS:
        raise ProjectCommandError("COMMAND_EXECUTION_MODEL_MISMATCH", f"'{kind}' is not supported for scene-owned objects")
    # Global commands that traverse/change scenes need explicit v2 integration too.
    elif scene is None and any(item.get("schema_version") == 2 for item in scenes):
        if kind not in {"asset.upsert", "asset.delete", "animation.upsert", "animation.delete",
                        "audio_asset.upsert", "audio_asset.delete", "audio_cue.upsert", "audio_cue.delete",
                        "scene.add", "project.set_entry_scene"}:
            raise ProjectCommandError("COMMAND_EXECUTION_MODEL_MISMATCH", f"'{kind}' has not been integrated with mixed scene models")


def apply_object_command(scenes, command):
    kind = command["kind"]
    fields = {
        "object.add": {"object", "visible_in_states"},
        "object.delete": {"object_id"},
        "object.set_defaults": {"object_id", "properties"},
        "object.bind_animation": {"object_id", "animation_ref"},
        "object.clear_animation": {"object_id"},
        "object_override.set": {"object_id", "state_id", "properties"},
        "object_override.clear": {"object_id", "state_id", "properties"},
        "object_actions.set": {"owner_kind", "owner_id", "actions"},
    }[kind]
    required = {"kind", "scene_id"} | fields
    if kind == "object.add":
        required -= {"visible_in_states"}
    _require_command_fields(command, required, {"kind", "scene_id", "command_id"} | fields)
    scene = next(item for item in scenes if item["scene_id"] == command["scene_id"])
    if kind == "object.add":
        obj = deepcopy(command["object"])
        if not isinstance(obj, dict):
            raise ProjectCommandError("OBJECT_TYPE_INVALID", "object must be a definition")
        selected = command.get("visible_in_states")
        if "visible_in_states" in command:
            ids = {state["state_id"] for state in scene["states"]}
            if not isinstance(selected, list) or any(not isinstance(ref, str) or ref not in ids for ref in selected) or len(set(selected)) != len(selected):
                raise ProjectCommandError("GRAPH_STATE_UNKNOWN", "visible_in_states must contain distinct existing states")
            if not isinstance(obj.get("defaults"), dict) or not isinstance(obj.get("object_id"), str):
                raise ProjectCommandError("OBJECT_TYPE_INVALID", "object requires defaults and a stable object_id")
            obj["defaults"]["visible"] = False
            for state in scene["states"]:
                if state["state_id"] in selected:
                    state["object_overrides"].append({"object_ref": obj["object_id"], "visible": True})
        scene["objects"].append(obj)
    elif kind == "object_actions.set":
        if command["owner_kind"] not in ("route", "handler"):
            raise ProjectCommandError("COMMAND_TARGET_INVALID", "owner_kind must be route or handler")
        collection, key = ("routes", "route_id") if command["owner_kind"] == "route" else ("event_handlers", "handler_id")
        owner = next((item for item in scene.get(collection, []) if item[key] == command["owner_id"]), None)
        if owner is None:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", "route/handler does not exist")
        owner["actions"] = deepcopy(command["actions"])
    else:
        obj = next((item for item in scene["objects"] if item["object_id"] == command["object_id"]), None)
        if obj is None:
            raise ProjectCommandError("OBJECT_REFERENCE_UNKNOWN", "object does not exist")
        if kind == "object.delete":
            if any(action.get("object_ref") == obj["object_id"] for route in [*scene["routes"], *scene.get("event_handlers", [])] for action in route["actions"]):
                raise ProjectCommandError("OBJECT_IN_USE", "remove object actions before deleting the object")
            scene["objects"].remove(obj)
            for state in scene["states"]:
                state["object_overrides"] = [item for item in state["object_overrides"] if item["object_ref"] != obj["object_id"]]
        elif kind == "object.set_defaults":
            values = command["properties"]
            if not isinstance(values, dict) or not values or values.keys() - PROPERTY_KEYS:
                raise ProjectCommandError("OBJECT_PROPERTY_INVALID", "properties must select x, y, visible or visual_ref")
            obj["defaults"].update(deepcopy(values))
        elif kind == "object.bind_animation":
            obj["animation_ref"] = command["animation_ref"]
        elif kind == "object.clear_animation":
            obj.pop("animation_ref", None)
        else:
            state = next((item for item in scene["states"] if item["state_id"] == command["state_id"]), None)
            if state is None:
                raise ProjectCommandError("GRAPH_STATE_UNKNOWN", "state does not exist")
            if kind == "object_override.clear":
                values = command["properties"]
                if not isinstance(values, list):
                    raise ProjectCommandError("OBJECT_PROPERTY_INVALID", "properties must be a list of individual property names")
                try:
                    updated = clear_object_override(state, obj["object_id"], values)
                except SceneObjectError as exc:
                    raise ProjectCommandError(exc.issue.code, exc.issue.message) from exc
                state.update(updated)
            else:
                values = command["properties"]
                if not isinstance(values, dict) or not values or values.keys() - PROPERTY_KEYS:
                    raise ProjectCommandError("OBJECT_PROPERTY_INVALID", "properties must select individual object properties")
                override = next((item for item in state["object_overrides"] if item["object_ref"] == obj["object_id"]), None)
                if override is None:
                    override = {"object_ref": obj["object_id"]}
                    state["object_overrides"].append(override)
                override.update(deepcopy(values))
    return deepcopy(command)
