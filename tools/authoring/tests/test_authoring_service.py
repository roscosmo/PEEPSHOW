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
TRANSITION_SAMPLE = (
    WORKSPACE_ROOT
    / "examples"
    / "authoring"
    / "state_transition_slice.peepproj"
)


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
    marker = Image.new("RGBA", (24, 24), (255, 255, 255, 0))
    for y in range(24):
        for x in range(24):
            phase = x // 8
            local_x = x % 8
            if phase == 0:
                color = (0, 0, 0, 255) if local_x in {0, 7} else (255, 255, 255, 0)
            elif phase == 1:
                color = (0, 0, 0, 255) if y in {0, 23} else (255, 255, 255, 0)
            else:
                color = (0, 0, 0, 255) if local_x == y % 8 else (255, 255, 255, 0)
            marker.putpixel((x, y), color)
    marker.save(asset_dir / "marker.png", format="PNG")
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
            },
            {
                "asset_id": "marker",
                "asset_type": "masked_1bpp",
                "source_path": "assets/marker.png",
                "source_format": "png",
                "frames": [
                    {
                        "frame_id": "marker.phase_a",
                        "source_rect": {"x": 0, "y": 0, "width": 8, "height": 24},
                        "pivot_x": 0,
                        "pivot_y": 0,
                    },
                    {
                        "frame_id": "marker.phase_b",
                        "source_rect": {"x": 8, "y": 0, "width": 8, "height": 24},
                        "pivot_x": 0,
                        "pivot_y": 0,
                    },
                    {
                        "frame_id": "marker.phase_c",
                        "source_rect": {"x": 16, "y": 0, "width": 8, "height": 24},
                        "pivot_x": 0,
                        "pivot_y": 0,
                    },
                ],
            },
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

    for scene_path in (project_root / "scenes").glob("*.state.json"):
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
        self.assertEqual(11, SERVICE_API_VERSION)
        self.assertEqual(SERVICE_API_VERSION, result["service_api_version"])
        self.assertEqual(PROTOCOL_VERSION, result["protocol_version"])
        self.assertFalse(result["project_loaded"])
        self.assertIn("project.build_package", result["operations"])
        self.assertIn("project.compatibility_report", result["operations"])
        self.assertIn("project.apply_commands", result["operations"])
        self.assertIn("project.save", result["operations"])
        self.assertIn("project.undo", result["operations"])
        self.assertIn("project.redo", result["operations"])
        self.assertIn("project.scene_thumbnails", result["operations"])
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

    def test_apply_commands_renames_state_and_rejects_stale_revision(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        renamed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "state.rename",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "display_name": "Ready",
                        }
                    ],
                },
            )
        )
        self.assertEqual(2, renamed["project_revision"])
        self.assertTrue(renamed["dirty"])
        self.assertTrue(renamed["can_undo"])
        self.assertFalse(renamed["can_redo"])
        self.assertEqual(32, renamed["undo_limit"])
        self.assertEqual(
            [
                {
                    "kind": "state.rename",
                    "scene_id": "state_demo",
                    "state_id": "center",
                    "display_name": "Ready",
                }
            ],
            renamed["applied_commands"],
        )
        states = renamed["document"]["scenes"][0]["states"]
        self.assertEqual("Ready", next(state for state in states if state["state_id"] == "center")["display_name"])

        with self.assertRaisesRegex(ProtocolError, "outdated project revision") as stale:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": "Stale",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("PROJECT_REVISION_STALE", stale.exception.code)

    def test_apply_commands_rejects_invalid_state_rename(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": "",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("PROJECT_TEXT_INVALID", raised.exception.code)

    def test_route_set_target_updates_graph_and_undo_redo(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_target",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "target_state": "left",
                        }
                    ],
                },
            )
        )
        routes = changed["document"]["scenes"][0]["routes"]
        self.assertEqual(
            "left",
            next(route for route in routes if route["route_id"] == "center_to_right")["target_state"],
        )
        self.assertTrue(changed["dirty"])
        self.assertTrue(changed["can_undo"])

        undone = service.handle(request("project.undo", {"project_revision": changed["project_revision"]}))
        routes = undone["document"]["scenes"][0]["routes"]
        self.assertEqual(
            "right",
            next(route for route in routes if route["route_id"] == "center_to_right")["target_state"],
        )

        redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
        routes = redone["document"]["scenes"][0]["routes"]
        self.assertEqual(
            "left",
            next(route for route in routes if route["route_id"] == "center_to_right")["target_state"],
        )

    def test_route_set_target_rejects_unknown_target(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.set_target",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "target_state": "missing",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("COMMAND_TARGET_UNKNOWN", raised.exception.code)

    def test_route_add_scene_exit_creates_valid_direct_transition(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.add_scene_exit",
                            "scene_id": "state_details",
                            "logical_source": "BUTTON_A",
                            "target_scene": "state_demo",
                        }
                    ],
                },
            )
        )

        applied = changed["applied_commands"][0]
        route_id = applied["route_id"]
        details = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_details")
        route = next(route for route in details["routes"] if route["route_id"] == route_id)
        self.assertEqual("route.add_scene_exit", applied["kind"])
        self.assertEqual(route_id, applied["action_id"])
        self.assertEqual("BUTTON_A", applied["logical_source"])
        self.assertEqual("state_demo", route["target_scene"])
        self.assertEqual(route_id, route["action_ref"])
        self.assertEqual([], route["guards"])
        self.assertEqual([], route["actions"])
        self.assertEqual([state["state_id"] for state in details["states"]], route["from_states"])
        self.assertIn({"action_id": route_id, "logical_source": "BUTTON_A"}, details["input_actions"])
        self.assertIn(route_id, details["reactive_wait_default"]["event_interests"])
        self.assertIn(route_id, details["interaction_policy"]["meaningful_activity_actions"])

        reset = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": changed["project_revision"], "scene_id": "state_details"},
            )
        )
        preview = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": changed["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_A",
                },
            )
        )
        self.assertEqual("state_demo", preview["scene"]["scene_id"])
        self.assertTrue(changed["dirty"])
        self.assertTrue(changed["can_undo"])

    def test_route_add_scene_exit_rejects_duplicate_source(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.add_scene_exit",
                                "scene_id": "state_demo",
                                "logical_source": "BUTTON_A",
                                "target_scene": "state_details",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("INPUT_SOURCE_DUPLICATE", raised.exception.code)

    def test_route_delete_scene_exit_removes_route_and_action_binding(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.delete_scene_exit",
                            "scene_id": "state_demo",
                            "route_id": "open_details",
                        }
                    ],
                },
            )
        )

        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        self.assertNotIn("open_details", [route["route_id"] for route in demo["routes"]])
        self.assertNotIn("open_details", [action["action_id"] for action in demo["input_actions"]])
        self.assertNotIn("open_details", demo["reactive_wait_default"]["event_interests"])
        self.assertNotIn("open_details", demo["interaction_policy"]["meaningful_activity_actions"])
        self.assertEqual(
            {
                "kind": "route.delete_scene_exit",
                "scene_id": "state_demo",
                "route_id": "open_details",
                "action_id": "open_details",
                "target_scene": "state_details",
            },
            changed["applied_commands"][0],
        )
        self.assertTrue(changed["dirty"])

    def test_route_delete_scene_exit_rejects_local_route(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.delete_scene_exit",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("COMMAND_TARGET_INVALID", raised.exception.code)

    def test_scene_flow_node_position_is_editor_only_and_persists(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "layout.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            before = service.handle(
                request("project.build_package", {"project_revision": loaded["project_revision"]})
            )
            moved = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "editor.scene_flow.set_node_position",
                                "scene_id": "state_details",
                                "x": 512.4,
                                "y": -48.2,
                            }
                        ],
                    },
                )
            )
            after = service.handle(
                request("project.build_package", {"project_revision": moved["project_revision"]})
            )

            self.assertEqual(before["package"]["sha256"], after["package"]["sha256"])
            self.assertEqual(
                {"x": 512, "y": -48},
                moved["document"]["project"]["editor"]["scene_flow"]["nodes"]["state_details"],
            )
            self.assertTrue(moved["dirty"])
            saved = service.handle(request("project.save", {"project_revision": moved["project_revision"]}))
            self.assertIn("project.json", saved["saved_sources"])

            reloaded = load_project(project_root)
            self.assertEqual(
                {"x": 512, "y": -48},
                reloaded.normalized()["project"]["editor"]["scene_flow"]["nodes"]["state_details"],
            )

    def test_route_set_target_persists_on_save(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "route_target.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.set_target",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "target_state": "left",
                            }
                        ],
                    },
                )
            )
            service.handle(request("project.save", {"project_revision": changed["project_revision"]}))
            reloaded = load_project(project_root)
            routes = reloaded.normalized()["scenes"][0]["routes"]
            self.assertEqual(
                "left",
                next(route for route in routes if route["route_id"] == "center_to_right")["target_state"],
            )

    def test_route_set_target_switches_between_state_and_scene_targets(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(TRANSITION_SAMPLE)}))
        local = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_target",
                            "scene_id": "state_demo",
                            "route_id": "enter_target",
                            "target_state": "center",
                        }
                    ],
                },
            )
        )
        route = next(
            route
            for route in local["document"]["scenes"][0]["routes"]
            if route["route_id"] == "enter_target"
        )
        self.assertEqual("center", route["target_state"])
        self.assertNotIn("target_scene", route)

        remote = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": local["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_target",
                            "scene_id": "state_demo",
                            "route_id": "enter_target",
                            "target_scene": "state_target",
                        }
                    ],
                },
            )
        )
        route = next(
            route
            for route in remote["document"]["scenes"][0]["routes"]
            if route["route_id"] == "enter_target"
        )
        self.assertEqual("state_target", route["target_scene"])
        self.assertNotIn("target_state", route)

    def test_route_set_guard_updates_existing_guard_and_undo_redo(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_guard",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "guard_index": 0,
                            "variable_ref": "selected_index",
                            "operator": "ge",
                            "value": 1,
                        }
                    ],
                },
            )
        )
        guard = next(route for route in changed["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["guards"][0]
        self.assertEqual({"variable_ref": "selected_index", "operator": "ge", "value": 1}, guard)

        undone = service.handle(request("project.undo", {"project_revision": changed["project_revision"]}))
        guard = next(route for route in undone["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["guards"][0]
        self.assertEqual({"variable_ref": "selected_index", "operator": "eq", "value": 1}, guard)

        redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
        guard = next(route for route in redone["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["guards"][0]
        self.assertEqual({"variable_ref": "selected_index", "operator": "ge", "value": 1}, guard)

    def test_route_set_guard_rejects_invalid_fields(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        base_command = {
            "kind": "route.set_guard",
            "scene_id": "state_demo",
            "route_id": "center_to_right",
            "guard_index": 0,
            "variable_ref": "selected_index",
            "operator": "eq",
            "value": 1,
        }
        cases = [
            ({**base_command, "variable_ref": "missing"}, "GUARD_VARIABLE_UNKNOWN"),
            ({**base_command, "operator": "around"}, "GUARD_OPERATOR_INVALID"),
            ({**base_command, "value": True}, "GUARD_TYPE_MISMATCH"),
            ({**base_command, "guard_index": 99}, "COMMAND_INDEX_INVALID"),
        ]
        revision = loaded["project_revision"]
        for command, code in cases:
            with self.assertRaises(ProtocolError) as raised:
                service.handle(
                    request(
                        "project.apply_commands",
                        {"project_revision": revision, "commands": [command]},
                    )
                )
            self.assertEqual(code, raised.exception.code)

    def test_route_set_guard_persists_on_save(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "route_guard.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.set_guard",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "guard_index": 0,
                                "variable_ref": "selected_index",
                                "operator": "le",
                                "value": 2,
                            }
                        ],
                    },
                )
            )
            service.handle(request("project.save", {"project_revision": changed["project_revision"]}))
            reloaded = load_project(project_root)
            guard = next(route for route in reloaded.normalized()["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["guards"][0]
            self.assertEqual({"variable_ref": "selected_index", "operator": "le", "value": 2}, guard)

    def test_route_set_action_updates_existing_action_and_undo_redo(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_action",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "action_index": 0,
                            "action": {
                                "kind": "set_variable",
                                "variable_ref": "selected_index",
                                "operation": "add",
                                "value": 1,
                            },
                        }
                    ],
                },
            )
        )
        action = next(route for route in changed["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["actions"][0]
        self.assertEqual({"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 1}, action)

        undone = service.handle(request("project.undo", {"project_revision": changed["project_revision"]}))
        action = next(route for route in undone["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["actions"][0]
        self.assertEqual({"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": 2}, action)

        redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
        action = next(route for route in redone["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["actions"][0]
        self.assertEqual({"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 1}, action)

    def test_route_set_action_accepts_request_render_shape(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_action",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "action_index": 0,
                            "action": {"kind": "request_render"},
                        }
                    ],
                },
            )
        )
        action = next(route for route in changed["document"]["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["actions"][0]
        self.assertEqual({"kind": "request_render"}, action)

    def test_route_set_action_rejects_invalid_fields(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        base_command = {
            "kind": "route.set_action",
            "scene_id": "state_demo",
            "route_id": "center_to_right",
            "action_index": 0,
            "action": {
                "kind": "set_variable",
                "variable_ref": "selected_index",
                "operation": "assign",
                "value": 2,
            },
        }
        cases = [
            ({**base_command, "action_index": 99}, "COMMAND_INDEX_INVALID"),
            ({**base_command, "action": {"kind": "set_variable", "variable_ref": "missing", "operation": "assign", "value": 2}}, "ACTION_VARIABLE_UNKNOWN"),
            ({**base_command, "action": {"kind": "set_variable", "variable_ref": "selected_index", "operation": "multiply", "value": 2}}, "ACTION_OPERATION_INVALID"),
            ({**base_command, "action": {"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": False}}, "ACTION_TYPE_INVALID"),
            ({**base_command, "action": {"kind": "unknown"}}, "ACTION_KIND_INVALID"),
        ]
        revision = loaded["project_revision"]
        for command, code in cases:
            with self.assertRaises(ProtocolError) as raised:
                service.handle(
                    request(
                        "project.apply_commands",
                        {"project_revision": revision, "commands": [command]},
                    )
                )
            self.assertEqual(code, raised.exception.code)

    def test_route_set_action_persists_on_save(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "route_action.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.set_action",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "action_index": 0,
                                "action": {
                                    "kind": "set_variable",
                                    "variable_ref": "selected_index",
                                    "operation": "add",
                                    "value": 1,
                                },
                            }
                        ],
                    },
                )
            )
            service.handle(request("project.save", {"project_revision": changed["project_revision"]}))
            reloaded = load_project(project_root)
            action = next(route for route in reloaded.normalized()["scenes"][0]["routes"] if route["route_id"] == "center_to_right")["actions"][0]
            self.assertEqual({"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 1}, action)

    def test_undo_redo_round_trips_state_rename_and_dirty_baseline(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        self.assertFalse(loaded["can_undo"])
        self.assertFalse(loaded["can_redo"])
        original_states = loaded["document"]["scenes"][0]["states"]
        original_name = next(state for state in original_states if state["state_id"] == "center")["display_name"]
        renamed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "state.rename",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "display_name": "Undo Name",
                        }
                    ],
                },
            )
        )
        self.assertTrue(renamed["dirty"])

        undone = service.handle(request("project.undo", {"project_revision": renamed["project_revision"]}))
        states = undone["document"]["scenes"][0]["states"]
        self.assertEqual(original_name, next(state for state in states if state["state_id"] == "center")["display_name"])
        self.assertFalse(undone["dirty"])
        self.assertFalse(undone["can_undo"])
        self.assertTrue(undone["can_redo"])

        redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
        states = redone["document"]["scenes"][0]["states"]
        self.assertEqual("Undo Name", next(state for state in states if state["state_id"] == "center")["display_name"])
        self.assertTrue(redone["dirty"])
        self.assertTrue(redone["can_undo"])
        self.assertFalse(redone["can_redo"])

    def test_save_updates_dirty_baseline_without_clearing_undo_history(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "undo_save.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            renamed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": "Saved Undo",
                            }
                        ],
                    },
                )
            )
            saved = service.handle(request("project.save", {"project_revision": renamed["project_revision"]}))
            self.assertFalse(saved["dirty"])
            self.assertTrue(saved["can_undo"])

            undone = service.handle(request("project.undo", {"project_revision": saved["project_revision"]}))
            self.assertTrue(undone["dirty"])

            redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
            self.assertFalse(redone["dirty"])

    def test_undo_history_is_bounded_and_new_edit_clears_redo(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        revision = loaded["project_revision"]
        for index in range(35):
            result = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": revision,
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": f"Name {index}",
                            }
                        ],
                    },
                )
            )
            revision = result["project_revision"]
            self.assertEqual(32, result["undo_limit"])

        undo_count = 0
        while True:
            try:
                result = service.handle(request("project.undo", {"project_revision": revision}))
            except ProtocolError as exc:
                self.assertEqual("UNDO_UNAVAILABLE", exc.code)
                break
            undo_count += 1
            revision = result["project_revision"]
        self.assertEqual(32, undo_count)

        redone = service.handle(request("project.redo", {"project_revision": revision}))
        edited = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": redone["project_revision"],
                    "commands": [
                        {
                            "kind": "state.rename",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "display_name": "Redo Cleared",
                        }
                    ],
                },
            )
        )
        self.assertFalse(edited["can_redo"])

    def test_save_persists_renamed_state_source(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "save_test.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            renamed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": "Saved Name",
                            }
                        ],
                    },
                )
            )
            self.assertTrue(renamed["dirty"])
            saved = service.handle(
                request("project.save", {"project_revision": renamed["project_revision"]})
            )
            self.assertFalse(saved["dirty"])
            self.assertEqual(
                ["project.json", "scenes/state_demo.state.json", "scenes/state_details.state.json"],
                saved["saved_sources"],
            )

            reloaded = load_project(project_root)
            states = reloaded.normalized()["scenes"][0]["states"]
            self.assertEqual(
                "Saved Name",
                next(state for state in states if state["state_id"] == "center")["display_name"],
            )

    def test_save_rejects_stale_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "save_stale.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "display_name": "Unsaved",
                            }
                        ],
                    },
                )
            )
            with self.assertRaises(ProtocolError) as stale:
                service.handle(request("project.save", {"project_revision": loaded["project_revision"]}))
            self.assertEqual("PROJECT_REVISION_STALE", stale.exception.code)

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
                        "logical_source": "BUTTON_B",
                    },
                )
            )
            self.assertFalse(ignored["input"]["accepted"])
            self.assertEqual("right", ignored["scene"]["state_id"])

    def test_scene_thumbnails_return_package_backed_frames_without_touching_live_preview(self) -> None:
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

            result = service.handle(
                request("project.scene_thumbnails", {"project_revision": loaded["project_revision"]})
            )

            self.assertEqual(loaded["project_revision"], result["project_revision"])
            thumbnails = {item["scene_id"]: item["framebuffer"] for item in result["thumbnails"]}
            self.assertEqual({"state_demo", "state_details"}, set(thumbnails))
            for framebuffer in thumbnails.values():
                self.assertEqual(168, framebuffer["width"])
                self.assertEqual(144, framebuffer["height"])
                self.assertEqual(3024, len(base64.b64decode(framebuffer["data_base64"], validate=True)))

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
            self.assertEqual(reset["preview_revision"], advanced["preview_revision"])
            self.assertEqual("state_demo", advanced["scene"]["scene_id"])
            self.assertEqual(250, advanced["timeline"]["elapsed_ms"])

    def test_preview_replaces_state_scenes_and_resets_destination_epoch(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(TRANSITION_SAMPLE)}))
        reset = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
            )
        )
        source_hash = reset["framebuffer"]["sha256"]

        target = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_A",
                },
            )
        )
        self.assertEqual("state_target", target["scene"]["scene_id"])
        self.assertEqual("center", target["scene"]["state_id"])
        self.assertEqual(1, target["variables"]["selected_index"])
        self.assertEqual(0, target["timeline"]["elapsed_ms"])
        self.assertEqual(1, target["timeline"]["step_index"])
        self.assertNotEqual(source_hash, target["framebuffer"]["sha256"])

        returned = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_A",
                },
            )
        )
        self.assertEqual("state_demo", returned["scene"]["scene_id"])
        self.assertEqual("center", returned["scene"]["state_id"])
        self.assertEqual(1, returned["variables"]["selected_index"])
        self.assertEqual(0, returned["timeline"]["elapsed_ms"])
        self.assertEqual(source_hash, returned["framebuffer"]["sha256"])

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
