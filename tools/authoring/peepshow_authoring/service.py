"""Long-running host service for the PeepShow authoring toolchain."""

from __future__ import annotations

import base64
import hashlib
import sys
from pathlib import Path
from typing import Any, TextIO

from .compatibility import build_compatibility_report
from .compiler import EggCompileError, build_egg
from .egg_format import EggFormatError, parse_egg
from .project import ProjectBundle, ProjectCommandError, apply_project_commands, load_project, save_project
from .preview import PreviewError, StateScenePreview
from .protocol import (
    PROTOCOL_VERSION,
    ProtocolError,
    ServiceRequest,
    encode_message,
    error_response,
    parse_request,
    success_response,
)


SERVICE_API_VERSION = 5
SERVICE_NAME = "peepshow_authoring"
SERVICE_OPERATIONS = (
    "service.hello",
    "service.shutdown",
    "project.load",
    "project.validate",
    "project.normalize",
    "project.build_package",
    "project.compatibility_report",
    "project.apply_commands",
    "project.save",
    "project.preview_reset",
    "project.preview_input",
    "project.preview_advance",
)


def _safe_issue_path(value: str) -> str:
    path = Path(value)
    if path.is_absolute():
        return path.name or "project"
    return value


def _issues(bundle: ProjectBundle) -> list[dict[str, str]]:
    return [
        {
            "code": issue.code,
            "path": _safe_issue_path(issue.path),
            "message": issue.message,
        }
        for issue in bundle.issues
    ]


def _project_summary(bundle: ProjectBundle) -> dict[str, Any]:
    project = bundle.project
    return {
        "project_id": project.get("project_id"),
        "project_name": project.get("project_name"),
        "package_id": project.get("package", {}).get("package_id"),
        "target_profile": project.get("selected_target_profile"),
        "entry_scene": project.get("entry_scene"),
        "scene_count": len(bundle.scenes),
        "asset_frame_count": len(bundle.frames),
        "animation_count": len(bundle.animations),
    }


def _require_fields(params: dict[str, Any], required: set[str]) -> None:
    missing = required - params.keys()
    unknown = params.keys() - required
    if missing or unknown:
        raise ProtocolError(
            "OPERATION_PARAMS_INVALID",
            "operation parameters do not match the service API",
            details={"missing": sorted(missing), "unknown": sorted(unknown)},
        )


class AuthoringService:
    """Single-session deterministic facade over the headless authoring API."""

    def __init__(self) -> None:
        self._bundle: ProjectBundle | None = None
        self._project_revision = 0
        self._preview: StateScenePreview | None = None
        self._preview_revision = 0
        self._dirty = False
        self.shutdown_requested = False

    def _current_bundle(
        self,
        params: dict[str, Any],
        operation_fields: set[str] | None = None,
    ) -> ProjectBundle:
        _require_fields(params, {"project_revision"} | (operation_fields or set()))
        if self._bundle is None:
            raise ProtocolError("PROJECT_NOT_LOADED", "load a project before this operation")
        revision = params["project_revision"]
        if not isinstance(revision, int) or isinstance(revision, bool):
            raise ProtocolError("PROJECT_REVISION_INVALID", "project_revision must be an integer")
        if revision != self._project_revision:
            raise ProtocolError(
                "PROJECT_REVISION_STALE",
                "operation targets an outdated project revision",
                details={"current_project_revision": self._project_revision},
            )
        return self._bundle

    def _hello(self, params: dict[str, Any]) -> dict[str, Any]:
        _require_fields(params, set())
        return {
            "service": SERVICE_NAME,
            "service_api_version": SERVICE_API_VERSION,
            "protocol_version": PROTOCOL_VERSION,
            "operations": list(SERVICE_OPERATIONS),
            "project_loaded": self._bundle is not None,
            "project_revision": self._project_revision if self._bundle is not None else None,
        }

    def _shutdown(self, params: dict[str, Any]) -> dict[str, Any]:
        _require_fields(params, set())
        self.shutdown_requested = True
        return {"shutdown": True}

    def _load(self, params: dict[str, Any]) -> dict[str, Any]:
        _require_fields(params, {"path"})
        path = params["path"]
        if not isinstance(path, str) or not path:
            raise ProtocolError("PROJECT_PATH_INVALID", "path must be non-empty text")
        bundle = load_project(Path(path))
        self._bundle = bundle
        self._project_revision += 1
        self._preview = None
        self._dirty = False
        return {
            "project_revision": self._project_revision,
            "source_name": bundle.root.name,
            "valid": bundle.valid,
            "issues": _issues(bundle),
            "document": bundle.normalized() if bundle.valid else None,
            "summary": _project_summary(bundle),
            "dirty": self._dirty,
        }

    def _validate(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        return {
            "project_revision": self._project_revision,
            "valid": bundle.valid,
            "issues": _issues(bundle),
            "semantic_sha256": (
                hashlib.sha256(bundle.canonical_bytes()).hexdigest() if bundle.valid else None
            ),
        }

    def _normalize(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before normalization",
                details={"issues": _issues(bundle)},
            )
        return {
            "project_revision": self._project_revision,
            "document": bundle.normalized(),
            "canonical_sha256": hashlib.sha256(bundle.canonical_bytes()).hexdigest(),
        }

    def _build_package(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before package compilation",
                details={"issues": _issues(bundle)},
            )
        try:
            blob = build_egg(bundle)
            package = parse_egg(blob)
        except (EggCompileError, EggFormatError) as exc:
            raise ProtocolError("PACKAGE_BUILD_FAILED", str(exc)) from exc
        report = build_compatibility_report(bundle, blob)
        return {
            "project_revision": self._project_revision,
            "package": {
                "package_id": package.manifest["package_id"],
                "target_profile": package.manifest["target_profile"],
                "entry_scene": package.manifest["entry_scene"],
                "scene_count": len(package.scenes),
                "asset_frame_count": len(package.assets),
                "animation_count": len(package.animations),
                "chunk_count": len(package.chunks),
                "size_bytes": len(blob),
                "sha256": package.sha256,
                "blob_base64": base64.b64encode(blob).decode("ascii"),
            },
            "compatibility_report": report,
        }

    def _compatibility_report(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        try:
            blob = build_egg(bundle) if bundle.valid else None
        except EggCompileError as exc:
            raise ProtocolError("PACKAGE_BUILD_FAILED", str(exc)) from exc
        return {
            "project_revision": self._project_revision,
            "report": build_compatibility_report(bundle, blob),
        }

    def _apply_commands(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params, {"commands"})
        try:
            updated, applied = apply_project_commands(bundle, params["commands"])
        except ProjectCommandError as exc:
            raise ProtocolError(exc.code, exc.message) from exc
        self._bundle = updated
        self._project_revision += 1
        self._preview = None
        self._preview_revision += 1
        self._dirty = True
        return {
            "project_revision": self._project_revision,
            "valid": updated.valid,
            "issues": _issues(updated),
            "document": updated.normalized() if updated.valid else None,
            "summary": _project_summary(updated),
            "applied_commands": list(applied),
            "dirty": self._dirty,
        }

    def _save(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        try:
            saved_sources = save_project(bundle)
        except ProjectCommandError as exc:
            raise ProtocolError(exc.code, exc.message) from exc
        self._dirty = False
        return {
            "project_revision": self._project_revision,
            "valid": bundle.valid,
            "issues": _issues(bundle),
            "document": bundle.normalized() if bundle.valid else None,
            "summary": _project_summary(bundle),
            "dirty": self._dirty,
            "saved_sources": list(saved_sources),
        }

    def _preview_result(self, snapshot: dict[str, Any]) -> dict[str, Any]:
        return {
            "project_revision": self._project_revision,
            "preview_revision": self._preview_revision,
            **snapshot,
        }

    def _current_preview(
        self,
        params: dict[str, Any],
        operation_fields: set[str],
    ) -> StateScenePreview:
        self._current_bundle(params, operation_fields | {"preview_revision"})
        if self._preview is None:
            raise ProtocolError("PREVIEW_NOT_STARTED", "reset a selected-scene preview before this operation")
        revision = params["preview_revision"]
        if not isinstance(revision, int) or isinstance(revision, bool):
            raise ProtocolError("PREVIEW_REVISION_INVALID", "preview_revision must be an integer")
        if revision != self._preview_revision:
            raise ProtocolError(
                "PREVIEW_REVISION_STALE",
                "operation targets an outdated preview revision",
                details={"current_preview_revision": self._preview_revision},
            )
        return self._preview

    def _preview_reset(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params, {"scene_id"})
        scene_id = params["scene_id"]
        if not isinstance(scene_id, str) or not scene_id:
            raise ProtocolError("PREVIEW_SCENE_INVALID", "scene_id must be non-empty text")
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before preview",
                details={"issues": _issues(bundle)},
            )
        try:
            package = parse_egg(build_egg(bundle))
            preview = StateScenePreview(package, scene_id)
        except (EggCompileError, EggFormatError, PreviewError) as exc:
            raise ProtocolError("PREVIEW_START_FAILED", str(exc)) from exc
        self._preview = preview
        self._preview_revision += 1
        return self._preview_result(preview.snapshot())

    def _preview_input(self, params: dict[str, Any]) -> dict[str, Any]:
        preview = self._current_preview(params, {"logical_source"})
        logical_source = params["logical_source"]
        if not isinstance(logical_source, str) or not logical_source:
            raise ProtocolError("PREVIEW_INPUT_INVALID", "logical_source must be non-empty text")
        try:
            result = preview.apply_input(logical_source)
        except PreviewError as exc:
            raise ProtocolError("PREVIEW_INPUT_FAILED", str(exc)) from exc
        return self._preview_result(preview.snapshot(result))

    def _preview_advance(self, params: dict[str, Any]) -> dict[str, Any]:
        preview = self._current_preview(params, {"elapsed_ms"})
        try:
            preview.advance(params["elapsed_ms"])
        except PreviewError as exc:
            raise ProtocolError("PREVIEW_ADVANCE_FAILED", str(exc)) from exc
        return self._preview_result(preview.snapshot())

    def handle(self, request: ServiceRequest) -> dict[str, Any]:
        handlers = {
            "service.hello": self._hello,
            "service.shutdown": self._shutdown,
            "project.load": self._load,
            "project.validate": self._validate,
            "project.normalize": self._normalize,
            "project.build_package": self._build_package,
            "project.compatibility_report": self._compatibility_report,
            "project.apply_commands": self._apply_commands,
            "project.save": self._save,
            "project.preview_reset": self._preview_reset,
            "project.preview_input": self._preview_input,
            "project.preview_advance": self._preview_advance,
        }
        handler = handlers.get(request.operation)
        if handler is None:
            raise ProtocolError(
                "OPERATION_UNKNOWN",
                f"unknown operation '{request.operation}'",
                request_id=request.request_id,
                details={"operations": list(SERVICE_OPERATIONS)},
            )
        try:
            return handler(request.params)
        except ProtocolError as exc:
            if exc.request_id is None:
                exc.request_id = request.request_id
            raise


def run_service(input_stream: TextIO, output_stream: TextIO) -> int:
    service = AuthoringService()
    for line in input_stream:
        if not line.strip():
            continue
        request: ServiceRequest | None = None
        try:
            request = parse_request(line)
            result = service.handle(request)
            response = success_response(request.request_id, result)
        except ProtocolError as exc:
            response = error_response(exc)
        except (KeyError, OSError, TypeError, ValueError) as exc:
            response = error_response(
                ProtocolError(
                    "SERVICE_OPERATION_FAILED",
                    str(exc),
                    request_id=request.request_id if request is not None else None,
                )
            )
        output_stream.write(encode_message(response) + "\n")
        output_stream.flush()
        if service.shutdown_requested:
            break
    return 0


def main() -> int:
    return run_service(sys.stdin, sys.stdout)


if __name__ == "__main__":
    raise SystemExit(main())
