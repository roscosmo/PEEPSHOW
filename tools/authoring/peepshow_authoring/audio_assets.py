"""Deterministic sampled-SFX import and IMA ADPCM conversion."""

from __future__ import annotations

import io
import struct
import wave
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


AUDIO_SAMPLE_RATE_HZ = 16000
AUDIO_CHANNELS = 1
AUDIO_BLOCK_SAMPLES = 256
AUDIO_MAX_DURATION_MS = 2000
AUDIO_MAX_ASSETS = 32
AUDIO_MAX_CUES = 64
AUDIO_MAX_BANK_BYTES = 48 * 1024

_BLOCK_HEADER = struct.Struct("<hBBH")
_INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8)
_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
)


class AudioAssetError(ValueError):
    """Raised when source audio cannot enter the bounded STATE SFX subset."""


@dataclass(frozen=True)
class CompiledAudioAsset:
    asset_id: str
    source_path: str
    source_sample_rate_hz: int
    source_channels: int
    sample_rate_hz: int
    channels: int
    sample_count: int
    duration_ms: int
    decoded_pcm_bytes: int
    block_samples: int
    block_count: int
    adpcm: bytes


def _clamp_pcm(value: int) -> int:
    return max(-32768, min(32767, value))


def _divide_toward_zero(value: int, divisor: int) -> int:
    return value // divisor if value >= 0 else -((-value) // divisor)


def _decode_pcm(raw: bytes, sample_width: int, channels: int) -> list[int]:
    frame_bytes = sample_width * channels
    if len(raw) % frame_bytes:
        raise AudioAssetError("WAV payload does not contain complete PCM frames")
    samples: list[int] = []
    for frame_offset in range(0, len(raw), frame_bytes):
        total = 0
        for channel in range(channels):
            offset = frame_offset + channel * sample_width
            if sample_width == 1:
                value = (raw[offset] - 128) << 8
            elif sample_width == 2:
                value = int.from_bytes(raw[offset : offset + 2], "little", signed=True)
            elif sample_width == 3:
                packed = raw[offset : offset + 3]
                extended = packed + (b"\xff" if packed[2] & 0x80 else b"\x00")
                value = int.from_bytes(extended, "little", signed=True) >> 8
            else:
                value = int.from_bytes(raw[offset : offset + 4], "little", signed=True) >> 16
            total += value
        samples.append(_clamp_pcm(_divide_toward_zero(total, channels)))
    return samples


def _resample_linear(samples: list[int], source_rate_hz: int) -> list[int]:
    if source_rate_hz == AUDIO_SAMPLE_RATE_HZ:
        return samples
    output_count = max(
        1,
        (len(samples) * AUDIO_SAMPLE_RATE_HZ + source_rate_hz // 2)
        // source_rate_hz,
    )
    output: list[int] = []
    for index in range(output_count):
        position = index * source_rate_hz
        first = position // AUDIO_SAMPLE_RATE_HZ
        fraction = position % AUDIO_SAMPLE_RATE_HZ
        if first >= len(samples) - 1:
            output.append(samples[-1])
            continue
        weighted = (
            samples[first] * (AUDIO_SAMPLE_RATE_HZ - fraction)
            + samples[first + 1] * fraction
        )
        output.append(_clamp_pcm(_divide_toward_zero(weighted, AUDIO_SAMPLE_RATE_HZ)))
    return output


def _encode_nibble(sample: int, predictor: int, step_index: int) -> tuple[int, int, int]:
    step = _STEP_TABLE[step_index]
    difference = sample - predictor
    nibble = 8 if difference < 0 else 0
    remaining = -difference if difference < 0 else difference
    delta = step >> 3
    if remaining >= step:
        nibble |= 4
        remaining -= step
        delta += step
    if remaining >= step >> 1:
        nibble |= 2
        remaining -= step >> 1
        delta += step >> 1
    if remaining >= step >> 2:
        nibble |= 1
        delta += step >> 2
    predictor = predictor - delta if nibble & 8 else predictor + delta
    predictor = _clamp_pcm(predictor)
    step_index = max(0, min(88, step_index + _INDEX_TABLE[nibble & 7]))
    return nibble, predictor, step_index


def _decode_nibble(nibble: int, predictor: int, step_index: int) -> tuple[int, int]:
    step = _STEP_TABLE[step_index]
    delta = step >> 3
    if nibble & 4:
        delta += step
    if nibble & 2:
        delta += step >> 1
    if nibble & 1:
        delta += step >> 2
    predictor = predictor - delta if nibble & 8 else predictor + delta
    predictor = _clamp_pcm(predictor)
    step_index = max(0, min(88, step_index + _INDEX_TABLE[nibble & 7]))
    return predictor, step_index


def encode_ima_adpcm(samples: Iterable[int]) -> tuple[bytes, int]:
    source = tuple(_clamp_pcm(int(sample)) for sample in samples)
    if not source:
        raise AudioAssetError("sampled SFX must contain at least one PCM sample")
    encoded = bytearray()
    block_count = 0
    for first in range(0, len(source), AUDIO_BLOCK_SAMPLES):
        block = source[first : first + AUDIO_BLOCK_SAMPLES]
        predictor = block[0]
        step_index = 0
        encoded.extend(_BLOCK_HEADER.pack(predictor, step_index, 0, len(block)))
        pending = 0
        low_ready = False
        for sample in block[1:]:
            nibble, predictor, step_index = _encode_nibble(
                sample, predictor, step_index
            )
            if not low_ready:
                pending = nibble
                low_ready = True
            else:
                encoded.append(pending | (nibble << 4))
                low_ready = False
        if low_ready:
            encoded.append(pending)
        block_count += 1
    return bytes(encoded), block_count


def decode_ima_adpcm(
    payload: bytes,
    expected_samples: int,
    expected_blocks: int,
    block_samples: int = AUDIO_BLOCK_SAMPLES,
) -> tuple[int, ...]:
    if expected_samples <= 0 or expected_blocks <= 0:
        raise AudioAssetError("ADPCM metadata must describe non-empty audio")
    offset = 0
    output: list[int] = []
    for block_index in range(expected_blocks):
        if offset + _BLOCK_HEADER.size > len(payload):
            raise AudioAssetError("ADPCM block header is truncated")
        predictor, step_index, reserved, sample_count = _BLOCK_HEADER.unpack_from(
            payload, offset
        )
        offset += _BLOCK_HEADER.size
        if (
            reserved != 0
            or step_index > 88
            or sample_count == 0
            or sample_count > block_samples
        ):
            raise AudioAssetError("ADPCM block header is invalid")
        if block_index + 1 < expected_blocks and sample_count != block_samples:
            raise AudioAssetError("non-final ADPCM block is short")
        nibble_count = sample_count - 1
        encoded_bytes = (nibble_count + 1) // 2
        if offset + encoded_bytes > len(payload):
            raise AudioAssetError("ADPCM block payload is truncated")
        block_payload = payload[offset : offset + encoded_bytes]
        offset += encoded_bytes
        if nibble_count & 1 and block_payload and block_payload[-1] & 0xF0:
            raise AudioAssetError("ADPCM block padding nibble is non-zero")
        output.append(predictor)
        for nibble_index in range(nibble_count):
            packed = block_payload[nibble_index // 2]
            nibble = packed & 0x0F if nibble_index % 2 == 0 else packed >> 4
            predictor, step_index = _decode_nibble(nibble, predictor, step_index)
            output.append(predictor)
    if offset != len(payload) or len(output) != expected_samples:
        raise AudioAssetError("ADPCM payload size does not match its metadata")
    return tuple(output)


def pcm16_wav(samples: Iterable[int]) -> bytes:
    pcm = b"".join(struct.pack("<h", _clamp_pcm(int(sample))) for sample in samples)
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(AUDIO_CHANNELS)
        wav.setsampwidth(2)
        wav.setframerate(AUDIO_SAMPLE_RATE_HZ)
        wav.writeframes(pcm)
    return output.getvalue()


def import_sampled_sfx(project_root: Path, record: dict[str, object]) -> CompiledAudioAsset:
    source_path = record.get("source_path")
    if not isinstance(source_path, str) or not source_path:
        raise AudioAssetError("sampled SFX source_path must be non-empty text")
    relative = Path(source_path)
    if relative.is_absolute() or ".." in relative.parts:
        raise AudioAssetError("sampled SFX source must stay inside the project")
    path = (project_root / relative).resolve()
    try:
        path.relative_to(project_root.resolve())
    except ValueError as exc:
        raise AudioAssetError("sampled SFX source resolves outside the project") from exc
    try:
        with wave.open(str(path), "rb") as wav:
            channels = wav.getnchannels()
            sample_width = wav.getsampwidth()
            source_rate_hz = wav.getframerate()
            frame_count = wav.getnframes()
            if wav.getcomptype() != "NONE":
                raise AudioAssetError("sampled SFX WAV must contain uncompressed PCM")
            if channels not in {1, 2}:
                raise AudioAssetError("sampled SFX WAV must be mono or stereo")
            if sample_width not in {1, 2, 3, 4}:
                raise AudioAssetError("sampled SFX WAV must use 8, 16, 24, or 32-bit PCM")
            if not 8000 <= source_rate_hz <= 96000:
                raise AudioAssetError("sampled SFX WAV rate must be in 8000..96000 Hz")
            source_duration_ms = (
                frame_count * 1000 + source_rate_hz - 1
            ) // source_rate_hz
            if source_duration_ms > AUDIO_MAX_DURATION_MS:
                raise AudioAssetError(
                    f"sampled SFX exceeds the {AUDIO_MAX_DURATION_MS} ms STATE limit"
                )
            raw = wav.readframes(frame_count)
    except (FileNotFoundError, OSError, EOFError, wave.Error) as exc:
        raise AudioAssetError(f"could not read sampled SFX WAV: {exc}") from exc
    samples = _resample_linear(_decode_pcm(raw, sample_width, channels), source_rate_hz)
    duration_ms = (len(samples) * 1000 + AUDIO_SAMPLE_RATE_HZ // 2) // AUDIO_SAMPLE_RATE_HZ
    if duration_ms > AUDIO_MAX_DURATION_MS:
        raise AudioAssetError(
            f"converted SFX exceeds the {AUDIO_MAX_DURATION_MS} ms STATE limit"
        )
    adpcm, block_count = encode_ima_adpcm(samples)
    return CompiledAudioAsset(
        asset_id=str(record["asset_id"]),
        source_path=source_path,
        source_sample_rate_hz=source_rate_hz,
        source_channels=channels,
        sample_rate_hz=AUDIO_SAMPLE_RATE_HZ,
        channels=AUDIO_CHANNELS,
        sample_count=len(samples),
        duration_ms=duration_ms,
        decoded_pcm_bytes=len(samples) * 2,
        block_samples=AUDIO_BLOCK_SAMPLES,
        block_count=block_count,
        adpcm=adpcm,
    )
