"""Long-running host service for the PeepShow authoring toolchain."""

from __future__ import annotations

import base64
import hashlib
import sys
from pathlib import Path
from typing import Any, TextIO

from .audio_assets import (
    AUDIO_BLOCK_SAMPLES,
    AUDIO_MAX_ASSETS,
    AUDIO_MAX_BANK_BYTES,
    AUDIO_MAX_CUES,
    AUDIO_SAMPLE_RATE_HZ,
    AudioAssetError,
    decode_ima_adpcm,
    pcm16_wav,
)
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


SERVICE_API_VERSION = 21
UNDO_LIMIT = 32
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
    "project.undo",
    "project.redo",
    "project.scene_thumbnails",
    "project.audio_audition",
    "project.preview_reset",
    "project.preview_state",
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
        "audio_asset_count": len(bundle.audio_assets),
        "audio_cue_count": len(bundle.audio_cues),
    }


def _semantic_sha256(bundle: ProjectBundle) -> str | None:
    return hashlib.sha256(bundle.canonical_bytes()).hexdigest() if bundle.valid else None


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
        self._saved_sha256: str | None = None
        self._undo_stack: list[ProjectBundle] = []
        self._redo_stack: list[ProjectBundle] = []
        self.shutdown_requested = False

    def _dirty(self) -> bool:
        if self._bundle is None:
            return False
        return _semantic_sha256(self._bundle) != self._saved_sha256

    def _remember_undo(self, bundle: ProjectBundle) -> None:
        self._undo_stack.append(bundle)
        if len(self._undo_stack) > UNDO_LIMIT:
            del self._undo_stack[0 : len(self._undo_stack) - UNDO_LIMIT]
        self._redo_stack.clear()

    def _invalidate_preview(self) -> None:
        self._preview = None
        self._preview_revision += 1

    def _project_document_result(self, bundle: ProjectBundle) -> dict[str, Any]:
        return {
            "project_revision": self._project_revision,
            "valid": bundle.valid,
            "issues": _issues(bundle),
            "document": bundle.normalized() if bundle.valid else None,
            "summary": _project_summary(bundle),
            "dirty": self._dirty(),
            "can_undo": bool(self._undo_stack),
            "can_redo": bool(self._redo_stack),
            "undo_limit": UNDO_LIMIT,
        }

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
            "state_scene_presentation": {
                "record_format": "RND2",
                "load_compatible_formats": ["RND1", "RND2"],
                "package_layers": ["BACKGROUND", "SCENE", "UI"],
                "system_layers": ["OVERLAY"],
                "element_kinds": [
                    "sprite",
                    "line",
                    "outline_rect",
                    "filled_rect",
                    "circle",
                    "ellipse",
                ],
                "visibility": True,
                "z_order": True,
                "element_commands": [
                    "render_element.add",
                    "render_element.delete",
                    "render_element.set_bounds",
                    "render_element.set_layer",
                    "render_element.set_z_order",
                ],
                "state_override_commands": [
                    "state_placement.set_override",
                ],
                "asset_commands": [
                    "asset.upsert",
                    "asset.delete",
                ],
                "general_frame_animation": {
                    "state_placeable": False,
                    "purpose": "reserved for future SEQUENCE authoring",
                    "commands": [
                    "animation.upsert",
                    "animation.delete",
                    ],
                },
                "waiting_animation": {
                    "sprite_only": True,
                    "phase_count": {"minimum": 1, "maximum": 4},
                    "combined_step_count": {"minimum": 1, "maximum": 12},
                    "element_count_maximum": 32,
                    "quantum_ms": {"minimum": 1, "maximum": 60000},
                    "cycle_policies": ["loop"],
                    "commands": [
                        "waiting_visual.upsert",
                        "waiting_visual.delete",
                        "state.set_waiting_visual",
                        "render_element.bind_waiting_animation",
                        "render_element.clear_waiting_animation",
                    ],
                },
                "logical_inputs": [
                    "BUTTON_A",
                    "BUTTON_B",
                    "BUTTON_L",
                    "BUTTON_R",
                    "BUTTON_START",
                    "JOY_LEFT",
                    "JOY_RIGHT",
                    "JOY_UP",
                    "JOY_DOWN",
                    "JOY_UP_LEFT",
                    "JOY_UP_RIGHT",
                    "JOY_DOWN_LEFT",
                    "JOY_DOWN_RIGHT",
                ],
                "logical_input_events": ["press", "release", "hold", "repeat"],
                "joystick_policies": ["four_way", "eight_way"],
                "runtime_text": False,
                "build_time_text": {
                    "source_format": "system_font_text",
                    "font_ids": ["peepshow.system.8x8.basic.v1"],
                    "character_set": "printable_ascii_plus_newline",
                    "glyph_cell": {"width": 8, "height": 8},
                    "scale": {"minimum": 1, "maximum": 8, "integer_only": True},
                    "ink": "black",
                    "background": "transparent",
                    "frames_per_asset": 1,
                    "commands": ["asset.upsert", "asset.delete"],
                },
                "element_actions": {
                    "target": "destination_state_render_model",
                    "atomic_with_variable_actions": True,
                    "kinds": [
                        "set_element_visibility",
                        "set_element_position",
                        "set_element_frame",
                        "set_element_waiting_animation",
                    ],
                    "waiting_visual_linkage": {
                        "visibility": True,
                        "position": True,
                        "frame_selection_replaces_animation": False,
                        "animation_selection": {
                            "source": "waiting_visual_element",
                            "timeline_policies": ["preserve", "rebase"],
                            "requires_matching_cadence_and_step_count": True,
                        },
                    },
                },
                "system_actions": ["exit_to_shell"],
            },
            "state_scene_graph": {
                "command_batch_maximum": 64,
                "target_scene_actions": ["play_sfx"],
                "limits": {
                    "states": 64,
                    "render_models": 1,
                    "variables": 32,
                    "input_actions": 32,
                    "routes": 128,
                    "guards_per_route": 8,
                    "actions_per_route": 8,
                },
                "state_commands": [
                    "state.add",
                    "state.delete",
                    "state.rename",
                    "state.set_entry",
                    "state.set_waiting_visual",
                ],
                "state_placement_commands": [
                    "state_placement.set_override",
                ],
                "render_model_commands": [
                    "render_model.set_focus_index",
                ],
                "variable_commands": [
                    "variable.add",
                    "variable.update",
                    "variable.delete",
                ],
                "input_action_commands": [
                    "input_action.add",
                    "input_action.update",
                    "input_action.delete",
                ],
                "route_commands": [
                    "route.add",
                    "route.delete",
                    "route.set_action_ref",
                    "route.set_sources",
                    "route.set_target",
                    "route.add_scene_exit",
                    "route.delete_scene_exit",
                ],
                "guard_commands": [
                    "route.guard.add",
                    "route.guard.delete",
                    "route.guard.move",
                    "route.set_guard",
                ],
                "action_commands": [
                    "route.action.add",
                    "route.action.delete",
                    "route.action.move",
                    "route.set_action",
                ],
                "policy_commands": [
                    "scene.set_reactive_wait_default",
                    "scene.set_interaction_policy",
                    "scene.set_joystick_policy",
                ],
                "generic_delete_policy": "reject_if_referenced",
            },
            "state_scene_audio": {
                "host_package_support": True,
                "target_playback_status": "available_package_streamed_state_sfx",
                "source_format": "wav_pcm",
                "compiled_format": "ima_adpcm_4bit",
                "sample_rate_hz": AUDIO_SAMPLE_RATE_HZ,
                "channels": 1,
                "block_samples": AUDIO_BLOCK_SAMPLES,
                "maximum_duration_ms": None,
                "maximum_assets": AUDIO_MAX_ASSETS,
                "maximum_cues": AUDIO_MAX_CUES,
                "maximum_bank_bytes": AUDIO_MAX_BANK_BYTES,
                "voice_limit": 1,
                "route_action": "play_sfx",
                "survives_same_package_scene_replacement": True,
                "asset_commands": ["audio_asset.upsert", "audio_asset.delete"],
                "cue_commands": ["audio_cue.upsert", "audio_cue.delete"],
                "audition_operation": "project.audio_audition",
                "unsupported": ["looping", "music", "procedural_audio"],
            },
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
        self._preview_revision += 1
        self._saved_sha256 = _semantic_sha256(bundle)
        self._undo_stack.clear()
        self._redo_stack.clear()
        return {
            "project_revision": self._project_revision,
            "source_name": bundle.root.name,
            **self._project_document_result(bundle),
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
                "audio_asset_count": len(package.audio_assets),
                "audio_cue_count": len(package.audio_cues),
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

    def _scene_thumbnails(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before scene thumbnail generation",
                details={"issues": _issues(bundle)},
            )
        try:
            package = parse_egg(build_egg(bundle))
            thumbnails = [
                {
                    "scene_id": str(scene["scene_id"]),
                    "framebuffer": StateScenePreview(package, str(scene["scene_id"])).snapshot()["framebuffer"],
                }
                for scene in package.scenes
            ]
        except (EggCompileError, EggFormatError, PreviewError) as exc:
            raise ProtocolError("SCENE_THUMBNAILS_FAILED", str(exc)) from exc
        return {
            "project_revision": self._project_revision,
            "thumbnails": thumbnails,
        }

    def _audio_audition(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params, {"cue_id"})
        cue_id = params["cue_id"]
        if not isinstance(cue_id, str) or not cue_id:
            raise ProtocolError("AUDIO_CUE_INVALID", "cue_id must be non-empty text")
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before audio audition",
                details={"issues": _issues(bundle)},
            )
        try:
            package = parse_egg(build_egg(bundle))
            cue = next(
                (item for item in package.audio_cues if item["cue_id"] == cue_id),
                None,
            )
            if cue is None:
                raise ProtocolError("AUDIO_CUE_NOT_FOUND", f"audio cue '{cue_id}' is not present")
            asset = package.audio_assets[int(cue["asset_index"])]
            samples = decode_ima_adpcm(
                asset["adpcm"],
                int(asset["sample_count"]),
                int(asset["block_count"]),
                int(asset["block_samples"]),
            )
            wav = pcm16_wav(samples)
        except (EggCompileError, EggFormatError, AudioAssetError) as exc:
            raise ProtocolError("AUDIO_AUDITION_FAILED", str(exc)) from exc
        return {
            "project_revision": self._project_revision,
            "cue": {
                "cue_id": cue["cue_id"],
                "asset_id": cue["asset_id"],
                "priority": cue["priority"],
                "volume": cue["volume"],
            },
            "audio": {
                "encoding": "wav_pcm_s16le",
                "sample_rate_hz": asset["sample_rate_hz"],
                "channels": asset["channels"],
                "sample_count": asset["sample_count"],
                "duration_ms": asset["duration_ms"],
                "wav_base64": base64.b64encode(wav).decode("ascii"),
            },
        }

    def _apply_commands(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params, {"commands"})
        try:
            updated, applied = apply_project_commands(bundle, params["commands"])
        except ProjectCommandError as exc:
            raise ProtocolError(exc.code, exc.message) from exc
        self._remember_undo(bundle)
        self._bundle = updated
        self._project_revision += 1
        self._invalidate_preview()
        return {
            **self._project_document_result(updated),
            "applied_commands": list(applied),
        }

    def _save(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        try:
            saved_sources = save_project(bundle)
        except ProjectCommandError as exc:
            raise ProtocolError(exc.code, exc.message) from exc
        self._saved_sha256 = _semantic_sha256(bundle)
        return {
            **self._project_document_result(bundle),
            "saved_sources": list(saved_sources),
        }

    def _undo(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        if not self._undo_stack:
            raise ProtocolError("UNDO_UNAVAILABLE", "there is no command to undo")
        self._redo_stack.append(bundle)
        self._bundle = self._undo_stack.pop()
        self._project_revision += 1
        self._invalidate_preview()
        return self._project_document_result(self._bundle)

    def _redo(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params)
        if not self._redo_stack:
            raise ProtocolError("REDO_UNAVAILABLE", "there is no command to redo")
        self._undo_stack.append(bundle)
        if len(self._undo_stack) > UNDO_LIMIT:
            del self._undo_stack[0 : len(self._undo_stack) - UNDO_LIMIT]
        self._bundle = self._redo_stack.pop()
        self._project_revision += 1
        self._invalidate_preview()
        return self._project_document_result(self._bundle)

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

    def _preview_state(self, params: dict[str, Any]) -> dict[str, Any]:
        bundle = self._current_bundle(params, {"scene_id", "state_id"})
        scene_id = params["scene_id"]
        state_id = params["state_id"]
        if not isinstance(scene_id, str) or not scene_id:
            raise ProtocolError("PREVIEW_SCENE_INVALID", "scene_id must be non-empty text")
        if not isinstance(state_id, str) or not state_id:
            raise ProtocolError("PREVIEW_STATE_INVALID", "state_id must be non-empty text")
        if not bundle.valid:
            raise ProtocolError(
                "PROJECT_INVALID",
                "project must validate before preview",
                details={"issues": _issues(bundle)},
            )
        try:
            package = parse_egg(build_egg(bundle))
            preview = StateScenePreview(package, scene_id, state_id)
        except (EggCompileError, EggFormatError, PreviewError) as exc:
            raise ProtocolError("PREVIEW_STATE_FAILED", str(exc)) from exc
        return self._preview_result(preview.snapshot())

    def _preview_input(self, params: dict[str, Any]) -> dict[str, Any]:
        fields = {"logical_source"}
        if "event_kind" in params:
            fields.add("event_kind")
        preview = self._current_preview(params, fields)
        logical_source = params["logical_source"]
        event_kind = params.get("event_kind", "press")
        if not isinstance(logical_source, str) or not logical_source:
            raise ProtocolError("PREVIEW_INPUT_INVALID", "logical_source must be non-empty text")
        try:
            result = preview.apply_input(logical_source, event_kind)
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
            "project.undo": self._undo,
            "project.redo": self._redo,
            "project.scene_thumbnails": self._scene_thumbnails,
            "project.audio_audition": self._audio_audition,
            "project.preview_reset": self._preview_reset,
            "project.preview_state": self._preview_state,
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
