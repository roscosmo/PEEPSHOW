"""Load, validate, and normalize the first PeepShow STATE authoring subset."""

from __future__ import annotations

import hashlib
import json
import re
from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .image_assets import (
    ImageAssetError,
    Masked1bppFrame,
    import_masked_1bpp,
    resolve_project_path,
)


STABLE_ID = re.compile(r"^[a-z][a-z0-9_.-]{0,63}$")
GUARD_OPERATORS = {"eq", "ne", "lt", "le", "gt", "ge"}
ACTION_OPERATIONS = {"assign", "add"}
LOGICAL_INPUT_SOURCES = {
    "BUTTON_A",
    "BUTTON_B",
    "BUTTON_L",
    "BUTTON_R",
    "JOY_LEFT",
    "JOY_RIGHT",
    "JOY_UP",
    "JOY_DOWN",
}
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
    assets: tuple[dict[str, Any], ...]
    animations: tuple[dict[str, Any], ...]
    frames: tuple[Masked1bppFrame, ...]
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
            if isinstance(action, dict) and action.get("logical_source") == logical_source:
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
    variable_ref = command.get("variable_ref")
    operator = command.get("operator")
    value = command.get("value")
    guard_index = command.get("guard_index")
    _stable_id(scene_id, "command.scene_id", issues)
    _stable_id(route_id, "command.route_id", issues)
    _stable_id(variable_ref, "command.variable_ref", issues)
    if operator not in GUARD_OPERATORS:
        raise ProjectCommandError("GUARD_OPERATOR_INVALID", "unsupported comparison")
    if isinstance(value, bool) or not isinstance(value, int):
        raise ProjectCommandError("GUARD_TYPE_MISMATCH", "guard value must be an integer")
    if isinstance(guard_index, bool) or not isinstance(guard_index, int) or guard_index < 0:
        raise ProjectCommandError("COMMAND_INDEX_INVALID", "guard_index must be a non-negative integer")
    if issues:
        issue = issues[0]
        raise ProjectCommandError(issue.code, issue.message)

    for scene in scenes:
        if scene.get("scene_id") != scene_id:
            continue
        variable_ids = {
            variable.get("variable_id")
            for variable in scene.get("variables", [])
            if isinstance(variable, dict)
        }
        if variable_ref not in variable_ids:
            raise ProjectCommandError("GUARD_VARIABLE_UNKNOWN", f"unknown variable '{variable_ref}'")
        for route in scene.get("routes", []):
            if not isinstance(route, dict) or route.get("route_id") != route_id:
                continue
            guards = route.get("guards")
            if not isinstance(guards, list) or guard_index >= len(guards):
                raise ProjectCommandError("COMMAND_INDEX_INVALID", "guard_index does not select an existing guard")
            guards[guard_index] = {
                "variable_ref": variable_ref,
                "operator": operator,
                "value": value,
            }
            return {
                "kind": "route.set_guard",
                "scene_id": scene_id,
                "route_id": route_id,
                "guard_index": guard_index,
                "variable_ref": variable_ref,
                "operator": operator,
                "value": value,
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
        variable_ids = {
            variable.get("variable_id")
            for variable in scene.get("variables", [])
            if isinstance(variable, dict)
        }
        normalized_action: dict[str, Any]
        action_kind = action.get("kind")
        if action_kind == "set_variable":
            _require_command_fields(
                action,
                {"kind", "variable_ref", "operation", "value"},
                {"kind", "variable_ref", "operation", "value"},
            )
            variable_ref = action.get("variable_ref")
            operation = action.get("operation")
            value = action.get("value")
            action_issues: list[ValidationIssue] = []
            _stable_id(variable_ref, "command.action.variable_ref", action_issues)
            if action_issues:
                issue = action_issues[0]
                raise ProjectCommandError(issue.code, issue.message)
            if variable_ref not in variable_ids:
                raise ProjectCommandError("ACTION_VARIABLE_UNKNOWN", f"unknown variable '{variable_ref}'")
            if operation not in ACTION_OPERATIONS:
                raise ProjectCommandError("ACTION_OPERATION_INVALID", "action operation must be assign or add")
            if isinstance(value, bool) or not isinstance(value, int):
                raise ProjectCommandError("ACTION_TYPE_INVALID", "action value must be an integer")
            normalized_action = {
                "kind": "set_variable",
                "variable_ref": variable_ref,
                "operation": operation,
                "value": value,
            }
        elif action_kind == "request_render":
            _require_command_fields(action, {"kind"}, {"kind"})
            normalized_action = {"kind": "request_render"}
        else:
            raise ProjectCommandError("ACTION_KIND_INVALID", "unsupported V1 action")

        for route in scene.get("routes", []):
            if not isinstance(route, dict) or route.get("route_id") != route_id:
                continue
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
            _check_keys(editor, set(), "project.editor", issues, {"scene_flow"})
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


def _check_asset(
    asset: dict[str, Any],
    path: str,
    root: Path,
    issues: list[ValidationIssue],
) -> tuple[Masked1bppFrame, ...]:
    initial_issue_count = len(issues)
    _check_keys(
        asset,
        {"asset_id", "asset_type", "source_path", "source_format", "frames"},
        path,
        issues,
    )
    _stable_id(asset.get("asset_id"), f"{path}.asset_id", issues)
    if asset.get("asset_type") != "masked_1bpp":
        _issue(issues, "ASSET_FORMAT_UNSUPPORTED", f"{path}.asset_type", "must be masked_1bpp")
    if asset.get("source_format") != "png":
        _issue(issues, "ASSET_FORMAT_UNSUPPORTED", f"{path}.source_format", "must be png")
    if not isinstance(asset.get("source_path"), str) or not asset.get("source_path"):
        _issue(issues, "ASSET_SOURCE_INVALID", f"{path}.source_path", "must be a non-empty relative path")

    frame_records = _unique_ids(asset.get("frames"), "frame_id", f"{path}.frames", issues, 256)
    if not frame_records:
        _issue(issues, "ASSET_FRAME_MISSING", f"{path}.frames", "must contain at least one frame")
    for frame_id, frame in frame_records.items():
        frame_path = f"{path}.frames[{frame_id}]"
        _check_keys(frame, {"frame_id", "source_rect", "pivot_x", "pivot_y"}, frame_path, issues)

    if len(issues) == initial_issue_count and frame_records and isinstance(asset.get("source_path"), str):
        try:
            compiled = import_masked_1bpp(root, asset)
        except (ImageAssetError, KeyError) as exc:
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
    issues: list[ValidationIssue],
) -> None:
    base = f"scene[{source}]"
    _check_keys(scene, SCENE_KEYS, base, issues)
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
    logical_sources: set[str] = set()
    for action_id, action in input_actions.items():
        path = f"{base}.input_actions[{action_id}]"
        _check_keys(action, {"action_id", "logical_source"}, path, issues)
        source_value = action.get("logical_source")
        if source_value not in {
            "BUTTON_A",
            "BUTTON_B",
            "BUTTON_L",
            "BUTTON_R",
            "JOY_LEFT",
            "JOY_RIGHT",
            "JOY_UP",
            "JOY_DOWN",
        }:
            _issue(issues, "INPUT_SOURCE_INVALID", f"{path}.logical_source", "unsupported logical source")
        elif source_value in logical_sources:
            _issue(issues, "INPUT_SOURCE_DUPLICATE", f"{path}.logical_source", "logical source is already bound")
        else:
            logical_sources.add(source_value)

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
            required = {"element_id", "kind", "visual_ref", "x", "y", "width", "height", "z_order"}
            allowed = required | {"focus_role"}
            for key in sorted(required - element.keys()):
                _issue(issues, "PROJECT_FIELD_MISSING", f"{item_path}.{key}", "required field is missing")
            for key in sorted(element.keys() - allowed):
                _issue(issues, "PROJECT_FIELD_UNKNOWN", f"{item_path}.{key}", "field is not part of the V1 subset")
            _stable_id(element.get("visual_ref"), f"{item_path}.visual_ref", issues)
            kind = element.get("kind")
            if kind not in {"sprite", "text", "shape"}:
                _issue(issues, "RENDER_KIND_INVALID", f"{item_path}.kind", "must be sprite, text, or shape")
            else:
                element_kinds[element_id] = kind
                if kind == "sprite" and element.get("visual_ref") not in frame_ids | animation_ids:
                    _issue(
                        issues,
                        "ASSET_FRAME_UNKNOWN",
                        f"{item_path}.visual_ref",
                        "sprite visual_ref must select a compiled frame or animation",
                    )
            if element.get("focus_role", "none") not in {"none", "focus"}:
                _issue(issues, "RENDER_FOCUS_INVALID", f"{item_path}.focus_role", "must be none or focus")
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
        _check_keys(interaction, {"policy_id", "meaningful_activity_actions", "inactive_route", "bounded_deferrals"}, path, issues)
        _stable_id(interaction.get("policy_id"), f"{path}.policy_id", issues)
        if interaction.get("inactive_route") not in {"preserve_scene", "exit_to_shell"}:
            _issue(issues, "POWER_IDLE_ROUTE_MISSING", f"{path}.inactive_route", "must preserve_scene or exit_to_shell")
        meaningful = interaction.get("meaningful_activity_actions")
        if not isinstance(meaningful, list):
            _issue(issues, "PROJECT_TYPE_INVALID", f"{path}.meaningful_activity_actions", "must be an array")
        else:
            for index, action_ref in enumerate(meaningful):
                if action_ref not in input_actions:
                    _issue(issues, "INPUT_ACTION_UNKNOWN", f"{path}.meaningful_activity_actions[{index}]", "input action does not exist")
        if interaction.get("bounded_deferrals") != []:
            _issue(issues, "POWER_DEFERRAL_UNSUPPORTED", f"{path}.bounded_deferrals", "the V1 subset does not support deferrals")


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


def load_project(project_root: str | Path) -> ProjectBundle:
    root = Path(project_root)
    issues: list[ValidationIssue] = []
    if root.suffix != ".peepproj":
        _issue(issues, "PROJECT_SUFFIX_INVALID", str(root), "editable project directories must end in .peepproj")
    if not root.is_dir():
        _issue(issues, "PROJECT_ROOT_MISSING", str(root), "project directory does not exist")
        return ProjectBundle(root, {}, (), (), (), (), (), tuple(issues))

    project = _read_json(root / "project.json", issues, "PROJECT_MANIFEST_INVALID")
    if project is None:
        return ProjectBundle(root, {}, (), (), (), (), (), tuple(issues))
    _check_project(project, issues)

    assets: list[dict[str, Any]] = []
    animations: list[dict[str, Any]] = []
    frames: list[Masked1bppFrame] = []
    asset_ids: set[str] = set()
    frame_ids: set[str] = set()
    animation_ids: set[str] = set()
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
            base = f"assets[{source}]"
            _check_keys(catalog, {"schema_id", "schema_version", "assets", "animations"}, base, issues)
            if catalog.get("schema_id") != "peepshow.authoring.assets" or catalog.get("schema_version") != 1:
                _issue(issues, "ASSET_SCHEMA_UNSUPPORTED", base, "expected peepshow.authoring.assets version 1")

            catalog_assets = _unique_ids(catalog.get("assets"), "asset_id", f"{base}.assets", issues, 128)
            for asset_id, asset in catalog_assets.items():
                asset_path = f"{base}.assets[{asset_id}]"
                if asset_id in asset_ids:
                    _issue(issues, "PROJECT_ID_DUPLICATE", f"{asset_path}.asset_id", f"duplicate ID '{asset_id}'")
                    continue
                asset_ids.add(asset_id)
                declared_frames = asset.get("frames")
                if isinstance(declared_frames, list):
                    for frame in declared_frames:
                        if not isinstance(frame, dict) or not isinstance(frame.get("frame_id"), str):
                            continue
                        frame_id = frame["frame_id"]
                        if frame_id in frame_ids:
                            _issue(issues, "PROJECT_ID_DUPLICATE", f"{asset_path}.frames", f"duplicate frame ID '{frame_id}'")
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

    for animation in animations:
        for index, frame_ref in enumerate(animation.get("frame_refs", [])):
            if frame_ref not in frame_ids:
                _issue(
                    issues,
                    "ASSET_FRAME_UNKNOWN",
                    f"animation[{animation.get('animation_id')}].frame_refs[{index}]",
                    f"unknown frame '{frame_ref}'",
                )

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
            _check_scene(scene, str(source), frame_ids, animation_ids, issues)
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
    return ProjectBundle(
        root,
        project,
        tuple(scenes),
        tuple(loaded_scene_sources),
        tuple(assets),
        tuple(animations),
        tuple(frames),
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
    if len(commands) > 16:
        raise ProjectCommandError("COMMAND_LIST_INVALID", "commands must contain at most 16 entries")

    project = deepcopy(bundle.project)
    scenes = deepcopy(list(bundle.scenes))
    assets = deepcopy(list(bundle.assets))
    animations = deepcopy(list(bundle.animations))
    applied: list[dict[str, Any]] = []

    for command in commands:
        if not isinstance(command, dict):
            raise ProjectCommandError("COMMAND_SHAPE_INVALID", "each command must be an object")
        kind = command.get("kind")
        if kind == "state.rename":
            applied.append(_apply_state_rename(scenes, command))
        elif kind == "route.set_target":
            applied.append(_apply_route_set_target(scenes, command))
        elif kind == "route.add_scene_exit":
            applied.append(_apply_route_add_scene_exit(scenes, command))
        elif kind == "editor.scene_flow.set_node_position":
            applied.append(_apply_scene_flow_node_position(project, scenes, command))
        elif kind == "route.set_guard":
            applied.append(_apply_route_set_guard(scenes, command))
        elif kind == "route.set_action":
            applied.append(_apply_route_set_action(scenes, command))
        else:
            raise ProjectCommandError("COMMAND_KIND_UNKNOWN", f"unknown command kind '{kind}'")

    return (
        ProjectBundle(
            bundle.root,
            project,
            tuple(scenes),
            bundle.scene_sources,
            tuple(assets),
            tuple(animations),
            bundle.frames,
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
            temp_path.write_text(encoded, encoding="utf-8")
            temp_path.replace(path)
        except OSError as exc:
            try:
                temp_path.unlink(missing_ok=True)
            except OSError:
                pass
            raise ProjectCommandError("PROJECT_SAVE_FAILED", f"could not save scene source '{source}': {exc}") from exc
        written.append(source)
    return tuple(written)


def format_issues(issues: Iterable[ValidationIssue]) -> str:
    return "\n".join(f"{issue.code} {issue.path}: {issue.message}" for issue in issues)
