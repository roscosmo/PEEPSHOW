"""Concrete deterministic PKG1 container writer and independent reader."""

from __future__ import annotations

import hashlib
import struct
import zlib
from dataclasses import dataclass
from typing import Iterable

from .audio_assets import (
    AUDIO_BLOCK_SAMPLES,
    AUDIO_CHANNELS,
    AUDIO_SAMPLE_RATE_HZ,
    AudioAssetError,
    decode_ima_adpcm,
)

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
ROUTE_RECORD_V1 = struct.Struct("<9H")
ROUTE_RECORD_V2 = struct.Struct("<10H")
ROUTE_RECORD = ROUTE_RECORD_V2
GUARD_RECORD = struct.Struct("<HBBi")
OPERATION_RECORD = struct.Struct("<BBHHHi")
RENDER_HEADER = struct.Struct("<4s6H")
RENDER_MODEL_RECORD = struct.Struct("<4H")
RENDER_ELEMENT_RECORD_V1 = struct.Struct("<HHBBhhHHH")
RENDER_ELEMENT_RECORD_V2 = struct.Struct("<HHBBBBhhHHHH")
RENDER_ELEMENT_RECORD = RENDER_ELEMENT_RECORD_V2
WAIT_HEADER = struct.Struct("<4s10H")
WAIT_RECORD = struct.Struct("<8H")
WAIT_ELEMENT_RECORD = struct.Struct("<6H")
ASSET_HEADER = struct.Struct("<4s8H")
ASSET_RECORD = struct.Struct("<6H2h5I")
SPRITE_BANK_HEADER = struct.Struct("<4sHHII")
ANIMATION_HEADER = struct.Struct("<4s6H")
ANIMATION_RECORD = struct.Struct("<8H")
AUDIO_ASSET_HEADER = struct.Struct("<4s8H")
AUDIO_ASSET_RECORD = struct.Struct("<HH8I")
AUDIO_BANK_HEADER = struct.Struct("<4sHHII")
AUDIO_CUE_HEADER = struct.Struct("<4s6H")
AUDIO_CUE_RECORD = struct.Struct("<6H")
HEADER_CRC_OFFSET = 44

CHUNK_MANIFEST = 1
CHUNK_STRING_TABLE = 2
CHUNK_SCENE_TABLE = 3
CHUNK_STATE_GRAPH = 4
CHUNK_RENDER_MODELS = 5
CHUNK_WAITING_VISUALS = 6
CHUNK_ASSET_TABLE = 7
CHUNK_MASKED_1BPP_SPRITE_BANK = 8
CHUNK_ANIMATION_TABLE = 9
CHUNK_AUDIO_ASSET_TABLE = 10
CHUNK_AUDIO_ADPCM_BANK = 11
CHUNK_AUDIO_CUE_TABLE = 12
KNOWN_CHUNK_TYPES = {
    CHUNK_MANIFEST,
    CHUNK_STRING_TABLE,
    CHUNK_SCENE_TABLE,
    CHUNK_STATE_GRAPH,
    CHUNK_RENDER_MODELS,
    CHUNK_WAITING_VISUALS,
    CHUNK_ASSET_TABLE,
    CHUNK_MASKED_1BPP_SPRITE_BANK,
    CHUNK_ANIMATION_TABLE,
    CHUNK_AUDIO_ASSET_TABLE,
    CHUNK_AUDIO_ADPCM_BANK,
    CHUNK_AUDIO_CUE_TABLE,
}

ASSET_FLAG_OPAQUE = 1
ANIMATION_LOOP_POLICIES = {1, 2, 3, 4}


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
    scenes: tuple[dict[str, object], ...]
    assets: tuple[dict[str, object], ...]
    animations: tuple[dict[str, object], ...]
    audio_assets: tuple[dict[str, object], ...]
    audio_cues: tuple[dict[str, object], ...]
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


def _plane_bytes(payload: bytes, offset: int, size: int, minimum_offset: int, field: str) -> bytes:
    _require(size > 0, f"{field} size is zero")
    _require(offset >= minimum_offset and offset + size <= len(payload), f"{field} is outside sprite-bank bounds")
    return payload[offset : offset + size]


def _require_zero_padding(data: bytes, width: int, height: int, stride: int, field: str) -> None:
    remainder = width % 8
    if remainder == 0:
        return
    padding_mask = (1 << (8 - remainder)) - 1
    for row in range(height):
        _require(data[row * stride + stride - 1] & padding_mask == 0, f"{field} padding bits are not zero")


def _parse_assets(
    chunks: tuple[EggChunk, ...] | list[EggChunk],
    asset_index: int,
    sprite_index: int,
    animation_index: int,
    strings: tuple[str, ...],
) -> tuple[tuple[dict[str, object], ...], tuple[dict[str, object], ...]]:
    asset_payload = chunks[asset_index].payload
    sprite_payload = chunks[sprite_index].payload
    animation_payload = chunks[animation_index].payload

    _require(len(sprite_payload) >= SPRITE_BANK_HEADER.size, "sprite bank is truncated")
    sprite_header = SPRITE_BANK_HEADER.unpack_from(sprite_payload)
    _require(
        sprite_header[0] == b"SPB1" and sprite_header[1] == 1 and sprite_header[2] == SPRITE_BANK_HEADER.size,
        "unsupported sprite bank",
    )
    _require(sprite_header[3] == len(sprite_payload) - SPRITE_BANK_HEADER.size and sprite_header[4] == 0, "sprite bank size is inconsistent")

    _require(len(asset_payload) >= ASSET_HEADER.size, "asset table is truncated")
    asset_header = ASSET_HEADER.unpack_from(asset_payload)
    _require(
        asset_header[0] == b"AST1" and asset_header[1] == 1 and asset_header[2] == ASSET_HEADER.size,
        "unsupported asset table",
    )
    frame_count = asset_header[3]
    _require(asset_header[4] == sprite_index and asset_header[5] == animation_index, "asset table chunk indexes are invalid")
    _require(asset_header[6:] == (0, 0, 0), "asset table reserved fields are not zero")
    _require(len(asset_payload) == ASSET_HEADER.size + frame_count * ASSET_RECORD.size, "asset table size is inconsistent")

    assets: list[dict[str, object]] = []
    frame_names: set[str] = set()
    for index in range(frame_count):
        record = ASSET_RECORD.unpack_from(asset_payload, ASSET_HEADER.size + index * ASSET_RECORD.size)
        (
            asset_id_index,
            frame_id_index,
            width,
            height,
            stride,
            reserved,
            pivot_x,
            pivot_y,
            pixel_offset,
            pixel_size,
            mask_offset,
            mask_size,
            flags,
        ) = record
        asset_id = _string(strings, asset_id_index, "asset ID")
        frame_id = _string(strings, frame_id_index, "frame ID")
        _require(frame_id not in frame_names, "frame ID is duplicated")
        frame_names.add(frame_id)
        _require(reserved == 0 and flags & ~ASSET_FLAG_OPAQUE == 0, "asset record flags or reserved field are invalid")
        _require(1 <= width <= 168 and 1 <= height <= 144, "asset frame dimensions are invalid")
        _require(stride == (width + 7) // 8, "asset frame stride is invalid")
        expected_size = stride * height
        _require(pixel_size == expected_size, "asset pixel size is invalid")
        pixels = _plane_bytes(sprite_payload, pixel_offset, pixel_size, SPRITE_BANK_HEADER.size, "asset pixels")
        _require_zero_padding(pixels, width, height, stride, "asset pixels")
        if flags & ASSET_FLAG_OPAQUE:
            _require(mask_offset == 0 and mask_size == 0, "opaque asset must omit its mask")
            mask = b""
        else:
            _require(mask_size == expected_size, "asset mask size is invalid")
            mask = _plane_bytes(sprite_payload, mask_offset, mask_size, SPRITE_BANK_HEADER.size, "asset mask")
            _require_zero_padding(mask, width, height, stride, "asset mask")
        assets.append(
            {
                "asset_id": asset_id,
                "frame_id": frame_id,
                "width": width,
                "height": height,
                "row_stride_bytes": stride,
                "pivot_x": pivot_x,
                "pivot_y": pivot_y,
                "opaque": bool(flags & ASSET_FLAG_OPAQUE),
                "pixels": pixels,
                "mask": mask,
            }
        )

    _require(len(animation_payload) >= ANIMATION_HEADER.size, "animation table is truncated")
    animation_header = ANIMATION_HEADER.unpack_from(animation_payload)
    _require(
        animation_header[0] == b"ANI1" and animation_header[1] == 1 and animation_header[2] == ANIMATION_HEADER.size,
        "unsupported animation table",
    )
    animation_count, frame_ref_count, duration_count, animation_reserved = animation_header[3:7]
    _require(animation_reserved == 0 and frame_ref_count == duration_count, "animation header is invalid")
    record_offset = ANIMATION_HEADER.size
    frame_offset = record_offset + animation_count * ANIMATION_RECORD.size
    duration_offset = frame_offset + frame_ref_count * 2
    _require(duration_offset + duration_count * 4 == len(animation_payload), "animation table size is inconsistent")
    frame_refs = struct.unpack_from(f"<{frame_ref_count}H", animation_payload, frame_offset) if frame_ref_count else ()
    durations = struct.unpack_from(f"<{duration_count}I", animation_payload, duration_offset) if duration_count else ()

    animations: list[dict[str, object]] = []
    animation_names: set[str] = set()
    for index in range(animation_count):
        record = ANIMATION_RECORD.unpack_from(animation_payload, record_offset + index * ANIMATION_RECORD.size)
        animation_id_index, first_frame, count, first_duration, loop_policy, width, height, reserved = record
        animation_id = _string(strings, animation_id_index, "animation ID")
        _require(animation_id not in animation_names, "animation ID is duplicated")
        animation_names.add(animation_id)
        _require(reserved == 0 and loop_policy in ANIMATION_LOOP_POLICIES, "animation record is invalid")
        _require(1 <= count and first_frame + count <= frame_ref_count, "animation frame range is invalid")
        _require(first_duration + count <= duration_count, "animation duration range is invalid")
        selected_frames = frame_refs[first_frame : first_frame + count]
        selected_durations = durations[first_duration : first_duration + count]
        _require(all(frame < frame_count for frame in selected_frames), "animation references an unknown frame")
        _require(all(1 <= duration <= 60000 for duration in selected_durations), "animation duration is invalid")
        expected_width = max(int(assets[frame]["width"]) for frame in selected_frames)
        expected_height = max(int(assets[frame]["height"]) for frame in selected_frames)
        _require(width == expected_width and height == expected_height, "animation bounds are invalid")
        animations.append(
            {
                "animation_id": animation_id,
                "frame_indexes": tuple(selected_frames),
                "frame_duration_ms": tuple(selected_durations),
                "loop_policy": loop_policy,
                "width": width,
                "height": height,
            }
        )
    return tuple(assets), tuple(animations)


def _parse_audio(
    chunks: list[EggChunk],
    asset_index: int,
    bank_index: int,
    cue_index: int,
    strings: tuple[str, ...],
) -> tuple[tuple[dict[str, object], ...], tuple[dict[str, object], ...]]:
    asset_payload = chunks[asset_index].payload
    bank_payload = chunks[bank_index].payload
    cue_payload = chunks[cue_index].payload
    _require(len(asset_payload) >= AUDIO_ASSET_HEADER.size, "audio asset table is truncated")
    header = AUDIO_ASSET_HEADER.unpack_from(asset_payload)
    _require(
        header[0] == b"AUD1"
        and header[1] == 1
        and header[2] == AUDIO_ASSET_HEADER.size,
        "unsupported audio asset table",
    )
    asset_count, selected_bank, sample_rate, channels, block_samples, reserved = header[3:9]
    _require(
        selected_bank == bank_index
        and sample_rate == AUDIO_SAMPLE_RATE_HZ
        and channels == AUDIO_CHANNELS
        and block_samples == AUDIO_BLOCK_SAMPLES
        and reserved == 0,
        "audio asset header is invalid",
    )
    _require(
        len(asset_payload) == AUDIO_ASSET_HEADER.size + asset_count * AUDIO_ASSET_RECORD.size,
        "audio asset table size is inconsistent",
    )
    _require(len(bank_payload) >= AUDIO_BANK_HEADER.size, "audio ADPCM bank is truncated")
    bank_header = AUDIO_BANK_HEADER.unpack_from(bank_payload)
    _require(
        bank_header[0] == b"ADB1"
        and bank_header[1] == 1
        and bank_header[2] == AUDIO_BANK_HEADER.size
        and bank_header[3] == len(bank_payload) - AUDIO_BANK_HEADER.size
        and bank_header[4] == 0,
        "audio ADPCM bank header is invalid",
    )

    assets: list[dict[str, object]] = []
    asset_ids: set[str] = set()
    ranges: list[tuple[int, int]] = []
    for index in range(asset_count):
        record = AUDIO_ASSET_RECORD.unpack_from(
            asset_payload, AUDIO_ASSET_HEADER.size + index * AUDIO_ASSET_RECORD.size
        )
        (
            asset_id_index,
            flags,
            sample_count,
            duration_ms,
            decoded_pcm_bytes,
            data_offset,
            data_size,
            block_count,
            payload_crc,
            record_reserved,
        ) = record
        asset_id = _string(strings, asset_id_index, "audio asset ID")
        _require(asset_id not in asset_ids, "audio asset ID is duplicated")
        asset_ids.add(asset_id)
        _require(flags == 0 and record_reserved == 0, "audio asset flags are invalid")
        _require(
            sample_count > 0
            and duration_ms == (sample_count * 1000 + AUDIO_SAMPLE_RATE_HZ // 2) // AUDIO_SAMPLE_RATE_HZ
            and decoded_pcm_bytes == sample_count * 2
            and block_count == (sample_count + AUDIO_BLOCK_SAMPLES - 1) // AUDIO_BLOCK_SAMPLES,
            "audio asset metadata is invalid",
        )
        _require(
            data_offset >= AUDIO_BANK_HEADER.size
            and data_size > 0
            and data_offset + data_size <= len(bank_payload),
            "audio asset payload is outside the ADPCM bank",
        )
        adpcm = bank_payload[data_offset : data_offset + data_size]
        _require((zlib.crc32(adpcm) & 0xFFFFFFFF) == payload_crc, "audio asset CRC mismatch")
        try:
            decode_ima_adpcm(adpcm, sample_count, block_count, AUDIO_BLOCK_SAMPLES)
        except AudioAssetError as exc:
            raise EggFormatError(f"audio asset ADPCM is invalid: {exc}") from exc
        ranges.append((data_offset, data_offset + data_size))
        assets.append(
            {
                "asset_id": asset_id,
                "sample_rate_hz": sample_rate,
                "channels": channels,
                "sample_count": sample_count,
                "duration_ms": duration_ms,
                "decoded_pcm_bytes": decoded_pcm_bytes,
                "block_samples": block_samples,
                "block_count": block_count,
                "adpcm": adpcm,
            }
        )
    ordered = sorted(ranges)
    _require(
        all(ordered[index][1] <= ordered[index + 1][0] for index in range(len(ordered) - 1)),
        "audio asset payloads overlap",
    )

    _require(len(cue_payload) >= AUDIO_CUE_HEADER.size, "audio cue table is truncated")
    cue_header = AUDIO_CUE_HEADER.unpack_from(cue_payload)
    _require(
        cue_header[0] == b"ACU1"
        and cue_header[1] == 1
        and cue_header[2] == AUDIO_CUE_HEADER.size,
        "unsupported audio cue table",
    )
    cue_count, cue_asset_count, cue_reserved, cue_reserved2 = cue_header[3:7]
    _require(
        cue_asset_count == asset_count
        and cue_reserved == 0
        and cue_reserved2 == 0
        and len(cue_payload) == AUDIO_CUE_HEADER.size + cue_count * AUDIO_CUE_RECORD.size,
        "audio cue table size is inconsistent",
    )
    cues: list[dict[str, object]] = []
    cue_ids: set[str] = set()
    for index in range(cue_count):
        cue_id_index, selected_asset, priority, volume, flags, record_reserved = (
            AUDIO_CUE_RECORD.unpack_from(
                cue_payload, AUDIO_CUE_HEADER.size + index * AUDIO_CUE_RECORD.size
            )
        )
        cue_id = _string(strings, cue_id_index, "audio cue ID")
        _require(cue_id not in cue_ids, "audio cue ID is duplicated")
        cue_ids.add(cue_id)
        _require(
            selected_asset < asset_count
            and priority <= 255
            and volume <= 255
            and flags == 0
            and record_reserved == 0,
            "audio cue record is invalid",
        )
        cues.append(
            {
                "cue_id": cue_id,
                "asset_index": selected_asset,
                "asset_id": assets[selected_asset]["asset_id"],
                "priority": priority,
                "volume": volume,
            }
        )
    return tuple(assets), tuple(cues)


def _parse_render(payload: bytes, strings: tuple[str, ...]) -> dict[str, object]:
    _require(len(payload) >= RENDER_HEADER.size, "render chunk is truncated")
    values = RENDER_HEADER.unpack_from(payload)
    version = values[1]
    _require(
        values[0] == b"RND1" and version in {1, 2} and values[2] == RENDER_HEADER.size,
        "unsupported render chunk",
    )
    element_record = RENDER_ELEMENT_RECORD_V1 if version == 1 else RENDER_ELEMENT_RECORD_V2
    model_count, element_count = values[3], values[4]
    expected = RENDER_HEADER.size + model_count * RENDER_MODEL_RECORD.size + element_count * element_record.size
    _require(len(payload) == expected, "render chunk size is inconsistent")
    model_offset = RENDER_HEADER.size
    element_offset = model_offset + model_count * RENDER_MODEL_RECORD.size
    model_records: list[tuple[int, int, int, int]] = []
    for index in range(model_count):
        visual_id, focus, first_element, count = RENDER_MODEL_RECORD.unpack_from(
            payload, model_offset + index * RENDER_MODEL_RECORD.size
        )
        _string(strings, visual_id, "render visual_id")
        _require(first_element + count <= element_count, "render model element range is invalid")
        model_records.append((visual_id, focus, first_element, count))
    elements: list[dict[str, object]] = []
    for index in range(element_count):
        record = element_record.unpack_from(payload, element_offset + index * element_record.size)
        element_id = _string(strings, record[0], "render element_id")
        if version == 1:
            visual_ref = _string(strings, record[1], "render visual_ref")
            _require(record[2] in {1, 2, 3}, "render element kind is invalid")
            _require(record[3] in {0, 1}, "render focus role is invalid")
            _require(record[6] > 0 and record[7] > 0, "render element dimensions are invalid")
            layer = 2 if record[3] else 1
            visible = 1
            focus_role = record[3]
            x, y, width, height, z_order = record[4:9]
        else:
            visual_ref = None if record[1] == 0xFFFF else _string(strings, record[1], "render visual_ref")
            _require(record[2] in {1, 2, 3, 4, 5, 6}, "render element kind is invalid")
            _require(record[3] in {0, 1, 2}, "render package layer is invalid")
            _require(record[4] & ~0x03 == 0 and record[5] == 0 and record[11] == 0, "render flags or reserved fields are invalid")
            _require(not (record[4] & 0x01) or (record[2] == 1 and record[3] == 2 and record[4] & 0x02), "render focus element is invalid")
            _require((record[2] == 1 and visual_ref is not None) or (record[2] != 1 and visual_ref is None), "render visual reference is invalid")
            _require(record[8] > 0 and record[9] > 0, "render element dimensions are invalid")
            layer = record[3]
            visible = 1 if record[4] & 0x02 else 0
            focus_role = 1 if record[4] & 0x01 else 0
            x, y, width, height, z_order = record[6:11]
            if record[2] in {5, 6}:
                _require(width >= 3 and height >= 3 and width % 2 == 1 and height % 2 == 1, "ellipse bounds must be odd and at least 3")
            if record[2] == 5:
                _require(width == height, "circle bounds must be square")
        _require(x >= 0 and y >= 0 and x + width <= 168 and y + height <= 144, "render element exceeds the canvas")
        _require(z_order <= 255, "render z-order is invalid")
        elements.append(
            {
                "format_version": version,
                "element_id": element_id,
                "visual_ref": visual_ref,
                "kind": record[2],
                "layer": layer,
                "visible": visible,
                "focus_role": focus_role,
                "x": x,
                "y": y,
                "width": width,
                "height": height,
                "z_order": z_order,
            }
        )
    models: list[dict[str, object]] = []
    for visual_id, focus, first_element, count in model_records:
        models.append(
            {
                "visual_id": _string(strings, visual_id, "render visual_id"),
                "focus_index": focus,
                "elements": tuple(elements[first_element : first_element + count]),
            }
        )
    return {"format_version": version, "model_count": model_count, "element_count": element_count, "models": tuple(models)}


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
    wait_records: list[tuple[int, ...]] = []
    for index in range(wait_count):
        record = WAIT_RECORD.unpack_from(payload, wait_offset + index * WAIT_RECORD.size)
        _string(strings, record[0], "waiting_visual_id")
        _string(strings, record[1], "waiting presentation_id")
        _require(record[2] > 0 and 1 <= record[3] <= 12, "waiting cadence or step count is invalid")
        _require(record[4] < record[3] and record[5] == 1, "waiting settled step or cycle policy is invalid")
        _require(record[6] + record[7] <= element_count, "waiting element range is invalid")
        wait_records.append(record)
    phase_refs = struct.unpack_from(f"<{phase_count}H", payload, phase_offset) if phase_count else ()
    for ref in phase_refs:
        _string(strings, ref, "waiting phase visual")
    sequence = payload[sequence_offset:]
    elements: list[dict[str, object]] = []
    for index in range(element_count):
        record = WAIT_ELEMENT_RECORD.unpack_from(payload, element_offset + index * WAIT_ELEMENT_RECORD.size)
        element_id = _string(strings, record[0], "waiting element_id")
        source_element_ref = _string(strings, record[1], "waiting source_element_ref")
        first_phase, count_phase, first_step, count_step = record[2:6]
        _require(1 <= count_phase <= 4 and first_phase + count_phase <= phase_count, "waiting phase range is invalid")
        _require(first_step + count_step <= sequence_count, "waiting sequence range is invalid")
        _require(all(value < count_phase for value in sequence[first_step : first_step + count_step]), "waiting phase index is invalid")
        elements.append(
            {
                "element_id": element_id,
                "source_element_ref": source_element_ref,
                "phase_visual_refs": tuple(
                    _string(strings, ref, "waiting phase visual")
                    for ref in phase_refs[first_phase : first_phase + count_phase]
                ),
                "step_phase_indices": tuple(sequence[first_step : first_step + count_step]),
            }
        )
    waits: list[dict[str, object]] = []
    for record in wait_records:
        first_element, count = record[6], record[7]
        selected = tuple(elements[first_element : first_element + count])
        _require(
            all(len(element["step_phase_indices"]) == record[3] for element in selected),
            "waiting element sequence length does not match its visual",
        )
        waits.append(
            {
                "waiting_visual_id": _string(strings, record[0], "waiting_visual_id"),
                "presentation_id": _string(strings, record[1], "waiting presentation_id"),
                "phase_quantum_ms": record[2],
                "combined_step_count": record[3],
                "settled_step": record[4],
                "cycle_policy": record[5],
                "elements": selected,
            }
        )
    return {
        "waiting_count": wait_count,
        "element_count": element_count,
        "step_counts": tuple(int(wait["combined_step_count"]) for wait in waits),
        "waiting_visuals": tuple(waits),
    }


def _parse_graph(
    payload: bytes,
    strings: tuple[str, ...],
    render_count: int,
    waiting_count: int,
    audio_cue_count: int,
) -> dict[str, object]:
    _require(len(payload) >= GRAPH_HEADER.size, "state graph is truncated")
    values = GRAPH_HEADER.unpack_from(payload)
    graph_version = values[1]
    _require(
        values[0] == b"STG1" and graph_version in {1, 2, 3, 4} and values[2] == GRAPH_HEADER.size,
        "unsupported state graph",
    )
    route_record = ROUTE_RECORD_V1 if graph_version == 1 else ROUTE_RECORD_V2
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
        interaction_mode_field,
    ) = values[3:]
    interaction_mode = 2 if graph_version < 3 else (interaction_mode_field & 0x00FF)
    joystick_policy = 1 if graph_version < 4 else ((interaction_mode_field >> 8) & 0x00FF)
    _require(
        hold_fallback in {0, 1}
        and interaction_mode in {1, 2}
        and joystick_policy in {1, 2}
        and ((interaction_mode == 1 and inactive_route == 0) or (interaction_mode == 2 and inactive_route in {1, 2}))
        and (graph_version >= 3 or interaction_mode_field == 0),
        "state policy fields are invalid",
    )
    _string(strings, wait_policy_id, "wait policy ID")
    _string(strings, interaction_policy_id, "interaction policy ID")
    _require(entry_state < state_count and default_waiting < waiting_count, "state graph entry or wait reference is invalid")
    offsets = [GRAPH_HEADER.size]
    sizes = (
        variable_count * VARIABLE_RECORD.size,
        input_count * INPUT_RECORD.size,
        state_count * STATE_RECORD.size,
        route_count * route_record.size,
        source_count * 2,
        guard_count * GUARD_RECORD.size,
        operation_count * OPERATION_RECORD.size,
        event_count * 2,
        meaningful_count * 2,
    )
    for size in sizes:
        offsets.append(offsets[-1] + size)
    _require(offsets[-1] == len(payload), "state graph size is inconsistent")

    variables: list[dict[str, object]] = []
    for index in range(variable_count):
        record = VARIABLE_RECORD.unpack_from(payload, offsets[0] + index * VARIABLE_RECORD.size)
        variable_id = _string(strings, record[0], "variable ID")
        _require(
            record[1] == 1 and record[2] == 0 and record[4] <= record[3] <= record[5],
            "variable record is invalid",
        )
        variables.append(
            {
                "variable_id": variable_id,
                "initial": record[3],
                "minimum": record[4],
                "maximum": record[5],
            }
        )
    inputs: list[dict[str, object]] = []
    for index in range(input_count):
        record = INPUT_RECORD.unpack_from(payload, offsets[1] + index * INPUT_RECORD.size)
        action_id = _string(strings, record[0], "input action ID")
        logical_source = record[1] if graph_version < 4 else (record[1] & 0x00FF)
        logical_event = 1 if graph_version < 4 else ((record[1] >> 8) & 0x00FF)
        _require(logical_source in set(range(1, 14)), "input source is invalid")
        _require(logical_event in {1, 2, 3, 4}, "input event is invalid")
        inputs.append({"action_id": action_id, "logical_source": logical_source, "logical_event": logical_event})
    states: list[dict[str, object]] = []
    for index in range(state_count):
        record = STATE_RECORD.unpack_from(payload, offsets[2] + index * STATE_RECORD.size)
        state_id = _string(strings, record[0], "state ID")
        display_name = _string(strings, record[1], "state display name")
        _require(record[2] < render_count and record[3] < waiting_count, "state visual reference is invalid")
        states.append(
            {
                "state_id": state_id,
                "display_name": display_name,
                "render_model_index": record[2],
                "waiting_visual_index": record[3],
            }
        )
    source_states = struct.unpack_from(f"<{source_count}H", payload, offsets[4]) if source_count else ()
    _require(all(ref < state_count for ref in source_states), "route source-state index is invalid")
    route_records: list[tuple[int, ...]] = []
    for index in range(route_count):
        record = route_record.unpack_from(payload, offsets[3] + index * route_record.size)
        _string(strings, record[0], "route ID")
        if graph_version == 1:
            _require(record[1] < input_count and record[2] < state_count, "route action or target is invalid")
            range_offset = 3
        else:
            target_state, target_scene = record[2], record[3]
            _require(
                record[1] < input_count
                and ((target_state < state_count and target_scene == 0xFFFF)
                     or (target_state == 0xFFFF and target_scene < len(strings))),
                "route action or target is invalid",
            )
            range_offset = 4
        _require(record[range_offset] + record[range_offset + 1] <= source_count, "route source range is invalid")
        _require(record[range_offset + 2] + record[range_offset + 3] <= guard_count, "route guard range is invalid")
        _require(record[range_offset + 4] + record[range_offset + 5] <= operation_count, "route operation range is invalid")
        route_records.append(record)
    guards: list[dict[str, int]] = []
    for index in range(guard_count):
        record = GUARD_RECORD.unpack_from(payload, offsets[5] + index * GUARD_RECORD.size)
        _require(record[0] < variable_count and record[1] in {1, 2, 3, 4, 5, 6}, "guard record is invalid")
        guards.append({"variable_index": record[0], "operator": record[1], "value": record[3]})
    operations: list[dict[str, object]] = []
    for index in range(operation_count):
        record = OPERATION_RECORD.unpack_from(payload, offsets[6] + index * OPERATION_RECORD.size)
        if record[0] == 1:
            _require(
                record[1] in {1, 2}
                and record[2] < variable_count
                and record[3] == 0
                and record[4] == 0,
                "set-variable operation is invalid",
            )
            operations.append(
                {
                    "kind": record[0],
                    "operation": record[1],
                    "variable_index": record[2],
                    "value": record[5],
                }
            )
        elif record[0] == 2:
            _require(
                record[1] == 0
                and record[2] == 0xFFFF
                and record[3] == 0
                and record[4] == 0
                and record[5] == 0,
                "request-render operation is invalid",
            )
            operations.append({"kind": record[0]})
        elif record[0] == 3:
            _require(
                record[1] == 0
                and record[3] == 0
                and record[4] == 0
                and record[5] in {0, 1},
                "set-element-visibility operation is invalid",
            )
            operations.append(
                {
                    "kind": record[0],
                    "element_index": record[2],
                    "visible": record[5],
                }
            )
        elif record[0] == 4:
            _require(
                record[1] == 0 and record[4] == 0 and record[5] >= 0,
                "set-element-position operation is invalid",
            )
            operations.append(
                {
                    "kind": record[0],
                    "element_index": record[2],
                    "x": record[3],
                    "y": record[5],
                }
            )
        elif record[0] == 5:
            _require(
                record[1] == 0
                and record[3] < len(strings)
                and record[4] == 0
                and record[5] == 0,
                "set-element-frame operation is invalid",
            )
            operations.append(
                {
                    "kind": record[0],
                    "element_index": record[2],
                    "frame_ref": _string(strings, record[3], "element action frame"),
                }
            )
        elif record[0] == 6:
            _require(
                record[1] in {1, 2}
                and record[3] < waiting_count
                and record[4] < len(strings)
                and record[5] == 0,
                "set-element-waiting-animation operation is invalid",
            )
            operations.append(
                {
                    "kind": record[0],
                    "timeline_policy": record[1],
                    "element_index": record[2],
                    "waiting_visual_index": record[3],
                    "waiting_element_ref": _string(
                        strings,
                        record[4],
                        "element action waiting element",
                    ),
                }
            )
        elif record[0] == 7:
            _require(
                record[1] == 0
                and record[2] < audio_cue_count
                and record[3] == 0
                and record[4] == 0
                and record[5] == 0,
                "play-SFX operation is invalid",
            )
            operations.append({"kind": record[0], "cue_index": record[2]})
        elif record[0] == 8:
            _require(
                record[1] == 0
                and record[2] == 0
                and record[3] == 0
                and record[4] == 0
                and record[5] == 0,
                "exit-to-shell operation is invalid",
            )
            operations.append({"kind": record[0]})
        else:
            raise EggFormatError("operation record is invalid")
    routes: list[dict[str, object]] = []
    for record in route_records:
        if graph_version == 1:
            target_state_index = record[2]
            target_scene = None
            range_offset = 3
        else:
            target_state_index = None if record[2] == 0xFFFF else record[2]
            target_scene = None if record[3] == 0xFFFF else _string(strings, record[3], "target scene ID")
            range_offset = 4
        route_operations = tuple(
            operations[
                record[range_offset + 4] :
                record[range_offset + 4] + record[range_offset + 5]
            ]
        )
        if target_scene is not None:
            _require(
                all(int(operation["kind"]) == 7 for operation in route_operations),
                "direct scene replacement operation is invalid",
            )
        routes.append(
            {
                "route_id": _string(strings, record[0], "route ID"),
                "action_index": record[1],
                "target_state_index": target_state_index,
                "target_scene": target_scene,
                "source_state_indexes": tuple(
                    source_states[record[range_offset] : record[range_offset] + record[range_offset + 1]]
                ),
                "guards": tuple(
                    guards[record[range_offset + 2] : record[range_offset + 2] + record[range_offset + 3]]
                ),
                "operations": route_operations,
            }
        )
    event_refs = struct.unpack_from(f"<{event_count}H", payload, offsets[7]) if event_count else ()
    meaningful_refs = struct.unpack_from(f"<{meaningful_count}H", payload, offsets[8]) if meaningful_count else ()
    _require(all(ref < input_count for ref in event_refs + meaningful_refs), "policy input-action index is invalid")
    return {
        "format_version": graph_version,
        "entry_state": entry_state,
        "variable_count": variable_count,
        "input_count": input_count,
        "state_count": state_count,
        "route_count": route_count,
        "variables": tuple(variables),
        "inputs": tuple(inputs),
        "states": tuple(states),
        "routes": tuple(routes),
        "wait_policy_id": _string(strings, wait_policy_id, "wait policy ID"),
        "default_waiting_index": default_waiting,
        "hold_fallback_allowed": bool(hold_fallback),
        "event_interest_indexes": tuple(event_refs),
        "interaction_policy_id": _string(strings, interaction_policy_id, "interaction policy ID"),
        "interaction_mode": interaction_mode,
        "joystick_policy": joystick_policy,
        "inactive_route": inactive_route,
        "meaningful_action_indexes": tuple(meaningful_refs),
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
    asset_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_ASSET_TABLE]
    sprite_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_MASKED_1BPP_SPRITE_BANK]
    animation_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_ANIMATION_TABLE]
    asset_cardinality = (len(asset_indexes), len(sprite_indexes), len(animation_indexes))
    _require(asset_cardinality in {(0, 0, 0), (1, 1, 1)}, "asset chunk cardinality is invalid")
    audio_asset_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_AUDIO_ASSET_TABLE]
    audio_bank_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_AUDIO_ADPCM_BANK]
    audio_cue_indexes = [index for index, chunk in enumerate(chunks) if chunk.chunk_type == CHUNK_AUDIO_CUE_TABLE]
    audio_cardinality = (len(audio_asset_indexes), len(audio_bank_indexes), len(audio_cue_indexes))
    _require(audio_cardinality in {(0, 0, 0), (1, 1, 1)}, "audio chunk cardinality is invalid")
    strings = _parse_strings(string_chunks[0].payload)
    manifest = _parse_manifest(manifest_chunks[0].payload, strings)
    _require(fnv1a64(str(manifest["package_id"])) == package_id_hash, "package ID hash does not match manifest")
    if audio_asset_indexes:
        audio_assets, audio_cues = _parse_audio(
            chunks,
            audio_asset_indexes[0],
            audio_bank_indexes[0],
            audio_cue_indexes[0],
            strings,
        )
    else:
        audio_assets, audio_cues = (), ()

    scene_payload = scene_chunks[0].payload
    _require(len(scene_payload) >= SCENE_HEADER.size, "scene table is truncated")
    scene_header = SCENE_HEADER.unpack_from(scene_payload)
    _require(scene_header[0] == b"SCN1" and scene_header[1] == 1 and scene_header[2] == SCENE_HEADER.size, "unsupported scene table")
    scene_count = scene_header[3]
    _require(scene_header[4] == 0 and len(scene_payload) == SCENE_HEADER.size + scene_count * SCENE_RECORD.size, "scene table size is inconsistent")
    expected_chunk_count = (
        3
        + scene_count * 3
        + (3 if asset_indexes else 0)
        + (3 if audio_asset_indexes else 0)
    )
    _require(scene_count == manifest["scene_count"] and chunk_count == expected_chunk_count, "scene/chunk count is inconsistent")
    scenes: list[dict[str, object]] = []
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
            len(audio_cues),
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
                "graph": graph_summary,
                "render_format_version": render_summary["format_version"],
                "render_models": render_summary["models"],
                "waiting_visuals": wait_summary["waiting_visuals"],
            }
        )
    expected_scene_chunks = {index for index, chunk in enumerate(chunks) if chunk.chunk_type in {CHUNK_STATE_GRAPH, CHUNK_RENDER_MODELS, CHUNK_WAITING_VISUALS}}
    _require(used_scene_chunks == expected_scene_chunks, "unreferenced or multiply purposed scene chunks exist")
    scene_ids = {str(scene["scene_id"]) for scene in scenes}
    _require(str(manifest["entry_scene"]) in scene_ids, "manifest entry scene is missing")
    for scene in scenes:
        for route in scene["graph"]["routes"]:
            target_scene = route["target_scene"]
            _require(target_scene is None or str(target_scene) in scene_ids, "route target scene is missing")
    if asset_indexes:
        assets, animations = _parse_assets(
            chunks,
            asset_indexes[0],
            sprite_indexes[0],
            animation_indexes[0],
            strings,
        )
    else:
        assets, animations = (), ()
    return EggPackage(
        package_id_hash=package_id_hash,
        package_flags=package_flags,
        manifest_chunk_index=manifest_index,
        strings=strings,
        manifest=manifest,
        scenes=tuple(scenes),
        assets=assets,
        animations=animations,
        audio_assets=audio_assets,
        audio_cues=audio_cues,
        chunks=tuple(chunks),
        sha256=hashlib.sha256(blob).hexdigest(),
    )
