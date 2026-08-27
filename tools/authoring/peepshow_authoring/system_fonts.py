"""Frozen system-font resources for deterministic build-time text sprites."""

from __future__ import annotations

from typing import Any

from .image_assets import MAX_FRAME_HEIGHT, MAX_FRAME_WIDTH, Masked1bppFrame


SYSTEM_FONT_8X8_BASIC_ID = "peepshow.system.8x8.basic.v1"
SYSTEM_FONT_FIRST_CODEPOINT = 0x20
SYSTEM_FONT_LAST_CODEPOINT = 0x7E
SYSTEM_FONT_GLYPH_WIDTH = 8
SYSTEM_FONT_GLYPH_HEIGHT = 8
SYSTEM_FONT_SCALE_MIN = 1
SYSTEM_FONT_SCALE_MAX = 8
SYSTEM_FONT_TEXT_MAX = 256

# Frozen from firmware/peepshow_hw4_fw1/Core/Src/font8x8_basic.c. Each glyph
# contains eight rows and uses the HW4 table's least-significant-bit-left order.
_FONT_8X8_BASIC = bytes.fromhex(
    """
    0000000000000000 183C3C1818001800 3636000000000000 36367F367F363600
    0C3E031E301F0C00 006333180C666300 1C361C6E3B336E00 0606030000000000
    180C0606060C1800 060C1818180C0600 00663CFF3C660000 000C0C3F0C0C0000
    00000000000C0C06 0000003F00000000 00000000000C0C00 6030180C06030100
    3E63737B6F673E00 0C0E0C0C0C0C3F00 1E33301C06333F00 1E33301C30331E00
    383C36337F307800 3F031F3030331E00 1C06031F33331E00 3F3330180C0C0C00
    1E33331E33331E00 1E33333E30180E00 000C0C00000C0C00 000C0C00000C0C06
    180C0603060C1800 00003F00003F0000 060C1830180C0600 1E3330180C000C00
    3E637B7B7B031E00 0C1E33333F333300 3F66663E66663F00 3C66030303663C00
    1F36666666361F00 7F46161E16467F00 7F46161E16060F00 3C66030373667C00
    3333333F33333300 1E0C0C0C0C0C1E00 7830303033331E00 6766361E36666700
    0F06060646667F00 63777F7F6B636300 63676F7B73636300 1C36636363361C00
    3F66663E06060F00 1E3333333B1E3800 3F66663E36666700 1E33070E38331E00
    3F2D0C0C0C0C1E00 3333333333333F00 33333333331E0C00 6363636B7F776300
    6363361C1C366300 3333331E0C0C1E00 7F6331184C667F00 1E06060606061E00
    03060C1830604000 1E18181818181E00 081C366300000000 00000000000000FF
    0C0C180000000000 00001E303E336E00 0706063E66663B00 00001E3303331E00
    3830303E33336E00 00001E333F031E00 1C36060F06060F00 00006E33333E301F
    0706366E66666700 0C000E0C0C0C1E00 300030303033331E 070666361E366700
    0E0C0C0C0C0C1E00 0000337F7F6B6300 00001F3333333300 00001E3333331E00
    00003B66663E060F 00006E33333E3078 00003B6E66060F00 00003E031E301F00
    080C3E0C0C2C1800 0000333333336E00 00003333331E0C00 0000636B7F7F3600
    000063361C366300 00003333333E301F 00003F190C263F00 380C0C070C0C3800
    1818180018181800 070C0C380C0C0700 6E3B000000000000
    """
)


class SystemFontError(ValueError):
    """Raised when an authored system-font text asset cannot be rasterized."""


def _pivot(frame: dict[str, Any], field: str) -> int:
    value = frame.get(field)
    if isinstance(value, bool) or not isinstance(value, int):
        raise SystemFontError(f"{field} must be an integer")
    if not -32768 <= value <= 32767:
        raise SystemFontError(f"{field} must fit a signed 16-bit value")
    return value


def _validate_text(text: Any) -> tuple[str, ...]:
    if not isinstance(text, str) or not text or len(text) > SYSTEM_FONT_TEXT_MAX:
        raise SystemFontError(f"text must contain 1..{SYSTEM_FONT_TEXT_MAX} characters")
    for character in text:
        codepoint = ord(character)
        if character != "\n" and not SYSTEM_FONT_FIRST_CODEPOINT <= codepoint <= SYSTEM_FONT_LAST_CODEPOINT:
            raise SystemFontError("text supports printable ASCII and newline only")
    if not any(character not in {" ", "\n"} for character in text):
        raise SystemFontError("text must contain at least one visible glyph")
    return tuple(text.split("\n"))


def rasterize_system_font_text(asset: dict[str, Any]) -> Masked1bppFrame:
    if asset.get("font_id") != SYSTEM_FONT_8X8_BASIC_ID:
        raise SystemFontError(f"font_id must be {SYSTEM_FONT_8X8_BASIC_ID}")
    scale = asset.get("scale")
    if isinstance(scale, bool) or not isinstance(scale, int) or not SYSTEM_FONT_SCALE_MIN <= scale <= SYSTEM_FONT_SCALE_MAX:
        raise SystemFontError(f"scale must be in {SYSTEM_FONT_SCALE_MIN}..{SYSTEM_FONT_SCALE_MAX}")
    lines = _validate_text(asset.get("text"))
    width = max(len(line) for line in lines) * SYSTEM_FONT_GLYPH_WIDTH * scale
    height = len(lines) * SYSTEM_FONT_GLYPH_HEIGHT * scale
    if not 1 <= width <= MAX_FRAME_WIDTH or not 1 <= height <= MAX_FRAME_HEIGHT:
        raise SystemFontError(f"rasterized text must fit {MAX_FRAME_WIDTH}x{MAX_FRAME_HEIGHT}")

    frames = asset.get("frames")
    if not isinstance(frames, list) or len(frames) != 1 or not isinstance(frames[0], dict):
        raise SystemFontError("system-font text must declare exactly one frame")
    frame = frames[0]
    frame_id = frame.get("frame_id")
    if not isinstance(frame_id, str):
        raise SystemFontError("frame_id must be a string")
    pivot_x = _pivot(frame, "pivot_x")
    pivot_y = _pivot(frame, "pivot_y")

    stride = (width + 7) // 8
    pixels = bytearray(stride * height)
    for line_index, line in enumerate(lines):
        for character_index, character in enumerate(line):
            glyph_offset = (ord(character) - SYSTEM_FONT_FIRST_CODEPOINT) * SYSTEM_FONT_GLYPH_HEIGHT
            for glyph_y in range(SYSTEM_FONT_GLYPH_HEIGHT):
                source_row = _FONT_8X8_BASIC[glyph_offset + glyph_y]
                for glyph_x in range(SYSTEM_FONT_GLYPH_WIDTH):
                    if source_row & (1 << glyph_x) == 0:
                        continue
                    output_x = (character_index * SYSTEM_FONT_GLYPH_WIDTH + glyph_x) * scale
                    output_y = (line_index * SYSTEM_FONT_GLYPH_HEIGHT + glyph_y) * scale
                    for scale_y in range(scale):
                        row_offset = (output_y + scale_y) * stride
                        for scale_x in range(scale):
                            column = output_x + scale_x
                            pixels[row_offset + column // 8] |= 0x80 >> (column % 8)

    packed = bytes(pixels)
    return Masked1bppFrame(
        asset_id=asset["asset_id"],
        frame_id=frame_id,
        width=width,
        height=height,
        row_stride_bytes=stride,
        pivot_x=pivot_x,
        pivot_y=pivot_y,
        pixels=packed,
        mask=packed,
        opaque=False,
    )
