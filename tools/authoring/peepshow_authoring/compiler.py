"""Lower validated authoring projects into deterministic PKG1 .egg blobs."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path
from typing import Any, Iterable

from .egg_format import (
    ANIMATION_HEADER,
    ANIMATION_RECORD,
    ASSET_FLAG_OPAQUE,
    ASSET_HEADER,
    ASSET_RECORD,
    CHUNK_ANIMATION_TABLE,
    CHUNK_ASSET_TABLE,
    CHUNK_MANIFEST,
    CHUNK_MASKED_1BPP_SPRITE_BANK,
    CHUNK_RENDER_MODELS,
    CHUNK_SCENE_TABLE,
    CHUNK_STATE_GRAPH,
    CHUNK_STRING_TABLE,
    CHUNK_WAITING_VISUALS,
    GRAPH_HEADER,
    GUARD_RECORD,
    INPUT_RECORD,
    MANIFEST,
    OPERATION_RECORD,
    RENDER_ELEMENT_RECORD,
    RENDER_HEADER,
    RENDER_MODEL_RECORD,
    ROUTE_RECORD,
    SCENE_HEADER,
    SCENE_RECORD,
    SPRITE_BANK_HEADER,
    STATE_RECORD,
    STRING_HEADER,
    VARIABLE_RECORD,
    WAIT_ELEMENT_RECORD,
    WAIT_HEADER,
    WAIT_RECORD,
    EggChunkSpec,
    EggFormatError,
    build_container,
    parse_egg,
)
from .project import ProjectBundle


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
}
GUARD_OPERATORS = {"eq": 1, "ne": 2, "lt": 3, "le": 4, "gt": 5, "ge": 6}
VARIABLE_OPERATIONS = {"assign": 1, "add": 2}
RENDER_KINDS = {
    "sprite": 1,
    "line": 2,
    "outline_rect": 3,
    "filled_rect": 4,
    "circle": 5,
    "ellipse": 6,
}
RENDER_LAYERS = {"BACKGROUND": 0, "SCENE": 1, "UI": 2}
INACTIVE_ROUTES = {"preserve_scene": 1, "exit_to_shell": 2}
INTERACTION_MODES = {"continuous": 1, "timeout": 2}
ANIMATION_LOOPS = {"loop": 1, "once": 2, "hold_last": 3, "ping_pong": 4}


class EggCompileError(ValueError):
    """Raised when validated source cannot fit the frozen binary schema."""


def _u16(value: int, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFF:
        raise EggCompileError(f"{field} does not fit u16")
    return value


def _i16(value: int, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not -0x8000 <= value <= 0x7FFF:
        raise EggCompileError(f"{field} does not fit i16")
    return value


def _i32(value: int, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not -0x80000000 <= value <= 0x7FFFFFFF:
        raise EggCompileError(f"{field} does not fit int32")
    return value


def _string_table(bundle: ProjectBundle) -> tuple[tuple[str, ...], dict[str, int], bytes]:
    project = bundle.project
    package = project["package"]
    values: set[str] = {
        package["package_id"],
        package["display_name"],
        project["selected_target_profile"],
        project["entry_scene"],
    }
    for scene in bundle.scenes:
        values.update((scene["scene_id"], scene["display_name"]))
        values.update(record["variable_id"] for record in scene["variables"])
        values.update(record["action_id"] for record in scene["input_actions"])
        for state in scene["states"]:
            values.update((state["state_id"], state["display_name"]))
        values.update(record["route_id"] for record in scene["routes"])
        values.update(
            record["target_scene"]
            for record in scene["routes"]
            if "target_scene" in record
        )
        for model in scene["render_models"]:
            values.add(model["visual_id"])
            for element in model["elements"]:
                values.add(element["element_id"])
                if element["kind"] == "sprite":
                    values.add(element["visual_ref"])
        for waiting in scene["waiting_visuals"]:
            values.update((waiting["waiting_visual_id"], waiting["presentation_id"]))
            for element in waiting["elements"]:
                values.update((element["element_id"], element["source_element_ref"]))
                values.update(element["phase_visual_refs"])
        values.add(scene["reactive_wait_default"]["policy_id"])
        values.add(scene["interaction_policy"]["policy_id"])
    for asset in bundle.assets:
        values.add(asset["asset_id"])
        values.update(frame["frame_id"] for frame in asset["frames"])
    values.update(animation["animation_id"] for animation in bundle.animations)
    strings = tuple(sorted(values))
    if len(strings) > 0xFFFF:
        raise EggCompileError("string table contains more than 65535 strings")
    encoded = [value.encode("utf-8") for value in strings]
    if any(len(value) > 0xFFFF for value in encoded):
        raise EggCompileError("a string is longer than 65535 bytes")
    offsets = [0]
    payload = bytearray()
    for value in encoded:
        payload.extend(value)
        offsets.append(len(payload))
    header = STRING_HEADER.pack(b"STR1", 1, len(strings), len(payload))
    offset_table = struct.pack(f"<{len(offsets)}I", *offsets)
    return strings, {value: index for index, value in enumerate(strings)}, header + offset_table + payload


def _compile_manifest(project: dict[str, Any], strings: dict[str, int], scene_count: int) -> bytes:
    package = project["package"]
    version = tuple(int(part) for part in package["version"].split("."))
    for index, value in enumerate(version):
        _u16(value, f"package version field {index}")
    return MANIFEST.pack(
        b"MAN1",
        1,
        MANIFEST.size,
        strings[package["package_id"]],
        strings[package["display_name"]],
        strings[project["selected_target_profile"]],
        version[0],
        version[1],
        version[2],
        strings[project["entry_scene"]],
        _u16(scene_count, "scene count"),
        0,
    )


def _compile_render(scene: dict[str, Any], strings: dict[str, int]) -> bytes:
    model_records = bytearray()
    element_records = bytearray()
    element_count = 0
    for model in scene["render_models"]:
        first_element = element_count
        for element in model["elements"]:
            element_records.extend(
                RENDER_ELEMENT_RECORD.pack(
                    strings[element["element_id"]],
                    strings[element["visual_ref"]] if element["kind"] == "sprite" else 0xFFFF,
                    RENDER_KINDS[element["kind"]],
                    RENDER_LAYERS[element.get(
                        "layer",
                        "UI" if element.get("focus_role", "none") == "focus" else "SCENE",
                    )],
                    (1 if element.get("focus_role", "none") == "focus" else 0)
                    | (2 if element.get("visible", True) else 0),
                    0,
                    _i16(element["x"], "render x"),
                    _i16(element["y"], "render y"),
                    _u16(element["width"], "render width"),
                    _u16(element["height"], "render height"),
                    _u16(element["z_order"], "render z_order"),
                    0,
                )
            )
            element_count += 1
        model_records.extend(
            RENDER_MODEL_RECORD.pack(
                strings[model["visual_id"]],
                _u16(model["focus_index"], "focus index"),
                _u16(first_element, "first render element"),
                _u16(len(model["elements"]), "render element count"),
            )
        )
    header = RENDER_HEADER.pack(
        b"RND1",
        2,
        RENDER_HEADER.size,
        _u16(len(scene["render_models"]), "render model count"),
        _u16(element_count, "render element count"),
        0,
        0,
    )
    return header + model_records + element_records


def _compile_waiting(scene: dict[str, Any], strings: dict[str, int]) -> bytes:
    wait_records = bytearray()
    element_records = bytearray()
    phase_refs: list[int] = []
    sequence_values = bytearray()
    element_count = 0
    for waiting in scene["waiting_visuals"]:
        first_element = element_count
        for element in waiting["elements"]:
            first_phase = len(phase_refs)
            phase_refs.extend(strings[value] for value in element["phase_visual_refs"])
            first_step = len(sequence_values)
            sequence_values.extend(element["step_phase_indices"])
            element_records.extend(
                WAIT_ELEMENT_RECORD.pack(
                    strings[element["element_id"]],
                    strings[element["source_element_ref"]],
                    _u16(first_phase, "first phase reference"),
                    _u16(len(element["phase_visual_refs"]), "phase reference count"),
                    _u16(first_step, "first sequence step"),
                    _u16(len(element["step_phase_indices"]), "sequence step count"),
                )
            )
            element_count += 1
        wait_records.extend(
            WAIT_RECORD.pack(
                strings[waiting["waiting_visual_id"]],
                strings[waiting["presentation_id"]],
                _u16(waiting["phase_quantum_ms"], "waiting phase quantum"),
                _u16(waiting["combined_step_count"], "combined step count"),
                _u16(waiting["settled_step"], "settled step"),
                1,
                _u16(first_element, "first waiting element"),
                _u16(len(waiting["elements"]), "waiting element count"),
            )
        )
    header = WAIT_HEADER.pack(
        b"WAI1",
        1,
        WAIT_HEADER.size,
        _u16(len(scene["waiting_visuals"]), "waiting visual count"),
        _u16(element_count, "waiting element count"),
        _u16(len(phase_refs), "phase reference count"),
        _u16(len(sequence_values), "sequence value count"),
        0,
        0,
        0,
        0,
    )
    phases = struct.pack(f"<{len(phase_refs)}H", *phase_refs) if phase_refs else b""
    return header + wait_records + element_records + phases + sequence_values


def _compile_graph(scene: dict[str, Any], strings: dict[str, int]) -> bytes:
    variables = scene["variables"]
    inputs = scene["input_actions"]
    states = scene["states"]
    routes = scene["routes"]
    variable_index = {record["variable_id"]: index for index, record in enumerate(variables)}
    input_index = {record["action_id"]: index for index, record in enumerate(inputs)}
    state_index = {record["state_id"]: index for index, record in enumerate(states)}
    render_index = {record["visual_id"]: index for index, record in enumerate(scene["render_models"])}
    waiting_index = {record["waiting_visual_id"]: index for index, record in enumerate(scene["waiting_visuals"])}

    variable_records = bytearray()
    for variable in variables:
        variable_records.extend(
            VARIABLE_RECORD.pack(
                strings[variable["variable_id"]],
                1,
                0,
                _i32(variable["initial"], "variable initial"),
                _i32(variable["minimum"], "variable minimum"),
                _i32(variable["maximum"], "variable maximum"),
            )
        )
    input_records = b"".join(
        INPUT_RECORD.pack(strings[record["action_id"]], LOGICAL_SOURCES[record["logical_source"]])
        for record in inputs
    )
    state_records = b"".join(
        STATE_RECORD.pack(
            strings[record["state_id"]],
            strings[record["display_name"]],
            render_index[record["render_model_ref"]],
            waiting_index[record["waiting_visual_ref"]],
        )
        for record in states
    )

    route_records = bytearray()
    source_states: list[int] = []
    guard_records = bytearray()
    operation_records = bytearray()
    guard_count = 0
    operation_count = 0
    for route in routes:
        target_elements: dict[str, int] = {}
        if "target_state" in route:
            target_state_record = states[state_index[route["target_state"]]]
            target_render_model = scene["render_models"][
                render_index[target_state_record["render_model_ref"]]
            ]
            target_elements = {
                element["element_id"]: index
                for index, element in enumerate(target_render_model["elements"])
            }
        first_source = len(source_states)
        source_states.extend(state_index[value] for value in route["from_states"])
        first_guard = guard_count
        for guard in route["guards"]:
            guard_records.extend(
                GUARD_RECORD.pack(
                    variable_index[guard["variable_ref"]],
                    GUARD_OPERATORS[guard["operator"]],
                    0,
                    _i32(guard["value"], "guard value"),
                )
            )
            guard_count += 1
        first_operation = operation_count
        for operation in route["actions"]:
            if operation["kind"] == "set_variable":
                operation_records.extend(
                    OPERATION_RECORD.pack(
                        1,
                        VARIABLE_OPERATIONS[operation["operation"]],
                        variable_index[operation["variable_ref"]],
                        0,
                        0,
                        _i32(operation["value"], "operation value"),
                    )
                )
            elif operation["kind"] == "request_render":
                operation_records.extend(OPERATION_RECORD.pack(2, 0, 0xFFFF, 0, 0, 0))
            elif operation["kind"] == "set_element_visibility":
                operation_records.extend(
                    OPERATION_RECORD.pack(
                        3,
                        0,
                        target_elements[operation["element_ref"]],
                        0,
                        0,
                        1 if operation["visible"] else 0,
                    )
                )
            elif operation["kind"] == "set_element_position":
                operation_records.extend(
                    OPERATION_RECORD.pack(
                        4,
                        0,
                        target_elements[operation["element_ref"]],
                        _u16(operation["x"], "element action x"),
                        0,
                        _i32(operation["y"], "element action y"),
                    )
                )
            elif operation["kind"] == "set_element_frame":
                operation_records.extend(
                    OPERATION_RECORD.pack(
                        5,
                        0,
                        target_elements[operation["element_ref"]],
                        strings[operation["frame_ref"]],
                        0,
                        0,
                    )
                )
            elif operation["kind"] == "set_element_waiting_animation":
                source_waiting = scene["waiting_visuals"][
                    waiting_index[operation["waiting_visual_ref"]]
                ]
                source_element_index = next(
                    index
                    for index, element in enumerate(source_waiting["elements"])
                    if element["element_id"] == operation["waiting_element_ref"]
                )
                operation_records.extend(
                    OPERATION_RECORD.pack(
                        6,
                        1 if operation["timeline_policy"] == "preserve" else 2,
                        target_elements[operation["element_ref"]],
                        waiting_index[operation["waiting_visual_ref"]],
                        strings[source_waiting["elements"][source_element_index]["element_id"]],
                        0,
                    )
                )
            else:
                raise EggCompileError("unsupported STATE route action")
            operation_count += 1
        route_records.extend(
            ROUTE_RECORD.pack(
                strings[route["route_id"]],
                input_index[route["action_ref"]],
                state_index[route["target_state"]] if "target_state" in route else 0xFFFF,
                strings[route["target_scene"]] if "target_scene" in route else 0xFFFF,
                _u16(first_source, "first route source"),
                _u16(len(route["from_states"]), "route source count"),
                _u16(first_guard, "first route guard"),
                _u16(len(route["guards"]), "route guard count"),
                _u16(first_operation, "first route operation"),
                _u16(len(route["actions"]), "route operation count"),
            )
        )

    wait_policy = scene["reactive_wait_default"]
    interaction = scene["interaction_policy"]
    event_interests = [input_index[value] for value in wait_policy["event_interests"]]
    meaningful = [input_index[value] for value in interaction["meaningful_activity_actions"]]
    interaction_mode = INTERACTION_MODES[interaction["mode"]]
    inactive_route = INACTIVE_ROUTES.get(interaction.get("inactive_route"), 0)
    header = GRAPH_HEADER.pack(
        b"STG1",
        3,
        GRAPH_HEADER.size,
        state_index[scene["entry_state"]],
        _u16(len(variables), "variable count"),
        _u16(len(inputs), "input count"),
        _u16(len(states), "state count"),
        _u16(len(routes), "route count"),
        _u16(len(source_states), "route source count"),
        _u16(guard_count, "guard count"),
        _u16(operation_count, "operation count"),
        strings[wait_policy["policy_id"]],
        waiting_index[wait_policy["waiting_visual_ref"]],
        1 if wait_policy["hold_fallback_allowed"] else 0,
        _u16(len(event_interests), "event-interest count"),
        strings[interaction["policy_id"]],
        inactive_route,
        _u16(len(meaningful), "meaningful-action count"),
        interaction_mode,
    )
    sources = struct.pack(f"<{len(source_states)}H", *source_states) if source_states else b""
    events = struct.pack(f"<{len(event_interests)}H", *event_interests) if event_interests else b""
    meaningful_bytes = struct.pack(f"<{len(meaningful)}H", *meaningful) if meaningful else b""
    return (
        header
        + variable_records
        + input_records
        + state_records
        + route_records
        + sources
        + guard_records
        + operation_records
        + events
        + meaningful_bytes
    )


def _compile_scene_table(
    scenes: tuple[dict[str, Any], ...],
    strings: dict[str, int],
    chunk_indexes: dict[str, tuple[int, int, int]],
) -> bytes:
    records = bytearray()
    for scene in scenes:
        state_index = {record["state_id"]: index for index, record in enumerate(scene["states"])}
        graph_index, render_index, waiting_index = chunk_indexes[scene["scene_id"]]
        records.extend(
            SCENE_RECORD.pack(
                strings[scene["scene_id"]],
                strings[scene["display_name"]],
                1,
                state_index[scene["entry_state"]],
                graph_index,
                render_index,
                waiting_index,
                0,
                0,
            )
        )
    return SCENE_HEADER.pack(b"SCN1", 1, SCENE_HEADER.size, len(scenes), 0) + records


def _append_plane(payload: bytearray, plane: bytes, offsets: dict[bytes, int]) -> int:
    existing = offsets.get(plane)
    if existing is not None:
        return existing
    while len(payload) % 4:
        payload.append(0)
    offset = len(payload)
    payload.extend(plane)
    offsets[plane] = offset
    return offset


def _compile_asset_chunks(
    bundle: ProjectBundle,
    strings: dict[str, int],
    sprite_chunk_index: int,
    animation_chunk_index: int,
) -> tuple[bytes, bytes, bytes]:
    frames = tuple(sorted(bundle.frames, key=lambda frame: (frame.asset_id, frame.frame_id)))
    frame_indexes = {frame.frame_id: index for index, frame in enumerate(frames)}

    sprite_payload = bytearray(SPRITE_BANK_HEADER.size)
    plane_offsets: dict[bytes, int] = {}
    asset_records = bytearray()
    for frame in frames:
        pixel_offset = _append_plane(sprite_payload, frame.pixels, plane_offsets)
        if frame.opaque:
            mask_offset = 0
            mask_size = 0
            flags = ASSET_FLAG_OPAQUE
        else:
            mask_offset = _append_plane(sprite_payload, frame.mask, plane_offsets)
            mask_size = len(frame.mask)
            flags = 0
        asset_records.extend(
            ASSET_RECORD.pack(
                strings[frame.asset_id],
                strings[frame.frame_id],
                _u16(frame.width, "asset width"),
                _u16(frame.height, "asset height"),
                _u16(frame.row_stride_bytes, "asset row stride"),
                0,
                _i16(frame.pivot_x, "asset pivot x"),
                _i16(frame.pivot_y, "asset pivot y"),
                pixel_offset,
                len(frame.pixels),
                mask_offset,
                mask_size,
                flags,
            )
        )
    SPRITE_BANK_HEADER.pack_into(
        sprite_payload,
        0,
        b"SPB1",
        1,
        SPRITE_BANK_HEADER.size,
        len(sprite_payload) - SPRITE_BANK_HEADER.size,
        0,
    )
    asset_payload = ASSET_HEADER.pack(
        b"AST1",
        1,
        ASSET_HEADER.size,
        _u16(len(frames), "asset frame count"),
        _u16(sprite_chunk_index, "sprite chunk index"),
        _u16(animation_chunk_index, "animation chunk index"),
        0,
        0,
        0,
    ) + asset_records

    animation_records = bytearray()
    animation_frame_refs: list[int] = []
    animation_durations: list[int] = []
    for animation in sorted(bundle.animations, key=lambda item: item["animation_id"]):
        first_frame = len(animation_frame_refs)
        first_duration = len(animation_durations)
        selected_frames = [frame_indexes[frame_id] for frame_id in animation["frame_refs"]]
        animation_frame_refs.extend(selected_frames)
        animation_durations.extend(animation["frame_duration_ms"])
        width = max(frames[index].width for index in selected_frames)
        height = max(frames[index].height for index in selected_frames)
        animation_records.extend(
            ANIMATION_RECORD.pack(
                strings[animation["animation_id"]],
                _u16(first_frame, "animation first frame"),
                _u16(len(selected_frames), "animation frame count"),
                _u16(first_duration, "animation first duration"),
                ANIMATION_LOOPS[animation["loop_policy"]],
                _u16(width, "animation width"),
                _u16(height, "animation height"),
                0,
            )
        )
    frame_ref_bytes = (
        struct.pack(f"<{len(animation_frame_refs)}H", *animation_frame_refs)
        if animation_frame_refs
        else b""
    )
    duration_bytes = (
        struct.pack(f"<{len(animation_durations)}I", *animation_durations)
        if animation_durations
        else b""
    )
    animation_payload = (
        ANIMATION_HEADER.pack(
            b"ANI1",
            1,
            ANIMATION_HEADER.size,
            _u16(len(bundle.animations), "animation count"),
            _u16(len(animation_frame_refs), "animation frame-reference count"),
            _u16(len(animation_durations), "animation duration count"),
            0,
        )
        + animation_records
        + frame_ref_bytes
        + duration_bytes
    )
    return asset_payload, bytes(sprite_payload), animation_payload


def build_egg(bundle: ProjectBundle) -> bytes:
    if not bundle.valid:
        raise EggCompileError("project must validate before package compilation")
    scenes = tuple(sorted(bundle.scenes, key=lambda scene: scene["scene_id"]))
    _, string_indexes, string_payload = _string_table(bundle)
    chunk_indexes = {
        scene["scene_id"]: (3 + index * 3, 4 + index * 3, 5 + index * 3)
        for index, scene in enumerate(scenes)
    }
    chunks: list[EggChunkSpec] = [
        EggChunkSpec("strings", CHUNK_STRING_TABLE, string_payload),
        EggChunkSpec("manifest", CHUNK_MANIFEST, _compile_manifest(bundle.project, string_indexes, len(scenes))),
        EggChunkSpec("scenes", CHUNK_SCENE_TABLE, _compile_scene_table(scenes, string_indexes, chunk_indexes)),
    ]
    for scene in scenes:
        scene_id = scene["scene_id"]
        chunks.extend(
            (
                EggChunkSpec(f"state_graph.{scene_id}", CHUNK_STATE_GRAPH, _compile_graph(scene, string_indexes)),
                EggChunkSpec(f"render_models.{scene_id}", CHUNK_RENDER_MODELS, _compile_render(scene, string_indexes)),
                EggChunkSpec(f"waiting_visuals.{scene_id}", CHUNK_WAITING_VISUALS, _compile_waiting(scene, string_indexes)),
            )
        )
    if bundle.frames:
        asset_chunk_index = len(chunks)
        sprite_chunk_index = asset_chunk_index + 1
        animation_chunk_index = asset_chunk_index + 2
        asset_payload, sprite_payload, animation_payload = _compile_asset_chunks(
            bundle,
            string_indexes,
            sprite_chunk_index,
            animation_chunk_index,
        )
        chunks.extend(
            (
                EggChunkSpec("assets", CHUNK_ASSET_TABLE, asset_payload),
                EggChunkSpec("sprites.masked_1bpp", CHUNK_MASKED_1BPP_SPRITE_BANK, sprite_payload),
                EggChunkSpec("animations", CHUNK_ANIMATION_TABLE, animation_payload),
            )
        )
    try:
        blob = build_container(bundle.project["package"]["package_id"], chunks, manifest_chunk_index=1)
        parse_egg(blob)
        return blob
    except (KeyError, struct.error, EggFormatError) as exc:
        raise EggCompileError(f"could not emit a valid .egg: {exc}") from exc


def write_egg(bundle: ProjectBundle, output_path: str | Path) -> Path:
    output = Path(output_path)
    if output.suffix.lower() != ".egg":
        raise EggCompileError("installable package output must end in .egg")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build_egg(bundle))
    return output


def write_embedded_egg_c(
    bundle: ProjectBundle,
    output_path: str | Path,
    symbol: str = "g_ps_embedded_egg",
) -> Path:
    if not symbol or not symbol.replace("_", "a").isalnum() or symbol[0].isdigit():
        raise EggCompileError("embedded package symbol is not a C identifier")

    blob = build_egg(bundle)
    package_id = bundle.project["package"]["package_id"]
    digest = hashlib.sha256(blob).hexdigest()
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for offset in range(0, len(blob), 12):
        values = ", ".join(f"0x{value:02X}U" for value in blob[offset : offset + 12])
        rows.append(f"  {values},")
    source = (
        "/* Generated by egg-tool embed. Do not edit manually. */\n"
        f"/* package={package_id} sha256={digest} */\n"
        "#include <stdint.h>\n\n"
        "#include \"ps_embedded_egg.h\"\n\n"
        f"const uint8_t {symbol}[] =\n"
        "{\n"
        + "\n".join(rows)
        + "\n};\n\n"
        f"const uint32_t {symbol}_size = (uint32_t)sizeof({symbol});\n"
    )
    output.write_text(source, encoding="ascii", newline="\n")
    return output
