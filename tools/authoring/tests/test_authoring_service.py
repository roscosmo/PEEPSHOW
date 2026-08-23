from __future__ import annotations

import base64
import hashlib
import io
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


TOOL_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.compiler import build_egg  # noqa: E402
from peepshow_authoring.project import load_project  # noqa: E402
from peepshow_authoring.protocol import (  # noqa: E402
    PROTOCOL_VERSION,
    ProtocolError,
    ServiceRequest,
    parse_request,
)
from peepshow_authoring.service import AuthoringService, SERVICE_API_VERSION, run_service  # noqa: E402


SAMPLE = WORKSPACE_ROOT / "examples" / "authoring" / "state_slice.peepproj"


def request(operation: str, params: dict[str, object] | None = None) -> ServiceRequest:
    return ServiceRequest("test", operation, params or {})


def make_preview_project(parent: Path) -> Path:
    project_root = parent / "preview.peepproj"
    shutil.copytree(SAMPLE, project_root)
    asset_dir = project_root / "assets"
    shutil.rmtree(asset_dir)
    asset_dir.mkdir()

    project_path = project_root / "project.json"
    project = json.loads(project_path.read_text(encoding="utf-8"))
    project["asset_sources"] = ["assets/catalog.json"]
    project_path.write_text(json.dumps(project), encoding="utf-8")

    image = Image.new("RGBA", (16, 16), (255, 255, 255, 0))
    for y in range(16):
        for x in range(8):
            if x in {0, 7} or y in {0, 15}:
                image.putpixel((x, y), (0, 0, 0, 255))
        for x in range(8, 16):
            color = (0, 0, 0, 255) if x in {11, 12} else (255, 255, 255, 255)
            image.putpixel((x, y), color)
    image.save(asset_dir / "cursor.png", format="PNG")
    catalog = {
        "schema_id": "peepshow.authoring.assets",
        "schema_version": 1,
        "assets": [
            {
                "asset_id": "cursor",
                "asset_type": "masked_1bpp",
                "source_path": "assets/cursor.png",
                "source_format": "png",
                "frames": [
                    {
                        "frame_id": "cursor.phase_a",
                        "source_rect": {"x": 0, "y": 0, "width": 8, "height": 16},
                        "pivot_x": 0,
                        "pivot_y": 0,
                    },
                    {
                        "frame_id": "cursor.phase_b",
                        "source_rect": {"x": 8, "y": 0, "width": 8, "height": 16},
                        "pivot_x": 0,
                        "pivot_y": 0,
                    },
                ],
            }
        ],
        "animations": [
            {
                "animation_id": "cursor.blink",
                "frame_refs": ["cursor.phase_a", "cursor.phase_b"],
                "frame_duration_ms": [250, 250],
                "loop_policy": "loop",
            }
        ],
    }
    (asset_dir / "catalog.json").write_text(json.dumps(catalog), encoding="utf-8")

    scene_path = project_root / "scenes" / "state_demo.state.json"
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    for model in scene["render_models"]:
        cursor = next(element for element in model["elements"] if element["element_id"] == "cursor")
        cursor["kind"] = "sprite"
        cursor["visual_ref"] = "cursor.phase_a"
        model["elements"] = [cursor]
    cursor_wait = scene["waiting_visuals"][0]["elements"][0]
    cursor_wait["phase_visual_refs"] = ["cursor.phase_a", "cursor.phase_b"]
    scene["waiting_visuals"][0]["elements"] = [cursor_wait]
    scene_path.write_text(json.dumps(scene), encoding="utf-8")
    return project_root


def make_procedural_project(parent: Path) -> Path:
    project_root = parent / "procedural.peepproj"
    shutil.copytree(SAMPLE, project_root)
    project_path = project_root / "project.json"
    project = json.loads(project_path.read_text(encoding="utf-8"))
    project.pop("asset_sources", None)
    project_path.write_text(json.dumps(project), encoding="utf-8")

    scene_path = project_root / "scenes" / "state_demo.state.json"
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    for model in scene["render_models"]:
        cursor = next(element for element in model["elements"] if element["element_id"] == "cursor")
        marker = next(element for element in model["elements"] if element["element_id"] == "marker")
        cursor["kind"] = "shape"
        cursor["visual_ref"] = "cursor_outline"
        marker["kind"] = "shape"
        marker["visual_ref"] = "marker_outline"
    waiting = scene["waiting_visuals"][0]["elements"]
    waiting[0]["phase_visual_refs"] = ["cursor_phase_a", "cursor_phase_b"]
    waiting[1]["phase_visual_refs"] = [
        "marker_phase_a",
        "marker_phase_b",
        "marker_phase_c",
    ]
    scene_path.write_text(json.dumps(scene), encoding="utf-8")
    return project_root


class AuthoringProtocolTests(unittest.TestCase):
    def test_request_parser_accepts_the_frozen_v1_shape(self) -> None:
        parsed = parse_request(
            json.dumps(
                {
                    "protocol_version": PROTOCOL_VERSION,
                    "id": "request-1",
                    "operation": "service.hello",
                    "params": {},
                }
            )
        )
        self.assertEqual("request-1", parsed.request_id)
        self.assertEqual("service.hello", parsed.operation)
        self.assertEqual({}, parsed.params)

    def test_request_parser_rejects_unknown_fields_and_versions(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "fields do not match"):
            parse_request(
                json.dumps(
                    {
                        "protocol_version": PROTOCOL_VERSION,
                        "id": 1,
                        "operation": "service.hello",
                        "params": {},
                        "extra": True,
                    }
                )
            )
        with self.assertRaisesRegex(ProtocolError, "expected protocol version"):
            parse_request(
                json.dumps(
                    {
                        "protocol_version": 99,
                        "id": 1,
                        "operation": "service.hello",
                        "params": {},
                    }
                )
            )


class AuthoringServiceTests(unittest.TestCase):
    def test_hello_discovers_the_service_without_loading_a_project(self) -> None:
        service = AuthoringService()
        result = service.handle(request("service.hello"))
        self.assertEqual("peepshow_authoring", result["service"])
        self.assertEqual(3, SERVICE_API_VERSION)
        self.assertEqual(SERVICE_API_VERSION, result["service_api_version"])
        self.assertEqual(PROTOCOL_VERSION, result["protocol_version"])
        self.assertFalse(result["project_loaded"])
        self.assertIn("project.build_package", result["operations"])
        self.assertIn("project.compatibility_report", result["operations"])
        self.assertIn("project.preview_reset", result["operations"])
        self.assertIn("project.preview_input", result["operations"])
        self.assertIn("project.preview_advance", result["operations"])

    def test_load_validate_and_normalize_share_one_revision(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        self.assertTrue(loaded["valid"])
        self.assertEqual(1, loaded["project_revision"])
        self.assertEqual("state_slice.peepproj", loaded["source_name"])
        self.assertEqual("example.state_slice", loaded["summary"]["project_id"])
        self.assertNotIn(str(WORKSPACE_ROOT), json.dumps(loaded))

        validated = service.handle(
            request("project.validate", {"project_revision": loaded["project_revision"]})
        )
        normalized = service.handle(
            request("project.normalize", {"project_revision": loaded["project_revision"]})
        )
        expected = load_project(SAMPLE)
        self.assertTrue(validated["valid"])
        self.assertEqual(hashlib.sha256(expected.canonical_bytes()).hexdigest(), validated["semantic_sha256"])
        self.assertEqual(expected.normalized(), normalized["document"])

    def test_reloading_invalidates_prior_revision(self) -> None:
        service = AuthoringService()
        first = service.handle(request("project.load", {"path": str(SAMPLE)}))
        second = service.handle(request("project.load", {"path": str(SAMPLE)}))
        self.assertEqual(2, second["project_revision"])
        with self.assertRaisesRegex(ProtocolError, "outdated project revision") as raised:
            service.handle(
                request("project.validate", {"project_revision": first["project_revision"]})
            )
        self.assertEqual("PROJECT_REVISION_STALE", raised.exception.code)
        self.assertEqual(2, raised.exception.details["current_project_revision"])

    def test_package_operation_returns_the_existing_compiler_bytes(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        result = service.handle(
            request("project.build_package", {"project_revision": loaded["project_revision"]})
        )
        blob = base64.b64decode(result["package"]["blob_base64"], validate=True)
        expected = build_egg(load_project(SAMPLE))
        self.assertEqual(expected, blob)
        self.assertEqual(len(expected), result["package"]["size_bytes"])
        self.assertEqual(hashlib.sha256(expected).hexdigest(), result["package"]["sha256"])
        self.assertEqual(
            result["package"]["sha256"],
            result["compatibility_report"]["package"]["package_checksum"],
        )
        self.assertEqual("dev_only", result["compatibility_report"]["report_status"])

    def test_compatibility_report_is_deterministic_and_does_not_overclaim_hw6(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        params = {"project_revision": loaded["project_revision"]}
        first = service.handle(request("project.compatibility_report", params))["report"]
        second = service.handle(request("project.compatibility_report", params))["report"]
        self.assertEqual(first, second)
        self.assertEqual("dev_only", first["report_status"])
        self.assertEqual("pending_validation", first["target_profile"]["profile_status"])
        self.assertEqual(0, first["result_summary"]["blocking_count"])
        self.assertEqual(1, first["result_summary"]["advisory_count"])
        self.assertEqual("pending_validation", first["capabilities"][0]["admission_status"])
        self.assertNotIn(str(WORKSPACE_ROOT), json.dumps(first))

        checksum = first["artifacts"]["compatibility_report_checksum"]
        unsigned = json.loads(json.dumps(first))
        unsigned["artifacts"]["compatibility_report_checksum"] = None
        encoded = json.dumps(
            unsigned,
            ensure_ascii=True,
            separators=(",", ":"),
            sort_keys=True,
        ).encode("ascii")
        self.assertEqual(hashlib.sha256(encoded).hexdigest(), checksum)

    def test_selected_scene_preview_runs_compiled_routes_and_exact_sprite_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_preview_project(Path(temp_dir))
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            reset = service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
                )
            )

            self.assertEqual("center", reset["scene"]["state_id"])
            self.assertEqual(1, reset["timeline"]["step_index"])
            self.assertEqual(32, reset["framebuffer"]["black_pixel_count"])
            framebuffer = base64.b64decode(reset["framebuffer"]["data_base64"], validate=True)
            self.assertEqual(3024, len(framebuffer))

            advanced = service.handle(
                request(
                    "project.preview_advance",
                    {
                        "project_revision": loaded["project_revision"],
                        "preview_revision": reset["preview_revision"],
                        "elapsed_ms": 250,
                    },
                )
            )
            self.assertEqual(2, advanced["timeline"]["step_index"])
            self.assertEqual(44, advanced["framebuffer"]["black_pixel_count"])
            self.assertNotEqual(reset["framebuffer"]["sha256"], advanced["framebuffer"]["sha256"])

            moved = service.handle(
                request(
                    "project.preview_input",
                    {
                        "project_revision": loaded["project_revision"],
                        "preview_revision": reset["preview_revision"],
                        "logical_source": "BUTTON_R",
                    },
                )
            )
            self.assertEqual("right", moved["scene"]["state_id"])
            self.assertEqual(2, moved["variables"]["selected_index"])
            self.assertEqual(2, moved["timeline"]["step_index"])
            self.assertEqual("center_to_right", moved["input"]["route_id"])
            self.assertTrue(moved["input"]["accepted"])

            ignored = service.handle(
                request(
                    "project.preview_input",
                    {
                        "project_revision": loaded["project_revision"],
                        "preview_revision": reset["preview_revision"],
                        "logical_source": "BUTTON_A",
                    },
                )
            )
            self.assertFalse(ignored["input"]["accepted"])
            self.assertEqual("right", ignored["scene"]["state_id"])

    def test_preview_rejects_procedural_source_visuals_and_stale_sessions(self) -> None:
        service = AuthoringService()
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_procedural_project(Path(temp_dir))
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            with self.assertRaises(ProtocolError) as raised:
                service.handle(
                    request(
                        "project.preview_reset",
                        {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
                    )
                )
            self.assertEqual("PREVIEW_START_FAILED", raised.exception.code)
            self.assertIn("package-backed preview requires sprites", raised.exception.message)

        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_preview_project(Path(temp_dir))
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            first = service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
                )
            )
            service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
                )
            )
            with self.assertRaises(ProtocolError) as stale:
                service.handle(
                    request(
                        "project.preview_advance",
                        {
                            "project_revision": loaded["project_revision"],
                            "preview_revision": first["preview_revision"],
                            "elapsed_ms": 1,
                        },
                    )
                )
            self.assertEqual("PREVIEW_REVISION_STALE", stale.exception.code)

    def test_invalid_project_loads_for_diagnostics_but_cannot_build(self) -> None:
        service = AuthoringService()
        missing = SAMPLE.parent / "missing.peepproj"
        loaded = service.handle(request("project.load", {"path": str(missing)}))
        self.assertFalse(loaded["valid"])
        self.assertIsNone(loaded["document"])
        self.assertNotIn(str(WORKSPACE_ROOT), json.dumps(loaded))
        report = service.handle(
            request(
                "project.compatibility_report",
                {"project_revision": loaded["project_revision"]},
            )
        )["report"]
        self.assertEqual("failed", report["report_status"])
        self.assertNotIn(str(WORKSPACE_ROOT), json.dumps(report))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request("project.build_package", {"project_revision": loaded["project_revision"]})
            )
        self.assertEqual("PROJECT_INVALID", raised.exception.code)

    def test_ndjson_loop_recovers_from_bad_input_and_honors_shutdown(self) -> None:
        messages = [
            "not json",
            json.dumps(
                {
                    "protocol_version": PROTOCOL_VERSION,
                    "id": "unknown-request",
                    "operation": "project.unknown",
                    "params": {},
                }
            ),
            json.dumps(
                {
                    "protocol_version": PROTOCOL_VERSION,
                    "id": 1,
                    "operation": "service.hello",
                    "params": {},
                }
            ),
            json.dumps(
                {
                    "protocol_version": PROTOCOL_VERSION,
                    "id": 2,
                    "operation": "service.shutdown",
                    "params": {},
                }
            ),
            json.dumps(
                {
                    "protocol_version": PROTOCOL_VERSION,
                    "id": 3,
                    "operation": "service.hello",
                    "params": {},
                }
            ),
        ]
        output = io.StringIO()
        self.assertEqual(0, run_service(io.StringIO("\n".join(messages)), output))
        responses = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual(4, len(responses))
        self.assertFalse(responses[0]["ok"])
        self.assertEqual("REQUEST_JSON_INVALID", responses[0]["error"]["code"])
        self.assertFalse(responses[1]["ok"])
        self.assertEqual("unknown-request", responses[1]["id"])
        self.assertEqual("OPERATION_UNKNOWN", responses[1]["error"]["code"])
        self.assertTrue(responses[2]["ok"])
        self.assertEqual({"shutdown": True}, responses[3]["result"])


if __name__ == "__main__":
    unittest.main()
