"""Canonical target-profile loading shared by authoring and firmware generation."""

from __future__ import annotations

import hashlib
import json
from copy import deepcopy
from pathlib import Path
from typing import Any


TARGET_PROFILE_SOURCE_REF = (
    "tools/authoring/peepshow_authoring/target_profiles/"
    "hw6_fw0_development.json"
)
TARGET_PROFILE_PATH = Path(__file__).with_name("target_profiles") / (
    "hw6_fw0_development.json"
)
TARGET_PROFILE_HEADER_PATH = (
    Path(__file__).resolve().parents[3]
    / "firmware"
    / "peepshow_hw6_fw0"
    / "Core"
    / "Inc"
    / "ps_target_profile_autogen.h"
)


class TargetProfileError(ValueError):
    """Raised when the canonical target profile is missing or inconsistent."""


def _canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("ascii")


def _require_keys(value: dict[str, Any], required: set[str], path: str) -> None:
    actual = set(value)
    if actual != required:
        missing = sorted(required - actual)
        extra = sorted(actual - required)
        raise TargetProfileError(
            f"{path} keys invalid; missing={missing}, extra={extra}"
        )


def _positive_int(value: Any, path: str, maximum: int = 0xFFFFFFFF) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TargetProfileError(f"{path} must be an integer")
    if value <= 0 or value > maximum:
        raise TargetProfileError(f"{path} must be in 1..{maximum}")
    return value


def _validate_target_profile(profile: dict[str, Any]) -> None:
    _require_keys(
        profile,
        {
            "schema_id",
            "schema_version",
            "profile_id",
            "profile_version",
            "profile_status",
            "package",
            "audio",
        },
        "target_profile",
    )
    if profile["schema_id"] != "peepshow.target_profile":
        raise TargetProfileError("target_profile.schema_id is unsupported")
    if profile["schema_version"] != 1:
        raise TargetProfileError("target_profile.schema_version is unsupported")
    if profile["profile_id"] != "hw6_fw0_development":
        raise TargetProfileError("target_profile.profile_id is unsupported")
    _positive_int(profile["profile_version"], "target_profile.profile_version")
    if profile["profile_status"] not in {"pending_validation", "shipping"}:
        raise TargetProfileError("target_profile.profile_status is unsupported")

    package = profile["package"]
    if not isinstance(package, dict):
        raise TargetProfileError("target_profile.package must be an object")
    _require_keys(package, {"maximum_bytes", "resident_prefix_bytes"}, "package")
    maximum_bytes = _positive_int(package["maximum_bytes"], "package.maximum_bytes")
    resident_bytes = _positive_int(
        package["resident_prefix_bytes"], "package.resident_prefix_bytes"
    )
    if resident_bytes > maximum_bytes:
        raise TargetProfileError("package resident prefix exceeds package maximum")

    audio = profile["audio"]
    if not isinstance(audio, dict):
        raise TargetProfileError("target_profile.audio must be an object")
    _require_keys(audio, {"sampled_sfx"}, "audio")
    sampled_sfx = audio["sampled_sfx"]
    if not isinstance(sampled_sfx, dict):
        raise TargetProfileError("audio.sampled_sfx must be an object")
    _require_keys(
        sampled_sfx,
        {
            "target_playback_status",
            "compiled_format",
            "sample_rate_hz",
            "channels",
            "block_samples",
            "maximum_duration_ms",
            "maximum_assets",
            "maximum_cues",
            "maximum_bank_bytes",
            "voice_limit",
            "package_window_bytes",
            "package_window_count",
            "route_action",
            "survives_same_package_scene_replacement",
            "unsupported",
        },
        "audio.sampled_sfx",
    )
    if sampled_sfx["target_playback_status"] != (
        "available_package_streamed_state_sfx"
    ):
        raise TargetProfileError("sampled SFX playback status is unsupported")
    if sampled_sfx["compiled_format"] != "ima_adpcm_4bit":
        raise TargetProfileError("sampled SFX compiled format is unsupported")
    _positive_int(sampled_sfx["sample_rate_hz"], "audio.sample_rate_hz", 65535)
    if sampled_sfx["channels"] != 1:
        raise TargetProfileError("sampled SFX must be mono")
    _positive_int(sampled_sfx["block_samples"], "audio.block_samples", 65535)
    duration_ms = sampled_sfx["maximum_duration_ms"]
    if duration_ms is not None:
        _positive_int(duration_ms, "audio.maximum_duration_ms")
    _positive_int(sampled_sfx["maximum_assets"], "audio.maximum_assets", 65535)
    _positive_int(sampled_sfx["maximum_cues"], "audio.maximum_cues", 65535)
    bank_bytes = _positive_int(
        sampled_sfx["maximum_bank_bytes"], "audio.maximum_bank_bytes"
    )
    _positive_int(sampled_sfx["voice_limit"], "audio.voice_limit", 65535)
    window_bytes = _positive_int(
        sampled_sfx["package_window_bytes"], "audio.package_window_bytes"
    )
    _positive_int(
        sampled_sfx["package_window_count"], "audio.package_window_count", 65535
    )
    if bank_bytes > maximum_bytes:
        raise TargetProfileError("audio bank exceeds package maximum")
    if window_bytes > resident_bytes:
        raise TargetProfileError("audio package window exceeds resident prefix")
    if sampled_sfx["route_action"] != "play_sfx":
        raise TargetProfileError("sampled SFX route action is unsupported")
    if sampled_sfx["survives_same_package_scene_replacement"] is not True:
        raise TargetProfileError("sampled SFX scene-transition policy is unsupported")
    unsupported = sampled_sfx["unsupported"]
    if (
        not isinstance(unsupported, list)
        or any(not isinstance(item, str) or not item for item in unsupported)
        or len(set(unsupported)) != len(unsupported)
    ):
        raise TargetProfileError("audio.sampled_sfx.unsupported must be unique strings")


def _load_target_profile() -> dict[str, Any]:
    try:
        profile = json.loads(TARGET_PROFILE_PATH.read_text(encoding="ascii"))
    except (OSError, json.JSONDecodeError) as exc:
        raise TargetProfileError(f"could not load target profile: {exc}") from exc
    if not isinstance(profile, dict):
        raise TargetProfileError("target profile root must be an object")
    _validate_target_profile(profile)
    return profile


TARGET_PROFILE = _load_target_profile()
TARGET_PROFILE_ID = str(TARGET_PROFILE["profile_id"])
TARGET_PROFILE_HASH = hashlib.sha256(_canonical_bytes(TARGET_PROFILE)).hexdigest()
SUPPORTED_TARGET_PROFILE_IDS = frozenset({TARGET_PROFILE_ID})
TARGET_SAMPLED_SFX = TARGET_PROFILE["audio"]["sampled_sfx"]


def target_profile_for_id(profile_id: str) -> dict[str, Any]:
    if profile_id != TARGET_PROFILE_ID:
        raise TargetProfileError(f"unsupported target profile: {profile_id}")
    return deepcopy(TARGET_PROFILE)


def public_target_profile() -> dict[str, Any]:
    return {
        "profile_id": TARGET_PROFILE_ID,
        "profile_version": TARGET_PROFILE["profile_version"],
        "profile_status": TARGET_PROFILE["profile_status"],
        "profile_hash": TARGET_PROFILE_HASH,
        "package": deepcopy(TARGET_PROFILE["package"]),
        "audio": deepcopy(TARGET_PROFILE["audio"]),
    }


def render_firmware_header() -> str:
    package = TARGET_PROFILE["package"]
    sampled_sfx = TARGET_SAMPLED_SFX
    survives_scene = (
        1 if sampled_sfx["survives_same_package_scene_replacement"] else 0
    )
    return "\n".join(
        (
            "/* Generated by gen_target_profile.py. Do not edit manually. */",
            "#ifndef PS_TARGET_PROFILE_AUTOGEN_H",
            "#define PS_TARGET_PROFILE_AUTOGEN_H",
            "",
            f'#define PS_TARGET_PROFILE_ID ("{TARGET_PROFILE_ID}")',
            f'#define PS_TARGET_PROFILE_HASH ("{TARGET_PROFILE_HASH}")',
            "#define PS_TARGET_PROFILE_VERSION "
            f"({TARGET_PROFILE['profile_version']}UL)",
            "#define PS_TARGET_PROFILE_PACKAGE_MAX_BYTES "
            f"({package['maximum_bytes']}UL)",
            "#define PS_TARGET_PROFILE_PACKAGE_RESIDENT_BYTES "
            f"({package['resident_prefix_bytes']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_SAMPLE_RATE_HZ "
            f"({sampled_sfx['sample_rate_hz']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_CHANNELS "
            f"({sampled_sfx['channels']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_BLOCK_SAMPLES "
            f"({sampled_sfx['block_samples']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_ASSET_MAX "
            f"({sampled_sfx['maximum_assets']}U)",
            "#define PS_TARGET_PROFILE_AUDIO_CUE_MAX "
            f"({sampled_sfx['maximum_cues']}U)",
            "#define PS_TARGET_PROFILE_AUDIO_BANK_MAX_BYTES "
            f"({sampled_sfx['maximum_bank_bytes']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_VOICE_LIMIT "
            f"({sampled_sfx['voice_limit']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_PACKAGE_WINDOW_BYTES "
            f"({sampled_sfx['package_window_bytes']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_PACKAGE_WINDOW_COUNT "
            f"({sampled_sfx['package_window_count']}UL)",
            "#define PS_TARGET_PROFILE_AUDIO_SURVIVES_SCENE_REPLACEMENT "
            f"({survives_scene}UL)",
            "",
            "#endif /* PS_TARGET_PROFILE_AUTOGEN_H */",
            "",
        )
    )
