"""Development V2 wire records. The decoder consumes bytes, never source plans."""

from __future__ import annotations

import hashlib
import re
import struct

from .egg_format import (
    EggPackage, GRAPH_HEADER, VARIABLE_RECORD, EVENT_RECORD, STATE_RECORD,
    ROUTE_RECORD_V2, GUARD_RECORD, SCENE_HEADER, SCENE_RECORD,
    CHUNK_ASSET_TABLE, CHUNK_MASKED_1BPP_SPRITE_BANK, CHUNK_ANIMATION_TABLE,
    CHUNK_SCENE_TABLE, CHUNK_STATE_GRAPH, CHUNK_RENDER_MODELS,
    CHUNK_WAITING_VISUALS, CHUNK_SCENE_OBJECTS, CHUNK_OBJECT_CONTROLS,
    _require, _string, _parse_assets, _parse_graph, _parse_render, _parse_wait,
    parse_egg,
)


OBJECT_HEADER = struct.Struct("<4s6H")
OBJECT_RECORD = struct.Struct("<HBBBBHHhhHHH")
CONTROL_HEADER = struct.Struct("<4s8H")
STATE_RANGE = struct.Struct("<HH")
OVERRIDE_RECORD = struct.Struct("<HHiiHH")
OBJECT_OPERATION = struct.Struct("<BBHiiHH")
NONE = 0xFFFF
KINDS = {"sprite": 1, "line": 2, "outline_rect": 3, "filled_rect": 4,
         "circle": 5, "ellipse": 6, "filled_circle": 7, "filled_ellipse": 8}
LAYERS = {"BACKGROUND": 0, "SCENE": 1, "UI": 2}
OPCODES = {"object.set_position": 1, "object.move_by": 2, "object.set_visibility": 3,
           "object.set_frame": 4, "object.clear_frame": 5}


def compile_object_chunks(scene, strings, frames, animations):
    frame_indexes = {frame.frame_id: index for index, frame in enumerate(sorted(frames, key=lambda frame: (frame.asset_id, frame.frame_id)))}
    clip_indexes = {clip["animation_id"]: index for index, clip in enumerate(sorted(animations, key=lambda clip: clip["animation_id"]))}
    objects = scene["objects"]
    indexes = {obj["object_id"]: index for index, obj in enumerate(objects)}
    records = bytearray()
    for obj in objects:
        defaults = obj["defaults"]
        flags = int(defaults["visible"]) | (2 if obj.get("line_direction") == "up_right" else 0) | (4 if obj.get("focus_role") == "focus" else 0)
        records.extend(OBJECT_RECORD.pack(strings[obj["object_id"]], KINDS[obj["kind"]], LAYERS[obj["layer"]],
            obj["z_order"], flags, obj["width"], obj["height"], defaults["x"], defaults["y"],
            frame_indexes[defaults["visual_ref"]] if "visual_ref" in defaults else NONE,
            clip_indexes[obj["animation_ref"]] if "animation_ref" in obj else NONE, 0))
    ranges, overrides, operations = bytearray(), bytearray(), bytearray()
    for state in scene["states"]:
        ranges.extend(STATE_RANGE.pack(len(overrides) // OVERRIDE_RECORD.size, len(state["object_overrides"])))
        for item in state["object_overrides"]:
            mask = sum(bit for key, bit in (("x", 1), ("y", 2), ("visual_ref", 4), ("visible", 8)) if key in item)
            overrides.extend(OVERRIDE_RECORD.pack(indexes[item["object_ref"]], mask, item.get("x", 0), item.get("y", 0),
                frame_indexes[item["visual_ref"]] if "visual_ref" in item else NONE, int(item.get("visible", False))))
    for route in [*scene["routes"], *scene.get("event_handlers", [])]:
        for action in route["actions"]:
            if not action["kind"].startswith("object."):
                continue
            code = OPCODES[action["kind"]]
            xkey, ykey = ("dx", "dy") if code == 2 else ("x", "y")
            mask = int(xkey in action) | (2 if ykey in action else 0)
            operations.extend(OBJECT_OPERATION.pack(code, mask, indexes[action["object_ref"]],
                action.get(xkey, 0), action.get(ykey, 0),
                frame_indexes[action["frame_ref"]] if "frame_ref" in action else NONE,
                int(action.get("visible", False))))
    return (
        OBJECT_HEADER.pack(b"OBJ2", 1, OBJECT_HEADER.size, len(objects), OBJECT_RECORD.size, 0, 0) + records,
        CONTROL_HEADER.pack(b"OCT2", 1, CONTROL_HEADER.size, len(scene["states"]),
            len(overrides) // OVERRIDE_RECORD.size, len(operations) // OBJECT_OPERATION.size,
            STATE_RANGE.size, OVERRIDE_RECORD.size, OBJECT_OPERATION.size) + ranges + overrides + operations,
    )


def _stable_id(strings, index, field):
    value = _string(strings, index, field)
    _require(re.fullmatch(r"[a-z][a-z0-9_.-]{0,63}", value) is not None, f"{field} is not a stable ID")
    return value


def _frame(index, obj, assets):
    _require(obj["kind"] == 1 and index < len(assets), "object frame reference is invalid")
    frame = assets[index]
    _require((frame["width"], frame["height"]) == (obj["width"], obj["height"]), "object frame dimensions mismatch")


def _position(obj, x, y):
    _require(0 <= x <= 168 - obj["width"] and 0 <= y <= 144 - obj["height"], "object position is outside canvas")


def _objects(payload, strings, assets, animations):
    _require(len(payload) >= OBJECT_HEADER.size, "object table is truncated")
    header = OBJECT_HEADER.unpack_from(payload)
    _require(header[:3] == (b"OBJ2", 1, OBJECT_HEADER.size) and header[4:] == (OBJECT_RECORD.size, 0, 0), "object table header is invalid")
    count = header[3]
    _require(1 <= count <= 32 and len(payload) == OBJECT_HEADER.size + count * OBJECT_RECORD.size, "object table count/size is invalid")
    objects, ids = [], set()
    for index in range(count):
        record = OBJECT_RECORD.unpack_from(payload, OBJECT_HEADER.size + index * OBJECT_RECORD.size)
        name, kind, layer, z, flags, width, height, x, y, frame, clip, reserved = record
        object_id = _stable_id(strings, name, "object ID")
        _require(object_id not in ids, "duplicate object ID")
        ids.add(object_id)
        _require(kind in range(1, 9) and layer in range(3) and flags & ~7 == 0 and reserved == 0, "object kind/flags/reserved invalid")
        _require(not flags & 2 or kind == 2, "line direction on non-line object")
        _require(1 <= width <= 168 and 1 <= height <= 144, "object dimensions invalid")
        if kind in {5, 6, 7, 8}:
            _require(width >= 3 and height >= 3 and width % 2 == height % 2 == 1, "round object bounds invalid")
            _require(kind not in {5, 7} or width == height, "circle bounds must be square")
        obj = {"object_id": object_id, "kind": kind, "layer": layer, "z_order": z, "flags": flags,
               "width": width, "height": height, "x": x, "y": y, "frame_index": frame, "clip_index": clip}
        _position(obj, x, y)
        if kind == 1:
            _frame(frame, obj, assets)
            if clip != NONE:
                _require(clip < len(animations), "object clip reference is invalid")
                animation = animations[clip]
                _require(animation["loop_policy"] == 1, "object clip must loop")
                for ref in animation["frame_indexes"]:
                    _frame(ref, obj, assets)
        else:
            _require(frame == clip == NONE, "primitive references sprite content")
        objects.append(obj)
    return tuple(objects)


def _controls(payload, objects, assets):
    _require(len(payload) >= CONTROL_HEADER.size, "object controls truncated")
    header = CONTROL_HEADER.unpack_from(payload)
    _require(header[:3] == (b"OCT2", 1, CONTROL_HEADER.size) and header[6:] == (STATE_RANGE.size, OVERRIDE_RECORD.size, OBJECT_OPERATION.size), "object control header invalid")
    state_count, override_count, operation_count = header[3:6]
    _require(1 <= state_count <= 64 and override_count <= 2048 and operation_count <= 1152, "object control count invalid")
    override_offset = CONTROL_HEADER.size + state_count * STATE_RANGE.size
    operation_offset = override_offset + override_count * OVERRIDE_RECORD.size
    _require(len(payload) == operation_offset + operation_count * OBJECT_OPERATION.size, "object control size invalid")
    state_overrides, cursor = [], 0
    for state in range(state_count):
        first, count = STATE_RANGE.unpack_from(payload, CONTROL_HEADER.size + state * STATE_RANGE.size)
        _require(first == cursor and count <= len(objects) and first + count <= override_count, "override range is not a partition")
        cursor += count
        selected, seen = [], set()
        for index in range(first, first + count):
            obj_index, mask, x, y, frame, visible = OVERRIDE_RECORD.unpack_from(payload, override_offset + index * OVERRIDE_RECORD.size)
            _require(obj_index < len(objects) and obj_index not in seen, "override object reference invalid or duplicate")
            seen.add(obj_index)
            _require(0 < mask <= 15, "override property mask invalid")
            _require((mask & 1 or x == 0) and (mask & 2 or y == 0) and (mask & 4 or frame == NONE)
                     and (mask & 8 or visible == 0) and visible in {0, 1}, "override unused fields invalid")
            obj = objects[obj_index]
            _position(obj, x if mask & 1 else obj["x"], y if mask & 2 else obj["y"])
            if mask & 4:
                _frame(frame, obj, assets)
            selected.append({"object_index": obj_index, "mask": mask, "x": x, "y": y, "frame_index": frame, "visible": visible})
        state_overrides.append(tuple(selected))
    _require(cursor == override_count, "orphan override records")
    operations = []
    for index in range(operation_count):
        code, mask, obj_index, x, y, frame, visible = OBJECT_OPERATION.unpack_from(payload, operation_offset + index * OBJECT_OPERATION.size)
        _require(1 <= code <= 5 and obj_index < len(objects), "object operation kind/reference invalid")
        _require(mask in {1, 2, 3} if code in {1, 2} else mask == 0, "object operation axis mask invalid")
        _require((mask & 1 or x == 0) and (mask & 2 or y == 0), "object operation unused coordinates invalid")
        _require(visible in {0, 1} and (code == 3 or visible == 0), "object operation visibility invalid")
        if code == 4:
            _frame(frame, objects[obj_index], assets)
        else:
            _require(frame == NONE, "object operation unused frame invalid")
        _require(code != 5 or objects[obj_index]["kind"] == 1, "clear frame requires sprite")
        operations.append({"opcode": code, "axis_mask": mask, "object_index": obj_index,
                           "x": x, "y": y, "frame_index": frame, "visible": visible})
    return tuple(state_overrides), tuple(operations)


def _object_graph(payload, strings, audio_cues, operation_count):
    _require(len(payload) >= GRAPH_HEADER.size, "object graph truncated")
    header = GRAPH_HEADER.unpack_from(payload)
    variables, bindings, states, routes, sources, guards, operations = header[4:11]
    _require(variables <= 32 and bindings <= 48 and 1 <= states <= 64 and routes <= 144
             and sources <= 9216 and guards <= 1152 and operations <= 1152, "object graph count exceeds development bounds")
    graph = _parse_graph(payload, strings, 0, 0, len(audio_cues), object_operation_count=operation_count)
    _require(len(graph["inputs"]) <= 32 and len(graph["event_bindings"]) <= 16, "object graph binding limits exceeded")
    for records, key in ((graph["variables"], "variable_id"), (graph["bindings"], "binding_id"),
                         (graph["states"], "state_id"), (graph["routes"], "route_id")):
        ids = [record[key] for record in records]
        _require(len(ids) == len(set(ids)) and all(re.fullmatch(r"[a-z][a-z0-9_.-]{0,63}", name) for name in ids), "duplicate or invalid graph ID")
    route_offset = GRAPH_HEADER.size + variables * VARIABLE_RECORD.size + bindings * EVENT_RECORD.size + states * STATE_RECORD.size
    guard_offset = route_offset + routes * ROUTE_RECORD_V2.size + sources * 2
    totals = [0, 0, 0]
    for index in range(routes):
        record = ROUTE_RECORD_V2.unpack_from(payload, route_offset + index * ROUTE_RECORD_V2.size)
        for slot, (first, count) in enumerate(zip(record[4::2], record[5::2])):
            _require(first == totals[slot] and count <= (64 if slot == 0 else 8), "graph ranges must partition records")
            totals[slot] += count
    _require(totals == [sources, guards, operations], "orphan graph records")
    for index in range(guards):
        record = GUARD_RECORD.unpack_from(payload, guard_offset + index * GUARD_RECORD.size)
        _require(record[2] == 0, "guard reserved field is nonzero")
    refs = [op["object_operation_index"] for route in graph["routes"] for op in route["operations"] if op["kind"] == 12]
    _require(refs == list(range(operation_count)), "object operations must be referenced once in order")
    return graph


def _legacy_links(scene, assets):
    frames = {frame["frame_id"]: frame for frame in assets}

    def check_frame(ref, element):
        _require(element["kind"] == 1 and ref in frames, "legacy sprite frame reference invalid")
        frame = frames[ref]
        _require((frame["width"], frame["height"]) == (element["width"], element["height"]), "legacy sprite frame dimensions mismatch")

    models, waits, graph = scene["render_models"], scene["waiting_visuals"], scene["graph"]
    for model in models:
        ids = [element["element_id"] for element in model["elements"]]
        _require(len(ids) == len(set(ids)), "duplicate legacy element ID")
        for element in model["elements"]:
            if element["kind"] == 1:
                check_frame(element["visual_ref"], element)
    for state in graph["states"]:
        elements = {item["element_id"]: item for item in models[state["render_model_index"]]["elements"]}
        for track in waits[state["waiting_visual_index"]]["elements"]:
            _require(track["source_element_ref"] in elements, "legacy waiting source missing")
            for ref in track["phase_visual_refs"]:
                check_frame(ref, elements[track["source_element_ref"]])
    for route in graph["routes"]:
        for operation in route["operations"]:
            if operation["kind"] not in {3, 4, 5, 6}:
                continue
            target = route["target_state_index"]
            _require(target is not None, "legacy element operation requires state")
            elements = models[graph["states"][target]["render_model_index"]]["elements"]
            _require(operation["element_index"] < len(elements), "legacy element operation target invalid")
            element = elements[operation["element_index"]]
            if operation["kind"] == 4:
                _position(element, operation["x"], operation["y"])
            elif operation["kind"] == 5:
                check_frame(operation["frame_ref"], element)
            elif operation["kind"] == 6:
                track = next((item for item in waits[operation["waiting_visual_index"]]["elements"]
                              if item["element_id"] == operation["waiting_element_ref"]), None)
                _require(track is not None and track["source_element_ref"] == element["element_id"], "legacy waiting operation target invalid")
                for ref in track["phase_visual_refs"]:
                    check_frame(ref, element)


def parse_object_package(blob, chunks, strings, manifest, manifest_index, package_id_hash, audio_assets, audio_cues):
    """Called only after the shared container integrity/bounds checks pass."""
    asset_index = next((i for i, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_ASSET_TABLE), None)
    if asset_index is None:
        assets, animations = (), ()
    else:
        assets, animations = _parse_assets(chunks, asset_index,
            next(i for i, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_MASKED_1BPP_SPRITE_BANK),
            next(i for i, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_ANIMATION_TABLE), strings)
    payload = next(chunk.payload for chunk in chunks if chunk.chunk_type == CHUNK_SCENE_TABLE)
    _require(len(payload) >= SCENE_HEADER.size, "V2 scene table truncated")
    header = SCENE_HEADER.unpack_from(payload)
    _require(header[:3] == (b"SCN2", 2, SCENE_HEADER.size) and header[4] == 0, "V2 scene header invalid")
    count = header[3]
    _require(1 <= count <= 32 and count == manifest["scene_count"] and len(payload) == SCENE_HEADER.size + count * SCENE_RECORD.size, "V2 scene count/size invalid")
    _require(len(chunks) == 3 + count * 3 + (3 if asset_index is not None else 0) + (3 if audio_assets else 0), "V2 chunk count mismatch")
    _require(manifest["flags"] == 0, "V2 manifest flags invalid")
    scenes, ids, used = [], set(), set()
    for index in range(count):
        name, display, kind, entry, graph_index, visual_index, control_index, model, reserved = SCENE_RECORD.unpack_from(payload, SCENE_HEADER.size + index * SCENE_RECORD.size)
        scene_id = _stable_id(strings, name, "scene ID")
        _require(scene_id not in ids and kind == 1 and model in {1, 2} and reserved == 0, "V2 scene identity/model invalid")
        ids.add(scene_id)
        indexes = (graph_index, visual_index, control_index)
        _require(len(set(indexes)) == 3 and not used.intersection(indexes) and max(indexes) < len(chunks), "scene chunks are shared or invalid")
        used.update(indexes)
        types = ((CHUNK_STATE_GRAPH, CHUNK_RENDER_MODELS, CHUNK_WAITING_VISUALS) if model == 1
                 else (CHUNK_STATE_GRAPH, CHUNK_SCENE_OBJECTS, CHUNK_OBJECT_CONTROLS))
        _require(tuple(chunks[i].chunk_type for i in indexes) == types, "scene chunk types do not match model")
        scene = {"scene_id": scene_id, "display_name": _string(strings, display, "scene display name"),
                 "scene_type": kind, "entry_state": entry, "execution_model": model, "flags": 0}
        if model == 2:
            objects = _objects(chunks[visual_index].payload, strings, assets, animations)
            overrides, operations = _controls(chunks[control_index].payload, objects, assets)
            graph = _object_graph(chunks[graph_index].payload, strings, audio_cues, len(operations))
            _require(len(overrides) == graph["state_count"], "object controls state count mismatch")
            scene.update(objects=objects, state_overrides=overrides, object_operations=operations)
        else:
            render = _parse_render(chunks[visual_index].payload, strings)
            wait = _parse_wait(chunks[control_index].payload, strings)
            graph = _parse_graph(chunks[graph_index].payload, strings, render["model_count"], wait["waiting_count"], len(audio_cues))
            scene.update(render_format_version=render["format_version"], render_models=render["models"], waiting_visuals=wait["waiting_visuals"])
        _require(entry == graph["entry_state"], "scene entry does not match graph")
        scene.update(graph=graph, state_count=graph["state_count"], route_count=graph["route_count"])
        if model == 1:
            _legacy_links(scene, assets)
        scenes.append(scene)
    _require(any(scene["execution_model"] == 2 for scene in scenes), "V2 requires an object scene")
    scene_types = {CHUNK_STATE_GRAPH, CHUNK_RENDER_MODELS, CHUNK_WAITING_VISUALS, CHUNK_SCENE_OBJECTS, CHUNK_OBJECT_CONTROLS}
    _require(used == {i for i, chunk in enumerate(chunks) if chunk.chunk_type in scene_types}, "unreferenced scene chunks")
    _require(manifest["entry_scene"] in ids, "entry scene is missing")
    for scene in scenes:
        for route in scene["graph"]["routes"]:
            target = route["target_scene"]
            _require(target is None or target in ids and target != scene["scene_id"], "scene target is missing or self-referential")
    return EggPackage(package_id_hash, 0, manifest_index, strings, manifest, tuple(scenes),
                      assets, animations, audio_assets, audio_cues, tuple(chunks), hashlib.sha256(blob).hexdigest())


def parse_development_egg_v2(blob: bytes) -> EggPackage:
    return parse_egg(blob, _development_v2=True)
