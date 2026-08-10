#!/usr/bin/env python3
"""Generate the HW6 FW0 compile-time knobs header."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


KEY_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")


def _format_value(value: Any) -> str:
    if isinstance(value, bool):
        return "(1)" if value else "(0)"
    if isinstance(value, int):
        return f"({value})"
    if isinstance(value, float):
        return f"({format(value, '.15g')})"
    raise TypeError(f"unsupported knob value type: {type(value).__name__}")


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise TypeError(f"{path} must contain a JSON object")
    return data


def _validate_required(schema: dict[str, Any], knobs: dict[str, Any]) -> None:
    required = schema.get("required")
    if not isinstance(required, list):
        return
    missing = [str(k) for k in required if str(k) not in knobs]
    if missing:
        raise ValueError("missing required knobs: " + ", ".join(sorted(missing)))


def _validate_bounds(schema: dict[str, Any], knobs: dict[str, Any]) -> None:
    properties = schema.get("properties")
    if not isinstance(properties, dict):
        return

    for key, value in knobs.items():
        prop = properties.get(key)
        if not isinstance(prop, dict):
            raise ValueError(f"knob {key!r} is not defined in schema")

        expected_type = prop.get("type")
        if expected_type == "integer":
            if isinstance(value, bool) or not isinstance(value, int):
                raise TypeError(f"knob {key!r} must be an integer")
            minimum = prop.get("minimum")
            maximum = prop.get("maximum")
            if isinstance(minimum, int) and value < minimum:
                raise ValueError(f"knob {key!r} is below minimum {minimum}")
            if isinstance(maximum, int) and value > maximum:
                raise ValueError(f"knob {key!r} is above maximum {maximum}")
        elif expected_type == "boolean":
            if not isinstance(value, bool):
                raise TypeError(f"knob {key!r} must be a boolean")
        elif expected_type is not None:
            raise TypeError(f"unsupported schema type for {key!r}: {expected_type!r}")


def _validate_knobs(knobs: dict[str, Any]) -> None:
    if not knobs:
        raise ValueError("knobs.json must contain at least one knob")
    for key, value in knobs.items():
        if not KEY_PATTERN.match(key):
            raise ValueError(f"invalid knob name: {key!r}")
        if key.startswith("knob_"):
            raise ValueError(f"knob name must not start with 'knob_': {key!r}")
        if isinstance(value, (dict, list)) or value is None:
            raise TypeError(f"unsupported knob value for {key!r}: {type(value).__name__}")


def _render_header(knobs: dict[str, Any]) -> str:
    lines: list[str] = [
        "/* AUTO-GENERATED FILE. DO NOT EDIT. */",
        "/* Source: config/knobs.json */",
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


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    knobs_path = root / "config" / "knobs.json"
    schema_path = root / "config" / "knobs.schema.json"
    header_path = root / "Core" / "Inc" / "knobs_autogen.h"

    knobs = _load_json(knobs_path)
    schema = _load_json(schema_path)
    _validate_knobs(knobs)
    _validate_required(schema, knobs)
    _validate_bounds(schema, knobs)

    header_path.write_text(_render_header(knobs), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()