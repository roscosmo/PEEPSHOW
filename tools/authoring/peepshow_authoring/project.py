"""Load, validate, and normalize the first PeepShow STATE authoring subset."""

from __future__ import annotations

import hashlib
import json
import re
from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .audio_assets import (
    AUDIO_MAX_ASSETS,
    AUDIO_MAX_BANK_BYTES,
    AUDIO_MAX_CUES,
    AudioAssetError,
    CompiledAudioAsset,
    import_sampled_sfx,
)
from .image_assets import (
    ImageAssetError,
    Masked1bppFrame,
    import_masked_1bpp,
    resolve_project_path,
)
from .system_fonts import SystemFontError, rasterize_system_font_text


STABLE_ID = re.compile(r"^[a-z][a-z0-9_.-]{0,63}$")
GUARD_OPERATORS = {"eq", "ne", "lt", "le", "gt", "ge"}
ACTION_OPERATIONS = {"assign", "add"}
LOGICAL_INPUT_SOURCES = {
    "BUTTON_A",
    "BUTTON_B",
    "BUTTON_L",
    "BUTTON_R",
    "BUTTON_START",
    "JOY_LEFT",
    "JOY_RIGHT",
    "JOY_UP",
    "JOY_DOWN",
    "JOY_UP_LEFT",
    "JOY_UP_RIGHT",
    "JOY_DOWN_LEFT",
    "JOY_DOWN_RIGHT",
}
LOGICAL_INPUT_EVENT_KINDS = {"press", "release", "hold", "repeat"}
JOYSTICK_POLICIES = {"four_way", "eight_way"}
PROJECT_KEYS = {
    "schema_id",
    "schema_version",
    "project_id",
    "project_name",
    "package",
    "selected_target_profile",
    "entry_scene",
    "scene_sources",
    "validation",
}
PROJECT_OPTIONAL_KEYS = {"asset_sources", "editor"}
SCENE_KEYS = {
    "schema_id",
    "schema_version",
    "scene_id",
    "display_name",
    "scene_type",
    "entry_state",
    "variables",
    "input_actions",
    "states",
    "routes",
    "render_models",
    "waiting_visuals",
    "reactive_wait_default",
    "interaction_policy",
}
SCENE_OPTIONAL_KEYS = {"joystick_policy"}


@dataclass(frozen=True)
class ValidationIssue:
    code: str
    path: str
    message: str


@dataclass(frozen=True)
class ProjectBundle:
    root: Path
    project: dict[str, Any]
    scenes: tuple[dict[str, Any], ...]
    scene_sources: tuple[str, ...]
    asset_catalogs: tuple[dict[str, Any], ...]
    asset_catalog_sources: tuple[str, ...]
    assets: tuple[dict[str, Any], ...]
    animations: tuple[dict[str, Any], ...]
    frames: tuple[Masked1bppFrame, ...]
    audio_assets: tuple[CompiledAudioAsset, ...]
    audio_cues: tuple[dict[str, Any], ...]
    issues: tuple[ValidationIssue, ...]

    @property
    def valid(self) -> bool:
        return not self.issues

    def normalized(self) -> dict[str, Any]:
        if not self.valid:
            raise ValueError("cannot normalize an invalid project")
        return {
            "format": "peepshow.normalized.authoring",
            "format_version": 1,
            "project": self.project,
            "scenes": list(self.scenes),
            "assets": list(self.assets),
            "animations": list(self.animations),
            "audio_assets": [
                {
                    "asset_id": asset.asset_id,
                    "source_path": asset.source_path,
                    "source_sample_rate_hz": asset.source_sample_rate_hz,
                    "source_channels": asset.source_channels,
                    "sample_rate_hz": asset.sample_rate_hz,
                    "channels": asset.channels,
                    "sample_count": asset.sample_count,
                    "duration_ms": asset.duration_ms,
                    "decoded_pcm_bytes": asset.decoded_pcm_bytes,
                    "block_samples": asset.block_samples,
                    "block_count": asset.block_count,
                    "adpcm_bytes": len(asset.adpcm),
                    "adpcm_sha256": hashlib.sha256(asset.adpcm).hexdigest(),
                }
                for asset in self.audio_assets
            ],
            "audio_cues": list(self.audio_cues),
            "compiled_asset_frames": [
                {
                    "asset_id": frame.asset_id,
                    "frame_id": frame.frame_id,
                    "width": frame.width,
                    "height": frame.height,
                    "row_stride_bytes": frame.row_stride_bytes,
                    "pivot_x": frame.pivot_x,
                    "pivot_y": frame.pivot_y,
                    "opaque": frame.opaque,
                    "pixels_sha256": hashlib.sha256(frame.pixels).hexdigest(),
                    "mask_sha256": hashlib.sha256(frame.mask).hexdigest(),
                }
                for frame in sorted(self.frames, key=lambda item: (item.asset_id, item.frame_id))
            ],
        }

    def canonical_bytes(self) -> bytes:
        return (
            json.dumps(
                self.normalized(),
                ensure_ascii=True,
                separators=(",", ":"),
                sort_keys=True,
            )
            + "\n"
        ).encode("ascii")


class ProjectCommandError(ValueError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


def _issue(issues: list[ValidationIssue], code: str, path: str, message: str) -> None:
    issues.append(ValidationIssue(code, path, message))


def _require_command_fields(command: dict[str, Any], required: set[str], allowed: set[str]) -> None:
    missing = required - command.keys()
    unknown = command.keys() - allowed
    if missing or unknown:
        raise ProjectCommandError(
            "COMMAND_SHAPE_INVALID",
            f"command fields do not match the command API: missing={sorted(missing)} unknown={sorted(unknown)}",
        )


def _command_scene(scenes: list[dict[str, Any]], scene_id: Any) -> dict[str, Any]:
    issues: list[ValidationIssue] = []
    _stable_id(scene_id, "command.scene_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    for scene in scenes:
        if isinstance(scene, dict) and scene.get("scene_id") == scene_id:
            return scene
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _command_record(
    scene: dict[str, Any],
    collection: str,
    id_field: str,
    record_id: Any,
) -> dict[str, Any]:
    records = scene.get(collection)
    if not isinstance(records, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"scene.{collection} must be an array")
    for record in records:
        if isinstance(record, dict) and record.get(id_field) == record_id:
            return record
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown {id_field} '{record_id}'")


def _command_index(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ProjectCommandError("COMMAND_INDEX_INVALID", f"{field} must be a non-negative integer")
    return value


def _command_move_index(value: Any, field: str, length: int) -> int:
    index = _command_index(value, field)
    if index >= length:
        raise ProjectCommandError("COMMAND_INDEX_INVALID", f"{field} does not select an existing entry")
    return index


def _apply_state_add(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "state"},
        {"kind", "scene_id", "state", "command_id"},
    )
    scene = _command_scene(scenes, command.get("scene_id"))
    state = command.get("state")
    if not isinstance(state, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "state must be an object")
    states = scene.get("states")
    if not isinstance(states, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.states must be an array")
    if len(states) >= 64:
        raise ProjectCommandError("PROJECT_LIMIT_EXCEEDED", "scene supports at most 64 states")
    state_id = state.get("state_id")
    issues: list[ValidationIssue] = []
    _stable_id(state_id, "command.state.state_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    if any(isinstance(item, dict) and item.get("state_id") == state_id for item in states):
        raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"state '{state_id}' already exists")
    normalized = deepcopy(state)
    states.append(normalized)
    return {"kind": "state.add", "scene_id": scene.get("scene_id"), "state": normalized}


def _apply_state_delete(
    project: dict[str, Any],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "state_id"},
        {"kind", "scene_id", "state_id", "command_id"},
    )
    scene = _command_scene(scenes, command.get("scene_id"))
    state_id = command.get("state_id")
    issues: list[ValidationIssue] = []
    _stable_id(state_id, "command.state_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    states = scene.get("states")
    if not isinstance(states, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.states must be an array")
    if len(states) <= 1:
        raise ProjectCommandError("COMMAND_TARGET_IN_USE", "the last state cannot be deleted")
    if scene.get("entry_state") == state_id:
        raise ProjectCommandError("COMMAND_TARGET_IN_USE", "entry state must be changed before deletion")
    for route in scene.get("routes", []):
        if not isinstance(route, dict):
            continue
        if route.get("target_state") == state_id or state_id in route.get("from_states", []):
            raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"state '{state_id}' is referenced by route '{route.get('route_id')}'")
    for index, state in enumerate(states):
        if isinstance(state, dict) and state.get("state_id") == state_id:
            states.pop(index)
            editor = project.get("editor")
            if isinstance(editor, dict):
                graph = editor.get("state_graph")
                graph_scenes = graph.get("scenes") if isinstance(graph, dict) else None
                layout = graph_scenes.get(scene.get("scene_id")) if isinstance(graph_scenes, dict) else None
                nodes = layout.get("nodes") if isinstance(layout, dict) else None
                if isinstance(nodes, dict):
                    nodes.pop(state_id, None)
            return {"kind": "state.delete", "scene_id": scene.get("scene_id"), "state_id": state_id}
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown state '{state_id}'")


def _apply_state_set_reference(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    if kind == "state.set_entry":
        _require_command_fields(command, {"kind", "scene_id", "state_id"}, {"kind", "scene_id", "state_id", "command_id"})
        scene = _command_scene(scenes, command.get("scene_id"))
        state = _command_record(scene, "states", "state_id", command.get("state_id"))
        scene["entry_state"] = state.get("state_id")
        return {"kind": kind, "scene_id": scene.get("scene_id"), "state_id": state.get("state_id")}
    _require_command_fields(
        command,
        {"kind", "scene_id", "state_id", "render_model_ref"},
        {"kind", "scene_id", "state_id", "render_model_ref", "command_id"},
    )
    scene = _command_scene(scenes, command.get("scene_id"))
    state = _command_record(scene, "states", "state_id", command.get("state_id"))
    model = _command_record(scene, "render_models", "visual_id", command.get("render_model_ref"))
    state["render_model_ref"] = model.get("visual_id")
    return {
        "kind": kind,
        "scene_id": scene.get("scene_id"),
        "state_id": state.get("state_id"),
        "render_model_ref": model.get("visual_id"),
    }


def _apply_render_model_add(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "render_model"}, {"kind", "scene_id", "render_model", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    model = command.get("render_model")
    if not isinstance(model, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "render_model must be an object")
    models = scene.get("render_models")
    if not isinstance(models, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.render_models must be an array")
    if len(models) >= 64:
        raise ProjectCommandError("PROJECT_LIMIT_EXCEEDED", "scene supports at most 64 render models")
    visual_id = model.get("visual_id")
    issues: list[ValidationIssue] = []
    _stable_id(visual_id, "command.render_model.visual_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    if any(isinstance(item, dict) and item.get("visual_id") == visual_id for item in models):
        raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"render model '{visual_id}' already exists")
    normalized = deepcopy(model)
    models.append(normalized)
    return {"kind": "render_model.add", "scene_id": scene.get("scene_id"), "render_model": normalized}


def _apply_render_model_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "visual_id"}, {"kind", "scene_id", "visual_id", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    visual_id = command.get("visual_id")
    for state in scene.get("states", []):
        if isinstance(state, dict) and state.get("render_model_ref") == visual_id:
            raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"render model '{visual_id}' is referenced by state '{state.get('state_id')}'")
    models = scene.get("render_models")
    if not isinstance(models, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.render_models must be an array")
    for index, model in enumerate(models):
        if isinstance(model, dict) and model.get("visual_id") == visual_id:
            models.pop(index)
            return {"kind": "render_model.delete", "scene_id": scene.get("scene_id"), "visual_id": visual_id}
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown render model '{visual_id}'")


def _apply_render_model_set_focus(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "visual_id", "focus_index"}, {"kind", "scene_id", "visual_id", "focus_index", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    model = _command_record(scene, "render_models", "visual_id", command.get("visual_id"))
    focus_index = command.get("focus_index")
    if isinstance(focus_index, bool) or not isinstance(focus_index, int) or not 0 <= focus_index <= 255:
        raise ProjectCommandError("RENDER_FOCUS_INVALID", "focus_index must be in 0..255")
    model["focus_index"] = focus_index
    return {"kind": "render_model.set_focus_index", "scene_id": scene.get("scene_id"), "visual_id": model.get("visual_id"), "focus_index": focus_index}


def _apply_variable_upsert(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    _require_command_fields(command, {"kind", "scene_id", "variable"}, {"kind", "scene_id", "variable", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    variable = command.get("variable")
    if not isinstance(variable, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "variable must be an object")
    variables = scene.get("variables")
    if not isinstance(variables, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.variables must be an array")
    variable_id = variable.get("variable_id")
    issues: list[ValidationIssue] = []
    _stable_id(variable_id, "command.variable.variable_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    existing = next((item for item in variables if isinstance(item, dict) and item.get("variable_id") == variable_id), None)
    normalized = deepcopy(variable)
    if kind == "variable.add":
        if existing is not None:
            raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"variable '{variable_id}' already exists")
        if len(variables) >= 32:
            raise ProjectCommandError("PROJECT_LIMIT_EXCEEDED", "scene supports at most 32 variables")
        variables.append(normalized)
    else:
        if existing is None:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown variable '{variable_id}'")
        variables[variables.index(existing)] = normalized
    return {"kind": kind, "scene_id": scene.get("scene_id"), "variable": normalized}


def _apply_variable_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "variable_id"}, {"kind", "scene_id", "variable_id", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    variable_id = command.get("variable_id")
    for route in scene.get("routes", []):
        if not isinstance(route, dict):
            continue
        used = any(isinstance(guard, dict) and guard.get("variable_ref") == variable_id for guard in route.get("guards", []))
        used = used or any(isinstance(action, dict) and action.get("variable_ref") == variable_id for action in route.get("actions", []))
        if used:
            raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"variable '{variable_id}' is referenced by route '{route.get('route_id')}'")
    variables = scene.get("variables")
    if not isinstance(variables, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.variables must be an array")
    for index, variable in enumerate(variables):
        if isinstance(variable, dict) and variable.get("variable_id") == variable_id:
            variables.pop(index)
            return {"kind": "variable.delete", "scene_id": scene.get("scene_id"), "variable_id": variable_id}
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown variable '{variable_id}'")


def _apply_input_action_upsert(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    _require_command_fields(command, {"kind", "scene_id", "input_action"}, {"kind", "scene_id", "input_action", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    action = command.get("input_action")
    if not isinstance(action, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "input_action must be an object")
    actions = scene.get("input_actions")
    if not isinstance(actions, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.input_actions must be an array")
    action_id = action.get("action_id")
    issues: list[ValidationIssue] = []
    _stable_id(action_id, "command.input_action.action_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    existing = next((item for item in actions if isinstance(item, dict) and item.get("action_id") == action_id), None)
    normalized = deepcopy(action)
    if kind == "input_action.add":
        if existing is not None:
            raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"input action '{action_id}' already exists")
        if len(actions) >= 32:
            raise ProjectCommandError("PROJECT_LIMIT_EXCEEDED", "scene supports at most 32 input actions")
        actions.append(normalized)
    else:
        if existing is None:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown input action '{action_id}'")
        actions[actions.index(existing)] = normalized
    return {"kind": kind, "scene_id": scene.get("scene_id"), "input_action": normalized}


def _apply_input_action_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "action_id"}, {"kind", "scene_id", "action_id", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    action_id = command.get("action_id")
    for route in scene.get("routes", []):
        if isinstance(route, dict) and route.get("action_ref") == action_id:
            raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"input action '{action_id}' is referenced by route '{route.get('route_id')}'")
    wait_policy = scene.get("reactive_wait_default")
    if isinstance(wait_policy, dict) and action_id in wait_policy.get("event_interests", []):
        raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"input action '{action_id}' is a reactive wait interest")
    interaction = scene.get("interaction_policy")
    if isinstance(interaction, dict) and action_id in interaction.get("meaningful_activity_actions", []):
        raise ProjectCommandError("COMMAND_TARGET_IN_USE", f"input action '{action_id}' is meaningful activity")
    actions = scene.get("input_actions")
    if not isinstance(actions, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.input_actions must be an array")
    for index, action in enumerate(actions):
        if isinstance(action, dict) and action.get("action_id") == action_id:
            actions.pop(index)
            return {"kind": "input_action.delete", "scene_id": scene.get("scene_id"), "action_id": action_id}
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown input action '{action_id}'")


def _apply_route_add(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "route"}, {"kind", "scene_id", "route", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    route = command.get("route")
    if not isinstance(route, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "route must be an object")
    routes = scene.get("routes")
    if not isinstance(routes, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.routes must be an array")
    if len(routes) >= 128:
        raise ProjectCommandError("PROJECT_LIMIT_EXCEEDED", "scene supports at most 128 routes")
    route_id = route.get("route_id")
    issues: list[ValidationIssue] = []
    _stable_id(route_id, "command.route.route_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    if any(isinstance(item, dict) and item.get("route_id") == route_id for item in routes):
        raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"route '{route_id}' already exists")
    normalized = deepcopy(route)
    routes.append(normalized)
    return {"kind": "route.add", "scene_id": scene.get("scene_id"), "route": normalized}


def _apply_route_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "scene_id", "route_id"}, {"kind", "scene_id", "route_id", "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    route_id = command.get("route_id")
    routes = scene.get("routes")
    if not isinstance(routes, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.routes must be an array")
    for index, route in enumerate(routes):
        if isinstance(route, dict) and route.get("route_id") == route_id:
            routes.pop(index)
            return {"kind": "route.delete", "scene_id": scene.get("scene_id"), "route_id": route_id}
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown route '{route_id}'")


def _apply_route_set_binding(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    field = "from_states" if kind == "route.set_sources" else "action_ref"
    _require_command_fields(command, {"kind", "scene_id", "route_id", field}, {"kind", "scene_id", "route_id", field, "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    route = _command_record(scene, "routes", "route_id", command.get("route_id"))
    value = command.get(field)
    if field == "from_states":
        if not isinstance(value, list) or not value:
            raise ProjectCommandError("ROUTE_SOURCE_MISSING", "from_states must be a non-empty array")
        value = list(value)
    else:
        _command_record(scene, "input_actions", "action_id", value)
    route[field] = value
    return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), field: value}


def _apply_scene_policy(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    if kind == "scene.set_reactive_wait_default":
        field = "reactive_wait_default"
    elif kind == "scene.set_interaction_policy":
        field = "interaction_policy"
    else:
        field = "joystick_policy"
    _require_command_fields(command, {"kind", "scene_id", field}, {"kind", "scene_id", field, "command_id"})
    scene = _command_scene(scenes, command.get("scene_id"))
    policy = command.get(field)
    if field == "joystick_policy":
        if policy not in JOYSTICK_POLICIES:
            raise ProjectCommandError("JOYSTICK_POLICY_INVALID", "joystick_policy must be four_way or eight_way")
    elif not isinstance(policy, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"{field} must be an object")
    normalized = deepcopy(policy)
    scene[field] = normalized
    return {"kind": kind, "scene_id": scene.get("scene_id"), field: normalized}


def _apply_state_rename(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "state_id", "display_name"},
        {"kind", "scene_id", "state_id", "display_name", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    state_id = command.get("state_id")
    display_name = command.get("display_name")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(state_id, "command.state_id", issues)
    _text(display_name, "command.display_name", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        for state in scene.get("states", []):
            if isinstance(state, dict) and state.get("state_id") == state_id:
                state["display_name"] = display_name
                return {
                    "kind": "state.rename",
                    "scene_id": scene_id,
                    "state_id": state_id,
                    "display_name": display_name,
                }
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown state '{state_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _apply_route_set_target(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "route_id"},
        {"kind", "scene_id", "route_id", "target_state", "target_scene", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    route_id = command.get("route_id")
    target_state = command.get("target_state")
    target_scene = command.get("target_scene")
    has_target_state = "target_state" in command
    has_target_scene = "target_scene" in command
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(route_id, "command.route_id", issues)
    if has_target_state == has_target_scene:
        raise ProjectCommandError(
            "COMMAND_SHAPE_INVALID",
            "route.set_target requires exactly one of target_state or target_scene",
        )
    if has_target_state:
        _stable_id(target_state, "command.target_state", issues)
    else:
        _stable_id(target_scene, "command.target_scene", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    scene_ids = {
        scene.get("scene_id")
        for scene in scenes
        if isinstance(scene, dict)
    }
    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        state_ids = {
            state.get("state_id")
            for state in scene.get("states", [])
            if isinstance(state, dict)
        }
        if has_target_state and target_state not in state_ids:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown target state '{target_state}'")
        if has_target_scene and target_scene not in scene_ids:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown target scene '{target_scene}'")
        for route in scene.get("routes", []):
            if isinstance(route, dict) and route.get("route_id") == route_id:
                if has_target_scene and route.get("actions"):
                    raise ProjectCommandError(
                        "SCENE_TRANSITION_ACTION_UNSUPPORTED",
                        "direct scene replacement requires an empty route action list",
                    )
                result = {
                    "kind": "route.set_target",
                    "scene_id": scene_id,
                    "route_id": route_id,
                }
                if has_target_state:
                    route.pop("target_scene", None)
                    route["target_state"] = target_state
                    result["target_state"] = target_state
                else:
                    route.pop("target_state", None)
                    route["target_scene"] = target_scene
                    result["target_scene"] = target_scene
                return result
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown route '{route_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _route_id_component(value: str) -> str:
    return re.sub(r"[^a-z0-9_.-]+", "_", value.lower()).strip("_") or "exit"


def _unique_generated_route_id(scene: dict[str, Any], logical_source: str, target_scene: str) -> str:
    base = f"exit_{_route_id_component(logical_source)}_to_{_route_id_component(target_scene)}"
    existing = {
        route.get("route_id")
        for route in scene.get("routes", [])
        if isinstance(route, dict)
    }
    if base not in existing:
        return base
    for suffix in range(2, 100):
        candidate = f"{base}_{suffix}"
        if candidate not in existing:
            return candidate
    raise ProjectCommandError("COMMAND_TARGET_INVALID", "could not generate a unique route ID")


def _apply_route_add_scene_exit(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "logical_source", "target_scene"},
        {"kind", "scene_id", "logical_source", "target_scene", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    logical_source = command.get("logical_source")
    target_scene = command.get("target_scene")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(target_scene, "command.target_scene", issues)
    if logical_source not in LOGICAL_INPUT_SOURCES:
        raise ProjectCommandError("INPUT_SOURCE_INVALID", "unsupported logical source")
    if scene_id == target_scene:
        raise ProjectCommandError("COMMAND_TARGET_INVALID", "scene exits must target another scene")
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    scene_ids = {
        scene.get("scene_id")
        for scene in scenes
        if isinstance(scene, dict)
    }
    if target_scene not in scene_ids:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown target scene '{target_scene}'")

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        states = [
            state.get("state_id")
            for state in scene.get("states", [])
            if isinstance(state, dict) and isinstance(state.get("state_id"), str)
        ]
        if not states:
            raise ProjectCommandError("COMMAND_TARGET_INVALID", "scene has no states to exit from")
        for action in scene.get("input_actions", []):
            if (
                isinstance(action, dict)
                and action.get("logical_source") == logical_source
                and action.get("event_kind", "press") == "press"
            ):
                raise ProjectCommandError("INPUT_SOURCE_DUPLICATE", "logical source is already bound")

        route_id = _unique_generated_route_id(scene, logical_source, target_scene)
        input_action = {"action_id": route_id, "logical_source": logical_source}
        route = {
            "route_id": route_id,
            "action_ref": route_id,
            "from_states": states,
            "guards": [],
            "actions": [],
            "target_scene": target_scene,
        }
        scene.setdefault("input_actions", []).append(input_action)
        scene.setdefault("routes", []).append(route)

        wait_policy = scene.get("reactive_wait_default")
        if isinstance(wait_policy, dict):
            interests = wait_policy.get("event_interests")
            if isinstance(interests, list) and route_id not in interests:
                interests.append(route_id)
        interaction = scene.get("interaction_policy")
        if isinstance(interaction, dict):
            meaningful = interaction.get("meaningful_activity_actions")
            if isinstance(meaningful, list) and route_id not in meaningful:
                meaningful.append(route_id)

        return {
            "kind": "route.add_scene_exit",
            "scene_id": scene_id,
            "route_id": route_id,
            "action_id": route_id,
            "logical_source": logical_source,
            "target_scene": target_scene,
            "from_states": states,
        }
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _apply_route_delete_scene_exit(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "route_id"},
        {"kind", "scene_id", "route_id", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    route_id = command.get("route_id")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(route_id, "command.route_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        routes = scene.get("routes")
        if not isinstance(routes, list):
            raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.routes must be an array")
        for index, route in enumerate(routes):
            if not isinstance(route, dict) or route.get("route_id") != route_id:
                continue
            if "target_scene" not in route:
                raise ProjectCommandError("COMMAND_TARGET_INVALID", "route is not a scene exit")
            if route.get("actions"):
                raise ProjectCommandError(
                    "SCENE_TRANSITION_ACTION_UNSUPPORTED",
                    "only actionless direct scene exits can be deleted in this edit slice",
                )
            action_ref = route.get("action_ref")
            target_scene = route.get("target_scene")
            routes.pop(index)

            input_actions = scene.get("input_actions")
            if isinstance(input_actions, list):
                scene["input_actions"] = [
                    action
                    for action in input_actions
                    if not (isinstance(action, dict) and action.get("action_id") == action_ref)
                ]
            wait_policy = scene.get("reactive_wait_default")
            if isinstance(wait_policy, dict) and isinstance(wait_policy.get("event_interests"), list):
                wait_policy["event_interests"] = [
                    item for item in wait_policy["event_interests"] if item != action_ref
                ]
            interaction = scene.get("interaction_policy")
            if isinstance(interaction, dict) and isinstance(interaction.get("meaningful_activity_actions"), list):
                interaction["meaningful_activity_actions"] = [
                    item for item in interaction["meaningful_activity_actions"] if item != action_ref
                ]
            return {
                "kind": "route.delete_scene_exit",
                "scene_id": scene_id,
                "route_id": route_id,
                "action_id": action_ref,
                "target_scene": target_scene,
            }
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown route '{route_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _render_coordinate(value: Any, path: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ProjectCommandError("RENDER_BOUNDS_INVALID", f"{path} must be an integer")
    if value < 0:
        raise ProjectCommandError("RENDER_BOUNDS_INVALID", f"{path} must be non-negative")
    return value


def _render_dimension(value: Any, path: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 1:
        raise ProjectCommandError("RENDER_BOUNDS_INVALID", f"{path} must be a positive integer")
    return value


def _render_z_order(value: Any) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 255:
        raise ProjectCommandError("RENDER_BOUNDS_INVALID", "command.z_order must be in 0..255")
    return value


def _target_render_model(
    scenes: list[dict[str, Any]],
    scene_id: Any,
    render_model_id: Any,
) -> dict[str, Any]:
    issues: list[ValidationIssue] = []
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(render_model_id, "command.render_model_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    scene = next((item for item in scenes if item.get("scene_id") == scene_id), None)
    if scene is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")
    render_models = scene.get("render_models")
    if not isinstance(render_models, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.render_models must be an array")
    render_model = next(
        (
            item
            for item in render_models
            if isinstance(item, dict) and item.get("visual_id") == render_model_id
        ),
        None,
    )
    if render_model is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown render model '{render_model_id}'")
    return render_model


def _target_render_element(render_model: dict[str, Any], element_id: Any) -> dict[str, Any]:
    issues: list[ValidationIssue] = []
    _stable_id(element_id, "command.element_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    elements = render_model.get("elements")
    if not isinstance(elements, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "render_model.elements must be an array")
    element = next(
        (
            item
            for item in elements
            if isinstance(item, dict) and item.get("element_id") == element_id
        ),
        None,
    )
    if element is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown render element '{element_id}'")
    return element


def _validate_render_bounds(x: int, y: int, width: int, height: int) -> None:
    if x + width > 168 or y + height > 144:
        raise ProjectCommandError("RENDER_BOUNDS_INVALID", "element would be outside the 168x144 display")


def _apply_render_element_add(
    scenes: list[dict[str, Any]],
    frames: list[Masked1bppFrame],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "render_model_id", "element"},
        {"kind", "scene_id", "render_model_id", "element", "command_id"},
    )
    element = command.get("element")
    if not isinstance(element, dict):
        raise ProjectCommandError("COMMAND_SHAPE_INVALID", "command.element must be an object")
    required = {"element_id", "kind", "x", "y", "width", "height", "z_order"}
    allowed = required | {"visual_ref", "focus_role", "layer", "visible"}
    _require_command_fields(element, required, allowed)
    element_id = element.get("element_id")
    issues: list[ValidationIssue] = []
    _stable_id(element_id, "command.element.element_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    kind = element.get("kind")
    if kind not in {"sprite", "line", "outline_rect", "filled_rect", "circle", "ellipse"}:
        raise ProjectCommandError("RENDER_KIND_INVALID", "unsupported retained element type")
    x = _render_coordinate(element.get("x"), "command.element.x")
    y = _render_coordinate(element.get("y"), "command.element.y")
    width = _render_dimension(element.get("width"), "command.element.width")
    height = _render_dimension(element.get("height"), "command.element.height")
    _validate_render_bounds(x, y, width, height)
    _render_z_order(element.get("z_order"))
    layer = element.get("layer", "UI" if element.get("focus_role", "none") == "focus" else "SCENE")
    visible = element.get("visible", True)
    focus_role = element.get("focus_role", "none")
    if layer not in {"BACKGROUND", "SCENE", "UI"}:
        raise ProjectCommandError("RENDER_LAYER_INVALID", "layer must be BACKGROUND, SCENE, or UI")
    if not isinstance(visible, bool):
        raise ProjectCommandError("RENDER_VISIBILITY_INVALID", "visible must be boolean")
    if focus_role not in {"none", "focus"}:
        raise ProjectCommandError("RENDER_FOCUS_INVALID", "focus_role must be none or focus")
    visual_ref = element.get("visual_ref")
    available_visuals = {frame.frame_id for frame in frames}
    if kind == "sprite":
        if visual_ref not in available_visuals:
            raise ProjectCommandError("ASSET_FRAME_UNKNOWN", f"unknown sprite visual '{visual_ref}'")
    elif "visual_ref" in element:
        raise ProjectCommandError("RENDER_VISUAL_REF_INVALID", "primitives do not reference assets")
    if focus_role == "focus" and (kind != "sprite" or layer != "UI" or not visible):
        raise ProjectCommandError("RENDER_FOCUS_INVALID", "focus must be a visible UI sprite")

    render_model = _target_render_model(
        scenes,
        command.get("scene_id"),
        command.get("render_model_id"),
    )
    elements = render_model.setdefault("elements", [])
    if any(isinstance(item, dict) and item.get("element_id") == element_id for item in elements):
        raise ProjectCommandError("PROJECT_ID_DUPLICATE", f"render element '{element_id}' already exists")
    elements.append(deepcopy(element))
    return {
        "kind": "render_element.add",
        "scene_id": command.get("scene_id"),
        "render_model_id": command.get("render_model_id"),
        "element_id": element_id,
    }


def _apply_render_element_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "render_model_id", "element_id"},
        {"kind", "scene_id", "render_model_id", "element_id", "command_id"},
    )
    scene_id = command.get("scene_id")
    element_id = command.get("element_id")
    render_model = _target_render_model(scenes, scene_id, command.get("render_model_id"))
    element = _target_render_element(render_model, element_id)
    if element.get("focus_role", "none") == "focus":
        raise ProjectCommandError("RENDER_FOCUS_INVALID", "the focus element cannot be deleted")
    scene = next(item for item in scenes if item.get("scene_id") == scene_id)
    for waiting in scene.get("waiting_visuals", []):
        for animated in waiting.get("elements", []):
            if animated.get("source_element_ref") == element_id:
                raise ProjectCommandError(
                    "WAIT_ELEMENT_IN_USE",
                    f"render element '{element_id}' is used by waiting visual '{waiting.get('waiting_visual_id')}'",
                )
    render_model["elements"].remove(element)
    return {
        "kind": "render_element.delete",
        "scene_id": scene_id,
        "render_model_id": command.get("render_model_id"),
        "element_id": element_id,
    }


def _apply_render_element_set_bounds(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "render_model_id", "element_id", "x", "y", "width", "height"},
        {"kind", "scene_id", "render_model_id", "element_id", "x", "y", "width", "height", "command_id"},
    )
    x = _render_coordinate(command.get("x"), "command.x")
    y = _render_coordinate(command.get("y"), "command.y")
    width = _render_dimension(command.get("width"), "command.width")
    height = _render_dimension(command.get("height"), "command.height")
    _validate_render_bounds(x, y, width, height)
    render_model = _target_render_model(scenes, command.get("scene_id"), command.get("render_model_id"))
    element = _target_render_element(render_model, command.get("element_id"))
    previous = {key: element.get(key) for key in ("x", "y", "width", "height")}
    element.update({"x": x, "y": y, "width": width, "height": height})
    return {
        "kind": "render_element.set_bounds",
        "scene_id": command.get("scene_id"),
        "render_model_id": command.get("render_model_id"),
        "element_id": command.get("element_id"),
        "previous": previous,
        "bounds": {"x": x, "y": y, "width": width, "height": height},
    }


def _apply_render_element_set_property(
    scenes: list[dict[str, Any]],
    frames: list[Masked1bppFrame],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    property_by_kind = {
        "render_element.set_layer": "layer",
        "render_element.set_visibility": "visible",
        "render_element.set_z_order": "z_order",
        "render_element.set_visual_ref": "visual_ref",
    }
    property_name = property_by_kind[kind]
    _require_command_fields(
        command,
        {"kind", "scene_id", "render_model_id", "element_id", property_name},
        {"kind", "scene_id", "render_model_id", "element_id", property_name, "command_id"},
    )
    render_model = _target_render_model(scenes, command.get("scene_id"), command.get("render_model_id"))
    element = _target_render_element(render_model, command.get("element_id"))
    value = command.get(property_name)
    if property_name == "layer":
        if value not in {"BACKGROUND", "SCENE", "UI"}:
            raise ProjectCommandError("RENDER_LAYER_INVALID", "layer must be BACKGROUND, SCENE, or UI")
        if element.get("focus_role", "none") == "focus" and value != "UI":
            raise ProjectCommandError("RENDER_FOCUS_INVALID", "the focus element must remain on UI")
    elif property_name == "visible":
        if not isinstance(value, bool):
            raise ProjectCommandError("RENDER_VISIBILITY_INVALID", "visible must be boolean")
        if element.get("focus_role", "none") == "focus" and not value:
            raise ProjectCommandError("RENDER_FOCUS_INVALID", "the focus element must remain visible")
    elif property_name == "z_order":
        value = _render_z_order(value)
    elif property_name == "visual_ref":
        if element.get("kind") != "sprite":
            raise ProjectCommandError("RENDER_VISUAL_REF_INVALID", "only sprites reference visuals")
        available_visuals = {frame.frame_id for frame in frames}
        if value not in available_visuals:
            raise ProjectCommandError("ASSET_FRAME_UNKNOWN", f"unknown sprite visual '{value}'")
    previous = element.get(property_name)
    element[property_name] = value
    return {
        "kind": kind,
        "scene_id": command.get("scene_id"),
        "render_model_id": command.get("render_model_id"),
        "element_id": command.get("element_id"),
        "previous": previous,
        property_name: value,
    }


def _apply_render_element_set_position(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "render_model_id", "element_id", "x", "y"},
        {"kind", "scene_id", "render_model_id", "element_id", "x", "y", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    render_model_id = command.get("render_model_id")
    element_id = command.get("element_id")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(render_model_id, "command.render_model_id", issues)
    _stable_id(element_id, "command.element_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    x = _render_coordinate(command.get("x"), "command.x")
    y = _render_coordinate(command.get("y"), "command.y")

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        render_models = scene.get("render_models")
        if not isinstance(render_models, list):
            raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.render_models must be an array")
        for render_model in render_models:
            if not isinstance(render_model, dict) or render_model.get("visual_id") != render_model_id:
                continue
            elements = render_model.get("elements")
            if not isinstance(elements, list):
                raise ProjectCommandError("PROJECT_TYPE_INVALID", "render_model.elements must be an array")
            for element in elements:
                if not isinstance(element, dict) or element.get("element_id") != element_id:
                    continue
                width = element.get("width")
                height = element.get("height")
                if not isinstance(width, int) or width < 1 or not isinstance(height, int) or height < 1:
                    raise ProjectCommandError("RENDER_BOUNDS_INVALID", "element size is invalid")
                if x + width > 168 or y + height > 144:
                    raise ProjectCommandError("RENDER_BOUNDS_INVALID", "element would be outside the 168x144 display")
                previous_x = element.get("x")
                previous_y = element.get("y")
                element["x"] = x
                element["y"] = y
                return {
                    "kind": "render_element.set_position",
                    "scene_id": scene_id,
                    "render_model_id": render_model_id,
                    "element_id": element_id,
                    "previous_x": previous_x,
                    "previous_y": previous_y,
                    "x": x,
                    "y": y,
                }
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown render element '{element_id}'")
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown render model '{render_model_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _apply_waiting_visual_upsert(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "waiting_visual"},
        {"kind", "scene_id", "waiting_visual", "command_id"},
    )
    scene_id = command.get("scene_id")
    waiting_visual = command.get("waiting_visual")
    issues: list[ValidationIssue] = []
    _stable_id(scene_id, "command.scene_id", issues)
    if not isinstance(waiting_visual, dict):
        raise ProjectCommandError("COMMAND_SHAPE_INVALID", "command.waiting_visual must be an object")
    waiting_visual_id = waiting_visual.get("waiting_visual_id")
    _stable_id(waiting_visual_id, "command.waiting_visual.waiting_visual_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    scene = next((item for item in scenes if item.get("scene_id") == scene_id), None)
    if scene is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")
    waiting_visuals = scene.setdefault("waiting_visuals", [])
    existing = next(
        (
            item
            for item in waiting_visuals
            if isinstance(item, dict) and item.get("waiting_visual_id") == waiting_visual_id
        ),
        None,
    )
    created = existing is None
    if created:
        if len(waiting_visuals) >= 32:
            raise ProjectCommandError("PROJECT_COUNT_INVALID", "scene already has 32 waiting visuals")
        waiting_visuals.append(deepcopy(waiting_visual))
    else:
        waiting_visuals[waiting_visuals.index(existing)] = deepcopy(waiting_visual)
    return {
        "kind": "waiting_visual.upsert",
        "scene_id": scene_id,
        "waiting_visual_id": waiting_visual_id,
        "created": created,
    }


def _apply_waiting_visual_delete(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "waiting_visual_id"},
        {"kind", "scene_id", "waiting_visual_id", "command_id"},
    )
    scene_id = command.get("scene_id")
    waiting_visual_id = command.get("waiting_visual_id")
    issues: list[ValidationIssue] = []
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(waiting_visual_id, "command.waiting_visual_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    scene = next((item for item in scenes if item.get("scene_id") == scene_id), None)
    if scene is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")
    if any(state.get("waiting_visual_ref") == waiting_visual_id for state in scene.get("states", [])):
        raise ProjectCommandError("WAIT_VISUAL_IN_USE", "waiting visual is referenced by a state")
    wait_default = scene.get("reactive_wait_default")
    if isinstance(wait_default, dict) and wait_default.get("waiting_visual_ref") == waiting_visual_id:
        raise ProjectCommandError("WAIT_VISUAL_IN_USE", "waiting visual is the reactive wait default")
    waiting_visuals = scene.get("waiting_visuals")
    if not isinstance(waiting_visuals, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "scene.waiting_visuals must be an array")
    target = next(
        (
            item
            for item in waiting_visuals
            if isinstance(item, dict) and item.get("waiting_visual_id") == waiting_visual_id
        ),
        None,
    )
    if target is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown waiting visual '{waiting_visual_id}'")
    waiting_visuals.remove(target)
    return {
        "kind": "waiting_visual.delete",
        "scene_id": scene_id,
        "waiting_visual_id": waiting_visual_id,
    }


def _apply_state_set_waiting_visual(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "state_id", "waiting_visual_id"},
        {"kind", "scene_id", "state_id", "waiting_visual_id", "command_id"},
    )
    scene_id = command.get("scene_id")
    state_id = command.get("state_id")
    waiting_visual_id = command.get("waiting_visual_id")
    issues: list[ValidationIssue] = []
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(state_id, "command.state_id", issues)
    _stable_id(waiting_visual_id, "command.waiting_visual_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    scene = next((item for item in scenes if item.get("scene_id") == scene_id), None)
    if scene is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")
    if waiting_visual_id not in {
        item.get("waiting_visual_id")
        for item in scene.get("waiting_visuals", [])
        if isinstance(item, dict)
    }:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown waiting visual '{waiting_visual_id}'")
    state = next(
        (
            item
            for item in scene.get("states", [])
            if isinstance(item, dict) and item.get("state_id") == state_id
        ),
        None,
    )
    if state is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown state '{state_id}'")
    previous = state.get("waiting_visual_ref")
    state["waiting_visual_ref"] = waiting_visual_id
    return {
        "kind": "state.set_waiting_visual",
        "scene_id": scene_id,
        "state_id": state_id,
        "previous": previous,
        "waiting_visual_id": waiting_visual_id,
    }


def _primary_asset_catalog(
    project: dict[str, Any],
    catalogs: list[dict[str, Any]],
    sources: list[str],
) -> dict[str, Any]:
    if catalogs:
        return catalogs[0]
    source = "assets/catalog.json"
    catalog = {
        "schema_id": "peepshow.authoring.assets",
        "schema_version": 1,
        "assets": [],
        "animations": [],
        "audio_assets": [],
        "audio_cues": [],
    }
    catalogs.append(catalog)
    sources.append(source)
    project.setdefault("asset_sources", []).append(source)
    return catalog


def _catalog_record(
    catalogs: list[dict[str, Any]],
    collection: str,
    id_field: str,
    record_id: Any,
) -> tuple[dict[str, Any], dict[str, Any]] | None:
    for catalog in catalogs:
        records = catalog.get(collection)
        if not isinstance(records, list):
            continue
        for record in records:
            if isinstance(record, dict) and record.get(id_field) == record_id:
                return catalog, record
    return None


def _apply_asset_upsert(
    project: dict[str, Any],
    catalogs: list[dict[str, Any]],
    sources: list[str],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "asset"}, {"kind", "asset", "command_id"})
    asset = command.get("asset")
    if not isinstance(asset, dict):
        raise ProjectCommandError("COMMAND_SHAPE_INVALID", "command.asset must be an object")
    asset_id = asset.get("asset_id")
    issues: list[ValidationIssue] = []
    _stable_id(asset_id, "command.asset.asset_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "assets", "asset_id", asset_id)
    created = located is None
    if located is None:
        catalog = _primary_asset_catalog(project, catalogs, sources)
        records = catalog.setdefault("assets", [])
        if len(records) >= 128:
            raise ProjectCommandError("PROJECT_COUNT_INVALID", "asset catalog already has 128 assets")
        records.append(deepcopy(asset))
    else:
        catalog, previous = located
        records = catalog["assets"]
        records[records.index(previous)] = deepcopy(asset)
    return {"kind": "asset.upsert", "asset_id": asset_id, "created": created}


def _apply_asset_delete(
    catalogs: list[dict[str, Any]],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "asset_id"}, {"kind", "asset_id", "command_id"})
    asset_id = command.get("asset_id")
    issues: list[ValidationIssue] = []
    _stable_id(asset_id, "command.asset_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "assets", "asset_id", asset_id)
    if located is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown asset '{asset_id}'")
    catalog, asset = located
    frame_ids = {
        frame.get("frame_id")
        for frame in asset.get("frames", [])
        if isinstance(frame, dict)
    }
    for animation_catalog in catalogs:
        for animation in animation_catalog.get("animations", []):
            if frame_ids.intersection(animation.get("frame_refs", [])):
                raise ProjectCommandError("ASSET_IN_USE", "asset frames are referenced by an animation")
    for scene in scenes:
        for model in scene.get("render_models", []):
            for element in model.get("elements", []):
                if element.get("visual_ref") in frame_ids:
                    raise ProjectCommandError("ASSET_IN_USE", "asset frame is referenced by a render element")
        for waiting in scene.get("waiting_visuals", []):
            for element in waiting.get("elements", []):
                if frame_ids.intersection(element.get("phase_visual_refs", [])):
                    raise ProjectCommandError("ASSET_IN_USE", "asset frame is referenced by a waiting visual")
    catalog["assets"].remove(asset)
    return {"kind": "asset.delete", "asset_id": asset_id}


def _apply_animation_upsert(
    project: dict[str, Any],
    catalogs: list[dict[str, Any]],
    sources: list[str],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(command, {"kind", "animation"}, {"kind", "animation", "command_id"})
    animation = command.get("animation")
    if not isinstance(animation, dict):
        raise ProjectCommandError("COMMAND_SHAPE_INVALID", "command.animation must be an object")
    animation_id = animation.get("animation_id")
    issues: list[ValidationIssue] = []
    _stable_id(animation_id, "command.animation.animation_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "animations", "animation_id", animation_id)
    created = located is None
    if located is None:
        catalog = _primary_asset_catalog(project, catalogs, sources)
        records = catalog.setdefault("animations", [])
        if len(records) >= 128:
            raise ProjectCommandError("PROJECT_COUNT_INVALID", "asset catalog already has 128 animations")
        records.append(deepcopy(animation))
    else:
        catalog, previous = located
        records = catalog["animations"]
        records[records.index(previous)] = deepcopy(animation)
    return {"kind": "animation.upsert", "animation_id": animation_id, "created": created}


def _apply_animation_delete(
    catalogs: list[dict[str, Any]],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "animation_id"},
        {"kind", "animation_id", "command_id"},
    )
    animation_id = command.get("animation_id")
    issues: list[ValidationIssue] = []
    _stable_id(animation_id, "command.animation_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "animations", "animation_id", animation_id)
    if located is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown animation '{animation_id}'")
    for scene in scenes:
        for model in scene.get("render_models", []):
            for element in model.get("elements", []):
                if element.get("visual_ref") == animation_id:
                    raise ProjectCommandError("ASSET_IN_USE", "animation is referenced by a render element")
    catalog, animation = located
    catalog["animations"].remove(animation)
    return {"kind": "animation.delete", "animation_id": animation_id}


def _apply_audio_asset_upsert(
    project: dict[str, Any],
    catalogs: list[dict[str, Any]],
    sources: list[str],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "audio_asset"},
        {"kind", "audio_asset", "command_id"},
    )
    asset = command.get("audio_asset")
    if not isinstance(asset, dict):
        raise ProjectCommandError(
            "COMMAND_SHAPE_INVALID", "command.audio_asset must be an object"
        )
    asset_id = asset.get("asset_id")
    issues: list[ValidationIssue] = []
    _stable_id(asset_id, "command.audio_asset.asset_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "audio_assets", "asset_id", asset_id)
    created = located is None
    if located is None:
        catalog = _primary_asset_catalog(project, catalogs, sources)
        records = catalog.setdefault("audio_assets", [])
        if len(records) >= AUDIO_MAX_ASSETS:
            raise ProjectCommandError(
                "PROJECT_COUNT_INVALID",
                f"asset catalog already has {AUDIO_MAX_ASSETS} audio assets",
            )
        records.append(deepcopy(asset))
    else:
        catalog, previous = located
        records = catalog.setdefault("audio_assets", [])
        records[records.index(previous)] = deepcopy(asset)
    return {
        "kind": "audio_asset.upsert",
        "asset_id": asset_id,
        "created": created,
    }


def _apply_audio_asset_delete(
    catalogs: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "asset_id"},
        {"kind", "asset_id", "command_id"},
    )
    asset_id = command.get("asset_id")
    located = _catalog_record(catalogs, "audio_assets", "asset_id", asset_id)
    if located is None:
        raise ProjectCommandError(
            "COMMAND_TARGET_UNKNOWN", f"unknown audio asset '{asset_id}'"
        )
    for catalog in catalogs:
        for cue in catalog.get("audio_cues", []):
            if isinstance(cue, dict) and cue.get("asset_ref") == asset_id:
                raise ProjectCommandError(
                    "ASSET_IN_USE", "audio asset is referenced by an audio cue"
                )
    catalog, asset = located
    catalog["audio_assets"].remove(asset)
    return {"kind": "audio_asset.delete", "asset_id": asset_id}


def _apply_audio_cue_upsert(
    project: dict[str, Any],
    catalogs: list[dict[str, Any]],
    sources: list[str],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "audio_cue"},
        {"kind", "audio_cue", "command_id"},
    )
    cue = command.get("audio_cue")
    if not isinstance(cue, dict):
        raise ProjectCommandError(
            "COMMAND_SHAPE_INVALID", "command.audio_cue must be an object"
        )
    cue_id = cue.get("cue_id")
    issues: list[ValidationIssue] = []
    _stable_id(cue_id, "command.audio_cue.cue_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    located = _catalog_record(catalogs, "audio_cues", "cue_id", cue_id)
    created = located is None
    if located is None:
        catalog = _primary_asset_catalog(project, catalogs, sources)
        records = catalog.setdefault("audio_cues", [])
        if len(records) >= AUDIO_MAX_CUES:
            raise ProjectCommandError(
                "PROJECT_COUNT_INVALID",
                f"asset catalog already has {AUDIO_MAX_CUES} audio cues",
            )
        records.append(deepcopy(cue))
    else:
        catalog, previous = located
        records = catalog.setdefault("audio_cues", [])
        records[records.index(previous)] = deepcopy(cue)
    return {"kind": "audio_cue.upsert", "cue_id": cue_id, "created": created}


def _apply_audio_cue_delete(
    catalogs: list[dict[str, Any]],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "cue_id"},
        {"kind", "cue_id", "command_id"},
    )
    cue_id = command.get("cue_id")
    located = _catalog_record(catalogs, "audio_cues", "cue_id", cue_id)
    if located is None:
        raise ProjectCommandError(
            "COMMAND_TARGET_UNKNOWN", f"unknown audio cue '{cue_id}'"
        )
    for scene in scenes:
        for route in scene.get("routes", []):
            for action in route.get("actions", []):
                if (
                    isinstance(action, dict)
                    and action.get("kind") == "play_sfx"
                    and action.get("cue_ref") == cue_id
                ):
                    raise ProjectCommandError(
                        "ASSET_IN_USE", "audio cue is referenced by a STATE action"
                    )
    catalog, cue = located
    catalog["audio_cues"].remove(cue)
    return {"kind": "audio_cue.delete", "cue_id": cue_id}


def _layout_coordinate(value: Any, path: str) -> int:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"{path} must be a number")
    if not -100000 <= value <= 100000:
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"{path} is outside the supported editor layout range")
    return int(round(value))


def _apply_scene_flow_node_position(
    project: dict[str, Any],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "x", "y"},
        {"kind", "scene_id", "x", "y", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    _stable_id(scene_id, "command.scene_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    if scene_id not in {
        scene.get("scene_id")
        for scene in scenes
        if isinstance(scene, dict)
    }:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")

    x = _layout_coordinate(command.get("x"), "command.x")
    y = _layout_coordinate(command.get("y"), "command.y")
    editor = project.setdefault("editor", {})
    if not isinstance(editor, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor must be an object")
    scene_flow = editor.setdefault("scene_flow", {})
    if not isinstance(scene_flow, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor.scene_flow must be an object")
    nodes = scene_flow.setdefault("nodes", {})
    if not isinstance(nodes, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor.scene_flow.nodes must be an object")
    nodes[scene_id] = {"x": x, "y": y}
    return {
        "kind": "editor.scene_flow.set_node_position",
        "scene_id": scene_id,
        "x": x,
        "y": y,
    }


def _apply_state_graph_node_position(
    project: dict[str, Any],
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "state_id", "x", "y"},
        {"kind", "scene_id", "state_id", "x", "y", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    state_id = command.get("state_id")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(state_id, "command.state_id", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    target_scene = next(
        (
            scene
            for scene in scenes
            if isinstance(scene, dict) and scene.get("scene_id") == scene_id
        ),
        None,
    )
    if target_scene is None:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")
    state_ids = {
        state.get("state_id")
        for state in target_scene.get("states", [])
        if isinstance(state, dict)
    }
    if state_id not in state_ids:
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown state '{state_id}'")

    x = _layout_coordinate(command.get("x"), "command.x")
    y = _layout_coordinate(command.get("y"), "command.y")
    editor = project.setdefault("editor", {})
    if not isinstance(editor, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor must be an object")
    state_graph = editor.setdefault("state_graph", {})
    if not isinstance(state_graph, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor.state_graph must be an object")
    graph_scenes = state_graph.setdefault("scenes", {})
    if not isinstance(graph_scenes, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "project.editor.state_graph.scenes must be an object")
    scene_layout = graph_scenes.setdefault(scene_id, {})
    if not isinstance(scene_layout, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"project.editor.state_graph.scenes[{scene_id}] must be an object")
    nodes = scene_layout.setdefault("nodes", {})
    if not isinstance(nodes, dict):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", f"project.editor.state_graph.scenes[{scene_id}].nodes must be an object")
    nodes[state_id] = {"x": x, "y": y}
    return {
        "kind": "editor.state_graph.set_node_position",
        "scene_id": scene_id,
        "state_id": state_id,
        "x": x,
        "y": y,
    }


def _normalize_guard(scene: dict[str, Any], guard: Any) -> dict[str, Any]:
    if not isinstance(guard, dict):
        raise ProjectCommandError("GUARD_TYPE_MISMATCH", "guard must be an object")
    _require_command_fields(guard, {"variable_ref", "operator", "value"}, {"variable_ref", "operator", "value"})
    variable_ref = guard.get("variable_ref")
    operator = guard.get("operator")
    value = guard.get("value")
    issues: list[ValidationIssue] = []
    _stable_id(variable_ref, "command.guard.variable_ref", issues)
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)
    variable_ids = {
        variable.get("variable_id")
        for variable in scene.get("variables", [])
        if isinstance(variable, dict)
    }
    if variable_ref not in variable_ids:
        raise ProjectCommandError("GUARD_VARIABLE_UNKNOWN", f"unknown variable '{variable_ref}'")
    if operator not in GUARD_OPERATORS:
        raise ProjectCommandError("GUARD_OPERATOR_INVALID", "unsupported comparison")
    if isinstance(value, bool) or not isinstance(value, int):
        raise ProjectCommandError("GUARD_TYPE_MISMATCH", "guard value must be an integer")
    return {"variable_ref": variable_ref, "operator": operator, "value": value}


def _route_target_elements(
    scene: dict[str, Any],
    route: dict[str, Any],
) -> dict[str, dict[str, Any]]:
    target_state = route.get("target_state")
    state = next(
        (
            item
            for item in scene.get("states", [])
            if isinstance(item, dict) and item.get("state_id") == target_state
        ),
        None,
    )
    if state is None:
        raise ProjectCommandError(
            "GRAPH_TRANSITION_TARGET_UNKNOWN",
            "element actions require a valid target_state",
        )
    render_model = next(
        (
            item
            for item in scene.get("render_models", [])
            if isinstance(item, dict)
            and item.get("visual_id") == state.get("render_model_ref")
        ),
        None,
    )
    if render_model is None:
        raise ProjectCommandError(
            "RENDER_MODEL_UNKNOWN",
            "target state render model does not exist",
        )
    return {
        str(item.get("element_id")): item
        for item in render_model.get("elements", [])
        if isinstance(item, dict) and isinstance(item.get("element_id"), str)
    }


def _normalize_action(
    scene: dict[str, Any],
    route: dict[str, Any],
    action: Any,
) -> dict[str, Any]:
    if not isinstance(action, dict):
        raise ProjectCommandError("ACTION_TYPE_INVALID", "action must be an object")
    action_kind = action.get("kind")
    if action_kind == "set_variable":
        _require_command_fields(action, {"kind", "variable_ref", "operation", "value"}, {"kind", "variable_ref", "operation", "value"})
        variable_ref = action.get("variable_ref")
        operation = action.get("operation")
        value = action.get("value")
        issues: list[ValidationIssue] = []
        _stable_id(variable_ref, "command.action.variable_ref", issues)
        if issues:
            issue = issues[0]
            raise ProjectCommandError(issue.code, issue.message)
        variable_ids = {
            variable.get("variable_id")
            for variable in scene.get("variables", [])
            if isinstance(variable, dict)
        }
        if variable_ref not in variable_ids:
            raise ProjectCommandError("ACTION_VARIABLE_UNKNOWN", f"unknown variable '{variable_ref}'")
        if operation not in ACTION_OPERATIONS:
            raise ProjectCommandError("ACTION_OPERATION_INVALID", "action operation must be assign or add")
        if isinstance(value, bool) or not isinstance(value, int):
            raise ProjectCommandError("ACTION_TYPE_INVALID", "action value must be an integer")
        return {"kind": "set_variable", "variable_ref": variable_ref, "operation": operation, "value": value}
    if action_kind == "request_render":
        _require_command_fields(action, {"kind"}, {"kind"})
        return {"kind": "request_render"}
    if action_kind == "play_sfx":
        _require_command_fields(
            action,
            {"kind", "cue_ref"},
            {"kind", "cue_ref"},
        )
        cue_ref = action.get("cue_ref")
        issues: list[ValidationIssue] = []
        _stable_id(cue_ref, "command.action.cue_ref", issues)
        if issues:
            issue = issues[0]
            raise ProjectCommandError(issue.code, issue.message)
        return {"kind": "play_sfx", "cue_ref": cue_ref}
    if action_kind in {
        "set_element_visibility",
        "set_element_position",
        "set_element_frame",
        "set_element_waiting_animation",
    }:
        elements = _route_target_elements(scene, route)
        element_ref = action.get("element_ref")
        issues: list[ValidationIssue] = []
        _stable_id(element_ref, "command.action.element_ref", issues)
        if issues:
            issue = issues[0]
            raise ProjectCommandError(issue.code, issue.message)
        element = elements.get(element_ref)
        if element is None:
            raise ProjectCommandError(
                "ACTION_ELEMENT_UNKNOWN",
                f"target state has no element '{element_ref}'",
            )
        if action_kind == "set_element_visibility":
            _require_command_fields(
                action,
                {"kind", "element_ref", "visible"},
                {"kind", "element_ref", "visible"},
            )
            visible = action.get("visible")
            if not isinstance(visible, bool):
                raise ProjectCommandError(
                    "ACTION_TYPE_INVALID", "visible must be true or false"
                )
            if not visible and element.get("focus_role", "none") == "focus":
                raise ProjectCommandError(
                    "RENDER_FOCUS_INVALID", "the focus element must remain visible"
                )
            return {
                "kind": action_kind,
                "element_ref": element_ref,
                "visible": visible,
            }
        if action_kind == "set_element_position":
            _require_command_fields(
                action,
                {"kind", "element_ref", "x", "y"},
                {"kind", "element_ref", "x", "y"},
            )
            x = action.get("x")
            y = action.get("y")
            if (
                isinstance(x, bool)
                or not isinstance(x, int)
                or isinstance(y, bool)
                or not isinstance(y, int)
            ):
                raise ProjectCommandError(
                    "ACTION_TYPE_INVALID", "position must use integer x and y"
                )
            if (
                x < 0
                or y < 0
                or x + int(element.get("width", 0)) > 168
                or y + int(element.get("height", 0)) > 144
            ):
                raise ProjectCommandError(
                    "RENDER_BOUNDS_INVALID",
                    "element action would move outside the 168x144 display",
                )
            return {
                "kind": action_kind,
                "element_ref": element_ref,
                "x": x,
                "y": y,
            }
        if action_kind == "set_element_waiting_animation":
            _require_command_fields(
                action,
                {
                    "kind",
                    "element_ref",
                    "waiting_visual_ref",
                    "waiting_element_ref",
                    "timeline_policy",
                },
                {
                    "kind",
                    "element_ref",
                    "waiting_visual_ref",
                    "waiting_element_ref",
                    "timeline_policy",
                },
            )
            waiting_visual_ref = action.get("waiting_visual_ref")
            waiting_element_ref = action.get("waiting_element_ref")
            timeline_policy = action.get("timeline_policy")
            issues = []
            _stable_id(
                waiting_visual_ref,
                "command.action.waiting_visual_ref",
                issues,
            )
            _stable_id(
                waiting_element_ref,
                "command.action.waiting_element_ref",
                issues,
            )
            if issues:
                issue = issues[0]
                raise ProjectCommandError(issue.code, issue.message)
            waiting_visual = next(
                (
                    item
                    for item in scene.get("waiting_visuals", [])
                    if isinstance(item, dict)
                    and item.get("waiting_visual_id") == waiting_visual_ref
                ),
                None,
            )
            if waiting_visual is None:
                raise ProjectCommandError(
                    "WAIT_VISUAL_UNKNOWN",
                    f"unknown waiting visual '{waiting_visual_ref}'",
                )
            waiting_element = next(
                (
                    item
                    for item in waiting_visual.get("elements", [])
                    if isinstance(item, dict)
                    and item.get("element_id") == waiting_element_ref
                ),
                None,
            )
            if waiting_element is None:
                raise ProjectCommandError(
                    "WAIT_ELEMENT_UNKNOWN",
                    f"waiting visual has no element '{waiting_element_ref}'",
                )
            if waiting_element.get("source_element_ref") != element_ref:
                raise ProjectCommandError(
                    "ACTION_WAIT_ELEMENT_MISMATCH",
                    "waiting animation must target the same retained element",
                )
            target_state = next(
                item
                for item in scene.get("states", [])
                if isinstance(item, dict)
                and item.get("state_id") == route.get("target_state")
            )
            target_waiting = next(
                item
                for item in scene.get("waiting_visuals", [])
                if isinstance(item, dict)
                and item.get("waiting_visual_id")
                == target_state.get("waiting_visual_ref")
            )
            if (
                waiting_visual.get("phase_quantum_ms")
                != target_waiting.get("phase_quantum_ms")
                or waiting_visual.get("combined_step_count")
                != target_waiting.get("combined_step_count")
            ):
                raise ProjectCommandError(
                    "ACTION_WAIT_TIMELINE_INCOMPATIBLE",
                    "waiting animation cadence and step count must match the target state",
                )
            if timeline_policy not in {"preserve", "rebase"}:
                raise ProjectCommandError(
                    "ACTION_TIMELINE_POLICY_INVALID",
                    "timeline_policy must be preserve or rebase",
                )
            if element.get("kind") != "sprite":
                raise ProjectCommandError(
                    "ACTION_ELEMENT_TYPE_INVALID",
                    "waiting animation selection requires a sprite element",
                )
            return {
                "kind": action_kind,
                "element_ref": element_ref,
                "waiting_visual_ref": waiting_visual_ref,
                "waiting_element_ref": waiting_element_ref,
                "timeline_policy": timeline_policy,
            }
        _require_command_fields(
            action,
            {"kind", "element_ref", "frame_ref"},
            {"kind", "element_ref", "frame_ref"},
        )
        frame_ref = action.get("frame_ref")
        issues = []
        _stable_id(frame_ref, "command.action.frame_ref", issues)
        if issues:
            issue = issues[0]
            raise ProjectCommandError(issue.code, issue.message)
        if element.get("kind") != "sprite":
            raise ProjectCommandError(
                "ACTION_ELEMENT_TYPE_INVALID",
                "frame selection requires a sprite element",
            )
        return {
            "kind": action_kind,
            "element_ref": element_ref,
            "frame_ref": frame_ref,
        }
    raise ProjectCommandError("ACTION_KIND_INVALID", "unsupported V1 action")


def _apply_route_guard_list(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    required = {"kind", "scene_id", "route_id", "guard_index"}
    allowed = required | {"guard", "target_index", "command_id"}
    if kind == "route.guard.add":
        required.add("guard")
    elif kind == "route.guard.move":
        required.add("target_index")
    _require_command_fields(command, required, allowed)
    scene = _command_scene(scenes, command.get("scene_id"))
    route = _command_record(scene, "routes", "route_id", command.get("route_id"))
    guards = route.get("guards")
    if not isinstance(guards, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "route.guards must be an array")
    guard_index = _command_index(command.get("guard_index"), "guard_index")
    if kind == "route.guard.add":
        if len(guards) >= 8:
            raise ProjectCommandError("GUARD_LIMIT_EXCEEDED", "route supports at most 8 guards")
        if guard_index > len(guards):
            raise ProjectCommandError("COMMAND_INDEX_INVALID", "guard_index must select an insertion position")
        guard = _normalize_guard(scene, command.get("guard"))
        guards.insert(guard_index, guard)
        return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "guard_index": guard_index, "guard": guard}
    source_index = _command_move_index(guard_index, "guard_index", len(guards))
    if kind == "route.guard.delete":
        guard = guards.pop(source_index)
        return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "guard_index": source_index, "guard": guard}
    target_index = _command_move_index(command.get("target_index"), "target_index", len(guards))
    guard = guards.pop(source_index)
    guards.insert(target_index, guard)
    return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "guard_index": source_index, "target_index": target_index}


def _apply_route_action_list(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    kind = command.get("kind")
    required = {"kind", "scene_id", "route_id", "action_index"}
    allowed = required | {"action", "target_index", "command_id"}
    if kind == "route.action.add":
        required.add("action")
    elif kind == "route.action.move":
        required.add("target_index")
    _require_command_fields(command, required, allowed)
    scene = _command_scene(scenes, command.get("scene_id"))
    route = _command_record(scene, "routes", "route_id", command.get("route_id"))
    if "target_scene" in route:
        raise ProjectCommandError("SCENE_TRANSITION_ACTION_UNSUPPORTED", "direct scene replacement requires an empty route action list")
    actions = route.get("actions")
    if not isinstance(actions, list):
        raise ProjectCommandError("PROJECT_TYPE_INVALID", "route.actions must be an array")
    action_index = _command_index(command.get("action_index"), "action_index")
    if kind == "route.action.add":
        if len(actions) >= 8:
            raise ProjectCommandError("ACTION_BUDGET_EXCEEDED", "route supports at most 8 actions")
        if action_index > len(actions):
            raise ProjectCommandError("COMMAND_INDEX_INVALID", "action_index must select an insertion position")
        action = _normalize_action(scene, route, command.get("action"))
        actions.insert(action_index, action)
        return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "action_index": action_index, "action": action}
    source_index = _command_move_index(action_index, "action_index", len(actions))
    if kind == "route.action.delete":
        action = actions.pop(source_index)
        return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "action_index": source_index, "action": action}
    target_index = _command_move_index(command.get("target_index"), "target_index", len(actions))
    action = actions.pop(source_index)
    actions.insert(target_index, action)
    return {"kind": kind, "scene_id": scene.get("scene_id"), "route_id": route.get("route_id"), "action_index": source_index, "target_index": target_index}


def _apply_route_set_guard(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "route_id", "guard_index", "variable_ref", "operator", "value"},
        {"kind", "scene_id", "route_id", "guard_index", "variable_ref", "operator", "value", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    route_id = command.get("route_id")
    guard_index = command.get("guard_index")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(route_id, "command.route_id", issues)
    if isinstance(guard_index, bool) or not isinstance(guard_index, int) or guard_index < 0:
        raise ProjectCommandError("COMMAND_INDEX_INVALID", "guard_index must be a non-negative integer")
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        normalized_guard = _normalize_guard(
            scene,
            {
                "variable_ref": command.get("variable_ref"),
                "operator": command.get("operator"),
                "value": command.get("value"),
            },
        )
        for route in scene.get("routes", []):
            if not isinstance(route, dict) or route.get("route_id") != route_id:
                continue
            guards = route.get("guards")
            if not isinstance(guards, list) or guard_index >= len(guards):
                raise ProjectCommandError("COMMAND_INDEX_INVALID", "guard_index does not select an existing guard")
            guards[guard_index] = normalized_guard
            return {
                "kind": "route.set_guard",
                "scene_id": scene_id,
                "route_id": route_id,
                "guard_index": guard_index,
                **normalized_guard,
            }
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown route '{route_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _apply_route_set_action(
    scenes: list[dict[str, Any]],
    command: dict[str, Any],
) -> dict[str, Any]:
    _require_command_fields(
        command,
        {"kind", "scene_id", "route_id", "action_index", "action"},
        {"kind", "scene_id", "route_id", "action_index", "action", "command_id"},
    )
    issues: list[ValidationIssue] = []
    scene_id = command.get("scene_id")
    route_id = command.get("route_id")
    action_index = command.get("action_index")
    action = command.get("action")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(route_id, "command.route_id", issues)
    if isinstance(action_index, bool) or not isinstance(action_index, int) or action_index < 0:
        raise ProjectCommandError("COMMAND_INDEX_INVALID", "action_index must be a non-negative integer")
    if not isinstance(action, dict):
        raise ProjectCommandError("ACTION_TYPE_INVALID", "action must be an object")
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        for route in scene.get("routes", []):
            if not isinstance(route, dict) or route.get("route_id") != route_id:
                continue
            normalized_action = _normalize_action(scene, route, action)
            actions = route.get("actions")
            if not isinstance(actions, list) or action_index >= len(actions):
                raise ProjectCommandError("COMMAND_INDEX_INVALID", "action_index does not select an existing action")
            actions[action_index] = normalized_action
            return {
                "kind": "route.set_action",
                "scene_id": scene_id,
                "route_id": route_id,
                "action_index": action_index,
                "action": normalized_action,
            }
        raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown route '{route_id}'")
    raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", f"unknown scene '{scene_id}'")


def _read_json(path: Path, issues: list[ValidationIssue], code: str) -> dict[str, Any] | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        _issue(issues, code, str(path), f"could not read file: {exc}")
        return None
    except json.JSONDecodeError as exc:
        _issue(issues, code, str(path), f"invalid JSON at line {exc.lineno}, column {exc.colno}")
        return None
    if not isinstance(value, dict):
        _issue(issues, code, str(path), "root value must be an object")
        return None
    return value


def _check_keys(
    value: dict[str, Any],
    required: set[str],
    path: str,
    issues: list[ValidationIssue],
    allowed: set[str] | None = None,
) -> None:
    allowed_keys = required if allowed is None else allowed
    for key in sorted(required - value.keys()):
        _issue(issues, "PROJECT_FIELD_MISSING", f"{path}.{key}", "required field is missing")
    for key in sorted(value.keys() - allowed_keys):
        _issue(issues, "PROJECT_FIELD_UNKNOWN", f"{path}.{key}", "field is not part of the V1 subset")


def _stable_id(value: Any, path: str, issues: list[ValidationIssue]) -> bool:
    if not isinstance(value, str) or STABLE_ID.fullmatch(value) is None:
        _issue(issues, "PROJECT_ID_INVALID", path, "must be a lowercase stable ID")
        return False
    return True


def _text(value: Any, path: str, issues: list[ValidationIssue], maximum: int = 96) -> bool:
    if not isinstance(value, str) or not 1 <= len(value) <= maximum:
        _issue(issues, "PROJECT_TEXT_INVALID", path, f"must be text with 1..{maximum} characters")
        return False
    return True


def _unique_ids(
    records: Any,
    field: str,
    path: str,
    issues: list[ValidationIssue],
    maximum: int,
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    if not isinstance(records, list):
        _issue(issues, "PROJECT_TYPE_INVALID", path, "must be an array")
        return result
    if len(records) > maximum:
        _issue(issues, "PROJECT_LIMIT_EXCEEDED", path, f"contains more than {maximum} records")
    for index, record in enumerate(records):
        item_path = f"{path}[{index}]"
        if not isinstance(record, dict):
            _issue(issues, "PROJECT_TYPE_INVALID", item_path, "must be an object")
            continue
        record_id = record.get(field)
        if not _stable_id(record_id, f"{item_path}.{field}", issues):
            continue
        if record_id in result:
            _issue(issues, "PROJECT_ID_DUPLICATE", f"{item_path}.{field}", f"duplicate ID '{record_id}'")
            continue
        result[record_id] = record
    return result


def _check_project(project: dict[str, Any], issues: list[ValidationIssue]) -> None:
    _check_keys(project, PROJECT_KEYS, "project", issues, PROJECT_KEYS | PROJECT_OPTIONAL_KEYS)
    if project.get("schema_id") != "peepshow.authoring.project" or project.get("schema_version") != 1:
        _issue(issues, "PROJECT_SCHEMA_UNSUPPORTED", "project", "expected peepshow.authoring.project version 1")
    _stable_id(project.get("project_id"), "project.project_id", issues)
    _text(project.get("project_name"), "project.project_name", issues)
    _stable_id(project.get("selected_target_profile"), "project.selected_target_profile", issues)
    _stable_id(project.get("entry_scene"), "project.entry_scene", issues)

    package = project.get("package")
    if not isinstance(package, dict):
        _issue(issues, "PROJECT_TYPE_INVALID", "project.package", "must be an object")
    else:
        expected = {"package_id", "display_name", "version"}
        _check_keys(package, expected, "project.package", issues)
        _stable_id(package.get("package_id"), "project.package.package_id", issues)
        _text(package.get("display_name"), "project.package.display_name", issues)
        version = package.get("version")
        if not isinstance(version, str) or re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
            _issue(issues, "PROJECT_VERSION_INVALID", "project.package.version", "must be major.minor.patch")

    sources = project.get("scene_sources")
    if not isinstance(sources, list) or not sources:
        _issue(issues, "SCENE_SOURCE_MISSING", "project.scene_sources", "must contain at least one scene source")
    elif len(sources) > 32:
        _issue(issues, "PROJECT_LIMIT_EXCEEDED", "project.scene_sources", "contains more than 32 scene sources")
    else:
        seen_sources: set[str] = set()
        for index, source in enumerate(sources):
            if not isinstance(source, str) or not source:
                _issue(issues, "SCENE_SOURCE_INVALID", f"project.scene_sources[{index}]", "must be a non-empty path")
            elif source in seen_sources:
                _issue(issues, "SCENE_SOURCE_DUPLICATE", f"project.scene_sources[{index}]", "scene source path is duplicated")
            else:
                seen_sources.add(source)

    asset_sources = project.get("asset_sources", [])
    if not isinstance(asset_sources, list):
        _issue(issues, "PROJECT_TYPE_INVALID", "project.asset_sources", "must be an array")
    elif len(asset_sources) > 16:
        _issue(issues, "PROJECT_LIMIT_EXCEEDED", "project.asset_sources", "contains more than 16 asset catalogs")
    else:
        seen_asset_sources: set[str] = set()
        for index, source in enumerate(asset_sources):
            path = f"project.asset_sources[{index}]"
            if not isinstance(source, str) or not source:
                _issue(issues, "ASSET_SOURCE_INVALID", path, "must be a non-empty path")
            elif source in seen_asset_sources:
                _issue(issues, "ASSET_SOURCE_DUPLICATE", path, "asset catalog path is duplicated")
            else:
                seen_asset_sources.add(source)

    validation = project.get("validation")
    if not isinstance(validation, dict):
        _issue(issues, "PROJECT_TYPE_INVALID", "project.validation", "must be an object")
    else:
        _check_keys(validation, {"build_profile", "ruleset_version"}, "project.validation", issues)
        if validation.get("build_profile") not in {"development", "shipping"}:
            _issue(issues, "PROJECT_BUILD_PROFILE_INVALID", "project.validation.build_profile", "must be development or shipping")
        ruleset = validation.get("ruleset_version")
        if not isinstance(ruleset, int) or not 1 <= ruleset <= 65535:
            _issue(issues, "PROJECT_RULESET_INVALID", "project.validation.ruleset_version", "must be in 1..65535")

    editor = project.get("editor")
    if editor is not None:
        if not isinstance(editor, dict):
            _issue(issues, "PROJECT_TYPE_INVALID", "project.editor", "must be an object")
        else:
            _check_keys(editor, set(), "project.editor", issues, {"scene_flow", "state_graph"})
            scene_flow = editor.get("scene_flow")
            if scene_flow is not None:
                if not isinstance(scene_flow, dict):
                    _issue(issues, "PROJECT_TYPE_INVALID", "project.editor.scene_flow", "must be an object")
                else:
                    _check_keys(scene_flow, set(), "project.editor.scene_flow", issues, {"nodes"})
                    nodes = scene_flow.get("nodes")
                    if nodes is not None:
                        if not isinstance(nodes, dict):
                            _issue(issues, "PROJECT_TYPE_INVALID", "project.editor.scene_flow.nodes", "must be an object")
                        elif len(nodes) > 128:
                            _issue(issues, "PROJECT_LIMIT_EXCEEDED", "project.editor.scene_flow.nodes", "contains more than 128 nodes")
                        else:
                            for scene_id, position in nodes.items():
                                path = f"project.editor.scene_flow.nodes[{scene_id}]"
                                _stable_id(scene_id, path, issues)
                                if not isinstance(position, dict):
                                    _issue(issues, "PROJECT_TYPE_INVALID", path, "must be an object")
                                    continue
                                _check_keys(position, {"x", "y"}, path, issues)
                                for axis in ("x", "y"):
                                    value = position.get(axis)
                                    if isinstance(value, bool) or not isinstance(value, (int, float)):
                                        _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.{axis}", "must be a number")
                                    elif not -100000 <= value <= 100000:
                                        _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.{axis}", "outside supported editor layout range")
            state_graph = editor.get("state_graph")
            if state_graph is not None:
                if not isinstance(state_graph, dict):
                    _issue(issues, "PROJECT_TYPE_INVALID", "project.editor.state_graph", "must be an object")
                else:
                    _check_keys(state_graph, set(), "project.editor.state_graph", issues, {"scenes"})
                    graph_scenes = state_graph.get("scenes")
                    if graph_scenes is not None:
                        if not isinstance(graph_scenes, dict):
                            _issue(issues, "PROJECT_TYPE_INVALID", "project.editor.state_graph.scenes", "must be an object")
                        elif len(graph_scenes) > 128:
                            _issue(issues, "PROJECT_LIMIT_EXCEEDED", "project.editor.state_graph.scenes", "contains more than 128 scenes")
                        else:
                            for scene_id, scene_layout in graph_scenes.items():
                                scene_path = f"project.editor.state_graph.scenes[{scene_id}]"
                                _stable_id(scene_id, scene_path, issues)
                                if not isinstance(scene_layout, dict):
                                    _issue(issues, "PROJECT_TYPE_INVALID", scene_path, "must be an object")
                                    continue
                                _check_keys(scene_layout, set(), scene_path, issues, {"nodes"})
                                nodes = scene_layout.get("nodes")
                                if nodes is None:
                                    continue
                                if not isinstance(nodes, dict):
                                    _issue(issues, "PROJECT_TYPE_INVALID", f"{scene_path}.nodes", "must be an object")
                                elif len(nodes) > 128:
                                    _issue(issues, "PROJECT_LIMIT_EXCEEDED", f"{scene_path}.nodes", "contains more than 128 nodes")
                                else:
                                    for state_id, position in nodes.items():
                                        path = f"{scene_path}.nodes[{state_id}]"
                                        _stable_id(state_id, path, issues)
                                        if not isinstance(position, dict):
                                            _issue(issues, "PROJECT_TYPE_INVALID", path, "must be an object")
                                            continue
                                        _check_keys(position, {"x", "y"}, path, issues)
                                        for axis in ("x", "y"):
                                            value = position.get(axis)
                                            if isinstance(value, bool) or not isinstance(value, (int, float)):
                                                _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.{axis}", "must be a number")
                                            elif not -100000 <= value <= 100000:
                                                _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.{axis}", "outside supported editor layout range")


def _check_asset(
    asset: dict[str, Any],
    path: str,
    root: Path,
    issues: list[ValidationIssue],
) -> tuple[Masked1bppFrame, ...]:
    initial_issue_count = len(issues)
    source_format = asset.get("source_format")
    if source_format == "png":
        required = {"asset_id", "asset_type", "source_path", "source_format", "frames"}
    elif source_format == "system_font_text":
        required = {"asset_id", "asset_type", "source_format", "font_id", "text", "scale", "frames"}
    else:
        required = {"asset_id", "asset_type", "source_format", "frames"}
    _check_keys(asset, required, path, issues)
    _stable_id(asset.get("asset_id"), f"{path}.asset_id", issues)
    if asset.get("asset_type") != "masked_1bpp":
        _issue(issues, "ASSET_FORMAT_UNSUPPORTED", f"{path}.asset_type", "must be masked_1bpp")
    if source_format not in {"png", "system_font_text"}:
        _issue(issues, "ASSET_FORMAT_UNSUPPORTED", f"{path}.source_format", "must be png or system_font_text")
    if source_format == "png" and (not isinstance(asset.get("source_path"), str) or not asset.get("source_path")):
        _issue(issues, "ASSET_SOURCE_INVALID", f"{path}.source_path", "must be a non-empty relative path")

    frame_records = _unique_ids(asset.get("frames"), "frame_id", f"{path}.frames", issues, 256)
    if not frame_records:
        _issue(issues, "ASSET_FRAME_MISSING", f"{path}.frames", "must contain at least one frame")
    for frame_id, frame in frame_records.items():
        frame_path = f"{path}.frames[{frame_id}]"
        frame_keys = {"frame_id", "source_rect", "pivot_x", "pivot_y"} if source_format == "png" else {"frame_id", "pivot_x", "pivot_y"}
        _check_keys(frame, frame_keys, frame_path, issues)

    if len(issues) == initial_issue_count and frame_records:
        try:
            compiled = (
                import_masked_1bpp(root, asset)
                if source_format == "png"
                else (rasterize_system_font_text(asset),)
            )
        except (ImageAssetError, SystemFontError, KeyError) as exc:
            _issue(issues, "ASSET_SOURCE_INVALID", path, str(exc))
        else:
            return compiled
    return ()


def _check_animation(animation: dict[str, Any], path: str, issues: list[ValidationIssue]) -> None:
    _check_keys(
        animation,
        {"animation_id", "frame_refs", "frame_duration_ms", "loop_policy"},
        path,
        issues,
    )
    _stable_id(animation.get("animation_id"), f"{path}.animation_id", issues)
    frame_refs = animation.get("frame_refs")
    durations = animation.get("frame_duration_ms")
    if not isinstance(frame_refs, list) or not 1 <= len(frame_refs) <= 256:
        _issue(issues, "ANIMATION_FRAME_COUNT_INVALID", f"{path}.frame_refs", "must contain 1..256 frame IDs")
    else:
        for index, frame_ref in enumerate(frame_refs):
            _stable_id(frame_ref, f"{path}.frame_refs[{index}]", issues)
    if not isinstance(durations, list) or not isinstance(frame_refs, list) or len(durations) != len(frame_refs):
        _issue(issues, "ANIMATION_DURATION_INVALID", f"{path}.frame_duration_ms", "must match frame_refs")
    else:
        for index, duration in enumerate(durations):
            if isinstance(duration, bool) or not isinstance(duration, int) or not 1 <= duration <= 60000:
                _issue(issues, "ANIMATION_DURATION_INVALID", f"{path}.frame_duration_ms[{index}]", "must be in 1..60000 ms")
    if animation.get("loop_policy") not in {"loop", "once", "hold_last", "ping_pong"}:
        _issue(issues, "ANIMATION_LOOP_INVALID", f"{path}.loop_policy", "unsupported loop policy")


def _check_variable(record: dict[str, Any], path: str, issues: list[ValidationIssue]) -> None:
    _check_keys(record, {"variable_id", "value_type", "initial", "minimum", "maximum"}, path, issues)
    if record.get("value_type") != "int32":
        _issue(issues, "VARIABLE_TYPE_INVALID", f"{path}.value_type", "V1 supports int32 only")
    values = (record.get("minimum"), record.get("initial"), record.get("maximum"))
    if any(isinstance(value, bool) or not isinstance(value, int) for value in values):
        _issue(issues, "VARIABLE_RANGE_INVALID", path, "minimum, initial, and maximum must be integers")
    elif not values[0] <= values[1] <= values[2]:
        _issue(issues, "VARIABLE_RANGE_INVALID", path, "must satisfy minimum <= initial <= maximum")


def _check_waiting_visual(
    record: dict[str, Any],
    path: str,
    element_kinds: dict[str, str],
    frame_ids: set[str],
    issues: list[ValidationIssue],
) -> None:
    required = {
        "waiting_visual_id",
        "presentation_id",
        "phase_quantum_ms",
        "combined_step_count",
        "settled_step",
        "cycle_policy",
        "elements",
    }
    _check_keys(record, required, path, issues)
    _stable_id(record.get("presentation_id"), f"{path}.presentation_id", issues)
    step_count = record.get("combined_step_count")
    settled_step = record.get("settled_step")
    quantum = record.get("phase_quantum_ms")
    if not isinstance(step_count, int) or not 1 <= step_count <= 12:
        _issue(issues, "WAIT_STEP_COUNT_INVALID", f"{path}.combined_step_count", "must be in 1..12")
        step_count = None
    if not isinstance(settled_step, int) or step_count is None or not 0 <= settled_step < step_count:
        _issue(issues, "WAIT_SETTLED_STEP_INVALID", f"{path}.settled_step", "must select a combined step")
    if not isinstance(quantum, int) or not 1 <= quantum <= 60000:
        _issue(issues, "WAIT_QUANTUM_INVALID", f"{path}.phase_quantum_ms", "must be in 1..60000 ms")
    if record.get("cycle_policy") != "loop":
        _issue(issues, "WAIT_CYCLE_INVALID", f"{path}.cycle_policy", "V1 supports loop only")
    waiting_elements = _unique_ids(record.get("elements"), "element_id", f"{path}.elements", issues, 32)
    for element_id, element in waiting_elements.items():
        item_path = f"{path}.elements[{element_id}]"
        required_element = {"element_id", "source_element_ref", "phase_visual_refs", "step_phase_indices"}
        _check_keys(element, required_element, item_path, issues)
        source_ref = element.get("source_element_ref")
        if source_ref not in element_kinds:
            _issue(issues, "WAIT_ELEMENT_UNKNOWN", f"{item_path}.source_element_ref", f"unknown render element '{source_ref}'")
        elif element_kinds[source_ref] != "sprite":
            _issue(issues, "WAIT_ELEMENT_TYPE_INVALID", f"{item_path}.source_element_ref", "waiting animation currently requires a sprite element")
        phase_refs = element.get("phase_visual_refs")
        if not isinstance(phase_refs, list) or not 1 <= len(phase_refs) <= 4:
            _issue(issues, "WAIT_PHASE_COUNT_INVALID", f"{item_path}.phase_visual_refs", "must contain 1..4 phases")
            phase_count = None
        else:
            phase_count = len(phase_refs)
            for index, phase_ref in enumerate(phase_refs):
                _stable_id(phase_ref, f"{item_path}.phase_visual_refs[{index}]", issues)
                if element_kinds.get(source_ref) == "sprite" and phase_ref not in frame_ids:
                    _issue(
                        issues,
                        "ASSET_FRAME_UNKNOWN",
                        f"{item_path}.phase_visual_refs[{index}]",
                        f"unknown frame '{phase_ref}'",
                    )
        indices = element.get("step_phase_indices")
        if not isinstance(indices, list) or step_count is None or len(indices) != step_count:
            _issue(issues, "WAIT_SEQUENCE_LENGTH_INVALID", f"{item_path}.step_phase_indices", "must match combined_step_count")
        elif phase_count is not None:
            for index, phase_index in enumerate(indices):
                if not isinstance(phase_index, int) or not 0 <= phase_index < phase_count:
                    _issue(issues, "WAIT_PHASE_INDEX_INVALID", f"{item_path}.step_phase_indices[{index}]", "does not select an authored phase")


def _check_scene(
    scene: dict[str, Any],
    source: str,
    frame_ids: set[str],
    animation_ids: set[str],
    audio_cue_ids: set[str],
    issues: list[ValidationIssue],
) -> None:
    base = f"scene[{source}]"
    _check_keys(scene, SCENE_KEYS, base, issues, SCENE_KEYS | SCENE_OPTIONAL_KEYS)
    if scene.get("schema_id") != "peepshow.authoring.state_scene" or scene.get("schema_version") != 1:
        _issue(issues, "SCENE_SCHEMA_UNSUPPORTED", base, "expected peepshow.authoring.state_scene version 1")
    _stable_id(scene.get("scene_id"), f"{base}.scene_id", issues)
    _text(scene.get("display_name"), f"{base}.display_name", issues)
    if scene.get("scene_type") != "STATE_SCENE":
        _issue(issues, "SCENE_TYPE_INVALID", f"{base}.scene_type", "the first authoring subset supports STATE_SCENE only")

    variables = _unique_ids(scene.get("variables"), "variable_id", f"{base}.variables", issues, 32)
    for variable_id, variable in variables.items():
        _check_variable(variable, f"{base}.variables[{variable_id}]", issues)

    input_actions = _unique_ids(scene.get("input_actions"), "action_id", f"{base}.input_actions", issues, 32)
    logical_bindings: set[tuple[str, str]] = set()
    for action_id, action in input_actions.items():
        path = f"{base}.input_actions[{action_id}]"
        _check_keys(
            action,
            {"action_id", "logical_source"},
            path,
            issues,
            {"action_id", "logical_source", "event_kind"},
        )
        source_value = action.get("logical_source")
        event_kind = action.get("event_kind", "press")
        if source_value not in LOGICAL_INPUT_SOURCES:
            _issue(issues, "INPUT_SOURCE_INVALID", f"{path}.logical_source", "unsupported logical source")
        if event_kind not in LOGICAL_INPUT_EVENT_KINDS:
            _issue(issues, "INPUT_EVENT_INVALID", f"{path}.event_kind", "must be press, release, hold, or repeat")
        elif source_value == "BUTTON_START" and event_kind != "press":
            _issue(issues, "INPUT_EVENT_SYSTEM_OWNED", f"{path}.event_kind", "START supports package press only")
        binding = (str(source_value), str(event_kind))
        if binding in logical_bindings:
            _issue(issues, "INPUT_BINDING_DUPLICATE", path, "logical source and event kind are already bound")
        else:
            logical_bindings.add(binding)

    joystick_policy = scene.get("joystick_policy", "four_way")
    if joystick_policy not in JOYSTICK_POLICIES:
        _issue(issues, "JOYSTICK_POLICY_INVALID", f"{base}.joystick_policy", "must be four_way or eight_way")
    diagonal_sources = {
        "JOY_UP_LEFT", "JOY_UP_RIGHT", "JOY_DOWN_LEFT", "JOY_DOWN_RIGHT"
    }
    if joystick_policy != "eight_way" and any(
        action.get("logical_source") in diagonal_sources
        for action in input_actions.values()
    ):
        _issue(issues, "JOYSTICK_POLICY_REQUIRED", f"{base}.joystick_policy", "diagonal bindings require eight_way")

    states = _unique_ids(scene.get("states"), "state_id", f"{base}.states", issues, 64)
    render_models = _unique_ids(scene.get("render_models"), "visual_id", f"{base}.render_models", issues, 64)
    waiting_visuals = _unique_ids(scene.get("waiting_visuals"), "waiting_visual_id", f"{base}.waiting_visuals", issues, 32)
    routes = _unique_ids(scene.get("routes"), "route_id", f"{base}.routes", issues, 128)

    entry_state = scene.get("entry_state")
    if entry_state not in states:
        _issue(issues, "GRAPH_ENTRY_MISSING", f"{base}.entry_state", f"unknown entry state '{entry_state}'")

    element_kinds: dict[str, str] = {}
    for visual_id, render_model in render_models.items():
        path = f"{base}.render_models[{visual_id}]"
        _check_keys(render_model, {"visual_id", "focus_index", "elements"}, path, issues)
        focus_index = render_model.get("focus_index")
        if not isinstance(focus_index, int) or not 0 <= focus_index <= 255:
            _issue(issues, "RENDER_FOCUS_INVALID", f"{path}.focus_index", "must be in 0..255")
        elements = _unique_ids(render_model.get("elements"), "element_id", f"{path}.elements", issues, 32)
        for element_id, element in elements.items():
            item_path = f"{path}.elements[{element_id}]"
            required = {"element_id", "kind", "x", "y", "width", "height", "z_order"}
            allowed = required | {"visual_ref", "focus_role", "layer", "visible"}
            for key in sorted(required - element.keys()):
                _issue(issues, "PROJECT_FIELD_MISSING", f"{item_path}.{key}", "required field is missing")
            for key in sorted(element.keys() - allowed):
                _issue(issues, "PROJECT_FIELD_UNKNOWN", f"{item_path}.{key}", "field is not part of the V1 subset")
            kind = element.get("kind")
            if kind not in {"sprite", "line", "outline_rect", "filled_rect", "circle", "ellipse"}:
                _issue(issues, "RENDER_KIND_INVALID", f"{item_path}.kind", "unsupported retained element type")
            else:
                element_kinds[element_id] = kind
                if kind == "sprite":
                    _stable_id(element.get("visual_ref"), f"{item_path}.visual_ref", issues)
                    if element.get("visual_ref") not in frame_ids:
                        _issue(
                            issues,
                            "ASSET_FRAME_UNKNOWN",
                            f"{item_path}.visual_ref",
                            "STATE sprite visual_ref must select a compiled frame; STOP2 loops use waiting_visual phases",
                        )
                elif "visual_ref" in element:
                    _issue(issues, "RENDER_VISUAL_REF_INVALID", f"{item_path}.visual_ref", "primitives do not reference assets")
            layer = element.get(
                "layer",
                "UI" if element.get("focus_role", "none") == "focus" else "SCENE",
            )
            if layer not in {"BACKGROUND", "SCENE", "UI"}:
                _issue(issues, "RENDER_LAYER_INVALID", f"{item_path}.layer", "must be BACKGROUND, SCENE, or UI")
            focus_role = element.get("focus_role", "none")
            if focus_role not in {"none", "focus"}:
                _issue(issues, "RENDER_FOCUS_INVALID", f"{item_path}.focus_role", "must be none or focus")
            elif focus_role == "focus" and (kind != "sprite" or layer != "UI" or element.get("visible", True) is not True):
                _issue(issues, "RENDER_FOCUS_INVALID", f"{item_path}.focus_role", "focus must be a visible UI sprite")
            if not isinstance(element.get("visible", True), bool):
                _issue(issues, "RENDER_VISIBILITY_INVALID", f"{item_path}.visible", "must be boolean")
            for field in ("x", "y", "z_order"):
                value = element.get(field)
                if not isinstance(value, int) or value < 0:
                    _issue(issues, "RENDER_BOUNDS_INVALID", f"{item_path}.{field}", "must be a non-negative integer")
            for field in ("width", "height"):
                value = element.get(field)
                if not isinstance(value, int) or value < 1:
                    _issue(issues, "RENDER_BOUNDS_INVALID", f"{item_path}.{field}", "must be a positive integer")
            z_order = element.get("z_order")
            if isinstance(z_order, int) and z_order > 255:
                _issue(issues, "RENDER_BOUNDS_INVALID", f"{item_path}.z_order", "must be in 0..255")
            x = element.get("x")
            y = element.get("y")
            width = element.get("width")
            height = element.get("height")
            if all(isinstance(value, int) and not isinstance(value, bool) for value in (x, y, width, height)):
                if x + width > 168 or y + height > 144:
                    _issue(issues, "RENDER_BOUNDS_INVALID", item_path, "element exceeds the 168x144 canvas")
                if kind in {"circle", "ellipse"} and (width < 3 or height < 3 or width % 2 == 0 or height % 2 == 0):
                    _issue(issues, "RENDER_GEOMETRY_INVALID", item_path, "circle and ellipse bounds must be odd and at least 3")
                if kind == "circle" and width != height:
                    _issue(issues, "RENDER_GEOMETRY_INVALID", item_path, "circle bounds must be square")

    for waiting_id, waiting in waiting_visuals.items():
        _check_waiting_visual(
            waiting,
            f"{base}.waiting_visuals[{waiting_id}]",
            element_kinds,
            frame_ids,
            issues,
        )

    for state_id, state in states.items():
        path = f"{base}.states[{state_id}]"
        _check_keys(state, {"state_id", "display_name", "render_model_ref", "waiting_visual_ref"}, path, issues)
        _text(state.get("display_name"), f"{path}.display_name", issues)
        if state.get("render_model_ref") not in render_models:
            _issue(issues, "RENDER_MODEL_UNKNOWN", f"{path}.render_model_ref", "render model does not exist")
        if state.get("waiting_visual_ref") not in waiting_visuals:
            _issue(issues, "WAIT_VISUAL_UNKNOWN", f"{path}.waiting_visual_ref", "waiting visual does not exist")

    for route_id, route in routes.items():
        path = f"{base}.routes[{route_id}]"
        target_elements: dict[str, dict[str, Any]] = {}
        required = {"route_id", "action_ref", "from_states", "guards", "actions"}
        allowed = required | {"target_state", "target_scene"}
        _check_keys(route, required, path, issues, allowed)
        if route.get("action_ref") not in input_actions:
            _issue(issues, "ROUTE_ACTION_UNKNOWN", f"{path}.action_ref", "input action does not exist")
        from_states = route.get("from_states")
        if not isinstance(from_states, list) or not from_states:
            _issue(issues, "ROUTE_SOURCE_MISSING", f"{path}.from_states", "must contain at least one state")
        else:
            for index, state_ref in enumerate(from_states):
                if state_ref not in states:
                    _issue(issues, "GRAPH_STATE_UNKNOWN", f"{path}.from_states[{index}]", f"unknown state '{state_ref}'")
        has_target_state = "target_state" in route
        has_target_scene = "target_scene" in route
        if has_target_state == has_target_scene:
            _issue(
                issues,
                "GRAPH_TRANSITION_TARGET_INVALID",
                path,
                "must declare exactly one of target_state or target_scene",
            )
        elif has_target_state and route.get("target_state") not in states:
            _issue(issues, "GRAPH_TRANSITION_TARGET_UNKNOWN", f"{path}.target_state", "target state does not exist")
        elif has_target_state:
            target_state_record = states[route.get("target_state")]
            target_model = render_models.get(target_state_record.get("render_model_ref"))
            if isinstance(target_model, dict):
                target_elements = {
                    str(item.get("element_id")): item
                    for item in target_model.get("elements", [])
                    if isinstance(item, dict)
                    and isinstance(item.get("element_id"), str)
                }
        elif has_target_scene:
            _stable_id(route.get("target_scene"), f"{path}.target_scene", issues)
            if route.get("actions"):
                _issue(
                    issues,
                    "SCENE_TRANSITION_ACTION_UNSUPPORTED",
                    f"{path}.actions",
                    "the initial direct scene-replacement slice does not carry scene-local actions across scenes",
                )
        guards = route.get("guards")
        if not isinstance(guards, list) or len(guards) > 8:
            _issue(issues, "GUARD_LIMIT_EXCEEDED", f"{path}.guards", "must contain at most 8 guards")
        else:
            for index, guard in enumerate(guards):
                guard_path = f"{path}.guards[{index}]"
                if not isinstance(guard, dict):
                    _issue(issues, "GUARD_TYPE_MISMATCH", guard_path, "must be an object")
                    continue
                _check_keys(guard, {"variable_ref", "operator", "value"}, guard_path, issues)
                if guard.get("variable_ref") not in variables:
                    _issue(issues, "GUARD_VARIABLE_UNKNOWN", f"{guard_path}.variable_ref", "variable does not exist")
                if guard.get("operator") not in {"eq", "ne", "lt", "le", "gt", "ge"}:
                    _issue(issues, "GUARD_OPERATOR_INVALID", f"{guard_path}.operator", "unsupported comparison")
                if isinstance(guard.get("value"), bool) or not isinstance(guard.get("value"), int):
                    _issue(issues, "GUARD_TYPE_MISMATCH", f"{guard_path}.value", "must be an integer")
        actions = route.get("actions")
        if not isinstance(actions, list) or len(actions) > 8:
            _issue(issues, "ACTION_BUDGET_EXCEEDED", f"{path}.actions", "must contain at most 8 actions")
        else:
            for index, action in enumerate(actions):
                action_path = f"{path}.actions[{index}]"
                if not isinstance(action, dict):
                    _issue(issues, "ACTION_TYPE_INVALID", action_path, "must be an object")
                    continue
                kind = action.get("kind")
                if kind == "set_variable":
                    _check_keys(action, {"kind", "variable_ref", "operation", "value"}, action_path, issues)
                    if action.get("variable_ref") not in variables:
                        _issue(issues, "ACTION_VARIABLE_UNKNOWN", f"{action_path}.variable_ref", "variable does not exist")
                    if action.get("operation") not in {"assign", "add"}:
                        _issue(issues, "ACTION_OPERATION_INVALID", f"{action_path}.operation", "must be assign or add")
                    if isinstance(action.get("value"), bool) or not isinstance(action.get("value"), int):
                        _issue(issues, "ACTION_TYPE_INVALID", f"{action_path}.value", "must be an integer")
                elif kind == "request_render":
                    _check_keys(action, {"kind"}, action_path, issues)
                elif kind == "play_sfx":
                    _check_keys(action, {"kind", "cue_ref"}, action_path, issues)
                    cue_ref = action.get("cue_ref")
                    _stable_id(cue_ref, f"{action_path}.cue_ref", issues)
                    if cue_ref not in audio_cue_ids:
                        _issue(
                            issues,
                            "AUDIO_CUE_UNKNOWN",
                            f"{action_path}.cue_ref",
                            f"unknown audio cue '{cue_ref}'",
                        )
                elif kind in {
                    "set_element_visibility",
                    "set_element_position",
                    "set_element_frame",
                    "set_element_waiting_animation",
                }:
                    element_ref = action.get("element_ref")
                    element = target_elements.get(element_ref)
                    if kind == "set_element_visibility":
                        _check_keys(
                            action,
                            {"kind", "element_ref", "visible"},
                            action_path,
                            issues,
                        )
                    elif kind == "set_element_position":
                        _check_keys(
                            action,
                            {"kind", "element_ref", "x", "y"},
                            action_path,
                            issues,
                        )
                    elif kind == "set_element_frame":
                        _check_keys(
                            action,
                            {"kind", "element_ref", "frame_ref"},
                            action_path,
                            issues,
                        )
                    else:
                        _check_keys(
                            action,
                            {
                                "kind",
                                "element_ref",
                                "waiting_visual_ref",
                                "waiting_element_ref",
                                "timeline_policy",
                            },
                            action_path,
                            issues,
                        )
                    _stable_id(element_ref, f"{action_path}.element_ref", issues)
                    if element is None:
                        _issue(
                            issues,
                            "ACTION_ELEMENT_UNKNOWN",
                            f"{action_path}.element_ref",
                            "target state render model has no matching element",
                        )
                        continue
                    if kind == "set_element_visibility":
                        visible = action.get("visible")
                        if not isinstance(visible, bool):
                            _issue(
                                issues,
                                "ACTION_TYPE_INVALID",
                                f"{action_path}.visible",
                                "must be true or false",
                            )
                        elif not visible and element.get("focus_role", "none") == "focus":
                            _issue(
                                issues,
                                "RENDER_FOCUS_INVALID",
                                f"{action_path}.visible",
                                "the focus element must remain visible",
                            )
                    elif kind == "set_element_position":
                        x = action.get("x")
                        y = action.get("y")
                        if (
                            isinstance(x, bool)
                            or not isinstance(x, int)
                            or isinstance(y, bool)
                            or not isinstance(y, int)
                        ):
                            _issue(
                                issues,
                                "ACTION_TYPE_INVALID",
                                action_path,
                                "position must use integer x and y",
                            )
                        elif (
                            x < 0
                            or y < 0
                            or x + int(element.get("width", 0)) > 168
                            or y + int(element.get("height", 0)) > 144
                        ):
                            _issue(
                                issues,
                                "RENDER_BOUNDS_INVALID",
                                action_path,
                                "element action exceeds the 168x144 canvas",
                            )
                    elif kind == "set_element_frame":
                        frame_ref = action.get("frame_ref")
                        _stable_id(frame_ref, f"{action_path}.frame_ref", issues)
                        if element.get("kind") != "sprite":
                            _issue(
                                issues,
                                "ACTION_ELEMENT_TYPE_INVALID",
                                f"{action_path}.element_ref",
                                "frame selection requires a sprite element",
                            )
                        if frame_ref not in frame_ids:
                            _issue(
                                issues,
                                "ASSET_FRAME_UNKNOWN",
                                f"{action_path}.frame_ref",
                                f"unknown frame '{frame_ref}'",
                            )
                    else:
                        waiting_visual_ref = action.get("waiting_visual_ref")
                        waiting_element_ref = action.get("waiting_element_ref")
                        waiting_visual = waiting_visuals.get(waiting_visual_ref)
                        waiting_element = None
                        _stable_id(
                            waiting_visual_ref,
                            f"{action_path}.waiting_visual_ref",
                            issues,
                        )
                        _stable_id(
                            waiting_element_ref,
                            f"{action_path}.waiting_element_ref",
                            issues,
                        )
                        if isinstance(waiting_visual, dict):
                            waiting_element = next(
                                (
                                    item
                                    for item in waiting_visual.get("elements", [])
                                    if isinstance(item, dict)
                                    and item.get("element_id")
                                    == waiting_element_ref
                                ),
                                None,
                            )
                        else:
                            _issue(
                                issues,
                                "WAIT_VISUAL_UNKNOWN",
                                f"{action_path}.waiting_visual_ref",
                                "waiting visual does not exist",
                            )
                        if waiting_element is None:
                            _issue(
                                issues,
                                "WAIT_ELEMENT_UNKNOWN",
                                f"{action_path}.waiting_element_ref",
                                "waiting visual has no matching element",
                            )
                        elif waiting_element.get("source_element_ref") != element_ref:
                            _issue(
                                issues,
                                "ACTION_WAIT_ELEMENT_MISMATCH",
                                f"{action_path}.waiting_element_ref",
                                "waiting animation must target the same retained element",
                            )
                        if element.get("kind") != "sprite":
                            _issue(
                                issues,
                                "ACTION_ELEMENT_TYPE_INVALID",
                                f"{action_path}.element_ref",
                                "waiting animation selection requires a sprite element",
                            )
                        if action.get("timeline_policy") not in {
                            "preserve",
                            "rebase",
                        }:
                            _issue(
                                issues,
                                "ACTION_TIMELINE_POLICY_INVALID",
                                f"{action_path}.timeline_policy",
                                "must be preserve or rebase",
                            )
                        target_state_record = states.get(route.get("target_state"))
                        target_waiting = (
                            waiting_visuals.get(
                                target_state_record.get("waiting_visual_ref")
                            )
                            if isinstance(target_state_record, dict)
                            else None
                        )
                        if (
                            isinstance(waiting_visual, dict)
                            and isinstance(target_waiting, dict)
                            and (
                                waiting_visual.get("phase_quantum_ms")
                                != target_waiting.get("phase_quantum_ms")
                                or waiting_visual.get("combined_step_count")
                                != target_waiting.get("combined_step_count")
                            )
                        ):
                            _issue(
                                issues,
                                "ACTION_WAIT_TIMELINE_INCOMPATIBLE",
                                action_path,
                                "waiting animation cadence and step count must match the target state",
                            )
                else:
                    _issue(issues, "ACTION_KIND_INVALID", f"{action_path}.kind", "unsupported V1 action")

    wait_policy = scene.get("reactive_wait_default")
    if not isinstance(wait_policy, dict):
        _issue(issues, "WAIT_POLICY_INVALID", f"{base}.reactive_wait_default", "must be an object")
    else:
        path = f"{base}.reactive_wait_default"
        _check_keys(wait_policy, {"policy_id", "waiting_visual_ref", "hold_fallback_allowed", "event_interests"}, path, issues)
        _stable_id(wait_policy.get("policy_id"), f"{path}.policy_id", issues)
        if wait_policy.get("waiting_visual_ref") not in waiting_visuals:
            _issue(issues, "WAIT_VISUAL_UNKNOWN", f"{path}.waiting_visual_ref", "waiting visual does not exist")
        if not isinstance(wait_policy.get("hold_fallback_allowed"), bool):
            _issue(issues, "WAIT_FALLBACK_INVALID", f"{path}.hold_fallback_allowed", "must be true or false")
        interests = wait_policy.get("event_interests")
        if not isinstance(interests, list) or not interests:
            _issue(issues, "WAIT_EVENT_INTEREST_MISSING", f"{path}.event_interests", "must contain at least one input action")
        else:
            for index, action_ref in enumerate(interests):
                if action_ref not in input_actions:
                    _issue(issues, "WAIT_EVENT_UNKNOWN", f"{path}.event_interests[{index}]", "input action does not exist")

    interaction = scene.get("interaction_policy")
    if not isinstance(interaction, dict):
        _issue(issues, "POWER_IDLE_ROUTE_MISSING", f"{base}.interaction_policy", "must be an object")
    else:
        path = f"{base}.interaction_policy"
        required = {"policy_id", "mode", "meaningful_activity_actions"}
        allowed = required | {"inactive_route", "bounded_deferrals"}
        _check_keys(interaction, required, path, issues, allowed)
        _stable_id(interaction.get("policy_id"), f"{path}.policy_id", issues)
        mode = interaction.get("mode")
        if mode not in {"continuous", "timeout"}:
            _issue(issues, "POWER_INTERACTION_MODE_INVALID", f"{path}.mode", "must be continuous or timeout")
        if mode == "timeout":
            if interaction.get("inactive_route") not in {"preserve_scene", "exit_to_shell"}:
                _issue(issues, "POWER_IDLE_ROUTE_MISSING", f"{path}.inactive_route", "must preserve_scene or exit_to_shell")
            if interaction.get("bounded_deferrals", []) != []:
                _issue(issues, "POWER_DEFERRAL_UNSUPPORTED", f"{path}.bounded_deferrals", "the V1 subset does not support deferrals")
        elif mode == "continuous":
            if "inactive_route" in interaction:
                _issue(issues, "POWER_CONTINUOUS_ROUTE_INVALID", f"{path}.inactive_route", "continuous mode must not declare an inactive route")
            if "bounded_deferrals" in interaction:
                _issue(issues, "POWER_CONTINUOUS_DEFERRAL_INVALID", f"{path}.bounded_deferrals", "continuous mode must not declare inactivity deferrals")
        meaningful = interaction.get("meaningful_activity_actions")
        if not isinstance(meaningful, list):
            _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.meaningful_activity_actions", "must be an array")
        else:
            for index, action_ref in enumerate(meaningful):
                if action_ref not in input_actions:
                    _issue(issues, "INPUT_ACTION_UNKNOWN", f"{path}.meaningful_activity_actions[{index}]", "input action does not exist")


def _scene_path(root: Path, source: Any, issues: list[ValidationIssue]) -> Path | None:
    if not isinstance(source, str) or not source:
        _issue(issues, "SCENE_SOURCE_INVALID", "project.scene_sources", "scene source must be a relative path")
        return None
    relative = Path(source)
    if relative.is_absolute() or ".." in relative.parts:
        _issue(issues, "SCENE_SOURCE_INVALID", source, "scene source must stay inside the project")
        return None
    path = (root / relative).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError:
        _issue(issues, "SCENE_SOURCE_INVALID", source, "scene source resolves outside the project")
        return None
    return path


def _compile_asset_catalogs(
    root: Path,
    catalogs: list[dict[str, Any]],
) -> tuple[
    list[dict[str, Any]],
    list[dict[str, Any]],
    list[Masked1bppFrame],
    list[CompiledAudioAsset],
    list[dict[str, Any]],
    list[ValidationIssue],
]:
    issues: list[ValidationIssue] = []
    assets: list[dict[str, Any]] = []
    animations: list[dict[str, Any]] = []
    frames: list[Masked1bppFrame] = []
    audio_assets: list[CompiledAudioAsset] = []
    audio_cues: list[dict[str, Any]] = []
    asset_ids: set[str] = set()
    frame_ids: set[str] = set()
    animation_ids: set[str] = set()
    audio_asset_ids: set[str] = set()
    audio_cue_ids: set[str] = set()
    for catalog_index, catalog in enumerate(catalogs):
        base = f"asset_catalogs[{catalog_index}]"
        _check_keys(
            catalog,
            {"schema_id", "schema_version", "assets", "animations"},
            base,
            issues,
            {
                "schema_id",
                "schema_version",
                "assets",
                "animations",
                "audio_assets",
                "audio_cues",
            },
        )
        if catalog.get("schema_id") != "peepshow.authoring.assets" or catalog.get("schema_version") != 1:
            _issue(issues, "ASSET_SCHEMA_UNSUPPORTED", base, "expected peepshow.authoring.assets version 1")
        catalog_assets = _unique_ids(catalog.get("assets"), "asset_id", f"{base}.assets", issues, 128)
        for asset_id, asset in catalog_assets.items():
            asset_path = f"{base}.assets[{asset_id}]"
            if asset_id in asset_ids:
                _issue(issues, "PROJECT_ID_DUPLICATE", f"{asset_path}.asset_id", f"duplicate ID '{asset_id}'")
                continue
            asset_ids.add(asset_id)
            for frame in asset.get("frames", []) if isinstance(asset.get("frames"), list) else []:
                if not isinstance(frame, dict) or not isinstance(frame.get("frame_id"), str):
                    continue
                frame_id = frame["frame_id"]
                if frame_id in frame_ids:
                    _issue(issues, "PROJECT_ID_DUPLICATE", f"{asset_path}.frames", f"duplicate ID '{frame_id}'")
                else:
                    frame_ids.add(frame_id)
            frames.extend(_check_asset(asset, asset_path, root, issues))
            assets.append(asset)
        catalog_animations = _unique_ids(
            catalog.get("animations"),
            "animation_id",
            f"{base}.animations",
            issues,
            128,
        )
        for animation_id, animation in catalog_animations.items():
            animation_path = f"{base}.animations[{animation_id}]"
            if animation_id in animation_ids:
                _issue(
                    issues,
                    "PROJECT_ID_DUPLICATE",
                    f"{animation_path}.animation_id",
                    f"duplicate ID '{animation_id}'",
                )
                continue
            animation_ids.add(animation_id)
            _check_animation(animation, animation_path, issues)
            animations.append(animation)
        catalog_audio_assets = _unique_ids(
            catalog.get("audio_assets", []),
            "asset_id",
            f"{base}.audio_assets",
            issues,
            AUDIO_MAX_ASSETS,
        )
        for audio_asset_id, audio_asset in catalog_audio_assets.items():
            audio_path = f"{base}.audio_assets[{audio_asset_id}]"
            if audio_asset_id in audio_asset_ids:
                _issue(
                    issues,
                    "PROJECT_ID_DUPLICATE",
                    f"{audio_path}.asset_id",
                    f"duplicate ID '{audio_asset_id}'",
                )
                continue
            audio_asset_ids.add(audio_asset_id)
            before = len(issues)
            _check_keys(
                audio_asset,
                {"asset_id", "asset_type", "source_path", "source_format"},
                audio_path,
                issues,
            )
            _stable_id(audio_asset_id, f"{audio_path}.asset_id", issues)
            if audio_asset.get("asset_type") != "sampled_sfx":
                _issue(
                    issues,
                    "AUDIO_ASSET_TYPE_INVALID",
                    f"{audio_path}.asset_type",
                    "must be sampled_sfx",
                )
            if audio_asset.get("source_format") != "wav":
                _issue(
                    issues,
                    "AUDIO_SOURCE_FORMAT_INVALID",
                    f"{audio_path}.source_format",
                    "must be wav",
                )
            if len(issues) == before:
                try:
                    audio_assets.append(import_sampled_sfx(root, audio_asset))
                except (AudioAssetError, KeyError) as exc:
                    _issue(issues, "AUDIO_SOURCE_INVALID", audio_path, str(exc))
        catalog_audio_cues = _unique_ids(
            catalog.get("audio_cues", []),
            "cue_id",
            f"{base}.audio_cues",
            issues,
            AUDIO_MAX_CUES,
        )
        for cue_id, cue in catalog_audio_cues.items():
            cue_path = f"{base}.audio_cues[{cue_id}]"
            if cue_id in audio_cue_ids:
                _issue(
                    issues,
                    "PROJECT_ID_DUPLICATE",
                    f"{cue_path}.cue_id",
                    f"duplicate ID '{cue_id}'",
                )
                continue
            audio_cue_ids.add(cue_id)
            _check_keys(
                cue,
                {"cue_id", "asset_ref", "priority", "volume"},
                cue_path,
                issues,
            )
            _stable_id(cue_id, f"{cue_path}.cue_id", issues)
            _stable_id(cue.get("asset_ref"), f"{cue_path}.asset_ref", issues)
            for field in ("priority", "volume"):
                value = cue.get(field)
                if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 255:
                    _issue(
                        issues,
                        "AUDIO_CUE_VALUE_INVALID",
                        f"{cue_path}.{field}",
                        "must be an integer in 0..255",
                    )
            audio_cues.append(cue)
    for animation in animations:
        for index, frame_ref in enumerate(animation.get("frame_refs", [])):
            if frame_ref not in frame_ids:
                _issue(
                    issues,
                    "ASSET_FRAME_UNKNOWN",
                    f"animation[{animation.get('animation_id')}].frame_refs[{index}]",
                    f"unknown frame '{frame_ref}'",
                )
    for cue in audio_cues:
        if cue.get("asset_ref") not in audio_asset_ids:
            _issue(
                issues,
                "AUDIO_ASSET_UNKNOWN",
                f"audio_cues[{cue.get('cue_id')}].asset_ref",
                f"unknown audio asset '{cue.get('asset_ref')}'",
            )
    if sum(len(asset.adpcm) for asset in audio_assets) > AUDIO_MAX_BANK_BYTES:
        _issue(
            issues,
            "AUDIO_BANK_BUDGET_EXCEEDED",
            "audio_assets",
            f"compiled audio exceeds the {AUDIO_MAX_BANK_BYTES}-byte STATE bank limit",
        )
    return assets, animations, frames, audio_assets, audio_cues, issues


def load_project(project_root: str | Path) -> ProjectBundle:
    root = Path(project_root)
    issues: list[ValidationIssue] = []
    if root.suffix != ".peepproj":
        _issue(issues, "PROJECT_SUFFIX_INVALID", str(root), "editable project directories must end in .peepproj")
    if not root.is_dir():
        _issue(issues, "PROJECT_ROOT_MISSING", str(root), "project directory does not exist")
        return ProjectBundle(root, {}, (), (), (), (), (), (), (), (), (), tuple(issues))

    project = _read_json(root / "project.json", issues, "PROJECT_MANIFEST_INVALID")
    if project is None:
        return ProjectBundle(root, {}, (), (), (), (), (), (), (), (), (), tuple(issues))
    _check_project(project, issues)

    asset_catalogs: list[dict[str, Any]] = []
    loaded_asset_sources: list[str] = []
    asset_sources = project.get("asset_sources", [])
    if isinstance(asset_sources, list):
        for source in asset_sources:
            try:
                path = resolve_project_path(root, source, "asset catalog path")
            except ImageAssetError as exc:
                _issue(issues, "ASSET_SOURCE_INVALID", f"project.asset_sources[{source}]", str(exc))
                continue
            catalog = _read_json(path, issues, "ASSET_SOURCE_INVALID")
            if catalog is None:
                continue
            asset_catalogs.append(catalog)
            loaded_asset_sources.append(str(source))
    (
        assets,
        animations,
        frames,
        audio_assets,
        audio_cues,
        catalog_issues,
    ) = _compile_asset_catalogs(root, asset_catalogs)
    issues.extend(catalog_issues)
    frame_ids = {frame.frame_id for frame in frames}
    animation_ids = {
        str(animation["animation_id"])
        for animation in animations
        if isinstance(animation.get("animation_id"), str)
    }
    audio_cue_ids = {
        str(cue["cue_id"])
        for cue in audio_cues
        if isinstance(cue.get("cue_id"), str)
    }

    scenes: list[dict[str, Any]] = []
    loaded_scene_sources: list[str] = []
    scene_ids: set[str] = set()
    sources = project.get("scene_sources")
    if isinstance(sources, list):
        for source in sources:
            path = _scene_path(root, source, issues)
            if path is None:
                continue
            scene = _read_json(path, issues, "SCENE_SOURCE_INVALID")
            if scene is None:
                continue
            _check_scene(
                scene,
                str(source),
                frame_ids,
                animation_ids,
                audio_cue_ids,
                issues,
            )
            scene_id = scene.get("scene_id")
            if isinstance(scene_id, str):
                if scene_id in scene_ids:
                    _issue(issues, "SCENE_ID_DUPLICATE", str(source), f"duplicate scene ID '{scene_id}'")
                scene_ids.add(scene_id)
            scenes.append(scene)
            loaded_scene_sources.append(str(source))

    entry_scene = project.get("entry_scene")
    if isinstance(entry_scene, str) and entry_scene not in scene_ids:
        _issue(issues, "SCENE_ENTRY_MISSING", "project.entry_scene", f"unknown entry scene '{entry_scene}'")
    editor_nodes = project.get("editor", {}).get("scene_flow", {}).get("nodes", {}) if isinstance(project.get("editor"), dict) else {}
    if isinstance(editor_nodes, dict):
        for scene_id in editor_nodes:
            if isinstance(scene_id, str) and scene_id not in scene_ids:
                _issue(
                    issues,
                    "SCENE_ID_UNKNOWN",
                    f"project.editor.scene_flow.nodes[{scene_id}]",
                    f"unknown scene '{scene_id}'",
                )
    state_graph_scenes = project.get("editor", {}).get("state_graph", {}).get("scenes", {}) if isinstance(project.get("editor"), dict) else {}
    if isinstance(state_graph_scenes, dict):
        scenes_by_id = {
            scene.get("scene_id"): scene
            for scene in scenes
            if isinstance(scene, dict) and isinstance(scene.get("scene_id"), str)
        }
        for scene_id, scene_layout in state_graph_scenes.items():
            if not isinstance(scene_id, str):
                continue
            target_scene = scenes_by_id.get(scene_id)
            if target_scene is None:
                _issue(
                    issues,
                    "SCENE_ID_UNKNOWN",
                    f"project.editor.state_graph.scenes[{scene_id}]",
                    f"unknown scene '{scene_id}'",
                )
                continue
            if not isinstance(scene_layout, dict):
                continue
            nodes = scene_layout.get("nodes")
            if not isinstance(nodes, dict):
                continue
            state_ids = {
                state.get("state_id")
                for state in target_scene.get("states", [])
                if isinstance(state, dict)
            }
            for state_id in nodes:
                if isinstance(state_id, str) and state_id not in state_ids:
                    _issue(
                        issues,
                        "GRAPH_STATE_UNKNOWN",
                        f"project.editor.state_graph.scenes[{scene_id}].nodes[{state_id}]",
                        f"unknown state '{state_id}'",
                    )
    for scene, source in zip(scenes, loaded_scene_sources):
        for route in scene.get("routes", []):
            if not isinstance(route, dict) or "target_scene" not in route:
                continue
            target_scene = route.get("target_scene")
            if isinstance(target_scene, str) and target_scene not in scene_ids:
                _issue(
                    issues,
                    "SCENE_TRANSITION_TARGET_UNKNOWN",
                    f"scene[{source}].routes[{route.get('route_id')}].target_scene",
                    f"unknown scene '{target_scene}'",
                )
    interaction_modes = {
        scene.get("interaction_policy", {}).get("mode")
        for scene in scenes
        if isinstance(scene.get("interaction_policy"), dict)
    }
    if len(interaction_modes) > 1:
        _issue(
            issues,
            "POWER_INTERACTION_MODE_CONFLICT",
            "project.scene_sources",
            "all scenes in one package must use the same interaction mode",
        )
    return ProjectBundle(
        root,
        project,
        tuple(scenes),
        tuple(loaded_scene_sources),
        tuple(asset_catalogs),
        tuple(loaded_asset_sources),
        tuple(assets),
        tuple(animations),
        tuple(frames),
        tuple(audio_assets),
        tuple(audio_cues),
        tuple(issues),
    )


def apply_project_commands(
    bundle: ProjectBundle,
    commands: Any,
) -> tuple[ProjectBundle, tuple[dict[str, Any], ...]]:
    if not bundle.valid:
        raise ProjectCommandError("PROJECT_INVALID", "project must validate before commands can be applied")
    if not isinstance(commands, list) or not commands:
        raise ProjectCommandError("COMMAND_LIST_INVALID", "commands must be a non-empty array")
    if len(commands) > 64:
        raise ProjectCommandError("COMMAND_LIST_INVALID", "commands must contain at most 64 entries")

    project = deepcopy(bundle.project)
    scenes = deepcopy(list(bundle.scenes))
    asset_catalogs = deepcopy(list(bundle.asset_catalogs))
    asset_catalog_sources = list(bundle.asset_catalog_sources)
    assets = deepcopy(list(bundle.assets))
    animations = deepcopy(list(bundle.animations))
    frames = list(bundle.frames)
    audio_assets = list(bundle.audio_assets)
    audio_cues = deepcopy(list(bundle.audio_cues))
    applied: list[dict[str, Any]] = []

    for command in commands:
        if not isinstance(command, dict):
            raise ProjectCommandError("COMMAND_SHAPE_INVALID", "each command must be an object")
        kind = command.get("kind")
        if kind == "state.add":
            applied.append(_apply_state_add(scenes, command))
        elif kind == "state.delete":
            applied.append(_apply_state_delete(project, scenes, command))
        elif kind in {"state.set_entry", "state.set_render_model"}:
            applied.append(_apply_state_set_reference(scenes, command))
        elif kind == "state.rename":
            applied.append(_apply_state_rename(scenes, command))
        elif kind == "render_model.add":
            applied.append(_apply_render_model_add(scenes, command))
        elif kind == "render_model.delete":
            applied.append(_apply_render_model_delete(scenes, command))
        elif kind == "render_model.set_focus_index":
            applied.append(_apply_render_model_set_focus(scenes, command))
        elif kind in {"variable.add", "variable.update"}:
            applied.append(_apply_variable_upsert(scenes, command))
        elif kind == "variable.delete":
            applied.append(_apply_variable_delete(scenes, command))
        elif kind in {"input_action.add", "input_action.update"}:
            applied.append(_apply_input_action_upsert(scenes, command))
        elif kind == "input_action.delete":
            applied.append(_apply_input_action_delete(scenes, command))
        elif kind == "route.add":
            applied.append(_apply_route_add(scenes, command))
        elif kind == "route.delete":
            applied.append(_apply_route_delete(scenes, command))
        elif kind in {"route.set_sources", "route.set_action_ref"}:
            applied.append(_apply_route_set_binding(scenes, command))
        elif kind == "route.set_target":
            applied.append(_apply_route_set_target(scenes, command))
        elif kind == "route.add_scene_exit":
            applied.append(_apply_route_add_scene_exit(scenes, command))
        elif kind == "route.delete_scene_exit":
            applied.append(_apply_route_delete_scene_exit(scenes, command))
        elif kind == "render_element.set_position":
            applied.append(_apply_render_element_set_position(scenes, command))
        elif kind == "render_element.add":
            applied.append(_apply_render_element_add(scenes, frames, command))
        elif kind == "render_element.delete":
            applied.append(_apply_render_element_delete(scenes, command))
        elif kind == "render_element.set_bounds":
            applied.append(_apply_render_element_set_bounds(scenes, command))
        elif kind in {
            "render_element.set_layer",
            "render_element.set_visibility",
            "render_element.set_z_order",
            "render_element.set_visual_ref",
        }:
            applied.append(_apply_render_element_set_property(scenes, frames, command))
        elif kind == "waiting_visual.upsert":
            applied.append(_apply_waiting_visual_upsert(scenes, command))
        elif kind == "waiting_visual.delete":
            applied.append(_apply_waiting_visual_delete(scenes, command))
        elif kind == "state.set_waiting_visual":
            applied.append(_apply_state_set_waiting_visual(scenes, command))
        elif kind == "asset.upsert":
            applied.append(_apply_asset_upsert(project, asset_catalogs, asset_catalog_sources, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "asset.delete":
            applied.append(_apply_asset_delete(asset_catalogs, scenes, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "animation.upsert":
            applied.append(_apply_animation_upsert(project, asset_catalogs, asset_catalog_sources, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "animation.delete":
            applied.append(_apply_animation_delete(asset_catalogs, scenes, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "audio_asset.upsert":
            applied.append(
                _apply_audio_asset_upsert(
                    project, asset_catalogs, asset_catalog_sources, command
                )
            )
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "audio_asset.delete":
            applied.append(_apply_audio_asset_delete(asset_catalogs, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "audio_cue.upsert":
            applied.append(
                _apply_audio_cue_upsert(
                    project, asset_catalogs, asset_catalog_sources, command
                )
            )
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "audio_cue.delete":
            applied.append(_apply_audio_cue_delete(asset_catalogs, scenes, command))
            assets, animations, frames, audio_assets, audio_cues, catalog_issues = _compile_asset_catalogs(bundle.root, asset_catalogs)
            if catalog_issues:
                issue = catalog_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
        elif kind == "editor.scene_flow.set_node_position":
            applied.append(_apply_scene_flow_node_position(project, scenes, command))
        elif kind == "editor.state_graph.set_node_position":
            applied.append(_apply_state_graph_node_position(project, scenes, command))
        elif kind == "route.set_guard":
            applied.append(_apply_route_set_guard(scenes, command))
        elif kind in {"route.guard.add", "route.guard.delete", "route.guard.move"}:
            applied.append(_apply_route_guard_list(scenes, command))
        elif kind == "route.set_action":
            applied.append(_apply_route_set_action(scenes, command))
        elif kind in {"route.action.add", "route.action.delete", "route.action.move"}:
            applied.append(_apply_route_action_list(scenes, command))
        elif kind in {"scene.set_reactive_wait_default", "scene.set_interaction_policy", "scene.set_joystick_policy"}:
            applied.append(_apply_scene_policy(scenes, command))
        else:
            raise ProjectCommandError("COMMAND_KIND_UNKNOWN", f"unknown command kind '{kind}'")

    frame_ids = {frame.frame_id for frame in frames}
    animation_ids = {
        animation.get("animation_id")
        for animation in animations
        if isinstance(animation.get("animation_id"), str)
    }
    audio_cue_ids = {
        cue.get("cue_id")
        for cue in audio_cues
        if isinstance(cue.get("cue_id"), str)
    }
    validation_issues: list[ValidationIssue] = []
    for index, scene in enumerate(scenes):
        _check_scene(
            scene,
            f"scenes[{index}]",
            frame_ids,
            animation_ids,
            audio_cue_ids,
            validation_issues,
        )
    if validation_issues:
        issue = validation_issues[0]
        raise ProjectCommandError(issue.code, f"{issue.path}: {issue.message}")

    return (
        ProjectBundle(
            bundle.root,
            project,
            tuple(scenes),
            bundle.scene_sources,
            tuple(asset_catalogs),
            tuple(asset_catalog_sources),
            tuple(assets),
            tuple(animations),
            tuple(frames),
            tuple(audio_assets),
            tuple(audio_cues),
            (),
        ),
        tuple(applied),
    )


def save_project(bundle: ProjectBundle) -> tuple[str, ...]:
    if not bundle.valid:
        raise ProjectCommandError("PROJECT_INVALID", "project must validate before it can be saved")
    if len(bundle.scene_sources) != len(bundle.scenes):
        raise ProjectCommandError("PROJECT_SAVE_FAILED", "loaded scene sources do not match scene records")

    written: list[str] = []
    project_path = bundle.root / "project.json"
    encoded_project = (
        json.dumps(
            bundle.project,
            ensure_ascii=True,
            indent=2,
            sort_keys=False,
        )
        + "\n"
    )
    project_temp_path = project_path.with_name(".project.json.tmp")
    try:
        project_temp_path.write_text(encoded_project, encoding="utf-8")
        project_temp_path.replace(project_path)
    except OSError as exc:
        try:
            project_temp_path.unlink(missing_ok=True)
        except OSError:
            pass
        raise ProjectCommandError("PROJECT_SAVE_FAILED", f"could not save project manifest: {exc}") from exc
    written.append("project.json")

    for source, scene in zip(bundle.scene_sources, bundle.scenes, strict=True):
        issues: list[ValidationIssue] = []
        path = _scene_path(bundle.root, source, issues)
        if path is None or issues:
            issue = issues[0] if issues else ValidationIssue("SCENE_SOURCE_INVALID", source, "scene source is invalid")
            raise ProjectCommandError(issue.code, issue.message)
        if not path.exists():
            raise ProjectCommandError("PROJECT_SAVE_FAILED", f"scene source '{source}' no longer exists")
        encoded = (
            json.dumps(
                scene,
                ensure_ascii=True,
                indent=2,
                sort_keys=False,
            )
            + "\n"
        )
        temp_path = path.with_name(f".{path.name}.tmp")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            temp_path.write_text(encoded, encoding="utf-8")
            temp_path.replace(path)
        except OSError as exc:
            try:
                temp_path.unlink(missing_ok=True)
            except OSError:
                pass
            raise ProjectCommandError("PROJECT_SAVE_FAILED", f"could not save scene source '{source}': {exc}") from exc
        written.append(source)

    if len(bundle.asset_catalog_sources) != len(bundle.asset_catalogs):
        raise ProjectCommandError("PROJECT_SAVE_FAILED", "loaded asset sources do not match asset catalogs")
    for source, catalog in zip(bundle.asset_catalog_sources, bundle.asset_catalogs, strict=True):
        try:
            path = resolve_project_path(bundle.root, source, "asset catalog path")
        except ImageAssetError as exc:
            raise ProjectCommandError("ASSET_SOURCE_INVALID", str(exc)) from exc
        encoded = (
            json.dumps(
                catalog,
                ensure_ascii=True,
                indent=2,
                sort_keys=False,
            )
            + "\n"
        )
        temp_path = path.with_name(f".{path.name}.tmp")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            temp_path.write_text(encoded, encoding="utf-8")
            temp_path.replace(path)
        except OSError as exc:
            try:
                temp_path.unlink(missing_ok=True)
            except OSError:
                pass
            raise ProjectCommandError("PROJECT_SAVE_FAILED", f"could not save asset source '{source}': {exc}") from exc
        written.append(source)
    return tuple(written)


def format_issues(issues: Iterable[ValidationIssue]) -> str:
    return "\n".join(f"{issue.code} {issue.path}: {issue.message}" for issue in issues)
