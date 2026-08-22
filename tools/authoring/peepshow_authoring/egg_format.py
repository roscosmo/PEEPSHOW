"""Concrete deterministic PKG1 container writer and independent reader."""

from __future__ import annotations

import hashlib
import struct
import zlib
from dataclasses import dataclass
from typing import Iterable


ALIGNMENT = 4
CONTAINER_VERSION = 1
HEADER = struct.Struct("<4sHHIIIHHHHIIQI16s")
CHUNK_ENTRY = struct.Struct("<HHIQIIIHHQ")
FOOTER = struct.Struct("<4sHH32s")
STRING_HEADER = struct.Struct("<4sHHI")
MANIFEST = struct.Struct("<4s10HI")
SCENE_HEADER = struct.Struct("<4s4H")
SCENE_RECORD = struct.Struct("<8HI")
GRAPH_HEADER = struct.Struct("<4s18H")
VARIABLE_RECORD = struct.Struct("<HBBiii")
INPUT_RECORD = struct.Struct("<HH")
STATE_RECORD = struct.Struct("<4H")
ROUTE_RECORD = struct.Struct("<9H")
GUARD_RECORD = struct.Struct("<HBBi")
OPERATION_RECORD = struct.Struct("<BBHHHi")
RENDER_HEADER = struct.Struct("<4s6H")
RENDER_MODEL_RECORD = struct.Struct("<4H")
RENDER_ELEMENT_RECORD = struct.Struct("<HHBBhhHHH")
WAIT_HEADER = struct.Struct("<4s10H")
WAIT_RECORD = struct.Struct("<8H")
WAIT_ELEMENT_RECORD = struct.Struct("<6H")
HEADER_CRC_OFFSET = 44

CHUNK_MANIFEST = 1
CHUNK_STRING_TABLE = 2
CHUNK_SCENE_TABLE = 3
CHUNK_STATE_GRAPH = 4
CHUNK_RENDER_MODELS = 5
CHUNK_WAITING_VISUALS = 6
KNOWN_CHUNK_TYPES = {
    CHUNK_MANIFEST,
    CHUNK_STRING_TABLE,
    CHUNK_SCENE_TABLE,
    CHUNK_STATE_GRAPH,
    CHUNK_RENDER_MODELS,
    CHUNK_WAITING_VISUALS,
}


class EggFormatError(ValueError):
    """Raised when a package violates the frozen PKG1 format."""


@dataclass(frozen=True)
class EggChunkSpec:
    chunk_id: str
    chunk_type: int
    payload: bytes
    format_version: int = 1
    flags: int = 0
    required_capability_hash: int = 0


@dataclass(frozen=True)
class EggChunk:
    chunk_type: int
    format_version: int
    flags: int
    chunk_id_hash: int
    offset: int
    size: int
    crc32: int
    alignment: int
    required_capability_hash: int
    payload: bytes


@dataclass(frozen=True)
class EggPackage:
    package_id_hash: int
    package_flags: int
    manifest_chunk_index: int
    strings: tuple[str, ...]
    manifest: dict[str, int | str]
    scenes: tuple[dict[str, int | str], ...]
    chunks: tuple[EggChunk, ...]
    sha256: str


def align_up(value: int, alignment: int = ALIGNMENT) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def fnv1a64(value: str) -> int:
    result = 0xCBF29CE484222325
    for byte in value.encode("utf-8"):
        result ^= byte
        result = (result * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return result


def build_container(
    package_id: str,
    chunks: Iterable[EggChunkSpec],
    manifest_chunk_index: int,
    package_flags: int = 0,
) -> bytes:
    chunk_list = tuple(chunks)
    if not chunk_list or len(chunk_list) > 128:
        raise EggFormatError("chunk count must be in 1..128")
    if not 0 <= manifest_chunk_index < len(chunk_list):
        raise EggFormatError("manifest chunk index is outside the chunk table")
    if chunk_list[manifest_chunk_index].chunk_type != CHUNK_MANIFEST:
        raise EggFormatError("manifest chunk index does not select a manifest")
    hashes = [fnv1a64(chunk.chunk_id) for chunk in chunk_list]
    if len(set(hashes)) != len(hashes):
        raise EggFormatError("chunk ID hash collision")

    table_offset = HEADER.size
    cursor = align_up(table_offset + len(chunk_list) * CHUNK_ENTRY.size)
    placements: list[tuple[int, EggChunkSpec]] = []
    for chunk in chunk_list:
        if chunk.chunk_type not in KNOWN_CHUNK_TYPES:
            raise EggFormatError(f"unknown chunk type {chunk.chunk_type}")
        if not chunk.payload:
            raise EggFormatError(f"chunk '{chunk.chunk_id}' is empty")
        cursor = align_up(cursor)
        placements.append((cursor, chunk))
        cursor += len(chunk.payload)
    footer_offset = align_up(cursor)
    package_size = footer_offset + FOOTER.size
    blob = bytearray(package_size)

    header_values = (
        b"PKG1",
        CONTAINER_VERSION,
        HEADER.size,
        package_size,
        table_offset,
        footer_offset,
        len(chunk_list),
        CHUNK_ENTRY.size,
        manifest_chunk_index,
        ALIGNMENT,
        package_flags,
        0,
        fnv1a64(package_id),
        0,
        bytes(16),
    )
    header = bytearray(HEADER.pack(*header_values))
    header_crc = zlib.crc32(header) & 0xFFFFFFFF
    struct.pack_into("<I", header, HEADER_CRC_OFFSET, header_crc)
    blob[: HEADER.size] = header

    for index, ((offset, chunk), chunk_hash) in enumerate(zip(placements, hashes)):
        crc = zlib.crc32(chunk.payload) & 0xFFFFFFFF
        entry = CHUNK_ENTRY.pack(
            chunk.chunk_type,
            chunk.format_version,
            chunk.flags,
            chunk_hash,
            offset,
            len(chunk.payload),
            crc,
            ALIGNMENT,
            0,
            chunk.required_capability_hash,
        )
        entry_offset = table_offset + index * CHUNK_ENTRY.size
        blob[entry_offset : entry_offset + CHUNK_ENTRY.size] = entry
        blob[offset : offset + len(chunk.payload)] = chunk.payload

    digest = hashlib.sha256(blob[:footer_offset]).digest()
    blob[footer_offset:] = FOOTER.pack(b"END1", 1, FOOTER.size, digest)
    return bytes(blob)


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise EggFormatError(message)


def _string(strings: tuple[str, ...], index: int, field: str) -> str:
    _require(0 <= index < len(strings), f"{field} string index is out of range")
    return strings[index]


def _parse_strings(payload: bytes) -> tuple[str, ...]:
    _require(len(payload) >= STRING_HEADER.size, "string table is truncated")
    magic, version, count, byte_size = STRING_HEADER.unpack_from(payload)
    _require(magic == b"STR1" and version == 1, "unsupported string table")
    offsets_size = (count + 1) * 4
    data_offset = STRING_HEADER.size + offsets_size
    _require(data_offset + byte_size == len(payload), "string table size is inconsistent")
    offsets = struct.unpack_from(f"<{count + 1}I", payload, STRING_HEADER.size)
    _require(offsets[0] == 0 and offsets[-1] == byte_size, "string table bounds are invalid")
    _require(all(offsets[i] <= offsets[i + 1] for i in range(count)), "string offsets are not ordered")
    data = payload[data_offset:]
    strings: list[str] = []
    for index in range(count):
        try:
            strings.append(data[offsets[index] : offsets[index + 1]].decode("utf-8"))
        except UnicodeDecodeError as exc:
            raise EggFormatError(f"string {index} is not UTF-8") from exc
    _require(strings == sorted(set(strings)), "string table must be unique and sorted")
    return tuple(strings)


def _parse_manifest(payload: bytes, strings: tuple[str, ...]) -> dict[str, int | str]:
    _require(len(payload) == MANIFEST.size, "manifest size is invalid")
    values = MANIFEST.unpack(payload)
    _require(values[0] == b"MAN1" and values[1] == 1 and values[2] == MANIFEST.size, "unsupported manifest")
    return {
        "package_id": _string(strings, values[3], "manifest package_id"),
        "display_name": _string(strings, values[4], "manifest display_name"),
        "target_profile": _string(strings, values[5], "manifest target_profile"),
        "version_major": values[6],
        "version_minor": values[7],
        "version_patch": values[8],
        "entry_scene": _string(strings, values[9], "manifest entry_scene"),
        "scene_count": values[10],
        "flags": values[11],
    }


def _parse_render(payload: bytes, strings: tuple[str, ...]) -> dict[str, object]:
    _require(len(payload) >= RENDER_HEADER.size, "render chunk is truncated")
    values = RENDER_HEADER.unpack_from(payload)
    _require(values[0] == b"RND1" and values[1] == 1 and values[2] == RENDER_HEADER.size, "unsupported render chunk")
    model_count, element_count = values[3], values[4]
    expected = RENDER_HEADER.size + model_count * RENDER_MODEL_RECORD.size + element_count * RENDER_ELEMENT_RECORD.size
    _require(len(payload) == expected, "render chunk size is inconsistent")
    model_offset = RENDER_HEADER.size
    element_offset = model_offset + model_count * RENDER_MODEL_RECORD.size
    models: list[tuple[int, int]] = []
    for index in range(model_count):
        visual_id, focus, first_element, count = RENDER_MODEL_RECORD.unpack_from(
            payload, model_offset + index * RENDER_MODEL_RECORD.size
        )
        _string(strings, visual_id, "render visual_id")
        _require(first_element + count <= element_count, "render model element range is invalid")
        models.append((first_element, count))
    for index in range(element_count):
        record = RENDER_ELEMENT_RECORD.unpack_from(payload, element_offset + index * RENDER_ELEMENT_RECORD.size)
        _string(strings, record[0], "render element_id")
        _string(strings, record[1], "render visual_ref")
        _require(record[2] in {1, 2, 3}, "render element kind is invalid")
        _require(record[3] in {0, 1}, "render focus role is invalid")
        _require(record[6] > 0 and record[7] > 0, "render element dimensions are invalid")
    return {"model_count": model_count, "element_count": element_count, "models": tuple(models)}


def _parse_wait(payload: bytes, strings: tuple[str, ...]) -> dict[str, object]:
    _require(len(payload) >= WAIT_HEADER.size, "waiting-visual chunk is truncated")
    values = WAIT_HEADER.unpack_from(payload)
    _require(values[0] == b"WAI1" and values[1] == 1 and values[2] == WAIT_HEADER.size, "unsupported waiting-visual chunk")
    wait_count, element_count, phase_count, sequence_count = values[3:7]
    wait_offset = WAIT_HEADER.size
    element_offset = wait_offset + wait_count * WAIT_RECORD.size
    phase_offset = element_offset + element_count * WAIT_ELEMENT_RECORD.size
    sequence_offset = phase_offset + phase_count * 2
    _require(sequence_offset + sequence_count == len(payload), "waiting-visual chunk size is inconsistent")
    waits: list[int] = []
    for index in range(wait_count):
        record = WAIT_RECORD.unpack_from(payload, wait_offset + index * WAIT_RECORD.size)
        _string(strings, record[0], "waiting_visual_id")
        _string(strings, record[1], "waiting presentation_id")
        _require(record[2] > 0 and 1 <= record[3] <= 12, "waiting cadence or step count is invalid")
        _require(record[4] < record[3] and record[5] == 1, "waiting settled step or cycle policy is invalid")
        _require(record[6] + record[7] <= element_count, "waiting element range is invalid")
        waits.append(record[3])
    phase_refs = struct.unpack_from(f"<{phase_count}H", payload, phase_offset) if phase_count else ()
    for ref in phase_refs:
        _string(strings, ref, "waiting phase visual")
    sequence = payload[sequence_offset:]
    for index in range(element_count):
        record = WAIT_ELEMENT_RECORD.unpack_from(payload, element_offset + index * WAIT_ELEMENT_RECORD.size)
        _string(strings, record[0], "waiting element_id")
        _string(strings, record[1], "waiting source_element_ref")
        first_phase, count_phase, first_step, count_step = record[2:6]
        _require(1 <= count_phase <= 4 and first_phase + count_phase <= phase_count, "waiting phase range is invalid")
        _require(first_step + count_step <= sequence_count, "waiting sequence range is invalid")
        _require(all(value < count_phase for value in sequence[first_step : first_step + count_step]), "waiting phase index is invalid")
    return {"waiting_count": wait_count, "element_count": element_count, "step_counts": tuple(waits)}


def _parse_graph(
    payload: bytes,
    strings: tuple[str, ...],
    render_count: int,
    waiting_count: int,
) -> dict[str, int]:
    _require(len(payload) >= GRAPH_HEADER.size, "state graph is truncated")
    values = GRAPH_HEADER.unpack_from(payload)
    _require(values[0] == b"STG1" and values[1] == 1 and values[2] == GRAPH_HEADER.size, "unsupported state graph")
    (
        entry_state,
        variable_count,
        input_count,
        state_count,
        route_count,
        source_count,
        guard_count,
        operation_count,
        wait_policy_id,
        default_waiting,
        hold_fallback,
        event_count,
        interaction_policy_id,
        inactive_route,
        meaningful_count,
        reserved,
    ) = values[3:]
    _require(reserved == 0 and hold_fallback in {0, 1} and inactive_route in {1, 2}, "state policy fields are invalid")
    _string(strings, wait_policy_id, "wait policy ID")
    _string(strings, interaction_policy_id, "interaction policy ID")
    _require(entry_state < state_count and default_waiting < waiting_count, "state graph entry or wait reference is invalid")
    offsets = [GRAPH_HEADER.size]
    sizes = (
        variable_count * VARIABLE_RECORD.size,
        input_count * INPUT_RECORD.size,
        state_count * STATE_RECORD.size,
        route_count * ROUTE_RECORD.size,
        source_count * 2,
        guard_count * GUARD_RECORD.size,
        operation_count * OPERATION_RECORD.size,
        event_count * 2,
        meaningful_count * 2,
    )
    for size in sizes:
        offsets.append(offsets[-1] + size)
    _require(offsets[-1] == len(payload), "state graph size is inconsistent")

    for index in range(variable_count):
        record = VARIABLE_RECORD.unpack_from(payload, offsets[0] + index * VARIABLE_RECORD.size)
        _string(strings, record[0], "variable ID")
        _require(
            record[1] == 1 and record[2] == 0 and record[4] <= record[3] <= record[5],
            "variable record is invalid",
        )
    for index in range(input_count):
        record = INPUT_RECORD.unpack_from(payload, offsets[1] + index * INPUT_RECORD.size)
        _string(strings, record[0], "input action ID")
        _require(record[1] in {1, 2, 3, 4}, "input source is invalid")
    state_refs: list[tuple[int, int]] = []
    for index in range(state_count):
        record = STATE_RECORD.unpack_from(payload, offsets[2] + index * STATE_RECORD.size)
        _string(strings, record[0], "state ID")
        _string(strings, record[1], "state display name")
        _require(record[2] < render_count and record[3] < waiting_count, "state visual reference is invalid")
        state_refs.append((record[2], record[3]))
    source_states = struct.unpack_from(f"<{source_count}H", payload, offsets[4]) if source_count else ()
    _require(all(ref < state_count for ref in source_states), "route source-state index is invalid")
    for index in range(route_count):
        record = ROUTE_RECORD.unpack_from(payload, offsets[3] + index * ROUTE_RECORD.size)
        _string(strings, record[0], "route ID")
        _require(record[1] < input_count and record[2] < state_count, "route action or target is invalid")
        _require(record[3] + record[4] <= source_count, "route source range is invalid")
        _require(record[5] + record[6] <= guard_count, "route guard range is invalid")
        _require(record[7] + record[8] <= operation_count, "route operation range is invalid")
    for index in range(guard_count):
        record = GUARD_RECORD.unpack_from(payload, offsets[5] + index * GUARD_RECORD.size)
        _require(record[0] < variable_count and record[1] in {1, 2, 3, 4, 5, 6}, "guard record is invalid")
    for index in range(operation_count):
        record = OPERATION_RECORD.unpack_from(payload, offsets[6] + index * OPERATION_RECORD.size)
        if record[0] == 1:
            _require(record[1] in {1, 2} and record[2] < variable_count, "set-variable operation is invalid")
        else:
            _require(record[0] == 2 and record[1] == 0 and record[2] == 0xFFFF, "operation record is invalid")
    event_refs = struct.unpack_from(f"<{event_count}H", payload, offsets[7]) if event_count else ()
    meaningful_refs = struct.unpack_from(f"<{meaningful_count}H", payload, offsets[8]) if meaningful_count else ()
    _require(all(ref < input_count for ref in event_refs + meaningful_refs), "policy input-action index is invalid")
    return {
        "entry_state": entry_state,
        "variable_count": variable_count,
        "input_count": input_count,
        "state_count": state_count,
        "route_count": route_count,
    }


def parse_egg(blob: bytes) -> EggPackage:
    _require(len(blob) >= HEADER.size + FOOTER.size, "package is truncated")
    values = HEADER.unpack_from(blob)
    (
        magic,
        version,
        header_size,
        package_size,
        table_offset,
        footer_offset,
        chunk_count,
        entry_size,
        manifest_index,
        alignment,
        package_flags,
        reserved,
        package_id_hash,
        header_crc,
        reserved_bytes,
    ) = values
    _require(magic == b"PKG1" and version == 1, "unsupported package container")
    _require(header_size == HEADER.size and entry_size == CHUNK_ENTRY.size, "container record size is invalid")
    _require(package_size == len(blob), "package size does not match the file")
    _require(table_offset == HEADER.size and alignment == ALIGNMENT, "container offsets or alignment are invalid")
    _require(1 <= chunk_count <= 128 and manifest_index < chunk_count, "chunk count or manifest index is invalid")
    _require(reserved == 0 and reserved_bytes == bytes(16), "header reserved fields are not zero")
    header_copy = bytearray(blob[:HEADER.size])
    struct.pack_into("<I", header_copy, HEADER_CRC_OFFSET, 0)
    _require((zlib.crc32(header_copy) & 0xFFFFFFFF) == header_crc, "header CRC mismatch")
    table_end = table_offset + chunk_count * CHUNK_ENTRY.size
    _require(table_end <= footer_offset and footer_offset + FOOTER.size == len(blob), "container bounds are invalid")
    footer_magic, footer_version, footer_size, digest = FOOTER.unpack_from(blob, footer_offset)
    _require(footer_magic == b"END1" and footer_version == 1 and footer_size == FOOTER.size, "integrity footer is invalid")
    _require(hashlib.sha256(blob[:footer_offset]).digest() == digest, "package SHA-256 mismatch")

    chunks: list[EggChunk] = []
    ranges: list[tuple[int, int]] = []
    chunk_hashes: set[int] = set()
    for index in range(chunk_count):
        entry = CHUNK_ENTRY.unpack_from(blob, table_offset + index * CHUNK_ENTRY.size)
        chunk_type, format_version, flags, chunk_hash, offset, size, crc, chunk_alignment, chunk_reserved, capability = entry
        _require(chunk_type in KNOWN_CHUNK_TYPES and format_version == 1, f"chunk {index} type or version is unsupported")
        _require(chunk_alignment == ALIGNMENT and offset % chunk_alignment == 0, f"chunk {index} alignment is invalid")
        _require(chunk_reserved == 0 and size > 0, f"chunk {index} reserved field or size is invalid")
        _require(offset >= align_up(table_end) and offset + size <= footer_offset, f"chunk {index} is outside payload bounds")
        _require(chunk_hash not in chunk_hashes, f"chunk {index} ID hash is duplicated")
        chunk_hashes.add(chunk_hash)
        payload = blob[offset : offset + size]
        _require((zlib.crc32(payload) & 0xFFFFFFFF) == crc, f"chunk {index} CRC mismatch")
        ranges.append((offset, offset + size))
        chunks.append(EggChunk(chunk_type, format_version, flags, chunk_hash, offset, size, crc, chunk_alignment, capability, payload))
    ordered_ranges = sorted(ranges)
    _require(all(ordered_ranges[i][1] <= ordered_ranges[i + 1][0] for i in range(len(ordered_ranges) - 1)), "chunk payloads overlap")
    _require(chunks[manifest_index].chunk_type == CHUNK_MANIFEST, "manifest index selects the wrong chunk type")

    string_chunks = [chunk for chunk in chunks if chunk.chunk_type == CHUNK_STRING_TABLE]
    manifest_chunks = [chunk for chunk in chunks if chunk.chunk_type == CHUNK_MANIFEST]
    scene_chunks = [chunk for chunk in chunks if chunk.chunk_type == CHUNK_SCENE_TABLE]
    _require(len(string_chunks) == len(manifest_chunks) == len(scene_chunks) == 1, "core chunk cardinality is invalid")
    strings = _parse_strings(string_chunks[0].payload)
    manifest = _parse_manifest(manifest_chunks[0].payload, strings)
    _require(fnv1a64(str(manifest["package_id"])) == package_id_hash, "package ID hash does not match manifest")

    scene_payload = scene_chunks[0].payload
    _require(len(scene_payload) >= SCENE_HEADER.size, "scene table is truncated")
    scene_header = SCENE_HEADER.unpack_from(scene_payload)
    _require(scene_header[0] == b"SCN1" and scene_header[1] == 1 and scene_header[2] == SCENE_HEADER.size, "unsupported scene table")
    scene_count = scene_header[3]
    _require(scene_header[4] == 0 and len(scene_payload) == SCENE_HEADER.size + scene_count * SCENE_RECORD.size, "scene table size is inconsistent")
    _require(scene_count == manifest["scene_count"] and chunk_count == 3 + scene_count * 3, "scene/chunk count is inconsistent")
    scenes: list[dict[str, int | str]] = []
    used_scene_chunks: set[int] = set()
    for index in range(scene_count):
        record = SCENE_RECORD.unpack_from(scene_payload, SCENE_HEADER.size + index * SCENE_RECORD.size)
        scene_id, display_name, scene_type, entry_state, graph_index, render_index, wait_index, flags, scene_reserved = record
        _require(scene_type == 1 and scene_reserved == 0, "scene type or reserved field is invalid")
        _require(max(graph_index, render_index, wait_index) < chunk_count, "scene chunk index is invalid")
        _require(chunks[graph_index].chunk_type == CHUNK_STATE_GRAPH, "scene graph chunk type is invalid")
        _require(chunks[render_index].chunk_type == CHUNK_RENDER_MODELS, "scene render chunk type is invalid")
        _require(chunks[wait_index].chunk_type == CHUNK_WAITING_VISUALS, "scene wait chunk type is invalid")
        used_scene_chunks.update((graph_index, render_index, wait_index))
        render_summary = _parse_render(chunks[render_index].payload, strings)
        wait_summary = _parse_wait(chunks[wait_index].payload, strings)
        graph_summary = _parse_graph(
            chunks[graph_index].payload,
            strings,
            int(render_summary["model_count"]),
            int(wait_summary["waiting_count"]),
        )
        _require(entry_state == graph_summary["entry_state"], "scene entry state does not match graph")
        scenes.append(
            {
                "scene_id": _string(strings, scene_id, "scene ID"),
                "display_name": _string(strings, display_name, "scene display name"),
                "scene_type": scene_type,
                "entry_state": entry_state,
                "state_count": graph_summary["state_count"],
                "route_count": graph_summary["route_count"],
                "flags": flags,
            }
        )
    expected_scene_chunks = {index for index, chunk in enumerate(chunks) if chunk.chunk_type in {CHUNK_STATE_GRAPH, CHUNK_RENDER_MODELS, CHUNK_WAITING_VISUALS}}
    _require(used_scene_chunks == expected_scene_chunks, "unreferenced or multiply purposed scene chunks exist")
    _require(str(manifest["entry_scene"]) in {str(scene["scene_id"]) for scene in scenes}, "manifest entry scene is missing")
    return EggPackage(
        package_id_hash=package_id_hash,
        package_flags=package_flags,
        manifest_chunk_index=manifest_index,
        strings=strings,
        manifest=manifest,
        scenes=tuple(scenes),
        chunks=tuple(chunks),
        sha256=hashlib.sha256(blob).hexdigest(),
    )
