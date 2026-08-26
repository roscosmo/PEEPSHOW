from __future__ import annotations

import base64
import hashlib
from dataclasses import dataclass
from typing import Any

from .egg_format import EggPackage


DISPLAY_WIDTH = 168
DISPLAY_HEIGHT = 144
DISPLAY_STRIDE = DISPLAY_WIDTH // 8
DISPLAY_BUFFER_SIZE = DISPLAY_STRIDE * DISPLAY_HEIGHT
LOGICAL_SOURCES = {
    "BUTTON_A": 1,
    "BUTTON_B": 2,
    "BUTTON_L": 3,
    "BUTTON_R": 4,
    "JOY_LEFT": 6,
    "JOY_RIGHT": 7,
    "JOY_UP": 8,
    "JOY_DOWN": 9,
}


class PreviewError(ValueError):
    """Raised when a validated package cannot execute in the current preview subset."""


@dataclass(frozen=True)
class PreviewInputResult:
    logical_source: str
    action_id: str | None
    accepted: bool
    route_id: str | None


class StateScenePreview:
    """Deterministic selected-scene executor over an independently parsed package."""

    def __init__(self, package: EggPackage, scene_id: str) -> None:
        self._package = package
        self._scenes = {str(scene["scene_id"]): scene for scene in package.scenes}
        self._frames = {str(frame["frame_id"]): frame for frame in package.assets}
        self._animations = {str(animation["animation_id"]): animation for animation in package.animations}
        self._elapsed_ms = 0
        self._activate_scene(scene_id)
        self._render_framebuffer()

    @property
    def scene_id(self) -> str:
        return str(self._scene["scene_id"])

    def _state(self) -> dict[str, object]:
        return self._graph["states"][self._state_index]

    def _waiting(self) -> dict[str, object]:
        return self._waiting_visuals[int(self._state()["waiting_visual_index"])]

    def _guard_passes(self, guard: dict[str, int]) -> bool:
        left = self._variables[guard["variable_index"]]
        right = guard["value"]
        operator = guard["operator"]
        if operator == 1:
            return left == right
        if operator == 2:
            return left != right
        if operator == 3:
            return left < right
        if operator == 4:
            return left <= right
        if operator == 5:
            return left > right
        if operator == 6:
            return left >= right
        raise PreviewError("compiled guard operator is unsupported")

    def _activate_scene(self, scene_id: str) -> None:
        scene = self._scenes.get(scene_id)
        if scene is None:
            raise PreviewError(f"scene '{scene_id}' is not present in the package")
        self._scene = scene
        self._graph = scene["graph"]
        self._render_models = scene["render_models"]
        self._waiting_visuals = scene["waiting_visuals"]
        self._state_index = int(self._graph["entry_state"])
        self._variables = [int(variable["initial"]) for variable in self._graph["variables"]]
        self._elapsed_ms = 0
        self._step_elapsed_ms = 0
        self._step_index = int(self._waiting()["settled_step"])
        self._validate_scene_subset()

    def apply_input(self, logical_source: str) -> PreviewInputResult:
        source = LOGICAL_SOURCES.get(logical_source)
        if source is None:
            raise PreviewError(f"logical source '{logical_source}' is unsupported")
        action_index = next(
            (
                index
                for index, action in enumerate(self._graph["inputs"])
                if int(action["logical_source"]) == source
            ),
            None,
        )
        if action_index is None:
            return PreviewInputResult(logical_source, None, False, None)
        action_id = str(self._graph["inputs"][action_index]["action_id"])
        route = next(
            (
                candidate
                for candidate in self._graph["routes"]
                if int(candidate["action_index"]) == action_index
                and self._state_index in candidate["source_state_indexes"]
                and all(self._guard_passes(guard) for guard in candidate["guards"])
            ),
            None,
        )
        if route is None:
            return PreviewInputResult(logical_source, action_id, False, None)

        target_scene = route["target_scene"]
        if target_scene is not None:
            if route["operations"]:
                raise PreviewError("direct scene replacement cannot execute scene-local actions")
            self._activate_scene(str(target_scene))
            self._render_framebuffer()
            return PreviewInputResult(logical_source, action_id, True, str(route["route_id"]))

        prior_waiting = self._waiting()
        variables = list(self._variables)
        definitions = self._graph["variables"]
        for operation in route["operations"]:
            if int(operation["kind"]) == 2:
                continue
            variable_index = int(operation["variable_index"])
            if int(operation["operation"]) == 1:
                value = int(operation["value"])
            elif int(operation["operation"]) == 2:
                value = variables[variable_index] + int(operation["value"])
            else:
                raise PreviewError("compiled variable operation is unsupported")
            definition = definitions[variable_index]
            if not int(definition["minimum"]) <= value <= int(definition["maximum"]):
                raise PreviewError(f"route '{route['route_id']}' writes a variable outside its compiled bounds")
            variables[variable_index] = value

        self._variables = variables
        target_state = route["target_state_index"]
        if target_state is None:
            raise PreviewError("compiled route has no target")
        self._state_index = int(target_state)
        next_waiting = self._waiting()
        if not self._compatible_timeline(prior_waiting, next_waiting):
            self._step_index = int(next_waiting["settled_step"])
            self._step_elapsed_ms = 0
        self._render_framebuffer()
        return PreviewInputResult(logical_source, action_id, True, str(route["route_id"]))

    @staticmethod
    def _compatible_timeline(first: dict[str, object], second: dict[str, object]) -> bool:
        return (
            first["presentation_id"] == second["presentation_id"]
            and first["phase_quantum_ms"] == second["phase_quantum_ms"]
            and first["combined_step_count"] == second["combined_step_count"]
        )

    def advance(self, elapsed_ms: int) -> None:
        if isinstance(elapsed_ms, bool) or not isinstance(elapsed_ms, int) or not 0 <= elapsed_ms <= 600000:
            raise PreviewError("elapsed_ms must be an integer in 0..600000")
        waiting = self._waiting()
        quantum = int(waiting["phase_quantum_ms"])
        step_count = int(waiting["combined_step_count"])
        total = self._step_elapsed_ms + elapsed_ms
        self._step_index = (self._step_index + total // quantum) % step_count
        self._step_elapsed_ms = total % quantum
        self._elapsed_ms += elapsed_ms
        self._render_framebuffer()

    def _visual_overrides(self) -> dict[str, str]:
        result: dict[str, str] = {}
        for element in self._waiting()["elements"]:
            phase_index = int(element["step_phase_indices"][self._step_index])
            result[str(element["source_element_ref"])] = str(element["phase_visual_refs"][phase_index])
        return result

    def _resolve_frame(self, visual_ref: str) -> dict[str, object]:
        frame = self._frames.get(visual_ref)
        if frame is not None:
            return frame
        animation = self._animations.get(visual_ref)
        if animation is None:
            raise PreviewError(f"visual '{visual_ref}' is not a package sprite frame or animation")
        durations = tuple(int(value) for value in animation["frame_duration_ms"])
        indexes = tuple(int(value) for value in animation["frame_indexes"])
        cycle = sum(durations)
        loop_policy = int(animation["loop_policy"])
        if loop_policy == 1:
            position = self._elapsed_ms % cycle
            sequence = indexes
            sequence_durations = durations
        elif loop_policy in {2, 3}:
            position = min(self._elapsed_ms, cycle - 1)
            sequence = indexes
            sequence_durations = durations
        elif loop_policy == 4:
            sequence = indexes + indexes[-2:0:-1]
            sequence_durations = durations + durations[-2:0:-1]
            ping_pong_cycle = sum(sequence_durations)
            position = self._elapsed_ms % ping_pong_cycle
        else:
            raise PreviewError("compiled animation loop policy is unsupported")
        for frame_index, duration in zip(sequence, sequence_durations):
            if position < duration:
                return self._package.assets[frame_index]
            position -= duration
        return self._package.assets[sequence[-1]]

    def _validate_scene_subset(self) -> None:
        for state in self._graph["states"]:
            model = self._render_models[int(state["render_model_index"])]
            waiting = self._waiting_visuals[int(state["waiting_visual_index"])]
            elements = {str(element["element_id"]): element for element in model["elements"]}
            for element in model["elements"]:
                if int(element["kind"]) != 1:
                    raise PreviewError(
                        f"element '{element['element_id']}' uses a procedural visual; "
                        "the package-backed preview requires sprites"
                    )
                self._require_native_frame(element, str(element["visual_ref"]))
            for animated in waiting["elements"]:
                source_id = str(animated["source_element_ref"])
                source = elements.get(source_id)
                if source is None:
                    raise PreviewError(
                        f"waiting visual '{waiting['waiting_visual_id']}' references missing element '{source_id}'"
                    )
                for visual_ref in animated["phase_visual_refs"]:
                    self._require_native_frame(source, str(visual_ref))

    def _require_native_frame(self, element: dict[str, object], visual_ref: str) -> None:
        frame = self._resolve_frame(visual_ref)
        if int(frame["width"]) != int(element["width"]) or int(frame["height"]) != int(element["height"]):
            raise PreviewError(f"element '{element['element_id']}' requires unsupported runtime scaling")

    @staticmethod
    def _plane_bit(plane: bytes, stride: int, x: int, y: int) -> int:
        return (plane[y * stride + x // 8] >> (7 - (x % 8))) & 1

    @staticmethod
    def _write_bit(framebuffer: bytearray, x: int, y: int, value: int) -> None:
        offset = y * DISPLAY_STRIDE + x // 8
        mask = 1 << (7 - (x % 8))
        if value:
            framebuffer[offset] |= mask
        else:
            framebuffer[offset] &= ~mask

    def _render_framebuffer(self) -> bytes:
        state = self._state()
        model = self._render_models[int(state["render_model_index"])]
        overrides = self._visual_overrides()
        framebuffer = bytearray(DISPLAY_BUFFER_SIZE)
        ordered = sorted(enumerate(model["elements"]), key=lambda item: (int(item[1]["z_order"]), item[0]))
        for _, element in ordered:
            visual_ref = overrides.get(str(element["element_id"]), str(element["visual_ref"]))
            frame = self._resolve_frame(visual_ref)
            width = int(frame["width"])
            height = int(frame["height"])
            stride = int(frame["row_stride_bytes"])
            origin_x = int(element["x"]) - int(frame["pivot_x"])
            origin_y = int(element["y"]) - int(frame["pivot_y"])
            pixels = frame["pixels"]
            mask = frame["mask"]
            opaque = bool(frame["opaque"])
            for source_y in range(height):
                target_y = origin_y + source_y
                if not 0 <= target_y < DISPLAY_HEIGHT:
                    continue
                for source_x in range(width):
                    target_x = origin_x + source_x
                    if not 0 <= target_x < DISPLAY_WIDTH:
                        continue
                    if opaque or self._plane_bit(mask, stride, source_x, source_y):
                        self._write_bit(
                            framebuffer,
                            target_x,
                            target_y,
                            self._plane_bit(pixels, stride, source_x, source_y),
                        )
        self._framebuffer = bytes(framebuffer)
        return self._framebuffer

    def snapshot(self, input_result: PreviewInputResult | None = None) -> dict[str, Any]:
        state = self._state()
        waiting = self._waiting()
        variable_values = {
            str(definition["variable_id"]): value
            for definition, value in zip(self._graph["variables"], self._variables)
        }
        event = None
        if input_result is not None:
            event = {
                "logical_source": input_result.logical_source,
                "action_id": input_result.action_id,
                "accepted": input_result.accepted,
                "route_id": input_result.route_id,
            }
        return {
            "scene": {
                "scene_id": self.scene_id,
                "state_index": self._state_index,
                "state_id": state["state_id"],
                "display_name": state["display_name"],
            },
            "timeline": {
                "elapsed_ms": self._elapsed_ms,
                "presentation_id": waiting["presentation_id"],
                "step_index": self._step_index,
                "step_elapsed_ms": self._step_elapsed_ms,
                "phase_quantum_ms": waiting["phase_quantum_ms"],
                "step_count": waiting["combined_step_count"],
            },
            "variables": variable_values,
            "input": event,
            "framebuffer": {
                "width": DISPLAY_WIDTH,
                "height": DISPLAY_HEIGHT,
                "row_stride_bytes": DISPLAY_STRIDE,
                "encoding": "row_major_msb_1bpp_black_is_1",
                "size_bytes": len(self._framebuffer),
                "black_pixel_count": sum(byte.bit_count() for byte in self._framebuffer),
                "sha256": hashlib.sha256(self._framebuffer).hexdigest(),
                "data_base64": base64.b64encode(self._framebuffer).decode("ascii"),
            },
        }
