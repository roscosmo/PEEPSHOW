"""Development-only scene-object source/migration primitives.

Not connected to service commands, project loading, preview or egg export yet.
Pure operations below are shared-backend semantics, not a second wire compiler.
Mutation/resolution helpers take previously validated definitions and live state.
"""

from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
import hashlib
import json
from typing import Any, Mapping

from .project import ProjectBundle, STABLE_ID, ValidationIssue


PROPERTY_KEYS = frozenset({"x", "y", "visible", "visual_ref"})
OBJECT_REQUIRED = frozenset({"object_id", "kind", "width", "height", "z_order", "layer", "defaults"})
OBJECT_OPTIONAL = frozenset({"animation_ref", "line_direction", "focus_role"})
OBJECT_KINDS = frozenset({"sprite", "line", "outline_rect", "filled_rect", "circle", "ellipse", "filled_circle", "filled_ellipse"})
OBJECT_ACTION_FIELDS = {
    "object.set_position": (frozenset(), frozenset({"x", "y"})),
    "object.move_by": (frozenset(), frozenset({"dx", "dy"})),
    "object.set_visibility": (frozenset({"visible"}), frozenset()),
    "object.set_frame": (frozenset({"frame_ref"}), frozenset()),
    "object.clear_frame": (frozenset(), frozenset()),
}


class SceneObjectError(ValueError):
    def __init__(self, code: str, path: str, message: str):
        super().__init__(f"{code}: {path}: {message}")
        self.issue = ValidationIssue(code, path, message)


def _integer(value: Any, minimum: int, maximum: int) -> bool:
    return type(value) is int and minimum <= value <= maximum


def validate_object_model(
    model: Mapping[str, Any],
    frame_sizes: Mapping[str, tuple[int, int]],
    animations: Mapping[str, Mapping[str, Any]],
    *,
    canvas: tuple[int, int] = (168, 144),
) -> tuple[ValidationIssue, ...]:
    """Validate only the v2 objects/states fragment, not a full scene or target admission."""
    issues: list[ValidationIssue] = []

    def issue(code: str, path: str, message: str) -> None:
        issues.append(ValidationIssue(code, path, message))

    def keys(value: Any, required: set | frozenset, allowed: set | frozenset, path: str) -> bool:
        if not isinstance(value, dict):
            issue("OBJECT_TYPE_INVALID", path, "must be an object")
            return False
        for key in sorted(required - value.keys()):
            issue("OBJECT_FIELD_MISSING", f"{path}.{key}", "required field is missing")
        for key in sorted(value.keys() - allowed):
            issue("OBJECT_FIELD_UNKNOWN", f"{path}.{key}", "field is not part of the object model")
        return True

    def records(value: Any, key: str, path: str) -> dict[str, dict]:
        result: dict[str, dict] = {}
        if not isinstance(value, list):
            issue("OBJECT_LIST_INVALID", path, "must be an array")
            return result
        for index, record in enumerate(value):
            record_id = record.get(key) if isinstance(record, dict) else None
            if not isinstance(record_id, str) or not STABLE_ID.fullmatch(record_id):
                issue("OBJECT_ID_INVALID", f"{path}[{index}].{key}", "must be a stable ID")
            elif record_id in result:
                issue("OBJECT_ID_DUPLICATE", f"{path}[{index}].{key}", "duplicate ID")
            else:
                result[record_id] = record
        return result

    def frame(value: Any, obj: dict, path: str) -> None:
        if obj.get("kind") != "sprite":
            issue("OBJECT_FRAME_INVALID", path, "only sprites select frames")
        elif not isinstance(value, str) or not STABLE_ID.fullmatch(value) or value not in frame_sizes:
            issue("ASSET_FRAME_UNKNOWN", path, "frame does not exist")
        elif frame_sizes[value] != (obj.get("width"), obj.get("height")):
            issue("OBJECT_FRAME_SIZE_INVALID", path, "frame must match object bounds")

    def properties(value: dict, obj: dict, path: str) -> None:
        for axis, size, extent in (("x", "width", canvas[0]), ("y", "height", canvas[1])):
            if axis in value:
                maximum = extent - obj[size] if _integer(obj.get(size), 1, extent) else -1
                if not _integer(value[axis], 0, maximum):
                    issue("OBJECT_BOUNDS_INVALID", f"{path}.{axis}", "whole object must fit inside the canvas")
        if "visible" in value and type(value["visible"]) is not bool:
            issue("OBJECT_VISIBILITY_INVALID", f"{path}.visible", "must be boolean")
        if "visual_ref" in value:
            frame(value["visual_ref"], obj, f"{path}.visual_ref")

    if not keys(model, {"objects", "states"}, {"objects", "states"}, "model"):
        return tuple(issues)
    objects = records(model.get("objects"), "object_id", "objects")
    states = records(model.get("states"), "state_id", "states")
    if not states:
        issue("OBJECT_STATE_MISSING", "states", "at least one state is required")
    for object_id, obj in objects.items():
        path = f"objects[{object_id}]"
        keys(obj, OBJECT_REQUIRED, OBJECT_REQUIRED | OBJECT_OPTIONAL, path)
        if not isinstance(obj.get("kind"), str) or obj["kind"] not in OBJECT_KINDS:
            issue("OBJECT_KIND_INVALID", f"{path}.kind", "unsupported object kind")
        if obj.get("layer") not in ("BACKGROUND", "SCENE", "UI"):
            issue("OBJECT_LAYER_INVALID", f"{path}.layer", "unsupported layer")
        for size, extent in (("width", canvas[0]), ("height", canvas[1]), ("z_order", 255)):
            if not _integer(obj.get(size), 0 if size == "z_order" else 1, extent):
                issue("OBJECT_BOUNDS_INVALID", f"{path}.{size}", "invalid dimension or draw order")
        width, height = obj.get("width"), obj.get("height")
        if obj.get("kind") in ("circle", "ellipse", "filled_circle", "filled_ellipse"):
            if not all(type(n) is int and n >= 3 and n % 2 == 1 for n in (width, height)):
                issue("OBJECT_GEOMETRY_INVALID", path, "round bounds must be odd and at least 3")
            if obj["kind"] in {"circle", "filled_circle"} and width != height:
                issue("OBJECT_GEOMETRY_INVALID", path, "circle bounds must be square")
        if "line_direction" in obj and (obj.get("kind") != "line" or obj["line_direction"] not in ("down_right", "up_right")):
            issue("OBJECT_LINE_INVALID", path, "line direction applies only to lines")
        if obj.get("focus_role", "none") not in ("none", "focus"):
            issue("OBJECT_FOCUS_INVALID", path, "unsupported focus role")
        required = {"x", "y", "visible"} | ({"visual_ref"} if obj.get("kind") == "sprite" else set())
        if keys(obj.get("defaults"), required, PROPERTY_KEYS, f"{path}.defaults"):
            properties(obj["defaults"], obj, f"{path}.defaults")
        if "animation_ref" in obj:
            ref = obj["animation_ref"]
            clip = animations.get(ref) if isinstance(ref, str) and STABLE_ID.fullmatch(ref) else None
            if obj.get("kind") != "sprite" or not isinstance(clip, dict):
                issue("OBJECT_ANIMATION_INVALID", f"{path}.animation_ref", "must reference an existing sprite clip")
            else:
                refs, durations = clip.get("frame_refs"), clip.get("frame_duration_ms")
                if not isinstance(refs, list) or not refs or not isinstance(durations, list) or len(refs) != len(durations):
                    issue("OBJECT_ANIMATION_INVALID", path, "clip frames and durations must be non-empty and aligned")
                else:
                    for index, ref in enumerate(refs):
                        frame(ref, obj, f"{path}.animation_ref.frames[{index}]")
                    if any(not _integer(n, 1, 60000) for n in durations):
                        issue("OBJECT_ANIMATION_INVALID", path, "invalid frame duration")
                if clip.get("loop_policy") != "loop":
                    issue("OBJECT_ANIMATION_UNSUPPORTED", path, "this development increment supports looping clips only")
    for state_id, state in states.items():
        path = f"states[{state_id}]"
        keys(state, {"state_id", "display_name", "object_overrides"}, {"state_id", "display_name", "object_overrides"}, path)
        if not isinstance(state.get("display_name"), str) or not 1 <= len(state["display_name"]) <= 96:
            issue("OBJECT_STATE_NAME_INVALID", path, "display name must contain 1..96 characters")
        overrides = records(state.get("object_overrides"), "object_ref", f"{path}.object_overrides")
        for ref, override in overrides.items():
            item_path = f"{path}.object_overrides[{ref}]"
            keys(override, {"object_ref"}, {"object_ref"} | PROPERTY_KEYS, item_path)
            if not (override.keys() & PROPERTY_KEYS):
                issue("OBJECT_OVERRIDE_EMPTY", item_path, "omit records that control no properties")
            if ref not in objects:
                issue("OBJECT_REFERENCE_UNKNOWN", item_path, "object does not exist in this scene")
            else:
                properties(override, objects[ref], item_path)
    return tuple(issues)


def initialize_objects(objects: list[dict]) -> dict[str, dict]:
    """Authored visual_ref is a fallback, never an implicit persistent mask."""
    return {obj["object_id"]: {**deepcopy(obj["defaults"]), "static_frame_ref": None} for obj in objects}


def resolve_object(obj: dict, live: dict, override: dict, animations: Mapping[str, dict], elapsed_ms: int) -> dict:
    """Elapsed time is scene-active time, supplied by the caller (not wall time)."""
    result = deepcopy(live)
    clip = animations.get(obj.get("animation_ref"))
    if clip is not None:
        durations = clip["frame_duration_ms"]
        remaining = elapsed_ms % sum(durations)
        for frame_ref, duration in zip(clip["frame_refs"], durations):
            if remaining < duration:
                result["visual_ref"] = frame_ref
                break
            remaining -= duration
    if live["static_frame_ref"] is not None:
        result["visual_ref"] = live["static_frame_ref"]
    result.update({key: value for key, value in override.items() if key in PROPERTY_KEYS})
    return result


def apply_object_actions(objects: list[dict], live: dict[str, dict], actions: list[dict], frame_sizes: Mapping[str, tuple[int, int]], *, canvas: tuple[int, int] = (168, 144)) -> dict[str, dict]:
    """Atomic object-only staging; absolute Y is down, relative dy is positive up."""
    definitions = {obj["object_id"]: obj for obj in objects}
    staged = deepcopy(live)
    for index, action in enumerate(actions):
        path = f"actions[{index}]"
        if not isinstance(action, dict) or not isinstance(action.get("kind"), str) or action["kind"] not in OBJECT_ACTION_FIELDS:
            raise SceneObjectError("OBJECT_ACTION_UNSUPPORTED", path, "not an object operation")
        required, optional = OBJECT_ACTION_FIELDS[action["kind"]]
        required = required | {"kind", "object_ref"}
        if not required <= action.keys() or action.keys() - required - optional:
            raise SceneObjectError("OBJECT_ACTION_FIELDS_INVALID", path, "invalid operation fields")
        ref = action["object_ref"]
        if not isinstance(ref, str) or ref not in definitions or ref not in staged:
            raise SceneObjectError("OBJECT_REFERENCE_UNKNOWN", path, "object does not exist in the current scene")
        obj, values = definitions[ref], staged[ref]
        if action["kind"] in {"object.set_position", "object.move_by"}:
            if not (action.keys() & optional):
                raise SceneObjectError("OBJECT_ACTION_AXIS_REQUIRED", path, "at least one axis is required")
            for field in optional & action.keys():
                if not _integer(action[field], -2147483648, 2147483647):
                    raise SceneObjectError("OBJECT_ACTION_VALUE_INVALID", f"{path}.{field}", "must fit signed int32")
            for axis, delta, dimension, extent in (("x", "dx", "width", canvas[0]), ("y", "dy", "height", canvas[1])):
                if action["kind"] == "object.set_position":
                    value = action.get(axis, values[axis])
                else:
                    value = values[axis] + action.get(delta, 0) * (1 if axis == "x" else -1)
                values[axis] = max(0, min(extent - obj[dimension], value))
        elif action["kind"] == "object.set_visibility":
            if type(action["visible"]) is not bool:
                raise SceneObjectError("OBJECT_ACTION_VALUE_INVALID", path, "visible must be boolean")
            values["visible"] = action["visible"]
        else:
            if obj["kind"] != "sprite":
                raise SceneObjectError("OBJECT_FRAME_INVALID", path, "only sprites select frames")
            if action["kind"] == "object.clear_frame":
                values["static_frame_ref"] = None
            else:
                ref = action["frame_ref"]
                if not isinstance(ref, str) or frame_sizes.get(ref) != (obj["width"], obj["height"]):
                    raise SceneObjectError("OBJECT_FRAME_INVALID", path, "frame must exist and match object bounds")
                values["static_frame_ref"] = ref
    return staged


def clear_object_override(state: dict, object_ref: str, properties: list[str]) -> dict:
    if not properties or any(not isinstance(key, str) or key not in PROPERTY_KEYS for key in properties):
        raise SceneObjectError("OBJECT_OVERRIDE_PROPERTY_INVALID", "properties", "specify x, y, visible or visual_ref; position is not an axis")
    result = deepcopy(state)
    for override in result["object_overrides"]:
        if override["object_ref"] == object_ref:
            for key in properties:
                override.pop(key, None)
    result["object_overrides"] = [item for item in result["object_overrides"] if len(item) > 1]
    return result


@dataclass(frozen=True)
class ObjectMigrationPlan:
    source_revision: str
    scene_id: str
    scene: dict
    new_animations: tuple[dict, ...]
    changes: tuple[str, ...]
    issues: tuple[ValidationIssue, ...]


def _revision(bundle: ProjectBundle) -> str:
    return hashlib.sha256(bundle.canonical_bytes()).hexdigest()


def plan_object_migration(bundle: ProjectBundle, scene_id: str, *, accept_continuous_animation: bool = False) -> ObjectMigrationPlan:
    """Return a non-writing development plan from already validated legacy source."""
    if not bundle.valid:
        raise SceneObjectError("MIGRATION_SOURCE_INVALID", scene_id, "validate the legacy project first")
    source = next((scene for scene in bundle.scenes if scene["scene_id"] == scene_id), None)
    if source is None or source.get("schema_version") != 1:
        raise SceneObjectError("MIGRATION_SOURCE_UNSUPPORTED", scene_id, "expected a legacy scene")
    scene = deepcopy(source)
    issues: list[ValidationIssue] = []
    changes = ["Preserve object IDs; replace state render copies with sparse temporary overrides."]
    objects = []
    for element in scene.pop("render_models")[0]["elements"]:
        obj = {key: value for key, value in element.items() if key not in PROPERTY_KEYS | {"element_id"}}
        obj["object_id"] = element["element_id"]
        obj["layer"] = element.get("layer", "UI" if element.get("focus_role") == "focus" else "SCENE")
        obj["defaults"] = {key: value for key, value in element.items() if key in PROPERTY_KEYS}
        obj["defaults"].setdefault("visible", True)
        objects.append(obj)
    scene["objects"] = objects
    waits = {item["waiting_visual_id"]: item for item in scene.pop("waiting_visuals")}
    referenced_waits = [waits[state["waiting_visual_ref"]] for state in scene["states"]]
    referenced_waits.append(waits[scene["reactive_wait_default"].pop("waiting_visual_ref")])
    clips = {clip["animation_id"]: clip for clip in bundle.animations}
    additions = []
    for obj in objects:
        path = f"scenes[{scene_id}].objects[{obj['object_id']}]"
        sequences = []
        ambiguous = False
        for wait in referenced_waits:
            tracks = [item for item in wait["elements"] if item["source_element_ref"] == obj["object_id"]]
            if len(tracks) > 1:
                ambiguous = True
            element = tracks[0] if tracks else None
            if element is None:
                sequences.append(None)
            else:
                refs = [element["phase_visual_refs"][phase] for phase in element["step_phase_indices"]]
                sequences.append({"frame_refs": refs, "frame_duration_ms": [wait["phase_quantum_ms"]] * len(refs), "loop_policy": wait["cycle_policy"]})
        if ambiguous:
            issues.append(ValidationIssue("MIGRATION_MULTIPLE_TRACKS_UNSUPPORTED", path, "multiple legacy tracks address this object; choose one explicitly"))
            continue
        if not any(item is not None for item in sequences):
            continue
        if any(item != sequences[0] for item in sequences) or sequences[0] is None:
            issues.append(ValidationIssue("MIGRATION_ANIMATED_OVERRIDE_UNSUPPORTED", path, "state-private animation differs; keep legacy or explicitly reauthor it"))
            continue
        if not accept_continuous_animation:
            issues.append(ValidationIssue("MIGRATION_CONTINUITY_CHOICE_REQUIRED", path, "explicitly accept scene-continuous playback and scene-entry phase zero"))
        changes.append(f"{obj['object_id']}: playback starts at sequence step zero and continues across state changes; legacy presentation/settled-step policies are removed.")
        clip_data = sequences[0]
        matching = next((ref for ref, clip in sorted(clips.items()) if all(clip.get(key) == value for key, value in clip_data.items())), None)
        if matching is None:
            digest = hashlib.sha256(json.dumps(clip_data, sort_keys=True).encode("ascii")).hexdigest()[:24]
            matching = f"migrated.{digest}"
            if matching in clips:
                issues.append(ValidationIssue("MIGRATION_CLIP_ID_CONFLICT", path, "generated clip ID conflicts with a different catalog clip"))
                continue
            clip = {"animation_id": matching, **deepcopy(clip_data)}
            clips[matching] = clip
            additions.append(clip)
        obj["animation_ref"] = matching
    for state in scene["states"]:
        state.pop("render_model_ref", None)
        state.pop("waiting_visual_ref")
        state["object_overrides"] = [
            {("object_ref" if key == "element_ref" else key): value for key, value in item.items()}
            for item in state.pop("placement_overrides", []) if item.keys() & PROPERTY_KEYS
        ]
    for kind in ("routes", "event_handlers"):
        for route in scene.get(kind, []):
            for index, action in enumerate(route["actions"]):
                if action["kind"].startswith("set_element_"):
                    issues.append(ValidationIssue("MIGRATION_ACTION_CHOICE_REQUIRED", f"scenes[{scene_id}].{kind}[{route.get('route_id', route.get('handler_id'))}].actions[{index}]", "destination-binding mutation cannot become a persistent object action implicitly"))
    scene["schema_version"] = 2
    sizes = {frame.frame_id: (frame.width, frame.height) for frame in bundle.frames}
    issues.extend(validate_object_model({"objects": objects, "states": scene["states"]}, sizes, clips))
    return ObjectMigrationPlan(_revision(bundle), scene_id, scene, tuple(additions), tuple(changes), tuple(issues))


def materialize_object_migration(bundle: ProjectBundle, plan: ObjectMigrationPlan, *, accept_continuous_animation: bool = False) -> tuple[dict, tuple[dict, ...]]:
    """Recompute from the source/review choice; never trust mutable plan contents or write files."""
    if not bundle.valid or _revision(bundle) != plan.source_revision:
        raise SceneObjectError("MIGRATION_SOURCE_CHANGED", plan.scene_id, "preview migration again against the current project")
    current = plan_object_migration(bundle, plan.scene_id, accept_continuous_animation=accept_continuous_animation)
    if current.issues:
        issue = current.issues[0]
        raise SceneObjectError(issue.code, issue.path, issue.message)
    return deepcopy(current.scene), deepcopy(current.new_animations)
