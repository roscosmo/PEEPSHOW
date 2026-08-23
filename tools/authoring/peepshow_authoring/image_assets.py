"""Deterministic source-image import for portable PeepShow sprite frames."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from PIL import Image, UnidentifiedImageError


MAX_SOURCE_DIMENSION = 4096
MAX_FRAME_WIDTH = 168
MAX_FRAME_HEIGHT = 144


class ImageAssetError(ValueError):
    """Raised when source art cannot compile to the declared pixel model."""


@dataclass(frozen=True)
class Masked1bppFrame:
    asset_id: str
    frame_id: str
    width: int
    height: int
    row_stride_bytes: int
    pivot_x: int
    pivot_y: int
    pixels: bytes
    mask: bytes
    opaque: bool


def resolve_project_path(root: Path, source: Any, field: str) -> Path:
    if not isinstance(source, str) or not source:
        raise ImageAssetError(f"{field} must be a non-empty relative path")
    relative = Path(source)
    if relative.is_absolute() or ".." in relative.parts:
        raise ImageAssetError(f"{field} must stay inside the project")
    path = (root / relative).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError as exc:
        raise ImageAssetError(f"{field} resolves outside the project") from exc
    return path


def _frame_rect(frame: dict[str, Any]) -> tuple[int, int, int, int]:
    rect = frame.get("source_rect")
    if not isinstance(rect, dict) or set(rect) != {"x", "y", "width", "height"}:
        raise ImageAssetError("source_rect must contain x, y, width, and height")
    values = tuple(rect.get(field) for field in ("x", "y", "width", "height"))
    if any(isinstance(value, bool) or not isinstance(value, int) for value in values):
        raise ImageAssetError("source_rect values must be integers")
    x, y, width, height = values
    if x < 0 or y < 0 or not 1 <= width <= MAX_FRAME_WIDTH or not 1 <= height <= MAX_FRAME_HEIGHT:
        raise ImageAssetError(
            f"source_rect must be positive and no larger than {MAX_FRAME_WIDTH}x{MAX_FRAME_HEIGHT}"
        )
    return x, y, width, height


def _pack_frame(
    image: Image.Image,
    asset_id: str,
    frame: dict[str, Any],
) -> Masked1bppFrame:
    x, y, width, height = _frame_rect(frame)
    if x + width > image.width or y + height > image.height:
        raise ImageAssetError("source_rect is outside the PNG bounds")
    pivot_x = frame.get("pivot_x")
    pivot_y = frame.get("pivot_y")
    if any(isinstance(value, bool) or not isinstance(value, int) for value in (pivot_x, pivot_y)):
        raise ImageAssetError("pivot_x and pivot_y must be integers")
    if not -32768 <= pivot_x <= 32767 or not -32768 <= pivot_y <= 32767:
        raise ImageAssetError("pivot_x and pivot_y must fit signed 16-bit values")

    rgba = image.crop((x, y, x + width, y + height)).convert("RGBA")
    stride = (width + 7) // 8
    pixels = bytearray(stride * height)
    mask = bytearray(stride * height)
    opaque = True
    for row in range(height):
        for column in range(width):
            red, green, blue, alpha = rgba.getpixel((column, row))
            if alpha == 0:
                opaque = False
                continue
            if (red, green, blue) == (0, 0, 0):
                pixels[row * stride + column // 8] |= 0x80 >> (column % 8)
            elif (red, green, blue) != (255, 255, 255):
                raise ImageAssetError(
                    f"visible pixel ({column + x},{row + y}) is not exact black or white"
                )
            mask[row * stride + column // 8] |= 0x80 >> (column % 8)

    return Masked1bppFrame(
        asset_id=asset_id,
        frame_id=frame["frame_id"],
        width=width,
        height=height,
        row_stride_bytes=stride,
        pivot_x=pivot_x,
        pivot_y=pivot_y,
        pixels=bytes(pixels),
        mask=b"" if opaque else bytes(mask),
        opaque=opaque,
    )


def import_masked_1bpp(root: Path, asset: dict[str, Any]) -> tuple[Masked1bppFrame, ...]:
    source = resolve_project_path(root, asset.get("source_path"), "source_path")
    try:
        with Image.open(source) as opened:
            if opened.format != "PNG":
                raise ImageAssetError("source_format png requires a PNG file")
            if not 1 <= opened.width <= MAX_SOURCE_DIMENSION or not 1 <= opened.height <= MAX_SOURCE_DIMENSION:
                raise ImageAssetError(f"PNG dimensions must be in 1..{MAX_SOURCE_DIMENSION}")
            opened.load()
            return tuple(_pack_frame(opened, asset["asset_id"], frame) for frame in asset["frames"])
    except FileNotFoundError as exc:
        raise ImageAssetError("source PNG does not exist") from exc
    except (OSError, UnidentifiedImageError) as exc:
        raise ImageAssetError(f"source PNG could not be decoded: {exc}") from exc
