"""Versioned newline-delimited JSON protocol for the authoring service."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from typing import Any


PROTOCOL_VERSION = 1
MAX_REQUEST_BYTES = 1024 * 1024
REQUEST_KEYS = {"protocol_version", "id", "operation", "params"}
OPERATION_NAME = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")


class ProtocolError(ValueError):
    """A recoverable request or service error returned to the client."""

    def __init__(
        self,
        code: str,
        message: str,
        *,
        request_id: str | int | None = None,
        details: dict[str, Any] | None = None,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.request_id = request_id
        self.details = details or {}


@dataclass(frozen=True)
class ServiceRequest:
    request_id: str | int
    operation: str
    params: dict[str, Any]


def _valid_request_id(value: Any) -> bool:
    return (
        (isinstance(value, str) and 1 <= len(value) <= 128)
        or (
            isinstance(value, int)
            and not isinstance(value, bool)
            and 0 <= value <= 0x7FFFFFFF
        )
    )


def parse_request(line: str) -> ServiceRequest:
    if len(line.encode("utf-8")) > MAX_REQUEST_BYTES:
        raise ProtocolError("REQUEST_TOO_LARGE", "request exceeds the 1 MiB protocol limit")
    try:
        value = json.loads(line)
    except json.JSONDecodeError as exc:
        raise ProtocolError(
            "REQUEST_JSON_INVALID",
            f"invalid JSON at column {exc.colno}",
        ) from exc
    if not isinstance(value, dict):
        raise ProtocolError("REQUEST_TYPE_INVALID", "request must be a JSON object")

    candidate_id = value.get("id")
    request_id = candidate_id if _valid_request_id(candidate_id) else None
    missing = REQUEST_KEYS - value.keys()
    unknown = value.keys() - REQUEST_KEYS
    if missing or unknown:
        raise ProtocolError(
            "REQUEST_FIELDS_INVALID",
            "request fields do not match the protocol",
            request_id=request_id,
            details={"missing": sorted(missing), "unknown": sorted(unknown)},
        )
    if value["protocol_version"] != PROTOCOL_VERSION:
        raise ProtocolError(
            "PROTOCOL_VERSION_UNSUPPORTED",
            f"expected protocol version {PROTOCOL_VERSION}",
            request_id=request_id,
            details={"supported_versions": [PROTOCOL_VERSION]},
        )
    if request_id is None:
        raise ProtocolError("REQUEST_ID_INVALID", "request ID must be a bounded string or integer")
    operation = value["operation"]
    if not isinstance(operation, str) or OPERATION_NAME.fullmatch(operation) is None:
        raise ProtocolError(
            "REQUEST_OPERATION_INVALID",
            "operation must be a dotted lowercase name",
            request_id=request_id,
        )
    params = value["params"]
    if not isinstance(params, dict):
        raise ProtocolError(
            "REQUEST_PARAMS_INVALID",
            "params must be a JSON object",
            request_id=request_id,
        )
    return ServiceRequest(request_id, operation, params)


def success_response(request_id: str | int, result: dict[str, Any]) -> dict[str, Any]:
    return {
        "protocol_version": PROTOCOL_VERSION,
        "id": request_id,
        "ok": True,
        "result": result,
    }


def error_response(error: ProtocolError) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "code": error.code,
        "message": error.message,
    }
    if error.details:
        payload["details"] = error.details
    return {
        "protocol_version": PROTOCOL_VERSION,
        "id": error.request_id,
        "ok": False,
        "error": payload,
    }


def encode_message(message: dict[str, Any]) -> str:
    return json.dumps(message, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
