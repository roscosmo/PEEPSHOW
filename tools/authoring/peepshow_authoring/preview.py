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
    "BUTTON_START": 5,
    "JOY_LEFT": 6,
    "JOY_RIGHT": 7,
    "JOY_UP": 8,
    "JOY_DOWN": 9,
    "JOY_UP_LEFT": 10,
    "JOY_UP_RIGHT": 11,
    "JOY_DOWN_LEFT": 12,
    "JOY_DOWN_RIGHT": 13,
}
LOGICAL_EVENTS = {"press": 1, "release": 2, "hold": 3, "repeat": 4}


class PreviewError(ValueError):
    """Raised when a validated package cannot execute in the current preview subset."""


@dataclass(frozen=True)
class PreviewInputResult:
    logical_source: str
    event_kind: str
    action_id: str | None
    accepted: bool
    route_id: str | None
    audio_events: tuple[dict[str, object], ...] = ()
    system_action: str | None = None


class StateScenePreview:
    """Deterministic selected-scene executor over an independently parsed package."""

    def __init__(self, package: EggPackage, scene_id: str, state_id: str | None = None) -> None:
        self._package = package
        self._scenes = {str(scene["scene_id"]): scene for scene in package.scenes}
        self._frames = {str(frame["frame_id"]): frame for frame in package.assets}
        self._animations = {str(animation["animation_id"]): animation for animation in package.animations}
        self._audio_cues = package.audio_cues
        self._elapsed_ms = 0
        self._activate_scene(scene_id)
        if state_id is not None:
            self.select_state(state_id)
        else:
            self._render_framebuffer()

    def select_state(self, state_id: str) -> None:
        state_index = next(
            (
                index
                for index, state in enumerate(self._graph["states"])
                if str(state["state_id"]) == state_id
            ),
            None,
        )
        if state_index is None:
            raise PreviewError(f"state '{state_id}' is not present in scene '{self.scene_id}'")
        self._state_index = state_index
        self._elapsed_ms = 0
        self._step_elapsed_ms = 0
        self._step_index = int(self._waiting()["settled_step"])
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
        self._element_overrides: dict[tuple[int, int], dict[str, object]] = {}
        self._waiting_element_overrides: dict[
            tuple[int, int], dict[str, object]
        ] = {}
        self._elapsed_ms = 0
        self._step_elapsed_ms = 0
        self._step_index = int(self._waiting()["settled_step"])
        self._validate_scene_subset()

    def apply_input(self, logical_source: str, event_kind: str = "press") -> PreviewInputResult:
        source = LOGICAL_SOURCES.get(logical_source)
        if source is None:
            raise PreviewError(f"logical source '{logical_source}' is unsupported")
        event = LOGICAL_EVENTS.get(event_kind)
        if event is None:
            raise PreviewError(f"logical event '{event_kind}' is unsupported")
        action_index = next(
            (
                index
                for index, action in enumerate(self._graph["inputs"])
                if int(action["logical_source"]) == source
                and int(action.get("logical_event", 1)) == event
            ),
            None,
        )
        if action_index is None:
            return PreviewInputResult(logical_source, event_kind, None, False, None)
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
            return PreviewInputResult(logical_source, event_kind, action_id, False, None)

        target_scene = route["target_scene"]
        if target_scene is not None:
            audio_events: list[dict[str, object]] = []
            for operation in route["operations"]:
                if int(operation["kind"]) != 7:
                    raise PreviewError(
                        "direct scene replacement can execute only play_sfx actions"
                    )
                cue_index = int(operation["cue_index"])
                if not 0 <= cue_index < len(self._audio_cues):
                    raise PreviewError("compiled SFX action cue is invalid")
                cue = self._audio_cues[cue_index]
                audio_events.append(
                    {
                        "kind": "play_sfx",
                        "cue_id": cue["cue_id"],
                        "asset_id": cue["asset_id"],
                        "priority": cue["priority"],
                        "volume": cue["volume"],
                    }
                )
            self._activate_scene(str(target_scene))
            self._render_framebuffer()
            return PreviewInputResult(
                logical_source,
                event_kind,
                action_id,
                True,
                str(route["route_id"]),
                tuple(audio_events),
            )

        prior_waiting = self._waiting()
        variables = list(self._variables)
        element_overrides = {
            key: dict(value) for key, value in self._element_overrides.items()
        }
        waiting_element_overrides = {
            key: dict(value)
            for key, value in self._waiting_element_overrides.items()
        }
        force_timeline_rebase = False
        audio_events: list[dict[str, object]] = []
        system_action: str | None = None
        definitions = self._graph["variables"]
        target_state = route["target_state_index"]
        if target_state is None:
            raise PreviewError("compiled route has no target")
        target_model_index = int(
            self._graph["states"][int(target_state)]["render_model_index"]
        )
        target_model = self._render_models[target_model_index]
        for operation in route["operations"]:
            kind = int(operation["kind"])
            if kind == 2:
                continue
            if kind == 7:
                cue_index = int(operation["cue_index"])
                if not 0 <= cue_index < len(self._audio_cues):
                    raise PreviewError("compiled SFX action cue is invalid")
                cue = self._audio_cues[cue_index]
                audio_events.append(
                    {
                        "kind": "play_sfx",
                        "cue_id": cue["cue_id"],
                        "asset_id": cue["asset_id"],
                        "priority": cue["priority"],
                        "volume": cue["volume"],
                    }
                )
                continue
            if kind == 8:
                system_action = "exit_to_shell"
                continue
            if kind in {3, 4, 5, 6}:
                element_index = int(operation["element_index"])
                if not 0 <= element_index < len(target_model["elements"]):
                    raise PreviewError("compiled element action target is invalid")
                element = target_model["elements"][element_index]
                key = (target_model_index, element_index)
                override = dict(element_overrides.get(key, {}))
                if kind == 3:
                    visible = int(operation["visible"])
                    if not visible and int(element.get("focus_role", 0)):
                        raise PreviewError("compiled action cannot hide the focus element")
                    override["visible"] = visible
                elif kind == 4:
                    x = int(operation["x"])
                    y = int(operation["y"])
                    if (
                        x < 0
                        or y < 0
                        or x + int(element["width"]) > DISPLAY_WIDTH
                        or y + int(element["height"]) > DISPLAY_HEIGHT
                    ):
                        raise PreviewError("compiled element position is outside the display")
                    override["x"] = x
                    override["y"] = y
                elif kind == 5:
                    frame_ref = str(operation["frame_ref"])
                    self._require_native_frame(element, frame_ref)
                    override["visual_ref"] = frame_ref
                else:
                    waiting = self._waiting_visuals[
                        int(operation["waiting_visual_index"])
                    ]
                    waiting_element_ref = str(operation["waiting_element_ref"])
                    waiting_element = next(
                        (
                            item
                            for item in waiting["elements"]
                            if str(item["element_id"]) == waiting_element_ref
                        ),
                        None,
                    )
                    if (
                        waiting_element is None
                        or str(waiting_element["source_element_ref"])
                        != str(element["element_id"])
                    ):
                        raise PreviewError(
                            "compiled waiting-animation action target is invalid"
                        )
                    if (
                        int(waiting["phase_quantum_ms"])
                        != int(self._waiting_visuals[int(
                            self._graph["states"][int(target_state)][
                                "waiting_visual_index"
                            ]
                        )]["phase_quantum_ms"])
                        or int(waiting["combined_step_count"])
                        != int(self._waiting_visuals[int(
                            self._graph["states"][int(target_state)][
                                "waiting_visual_index"
                            ]
                        )]["combined_step_count"])
                    ):
                        raise PreviewError(
                            "compiled waiting-animation timeline is incompatible"
                        )
                    for visual_ref in waiting_element["phase_visual_refs"]:
                        self._require_native_frame(element, str(visual_ref))
                    waiting_element_overrides[key] = dict(waiting_element)
                    force_timeline_rebase = (
                        force_timeline_rebase
                        or int(operation["timeline_policy"]) == 2
                    )
                element_overrides[key] = override
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
        self._element_overrides = element_overrides
        self._waiting_element_overrides = waiting_element_overrides
        self._state_index = int(target_state)
        next_waiting = self._waiting()
        if force_timeline_rebase or not self._compatible_timeline(
            prior_waiting, next_waiting
        ):
            self._step_index = int(next_waiting["settled_step"])
            self._step_elapsed_ms = 0
        self._render_framebuffer()
        return PreviewInputResult(
            logical_source,
            event_kind,
            action_id,
            True,
            str(route["route_id"]),
            tuple(audio_events),
            system_action,
        )

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
        state = self._state()
        model_index = int(state["render_model_index"])
        model = self._render_models[model_index]
        waiting_by_source = {
            str(element["source_element_ref"]): element
            for element in self._waiting()["elements"]
        }
        for element_index, render_element in enumerate(model["elements"]):
            element = self._waiting_element_overrides.get(
                (model_index, element_index),
                waiting_by_source.get(str(render_element["element_id"])),
            )
            if element is None:
                continue
            phase_index = int(element["step_phase_indices"][self._step_index])
            result[str(element["source_element_ref"])] = str(element["phase_visual_refs"][phase_index])
        return result

    def _resolved_element(
        self,
        model_index: int,
        element_index: int,
        element: dict[str, object],
    ) -> dict[str, object]:
        override = self._element_overrides.get((model_index, element_index))
        if not override:
            return element
        resolved = dict(element)
        resolved.update(override)
        return resolved

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
                version = int(element.get("format_version", 1))
                kind = int(element["kind"])
                if version == 1 and kind != 1:
                    raise PreviewError(
                        f"element '{element['element_id']}' uses a procedural visual; "
                        "the package-backed preview requires sprites"
                    )
                if version == 2 and kind not in {1, 2, 3, 4, 5, 6}:
                    raise PreviewError(f"element '{element['element_id']}' has an unsupported retained type")
                if kind == 1:
                    self._require_native_frame(element, str(element["visual_ref"]))
            for animated in waiting["elements"]:
                source_id = str(animated["source_element_ref"])
                source = elements.get(source_id)
                if source is None:
                    raise PreviewError(
                        f"waiting visual '{waiting['waiting_visual_id']}' references missing element '{source_id}'"
                    )
                if int(source["kind"]) != 1:
                    raise PreviewError(
                        f"waiting visual '{waiting['waiting_visual_id']}' requires a sprite source element"
                    )
                for visual_ref in animated["phase_visual_refs"]:
                    self._require_native_frame(source, str(visual_ref))
        for route in self._graph["routes"]:
            target_state = route["target_state_index"]
            if target_state is None:
                continue
            model_index = int(
                self._graph["states"][int(target_state)]["render_model_index"]
            )
            model = self._render_models[model_index]
            for operation in route["operations"]:
                kind = int(operation["kind"])
                if kind not in {3, 4, 5, 6}:
                    continue
                element_index = int(operation["element_index"])
                if not 0 <= element_index < len(model["elements"]):
                    raise PreviewError("compiled element action target is invalid")
                if kind == 5:
                    self._require_native_frame(
                        model["elements"][element_index],
                        str(operation["frame_ref"]),
                    )
                elif kind == 6:
                    waiting = self._waiting_visuals[
                        int(operation["waiting_visual_index"])
                    ]
                    waiting_element = next(
                        (
                            item
                            for item in waiting["elements"]
                            if str(item["element_id"])
                            == str(operation["waiting_element_ref"])
                        ),
                        None,
                    )
                    if waiting_element is None:
                        raise PreviewError(
                            "compiled waiting-animation action source is invalid"
                        )
                    if str(waiting_element["source_element_ref"]) != str(
                        model["elements"][element_index]["element_id"]
                    ):
                        raise PreviewError(
                            "compiled waiting-animation action target is invalid"
                        )
                    target_waiting = self._waiting_visuals[
                        int(self._graph["states"][int(target_state)][
                            "waiting_visual_index"
                        ])
                    ]
                    if (
                        int(waiting["phase_quantum_ms"])
                        != int(target_waiting["phase_quantum_ms"])
                        or int(waiting["combined_step_count"])
                        != int(target_waiting["combined_step_count"])
                    ):
                        raise PreviewError(
                            "compiled waiting-animation timeline is incompatible"
                        )
                    for visual_ref in waiting_element["phase_visual_refs"]:
                        self._require_native_frame(
                            model["elements"][element_index], str(visual_ref)
                        )

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

    @classmethod
    def _draw_line(cls, framebuffer: bytearray, x0: int, y0: int, x1: int, y1: int) -> None:
        dx = abs(x1 - x0)
        sx = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy = 1 if y0 < y1 else -1
        error = dx + dy
        while True:
            cls._write_bit(framebuffer, x0, y0, 1)
            if x0 == x1 and y0 == y1:
                return
            doubled = error * 2
            if doubled >= dy:
                error += dy
                x0 += sx
            if doubled <= dx:
                error += dx
                y0 += sy

    @classmethod
    def _draw_ellipse(cls, framebuffer: bytearray, center_x: int, center_y: int, radius_x: int, radius_y: int) -> None:
        x = 0
        y = radius_y
        radius_x_squared = radius_x * radius_x
        radius_y_squared = radius_y * radius_y
        dx = 0
        dy = 2 * radius_x_squared * y
        decision = radius_y_squared - radius_x_squared * radius_y + radius_x_squared // 4
        while dx < dy:
            for point_x, point_y in (
                (center_x + x, center_y + y),
                (center_x - x, center_y + y),
                (center_x + x, center_y - y),
                (center_x - x, center_y - y),
            ):
                cls._write_bit(framebuffer, point_x, point_y, 1)
            x += 1
            dx += 2 * radius_y_squared
            if decision < 0:
                decision += radius_y_squared + dx
            else:
                y -= 1
                dy -= 2 * radius_x_squared
                decision += radius_y_squared + dx - dy

        decision = (
            radius_y_squared * (x * x + x)
            + radius_y_squared // 4
            + radius_x_squared * ((y - 1) * (y - 1))
            - radius_x_squared * radius_y_squared
        )
        while y >= 0:
            for point_x, point_y in (
                (center_x + x, center_y + y),
                (center_x - x, center_y + y),
                (center_x + x, center_y - y),
                (center_x - x, center_y - y),
            ):
                cls._write_bit(framebuffer, point_x, point_y, 1)
            y -= 1
            dy -= 2 * radius_x_squared
            if decision > 0:
                decision += radius_x_squared - dy
            else:
                x += 1
                dx += 2 * radius_y_squared
                decision += radius_x_squared - dy + dx

    @classmethod
    def _draw_primitive(cls, framebuffer: bytearray, element: dict[str, object]) -> None:
        kind = int(element["kind"])
        x = int(element["x"])
        y = int(element["y"])
        width = int(element["width"])
        height = int(element["height"])
        right = x + width - 1
        bottom = y + height - 1
        if kind == 2:
            cls._draw_line(framebuffer, x, y, right, bottom)
        elif kind == 3:
            cls._draw_line(framebuffer, x, y, right, y)
            cls._draw_line(framebuffer, x, bottom, right, bottom)
            cls._draw_line(framebuffer, x, y, x, bottom)
            cls._draw_line(framebuffer, right, y, right, bottom)
        elif kind == 4:
            for row in range(y, bottom + 1):
                cls._draw_line(framebuffer, x, row, right, row)
        elif kind in {5, 6}:
            cls._draw_ellipse(framebuffer, x + width // 2, y + height // 2, width // 2, height // 2)
        else:
            raise PreviewError(f"element '{element['element_id']}' has an unsupported retained type")

    def _render_framebuffer(self) -> bytes:
        state = self._state()
        model_index = int(state["render_model_index"])
        model = self._render_models[model_index]
        overrides = self._visual_overrides()
        framebuffer = bytearray(DISPLAY_BUFFER_SIZE)
        ordered = sorted(
            (
                (index, self._resolved_element(model_index, index, element))
                for index, element in enumerate(model["elements"])
            ),
            key=lambda item: (int(item[1].get("layer", 1)), int(item[1]["z_order"]), item[0]),
        )
        for _, element in ordered:
            if not int(element.get("visible", 1)):
                continue
            if int(element["kind"]) != 1:
                self._draw_primitive(framebuffer, element)
                continue
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
                "event_kind": input_result.event_kind,
                "action_id": input_result.action_id,
                "accepted": input_result.accepted,
                "route_id": input_result.route_id,
                "audio_events": list(input_result.audio_events),
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
