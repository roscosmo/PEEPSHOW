#!/usr/bin/env python3
"""Generate knobs header + audio asset catalog from system UI audio.

Audio embedding is manifest-gated:
- `config/audio_manifest.json` controls which `.wav` files are embedded.
- Source folder: `Assets/audio/system_ui`
- If missing, a default manifest is created with system UI sounds only.
- During audio generation, manifest entries are auto-synced:
  - new files found in the source folder are appended
  - missing files are pruned
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import wave
from pathlib import Path
from typing import Any

KEY_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")

AUDIO_BLOCK_ALIGN = 1024
AUDIO_SAMPLES_PER_BLOCK = ((AUDIO_BLOCK_ALIGN - 4) * 2) + 1
AUDIO_MANIFEST_REL_PATH = Path("config") / "audio_manifest.json"
AUDIO_ROOT_DIR_REL_PATH = Path("Assets") / "audio"
AUDIO_SYSTEM_DIR_CANDIDATES: tuple[Path, ...] = (
    AUDIO_ROOT_DIR_REL_PATH / "system_ui",
    AUDIO_ROOT_DIR_REL_PATH / "system",
)
DEFAULT_SYSTEM_AUDIO_DIR_REL_PATH = AUDIO_SYSTEM_DIR_CANDIDATES[0]
DEFAULT_EMBEDDED_AUDIO: tuple[str, ...] = (
    "UI_Move.wav",
    "UI_Confirm.wav",
    "UI_Decline.wav",
    "UI_Denied.wav",
    "UI_map_open.wav",
    "UI_map_close.wav",
)

IMA_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
    209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
    3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
]

IMA_INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
]

# Old slot-based knob keys no longer used.
AUDIO_LEGACY_KEYS = (
    "audio_clip_ui_move_source",
    "audio_clip_ui_confirm_source",
    "audio_clip_ui_decline_source",
    "audio_clip_ui_denied_source",
    "audio_clip_game_sfx_source",
    "audio_clip_game_music_source",
    "audio_map_rt_confirm_clip",
    "audio_map_rt_cancel_clip",
    "audio_map_rt_joy_move_clip",
    "audio_map_rt_btn_a_clip",
    "audio_map_rt_btn_b_clip",
    "audio_map_rt_btn_l_clip",
    "audio_map_rt_btn_r_clip",
    "audio_map_rt_menu_clip",
)

AUDIO_ROUTE_DEFS: tuple[dict[str, Any], ...] = (
    {
        "key": "audio_map_ui_nav_clip",
        "label": "UI Navigate Sound",
        "section": "UI Routing",
        "description": "Sound played for UI navigation movement actions.",
        "default_file": "UI_Move.wav",
        "order": 34,
    },
    {
        "key": "audio_map_ui_confirm_clip",
        "label": "UI Confirm Sound",
        "section": "UI Routing",
        "description": "Sound played when UI confirms a selection.",
        "default_file": "UI_Confirm.wav",
        "order": 35,
    },
    {
        "key": "audio_map_ui_cancel_clip",
        "label": "UI Cancel Sound",
        "section": "UI Routing",
        "description": "Sound played when UI cancels/backs out.",
        "default_file": "UI_Decline.wav",
        "order": 36,
    },
    {
        "key": "audio_map_ui_denied_clip",
        "label": "UI Denied Sound",
        "section": "UI Routing",
        "description": "Sound played when UI rejects an action.",
        "default_file": "UI_Denied.wav",
        "order": 37,
    },
    {
        "key": "audio_map_game_action_clip",
        "label": "Game Action Sound",
        "section": "Game Routing",
        "description": "Generic gameplay action sound (for game-driven actions).",
        "default_file": "",
        "order": 38,
    },
    {
        "key": "audio_map_rt_move_clip",
        "label": "Realtime Move Sound",
        "section": "Realtime Routing",
        "description": "Sound played for movement cues emitted by active REALTIME gameplay.",
        "default_file": "",
        "order": 39,
        "aliases": ("audio_map_rt_joy_move_clip",),
    },
    {
        "key": "audio_map_rt_primary_clip",
        "label": "Realtime Primary Action Sound",
        "section": "Realtime Routing",
        "description": "Sound played for primary action cues emitted by active REALTIME gameplay.",
        "default_file": "",
        "order": 40,
        "aliases": ("audio_map_rt_confirm_clip", "audio_map_rt_btn_a_clip"),
    },
    {
        "key": "audio_map_rt_secondary_clip",
        "label": "Realtime Secondary Action Sound",
        "section": "Realtime Routing",
        "description": "Sound played for secondary action cues emitted by active REALTIME gameplay.",
        "default_file": "UI_Decline.wav",
        "order": 41,
        "aliases": ("audio_map_rt_cancel_clip", "audio_map_rt_btn_b_clip"),
    },
    {
        "key": "audio_map_rt_back_clip",
        "label": "Realtime Back/Menu Sound",
        "section": "Realtime Routing",
        "description": "Sound played for back/menu cues emitted by active REALTIME gameplay.",
        "default_file": "UI_Confirm.wav",
        "order": 42,
        "aliases": ("audio_map_rt_menu_clip", "audio_map_rt_btn_l_clip", "audio_map_rt_btn_r_clip"),
    },
    {
        "key": "audio_music_loop_clip",
        "label": "Looping Music Asset",
        "section": "Playback",
        "description": "Any direct play request of this asset ID loops continuously.",
        "default_file": "",
        "order": 43,
    },
)


def _format_value(value: Any) -> str:
    if isinstance(value, bool):
        return "(1)" if value else "(0)"
    if isinstance(value, int):
        return f"({value})"
    if isinstance(value, float):
        return f"({format(value, '.15g')})"
    if isinstance(value, str):
        return json.dumps(value)
    raise TypeError(f"unsupported knob value type: {type(value).__name__}")


def _validate(knobs: dict[str, Any]) -> None:
    if not knobs:
        raise ValueError("knobs.json must contain at least one knob")
    for key, value in knobs.items():
        if not KEY_PATTERN.match(key):
            raise ValueError(f"invalid knob name: {key!r}")
        if key.startswith("knob_"):
            raise ValueError(f"knob name must not start with 'knob_': {key!r}")
        if isinstance(value, (dict, list)) or value is None:
            raise TypeError(f"unsupported knob value for {key!r}: {type(value).__name__}")


def _inject_schema_defaults(knobs: dict[str, Any], schema: dict[str, Any]) -> dict[str, Any]:
    props = schema.get("properties")
    if not isinstance(props, dict):
        return dict(knobs)
    out = dict(knobs)
    for key, prop in props.items():
        if key in out:
            continue
        if isinstance(prop, dict) and ("default" in prop):
            out[str(key)] = prop.get("default")
    return out


def _validate_required(schema: dict[str, Any], knobs: dict[str, Any]) -> None:
    required = schema.get("required")
    if not isinstance(required, list):
        return
    missing = [str(k) for k in required if str(k) not in knobs]
    if missing:
        raise ValueError("missing required knobs after applying defaults: " + ", ".join(sorted(missing)))


def _render_header(knobs: dict[str, Any], source_rel: str) -> str:
    lines: list[str] = [
        "/* AUTO-GENERATED FILE. DO NOT EDIT. */",
        f"/* Source: {source_rel} */",
        "/* Generator: tools/gen_knobs.py */",
        "",
        "#ifndef KNOBS_AUTOGEN_H",
        "#define KNOBS_AUTOGEN_H",
        "",
    ]
    max_name_len = max(len(key) for key in knobs)
    for key in sorted(knobs):
        macro = f"KNOB_{key.upper()}"
        pad = " " * (max_name_len - len(key) + 1)
        lines.append(f"#define {macro}{pad}{_format_value(knobs[key])}")
    lines.extend(["", "#endif /* KNOBS_AUTOGEN_H */", ""])
    return "\n".join(lines)


def _canonical_audio_order(files: list[str]) -> list[str]:
    preferred = [
        "UI_Move.wav",
        "UI_Confirm.wav",
        "UI_Decline.wav",
        "UI_Denied.wav",
        "UI_map_open.wav",
        "UI_map_close.wav",
    ]
    rank = {name: ix for ix, name in enumerate(preferred)}
    return sorted(files, key=lambda n: (rank.get(n, 10_000), n.lower()))


def _list_audio_files(audio_dir: Path) -> list[str]:
    if not audio_dir.is_dir():
        return []
    files: list[str] = []
    for p in audio_dir.iterdir():
        if p.is_file() and p.suffix.lower() == ".wav":
            files.append(p.name)
    return _canonical_audio_order(files)


def _resolve_system_audio_dir(repo_root: Path) -> tuple[Path, Path]:
    # Prefer explicit split folders. Fall back to Assets/audio only for legacy layouts.
    for rel in AUDIO_SYSTEM_DIR_CANDIDATES:
        abs_dir = repo_root / rel
        if abs_dir.is_dir():
            return abs_dir, rel

    legacy = repo_root / AUDIO_ROOT_DIR_REL_PATH
    if legacy.is_dir():
        return legacy, AUDIO_ROOT_DIR_REL_PATH

    return repo_root / DEFAULT_SYSTEM_AUDIO_DIR_REL_PATH, DEFAULT_SYSTEM_AUDIO_DIR_REL_PATH


def _default_embedded_audio(audio_files: list[str]) -> list[str]:
    have = set(audio_files)
    return [name for name in DEFAULT_EMBEDDED_AUDIO if name in have]


def _write_audio_manifest(path: Path, embedded_files: list[str]) -> None:
    payload = {
        "embedded": embedded_files,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def _read_audio_manifest_embedded(path: Path) -> list[str]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise TypeError(f"{AUDIO_MANIFEST_REL_PATH.as_posix()} must be a JSON object")
    embedded_raw = raw.get("embedded", [])
    if not isinstance(embedded_raw, list):
        raise TypeError(f"{AUDIO_MANIFEST_REL_PATH.as_posix()} 'embedded' must be an array")
    out: list[str] = []
    for item in embedded_raw:
        if not isinstance(item, str):
            raise TypeError(f"{AUDIO_MANIFEST_REL_PATH.as_posix()} entries must be strings")
        name = item.strip()
        if name:
            out.append(name)
    return out


def _resolve_embedded_audio_files(
    repo_root: Path,
    audio_files: list[str],
    system_audio_rel: Path,
    allow_manifest_sync: bool,
) -> list[str]:
    manifest_path = repo_root / AUDIO_MANIFEST_REL_PATH
    if not manifest_path.is_file():
        defaults = _default_embedded_audio(audio_files)
        _write_audio_manifest(manifest_path, defaults)
        print(f"Created {AUDIO_MANIFEST_REL_PATH.as_posix()} with {len(defaults)} default embedded files.")
        return defaults

    embedded_raw = _read_audio_manifest_embedded(manifest_path)

    available = set(audio_files)
    selected: list[str] = []
    seen: set[str] = set()
    removed: list[str] = []
    for item in embedded_raw:
        name = item
        if name not in available:
            removed.append(name)
            continue
        if name in seen:
            continue
        selected.append(name)
        seen.add(name)

    if allow_manifest_sync:
        added: list[str] = []
        for name in audio_files:
            if name in seen:
                continue
            selected.append(name)
            seen.add(name)
            added.append(name)

        if added or removed:
            print(f"Audio manifest sync: +{len(added)} added, -{len(removed)} removed")
            if added:
                print("  + " + ", ".join(added))
            if removed:
                print("  - " + ", ".join(removed))
            _write_audio_manifest(manifest_path, selected)
            print(f"Updated {AUDIO_MANIFEST_REL_PATH.as_posix()}.")

        if selected:
            return selected
    else:
        if selected:
            return selected

    defaults = _default_embedded_audio(audio_files)
    if defaults:
        print("Warning: audio manifest has no valid embedded files; falling back to default system UI set.")
    else:
        print("Warning: audio manifest has no valid embedded files and no defaults are present.")
    return defaults


def _asset_id_for_file(audio_files: list[str], filename: str) -> int:
    if not filename:
        return 0
    try:
        return audio_files.index(filename) + 1
    except ValueError:
        return 0


def _sync_audio_schema(schema: dict[str, Any], audio_files: list[str]) -> tuple[dict[str, Any], bool]:
    changed = False
    props = schema.setdefault("properties", {})
    required = schema.setdefault("required", [])
    if not isinstance(props, dict):
        raise TypeError("knobs schema properties must be an object")
    if not isinstance(required, list):
        raise TypeError("knobs schema required must be an array")

    for legacy_key in AUDIO_LEGACY_KEYS:
        if legacy_key in props:
            del props[legacy_key]
            changed = True
        if legacy_key in required:
            required.remove(legacy_key)
            changed = True

    one_of = [{"const": 0, "title": "Silence"}]
    for ix, filename in enumerate(audio_files, start=1):
        one_of.append({"const": ix, "title": filename})

    for route in AUDIO_ROUTE_DEFS:
        key = str(route["key"])
        default_id = _asset_id_for_file(audio_files, str(route.get("default_file", "")))
        existing = props.get(key)
        desired = {
            "type": "integer",
            "minimum": 0,
            "maximum": len(audio_files),
            "default": default_id,
            "oneOf": one_of,
            "description": str(route["description"]),
            "category": "Audio",
            "gui_tab": "audio",
            "gui_section": str(route["section"]),
            "gui_label": str(route["label"]),
            "order": int(route["order"]),
            "unit": "asset",
            "restart_required": True,
        }
        if isinstance(existing, dict) and ("gui_advanced" in existing):
            desired["gui_advanced"] = bool(existing.get("gui_advanced"))
        if props.get(key) != desired:
            props[key] = desired
            changed = True
        if key not in required:
            required.append(key)
            changed = True

    return schema, changed


def _migrate_and_clamp_audio_knobs(
    knobs: dict[str, Any],
    embedded_audio_files: list[str],
    discovered_audio_files: list[str],
) -> dict[str, Any]:
    out = dict(knobs)
    max_id = len(embedded_audio_files)

    # Migrate aliases first.
    for route in AUDIO_ROUTE_DEFS:
        key = str(route["key"])
        if key in out:
            continue
        aliases = tuple(route.get("aliases", ()))
        for alias_key in aliases:
            if alias_key in out:
                out[key] = out[alias_key]
                break

    # Fallback migration from older realtime routing to contextual routing.
    if "audio_map_rt_primary_clip" not in out and "audio_map_game_action_clip" in out:
        out["audio_map_rt_primary_clip"] = out["audio_map_game_action_clip"]
    if "audio_map_rt_secondary_clip" not in out and "audio_map_rt_primary_clip" in out:
        out["audio_map_rt_secondary_clip"] = out["audio_map_rt_primary_clip"]
    if "audio_map_rt_back_clip" not in out and "audio_map_rt_secondary_clip" in out:
        out["audio_map_rt_back_clip"] = out["audio_map_rt_secondary_clip"]

    # Clamp values into valid ID range.
    for route in AUDIO_ROUTE_DEFS:
        key = str(route["key"])
        default_id = _asset_id_for_file(embedded_audio_files, str(route.get("default_file", "")))
        value = out.get(key)
        if not isinstance(value, int):
            value = default_id
        elif value > max_id:
            # Only remap values outside current embedded range.
            # This preserves explicitly selected embedded IDs.
            if 1 <= value <= len(discovered_audio_files):
                legacy_name = discovered_audio_files[value - 1]
                remapped = _asset_id_for_file(embedded_audio_files, legacy_name)
                value = remapped if remapped > 0 else default_id
            else:
                value = default_id
        if value < 0 or value > max_id:
            value = default_id
        out[key] = value
    return out


def _decode_nibble(predictor: int, index: int, nibble: int) -> tuple[int, int]:
    step = IMA_STEP_TABLE[index]
    diff = step >> 3
    if nibble & 1:
        diff += step >> 2
    if nibble & 2:
        diff += step >> 1
    if nibble & 4:
        diff += step
    predictor = predictor - diff if (nibble & 8) else predictor + diff
    predictor = max(-32768, min(32767, predictor))
    index += IMA_INDEX_TABLE[nibble & 0x0F]
    index = max(0, min(88, index))
    return predictor, index


def _encode_nibble(target: int, predictor: int, index: int) -> tuple[int, int, int]:
    best_nibble = 0
    best_predictor = predictor
    best_index = index
    best_err = 1 << 62
    for nibble in range(16):
        cand_predictor, cand_index = _decode_nibble(predictor, index, nibble)
        err = abs(cand_predictor - target)
        if err < best_err:
            best_err = err
            best_nibble = nibble
            best_predictor = cand_predictor
            best_index = cand_index
            if err == 0:
                break
    return best_nibble, best_predictor, best_index


def _resample_linear(samples: list[int], src_rate: int, dst_rate: int) -> list[int]:
    if src_rate == dst_rate:
        return list(samples)
    if not samples:
        return [0]
    if len(samples) == 1:
        return [samples[0]]
    out_len = max(1, int(round((len(samples) * dst_rate) / src_rate)))
    out: list[int] = []
    src_len = len(samples)
    ratio = src_rate / dst_rate
    for out_ix in range(out_len):
        src_pos = out_ix * ratio
        i0 = int(src_pos)
        if i0 >= src_len - 1:
            out.append(samples[-1])
            continue
        i1 = i0 + 1
        frac = src_pos - i0
        value = (1.0 - frac) * samples[i0] + frac * samples[i1]
        sval = int(round(value))
        sval = max(-32768, min(32767, sval))
        out.append(sval)
    return out


def _load_wav_mono_int16(path: Path) -> tuple[int, list[int]]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frame_count = wav.getnframes()
        raw = wav.readframes(frame_count)
    if sampwidth != 2:
        raise ValueError(f"{path.name}: only 16-bit PCM WAV is supported")
    if channels <= 0:
        raise ValueError(f"{path.name}: invalid channel count {channels}")
    sample_count = frame_count * channels
    samples = list(struct.unpack(f"<{sample_count}h", raw))
    if channels == 1:
        return sample_rate, samples
    mono: list[int] = []
    for i in range(0, len(samples), channels):
        frame = samples[i:i + channels]
        mono.append(int(round(sum(frame) / len(frame))))
    return sample_rate, mono


def _riff_chunks(path: Path) -> dict[bytes, list[bytes]]:
    blob = path.read_bytes()
    if len(blob) < 12:
        raise ValueError(f"{path.name}: WAV header too small")
    if blob[0:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise ValueError(f"{path.name}: not a RIFF/WAVE file")
    out: dict[bytes, list[bytes]] = {}
    off = 12
    end_blob = len(blob)
    while off + 8 <= end_blob:
        chunk_id = blob[off:off + 4]
        chunk_size = struct.unpack_from("<I", blob, off + 4)[0]
        start = off + 8
        end = start + chunk_size
        if end > end_blob:
            raise ValueError(f"{path.name}: chunk {chunk_id!r} exceeds file size")
        out.setdefault(chunk_id, []).append(blob[start:end])
        off = end + (chunk_size & 1)
    return out


def _encode_ima_adpcm(samples: list[int]) -> tuple[bytes, int]:
    if not samples:
        samples = [0]
    data = bytearray()
    sample_ix = 0
    payload_nibbles = (AUDIO_BLOCK_ALIGN - 4) * 2
    while sample_ix < len(samples):
        predictor = int(samples[sample_ix])
        predictor = max(-32768, min(32767, predictor))
        index = 0
        data.extend(struct.pack("<hBB", predictor, index, 0))
        sample_ix += 1
        nibs: list[int] = []
        for _ in range(payload_nibbles):
            target = int(samples[sample_ix]) if sample_ix < len(samples) else predictor
            if sample_ix < len(samples):
                sample_ix += 1
            nib, predictor, index = _encode_nibble(target, predictor, index)
            nibs.append(nib)
        for n in range(0, len(nibs), 2):
            data.append((nibs[n] & 0x0F) | ((nibs[n + 1] & 0x0F) << 4))
    return bytes(data), len(samples)


def _load_wav_as_runtime_clip(path: Path, target_sample_rate: int) -> tuple[bytes, int, int, int, int]:
    chunks = _riff_chunks(path)
    fmt_chunks = chunks.get(b"fmt ")
    data_chunks = chunks.get(b"data")
    if not fmt_chunks or not data_chunks:
        raise ValueError(f"{path.name}: missing fmt/data chunk")
    fmt = fmt_chunks[0]
    if len(fmt) < 16:
        raise ValueError(f"{path.name}: invalid fmt chunk")
    format_tag = struct.unpack_from("<H", fmt, 0)[0]

    # WAVE_FORMAT_IMA_ADPCM
    if format_tag == 0x0011:
        channels = struct.unpack_from("<H", fmt, 2)[0]
        sample_rate = struct.unpack_from("<I", fmt, 4)[0]
        block_align = struct.unpack_from("<H", fmt, 12)[0]
        bits_per_sample = struct.unpack_from("<H", fmt, 14)[0]
        cb_size = struct.unpack_from("<H", fmt, 16)[0] if len(fmt) >= 18 else 0
        if channels != 1:
            raise ValueError(f"{path.name}: IMA ADPCM must be mono")
        if bits_per_sample != 4:
            raise ValueError(f"{path.name}: IMA ADPCM bits/sample must be 4")
        if block_align < 8:
            raise ValueError(f"{path.name}: invalid IMA ADPCM block_align {block_align}")
        if sample_rate != target_sample_rate:
            raise ValueError(
                f"{path.name}: sample rate {sample_rate} does not match audio_sample_rate {target_sample_rate}"
            )
        if cb_size >= 2 and len(fmt) >= 20:
            samples_per_block = struct.unpack_from("<H", fmt, 18)[0]
        else:
            samples_per_block = ((block_align - 4) * 2) + 1
        payload = data_chunks[0]
        fact_chunks = chunks.get(b"fact")
        if fact_chunks and len(fact_chunks[0]) >= 4:
            total_samples = struct.unpack_from("<I", fact_chunks[0], 0)[0]
        else:
            full_blocks = len(payload) // block_align
            rem = len(payload) % block_align
            total_samples = full_blocks * samples_per_block
            if rem >= 4:
                total_samples += 1 + max(0, (rem - 4) * 2)
        return payload, int(total_samples), sample_rate, block_align, samples_per_block

    # WAVE_FORMAT_PCM
    if format_tag == 0x0001:
        src_rate, src_samples = _load_wav_mono_int16(path)
        mono = _resample_linear(src_samples, src_rate, target_sample_rate)
        encoded, total_samples = _encode_ima_adpcm(mono)
        return encoded, int(total_samples), int(target_sample_rate), AUDIO_BLOCK_ALIGN, AUDIO_SAMPLES_PER_BLOCK

    raise ValueError(f"{path.name}: unsupported WAV format tag {format_tag}")


def _c_hex_blob(data: bytes, cols: int = 16) -> str:
    lines: list[str] = []
    for i in range(0, len(data), cols):
        chunk = data[i:i + cols]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    return "\n".join(lines)


def _sanitize_c_ident(text: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9]+", " ", text).strip()
    if not cleaned:
        return "Clip"
    words = cleaned.split()
    ident = "".join(w[:1].upper() + w[1:] for w in words)
    if ident[0].isdigit():
        ident = f"Clip{ident}"
    return ident


def _sanitize_macro_token(text: str) -> str:
    token = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_").upper()
    if not token:
        token = "ASSET"
    if token[0].isdigit():
        token = f"ASSET_{token}"
    return token


def _render_audio_assets_c(
    encoded_assets: list[tuple[str, bytes, int, int, int, int]],
    source_rel_prefix: Path,
) -> str:
    lines: list[str] = ['#include "audio_assets.h"', ""]
    symbols: list[str] = []
    taken: set[str] = set()

    for name, payload, total_samples, sample_rate_hz, block_align, samples_per_block in encoded_assets:
        base = _sanitize_c_ident(Path(name).stem)
        sym = f"kAudioAsset{base}"
        suffix = 2
        while sym in taken:
            sym = f"kAudioAsset{base}{suffix}"
            suffix += 1
        taken.add(sym)
        symbols.append(sym)

        lines.append(f"/* Source: {source_rel_prefix.as_posix()}/{name} */")
        lines.append(f"static const uint8_t {sym}Data[] =")
        lines.append("{")
        lines.append(_c_hex_blob(payload))
        lines.append("};")
        lines.append("")
        lines.append(f"static const app_audio_adpcm_clip_t {sym} =")
        lines.append("{")
        lines.append(f"  {sym}Data,")
        lines.append(f"  (uint32_t)sizeof({sym}Data),")
        lines.append(f"  {sample_rate_hz}UL,")
        lines.append(f"  {block_align}U,")
        lines.append(f"  {samples_per_block}U,")
        lines.append(f"  {total_samples}UL")
        lines.append("};")
        lines.append("")

    lines.append("static const app_audio_adpcm_clip_t *const kAudioClipTable[] =")
    lines.append("{")
    lines.append("  (const app_audio_adpcm_clip_t *)0,")
    for sym in symbols:
        lines.append(f"  &{sym},")
    lines.append("};")
    lines.append("")

    lines.append("static const char *const kAudioClipNameTable[] =")
    lines.append("{")
    lines.append('  "Silence",')
    for name, _payload, _total_samples, _sample_rate_hz, _block_align, _samples_per_block in encoded_assets:
        lines.append(f'  "{name}",')
    lines.append("};")
    lines.append("")

    lines.append("uint32_t AppAudioAssets_Count(void)")
    lines.append("{")
    lines.append("  return (uint32_t)(sizeof(kAudioClipTable) / sizeof(kAudioClipTable[0]));")
    lines.append("}")
    lines.append("")

    lines.append("uint8_t AppAudioAssets_IsValidId(uint32_t asset_id)")
    lines.append("{")
    lines.append("  return ((asset_id > 0U) && (asset_id < AppAudioAssets_Count())) ? 1U : 0U;")
    lines.append("}")
    lines.append("")

    lines.append("const char *AppAudioAssets_Name(uint32_t asset_id)")
    lines.append("{")
    lines.append("  if (asset_id >= AppAudioAssets_Count())")
    lines.append("  {")
    lines.append("    return (const char *)0;")
    lines.append("  }")
    lines.append("  return kAudioClipNameTable[asset_id];")
    lines.append("}")
    lines.append("")

    lines.append("const app_audio_adpcm_clip_t *AppAudioAssets_GetClip(uint32_t asset_id)")
    lines.append("{")
    lines.append("  if (AppAudioAssets_IsValidId(asset_id) == 0U)")
    lines.append("  {")
    lines.append("    return (const app_audio_adpcm_clip_t *)0;")
    lines.append("  }")
    lines.append("  return kAudioClipTable[asset_id];")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def _generate_audio_assets(
    repo_root: Path,
    knobs: dict[str, Any],
    audio_files: list[str],
    source_audio_dir: Path,
    source_audio_rel: Path,
) -> str:
    audio_dir = source_audio_dir
    output_h = repo_root / "Core" / "Inc" / "audio_assets.h"
    output_c = repo_root / "Core" / "Src" / "audio_assets.c"

    sample_rate = int(knobs.get("audio_sample_rate", 16000))
    if sample_rate <= 0:
        raise ValueError("audio_sample_rate must be > 0")

    encoded_assets: list[tuple[str, bytes, int, int, int, int]] = []
    for source_name in audio_files:
        wav_path = audio_dir / source_name
        payload, total_samples, hz, block_align, samples_per_block = _load_wav_as_runtime_clip(wav_path, sample_rate)
        encoded_assets.append((source_name, payload, total_samples, hz, block_align, samples_per_block))

    header_text = "\n".join(
        [
            "#ifndef AUDIO_ASSETS_H",
            "#define AUDIO_ASSETS_H",
            "",
            "#include <stdint.h>",
            "",
            "typedef struct",
            "{",
            "  const uint8_t *data;",
            "  uint32_t data_size;",
            "  uint32_t sample_rate_hz;",
            "  uint16_t block_align;",
            "  uint16_t samples_per_block;",
            "  uint32_t total_samples;",
            "} app_audio_adpcm_clip_t;",
            "",
            "#ifndef APP_AUDIO_ASSET_NONE",
            "#define APP_AUDIO_ASSET_NONE (0U)",
            "#endif",
        ]
    )
    macro_lines: list[str] = []
    for ix, source_name in enumerate(audio_files, start=1):
        token = _sanitize_macro_token(Path(source_name).stem)
        macro_lines.append(f"#define APP_AUDIO_ASSET_{token} ({ix}U)")
    if macro_lines:
        header_text += "\n" + "\n".join(macro_lines)
    header_text += "\n\n"
    header_text += "\n".join(
        [
            "uint32_t AppAudioAssets_Count(void);",
            "uint8_t AppAudioAssets_IsValidId(uint32_t asset_id);",
            "const char *AppAudioAssets_Name(uint32_t asset_id);",
            "const app_audio_adpcm_clip_t *AppAudioAssets_GetClip(uint32_t asset_id);",
            "",
            "#endif /* AUDIO_ASSETS_H */",
            "",
        ]
    )
    c_text = _render_audio_assets_c(encoded_assets, source_audio_rel)

    output_h.parent.mkdir(parents=True, exist_ok=True)
    output_c.parent.mkdir(parents=True, exist_ok=True)
    output_h.write_text(header_text, encoding="utf-8", newline="\n")
    output_c.write_text(c_text, encoding="utf-8", newline="\n")

    return ", ".join([f"{ix+1}:{name}" for ix, name in enumerate(audio_files)])


def _write_schema(path: Path, schema: dict[str, Any]) -> None:
    text = json.dumps(schema, indent=2, ensure_ascii=False) + "\n"
    path.write_text(text, encoding="utf-8", newline="\n")


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--audio-only",
        action="store_true",
        help="Only sync audio schema/options and regenerate audio_assets.c/.h.",
    )
    parser.add_argument(
        "--knobs-only",
        action="store_true",
        help="Only regenerate knobs_autogen.h (still syncs schema defaults/options).",
    )
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    if args.audio_only and args.knobs_only:
        raise ValueError("--audio-only and --knobs-only are mutually exclusive")

    repo_root = Path(__file__).resolve().parents[1]
    source_path = repo_root / "config" / "knobs.json"
    schema_path = repo_root / "config" / "knobs.schema.json"
    output_path = repo_root / "Core" / "Inc" / "knobs_autogen.h"

    knobs_raw = json.loads(source_path.read_text(encoding="utf-8"))
    if not isinstance(knobs_raw, dict):
        raise TypeError("knobs.json top-level value must be an object")
    schema_raw = json.loads(schema_path.read_text(encoding="utf-8"))
    if not isinstance(schema_raw, dict):
        raise TypeError("knobs.schema.json top-level value must be an object")

    system_audio_dir, system_audio_rel = _resolve_system_audio_dir(repo_root)
    audio_files = _list_audio_files(system_audio_dir)
    embedded_audio_files = _resolve_embedded_audio_files(
        repo_root,
        audio_files,
        system_audio_rel,
        allow_manifest_sync=(not args.knobs_only),
    )
    schema_raw, schema_changed = _sync_audio_schema(schema_raw, embedded_audio_files)
    if schema_changed:
        _write_schema(schema_path, schema_raw)
        print("Updated config/knobs.schema.json audio options.")
    print(f"Audio source folder: {system_audio_rel.as_posix()}")
    print(f"Audio embedding set: {len(embedded_audio_files)} / {len(audio_files)} files")

    knobs = {str(key): value for key, value in knobs_raw.items()}
    knobs = _inject_schema_defaults(knobs, schema_raw)
    knobs = _migrate_and_clamp_audio_knobs(knobs, embedded_audio_files, audio_files)
    schema_props = schema_raw.get("properties", {})
    if isinstance(schema_props, dict):
        knobs = {k: v for k, v in knobs.items() if k in schema_props}
    _validate_required(schema_raw, knobs)
    _validate(knobs)

    if not args.audio_only:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        source_rel = source_path.relative_to(repo_root).as_posix()
        output_path.write_text(_render_header(knobs, source_rel), encoding="utf-8", newline="\n")
        print("Generated Core/Inc/knobs_autogen.h.")

    if not args.knobs_only:
        summary = _generate_audio_assets(
            repo_root,
            knobs,
            embedded_audio_files,
            system_audio_dir,
            system_audio_rel,
        )
        print("Generated Core/Inc/audio_assets.h and Core/Src/audio_assets.c.")
        print(f"Audio assets: {summary}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
