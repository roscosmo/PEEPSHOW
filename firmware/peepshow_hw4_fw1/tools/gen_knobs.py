#!/usr/bin/env python3
"""Generate Core/Inc/knobs_autogen.h from config/knobs.json."""

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
    """Return a new knob map with schema-level defaults filled in for missing keys."""
    props = schema.get("properties")
    if not isinstance(props, dict):
        return dict(knobs)

    out = dict(knobs)
    for key, prop in props.items():
        if key in out:
            continue
        if not isinstance(prop, dict):
            continue
        if "default" in prop:
            out[str(key)] = prop.get("default")
    return out


def _validate_required(schema: dict[str, Any], knobs: dict[str, Any]) -> None:
    required = schema.get("required")
    if not isinstance(required, list):
        return
    missing = [str(k) for k in required if str(k) not in knobs]
    if missing:
        raise ValueError(
            "missing required knobs after applying defaults: " + ", ".join(sorted(missing))
        )


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


def main() -> int:
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

    knobs = {str(key): value for key, value in knobs_raw.items()}
    knobs = _inject_schema_defaults(knobs, schema_raw)
    _validate_required(schema_raw, knobs)
    _validate(knobs)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    source_rel = source_path.relative_to(repo_root).as_posix()
    header_text = _render_header(knobs, source_rel)
    output_path.write_text(header_text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
