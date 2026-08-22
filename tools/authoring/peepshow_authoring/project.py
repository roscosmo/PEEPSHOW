"""Load, validate, and normalize the first PeepShow STATE authoring subset."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


STABLE_ID = re.compile(r"^[a-z][a-z0-9_.-]{0,63}$")
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


def _issue(issues: list[ValidationIssue], code: str, path: str, message: str) -> None:
    issues.append(ValidationIssue(code, path, message))


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
) -> None:
    for key in sorted(required - value.keys()):
        _issue(issues, "PROJECT_FIELD_MISSING", f"{path}.{key}", "required field is missing")
    for key in sorted(value.keys() - required):
        _issue(issues, "PROJECT_FIELD_UNKNOWN", f"{path}.{key}", "field is not part of the V1 subset")


def _stable_id(value: Any, path: str, issues: list[ValidationIssue]) -> bool:
    if not isinstance(value, str) or STABLE_ID.fullmatch(value) is None:
        _issue(issues, "PROJECT_ID_INVALID", path, "must be a lowercase stable ID")
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
    _check_keys(project, PROJECT_KEYS, "project", issues)
    if project.get("schema_id") != "peepshow.authoring.project" or project.get("schema_version") != 1:
        _issue(issues, "PROJECT_SCHEMA_UNSUPPORTED", "project", "expected peepshow.authoring.project version 1")
    _stable_id(project.get("project_id"), "project.project_id", issues)
    _stable_id(project.get("selected_target_profile"), "project.selected_target_profile", issues)
    _stable_id(project.get("entry_scene"), "project.entry_scene", issues)

    package = project.get("package")
    if not isinstance(package, dict):
        _issue(issues, "PROJECT_TYPE_INVALID", "project.package", "must be an object")
    else:
        expected = {"package_id", "display_name", "version"}
        _check_keys(package, expected, "project.package", issues)
        _stable_id(package.get("package_id"), "project.package.package_id", issues)
        version = package.get("version")
        if not isinstance(version, str) or re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
            _issue(issues, "PROJECT_VERSION_INVALID", "project.package.version", "must be major.minor.patch")

    sources = project.get("scene_sources")
    if not isinstance(sources, list) or not sources:
        _issue(issues, "SCENE_SOURCE_MISSING", "project.scene_sources", "must contain at least one scene source")
    elif len(sources) > 32:
        _issue(issues, "PROJECT_LIMIT_EXCEEDED", "project.scene_sources", "contains more than 32 scene sources")

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
    element_ids: set[str],
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
        if source_ref not in element_ids:
            _issue(issues, "WAIT_ELEMENT_UNKNOWN", f"{item_path}.source_element_ref", f"unknown render element '{source_ref}'")
        phase_refs = element.get("phase_visual_refs")
        if not isinstance(phase_refs, list) or not 1 <= len(phase_refs) <= 4:
            _issue(issues, "WAIT_PHASE_COUNT_INVALID", f"{item_path}.phase_visual_refs", "must contain 1..4 phases")
            phase_count = None
        else:
            phase_count = len(phase_refs)
            for index, phase_ref in enumerate(phase_refs):
                _stable_id(phase_ref, f"{item_path}.phase_visual_refs[{index}]", issues)
        indices = element.get("step_phase_indices")
        if not isinstance(indices, list) or step_count is None or len(indices) != step_count:
            _issue(issues, "WAIT_SEQUENCE_LENGTH_INVALID", f"{item_path}.step_phase_indices", "must match combined_step_count")
        elif phase_count is not None:
            for index, phase_index in enumerate(indices):
                if not isinstance(phase_index, int) or not 0 <= phase_index < phase_count:
                    _issue(issues, "WAIT_PHASE_INDEX_INVALID", f"{item_path}.step_phase_indices[{index}]", "does not select an authored phase")


def _check_scene(scene: dict[str, Any], source: str, issues: list[ValidationIssue]) -> None:
    base = f"scene[{source}]"
    _check_keys(scene, SCENE_KEYS, base, issues)
    if scene.get("schema_id") != "peepshow.authoring.state_scene" or scene.get("schema_version") != 1:
        _issue(issues, "SCENE_SCHEMA_UNSUPPORTED", base, "expected peepshow.authoring.state_scene version 1")
    _stable_id(scene.get("scene_id"), f"{base}.scene_id", issues)
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
        if source_value not in {"BUTTON_A", "BUTTON_B", "BUTTON_L", "BUTTON_R"}:
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

    element_ids: set[str] = set()
    for visual_id, render_model in render_models.items():
        path = f"{base}.render_models[{visual_id}]"
        _check_keys(render_model, {"visual_id", "focus_index", "elements"}, path, issues)
        focus_index = render_model.get("focus_index")
        if not isinstance(focus_index, int) or not 0 <= focus_index <= 255:
            _issue(issues, "RENDER_FOCUS_INVALID", f"{path}.focus_index", "must be in 0..255")
        elements = _unique_ids(render_model.get("elements"), "element_id", f"{path}.elements", issues, 32)
        element_ids.update(elements)
        for element_id, element in elements.items():
            item_path = f"{path}.elements[{element_id}]"
            required = {"element_id", "kind", "visual_ref", "x", "y", "width", "height", "z_order"}
            allowed = required | {"focus_role"}
            for key in sorted(required - element.keys()):
                _issue(issues, "PROJECT_FIELD_MISSING", f"{item_path}.{key}", "required field is missing")
            for key in sorted(element.keys() - allowed):
                _issue(issues, "PROJECT_FIELD_UNKNOWN", f"{item_path}.{key}", "field is not part of the V1 subset")
            _stable_id(element.get("visual_ref"), f"{item_path}.visual_ref", issues)
            if element.get("kind") not in {"sprite", "text", "shape"}:
                _issue(issues, "RENDER_KIND_INVALID", f"{item_path}.kind", "must be sprite, text, or shape")
            for field in ("x", "y", "z_order"):
                value = element.get(field)
                if not isinstance(value, int) or value < 0:
                    _issue(issues, "RENDER_BOUNDS_INVALID", f"{item_path}.{field}", "must be a non-negative integer")
            for field in ("width", "height"):
                value = element.get(field)
                if not isinstance(value, int) or value < 1:
                    _issue(issues, "RENDER_BOUNDS_INVALID", f"{item_path}.{field}", "must be a positive integer")

    for waiting_id, waiting in waiting_visuals.items():
        _check_waiting_visual(waiting, f"{base}.waiting_visuals[{waiting_id}]", element_ids, issues)

    for state_id, state in states.items():
        path = f"{base}.states[{state_id}]"
        _check_keys(state, {"state_id", "display_name", "render_model_ref", "waiting_visual_ref"}, path, issues)
        if state.get("render_model_ref") not in render_models:
            _issue(issues, "RENDER_MODEL_UNKNOWN", f"{path}.render_model_ref", "render model does not exist")
        if state.get("waiting_visual_ref") not in waiting_visuals:
            _issue(issues, "WAIT_VISUAL_UNKNOWN", f"{path}.waiting_visual_ref", "waiting visual does not exist")

    for route_id, route in routes.items():
        path = f"{base}.routes[{route_id}]"
        _check_keys(route, {"route_id", "action_ref", "from_states", "guards", "actions", "target_state"}, path, issues)
        if route.get("action_ref") not in input_actions:
            _issue(issues, "ROUTE_ACTION_UNKNOWN", f"{path}.action_ref", "input action does not exist")
        from_states = route.get("from_states")
        if not isinstance(from_states, list) or not from_states:
            _issue(issues, "ROUTE_SOURCE_MISSING", f"{path}.from_states", "must contain at least one state")
        else:
            for index, state_ref in enumerate(from_states):
                if state_ref not in states:
                    _issue(issues, "GRAPH_STATE_UNKNOWN", f"{path}.from_states[{index}]", f"unknown state '{state_ref}'")
        if route.get("target_state") not in states:
            _issue(issues, "GRAPH_TRANSITION_TARGET_UNKNOWN", f"{path}.target_state", "target state does not exist")
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
        return ProjectBundle(root, {}, (), tuple(issues))

    project = _read_json(root / "project.json", issues, "PROJECT_MANIFEST_INVALID")
    if project is None:
        return ProjectBundle(root, {}, (), tuple(issues))
    _check_project(project, issues)

    scenes: list[dict[str, Any]] = []
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
            _check_scene(scene, str(source), issues)
            scene_id = scene.get("scene_id")
            if isinstance(scene_id, str):
                if scene_id in scene_ids:
                    _issue(issues, "SCENE_ID_DUPLICATE", str(source), f"duplicate scene ID '{scene_id}'")
                scene_ids.add(scene_id)
            scenes.append(scene)

    entry_scene = project.get("entry_scene")
    if isinstance(entry_scene, str) and entry_scene not in scene_ids:
        _issue(issues, "SCENE_ENTRY_MISSING", "project.entry_scene", f"unknown entry scene '{entry_scene}'")
    return ProjectBundle(root, project, tuple(scenes), tuple(issues))


def format_issues(issues: Iterable[ValidationIssue]) -> str:
    return "\n".join(f"{issue.code} {issue.path}: {issue.message}" for issue in issues)
