from __future__ import annotations

import base64
import hashlib
import io
import json
import shutil
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

from PIL import Image


TOOL_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.compiler import build_egg  # noqa: E402
from peepshow_authoring.egg_format import parse_egg  # noqa: E402
from peepshow_authoring.project import load_project  # noqa: E402
from peepshow_authoring.protocol import (  # noqa: E402
    PROTOCOL_VERSION,
    ProtocolError,
    ServiceRequest,
    parse_request,
)
from peepshow_authoring.service import AuthoringService, SERVICE_API_VERSION, run_service  # noqa: E402


SAMPLE = WORKSPACE_ROOT / "tools" / "authoring" / "peepshow_authoring" / "test_project.peepproj"
PUBLIC_EXAMPLE = WORKSPACE_ROOT / "examples" / "authoring" / "state_slice.peepproj"
TRANSITION_SAMPLE = (
    WORKSPACE_ROOT
    / "examples"
    / "authoring"
    / "state_transition_slice.peepproj"
)


def request(operation: str, params: dict[str, object] | None = None) -> ServiceRequest:
    return ServiceRequest("test", operation, params or {})


def make_audio_project(parent: Path) -> Path:
    project_root = parent / "audio_preview.peepproj"
    shutil.copytree(SAMPLE, project_root)
    audio_path = project_root / "assets" / "select.wav"
    with wave.open(str(audio_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16000)
        wav.writeframes(
            b"".join(
                struct.pack("<h", 10000 if (index // 40) % 2 == 0 else -10000)
                for index in range(1600)
            )
        )
    catalog_path = project_root / "assets" / "catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    catalog["audio_assets"] = [
        {
            "asset_id": "ui.select",
            "asset_type": "sampled_sfx",
            "source_path": "assets/select.wav",
            "source_format": "wav",
        }
    ]
    catalog["audio_cues"] = [
        {
            "cue_id": "ui.select.cue",
            "asset_ref": "ui.select",
            "priority": 96,
            "volume": 224,
        }
    ]
    catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
    scene_path = project_root / "scenes" / "state_demo.state.json"
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    route = next(item for item in scene["routes"] if item["route_id"] == "center_to_right")
    route["actions"].append({"kind": "play_sfx", "cue_ref": "ui.select.cue"})
    scene_path.write_text(json.dumps(scene), encoding="utf-8")
    return project_root


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
            model["elements"] = [
                {
                    "element_id": "fill",
                    "kind": "filled_rect",
                    "layer": "BACKGROUND",
                    "x": 20,
                    "y": 20,
                    "width": 9,
                    "height": 7,
                    "z_order": 0,
                },
                {
                    "element_id": "line",
                    "kind": "line",
                    "layer": "SCENE",
                    "x": 32,
                    "y": 20,
                    "width": 9,
                    "height": 7,
                    "z_order": 1,
                },
                {
                    "element_id": "outline",
                    "kind": "outline_rect",
                    "layer": "SCENE",
                    "x": 44,
                    "y": 20,
                    "width": 9,
                    "height": 7,
                    "z_order": 2,
                },
                {
                    "element_id": "circle",
                    "kind": "circle",
                    "layer": "UI",
                    "x": 56,
                    "y": 20,
                    "width": 9,
                    "height": 9,
                    "z_order": 3,
                },
                {
                    "element_id": "ellipse",
                    "kind": "ellipse",
                    "layer": "UI",
                    "x": 68,
                    "y": 20,
                    "width": 11,
                    "height": 7,
                    "z_order": 4,
                },
            ]
        for waiting in scene["waiting_visuals"]:
            waiting["elements"] = []
        for state in scene["states"]:
            state["placement_overrides"] = []
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
        self.assertEqual(30, SERVICE_API_VERSION)
        self.assertEqual(SERVICE_API_VERSION, result["service_api_version"])
        self.assertEqual(PROTOCOL_VERSION, result["protocol_version"])
        self.assertFalse(result["project_loaded"])
        self.assertIn("project.create", result["operations"])
        self.assertIn("project.build_package", result["operations"])
        self.assertIn("project.compatibility_report", result["operations"])
        self.assertIn("project.apply_commands", result["operations"])
        self.assertIn("project.save", result["operations"])
        self.assertIn("project.undo", result["operations"])
        self.assertIn("project.redo", result["operations"])
        self.assertIn("project.scene_thumbnails", result["operations"])
        self.assertIn("project.audio_audition", result["operations"])
        self.assertIn("project.preview_reset", result["operations"])
        self.assertIn("project.preview_input", result["operations"])
        self.assertIn("project.preview_advance", result["operations"])
        self.assertIn(
            "editor.state_graph.set_route_layout",
            result["state_scene_graph"]["editor_layout_commands"],
        )
        self.assertEqual(3, result["state_scene_graph"]["route_layout_version"])
        profiles = result["target_profiles"]
        self.assertEqual("hw6_fw0_development", profiles["default_profile_id"])
        self.assertEqual(1, len(profiles["available"]))
        profile = profiles["available"][0]
        self.assertEqual(5242880, profile["package"]["maximum_bytes"])
        self.assertEqual(65536, profile["package"]["resident_prefix_bytes"])
        self.assertEqual(64, len(profile["profile_hash"]))
        self.assertEqual("RND2", result["state_scene_presentation"]["record_format"])
        self.assertEqual(
            ["BACKGROUND", "SCENE", "UI"],
            result["state_scene_presentation"]["package_layers"],
        )
        self.assertNotIn("OVERLAY", result["state_scene_presentation"]["package_layers"])
        self.assertIn("ellipse", result["state_scene_presentation"]["element_kinds"])
        self.assertEqual(
            ["press", "release", "hold", "repeat"],
            result["state_scene_presentation"]["logical_input_events"],
        )
        self.assertEqual(
            ["four_way", "eight_way"],
            result["state_scene_presentation"]["joystick_policies"],
        )
        self.assertFalse(result["state_scene_presentation"]["runtime_text"])
        text = result["state_scene_presentation"]["build_time_text"]
        self.assertEqual("system_font_text", text["source_format"])
        self.assertEqual(["peepshow.system.8x8.basic.v1"], text["font_ids"])
        self.assertEqual({"width": 8, "height": 8}, text["glyph_cell"])
        self.assertEqual(1, text["frames_per_asset"])
        element_actions = result["state_scene_presentation"]["element_actions"]
        self.assertEqual("destination_state_render_model", element_actions["target"])
        self.assertTrue(element_actions["atomic_with_variable_actions"])
        self.assertEqual(
            [
                "set_element_visibility",
                "set_element_position",
                "set_element_frame",
                "set_element_waiting_animation",
            ],
            element_actions["kinds"],
        )
        self.assertTrue(element_actions["waiting_visual_linkage"]["visibility"])
        self.assertTrue(element_actions["waiting_visual_linkage"]["position"])
        self.assertFalse(
            element_actions["waiting_visual_linkage"]["frame_selection_replaces_animation"]
        )
        animation_selection = element_actions["waiting_visual_linkage"][
            "animation_selection"
        ]
        self.assertEqual(
            ["preserve", "rebase"], animation_selection["timeline_policies"]
        )
        self.assertTrue(
            animation_selection["requires_matching_cadence_and_step_count"]
        )
        self.assertEqual(
            ["exit_to_shell"],
            result["state_scene_presentation"]["system_actions"],
        )
        graph = result["state_scene_graph"]
        self.assertEqual(["scene.add"], graph["scene_commands"])
        self.assertEqual(64, graph["command_batch_maximum"])
        self.assertEqual(["play_sfx"], graph["target_scene_actions"])
        self.assertEqual([], graph["peepos_trigger_commands"])
        peepos_triggers = graph["peepos_trigger_catalog"]
        self.assertEqual(
            [
                "step_count",
                "delay_elapsed",
                "local_schedule",
                "device_active",
                "device_inactive",
                "wake_resume",
                "animation_complete",
                "audio_marker",
                "peripheral_event",
            ],
            [trigger["kind"] for trigger in peepos_triggers],
        )
        self.assertTrue(
            all(trigger["support"] == "contract_only" for trigger in peepos_triggers)
        )
        self.assertIn("target_capability", peepos_triggers[0]["requires"])
        self.assertTrue(
            all(
                "firmware_event_dispatch" in trigger["requires"]
                for trigger in peepos_triggers
            )
        )
        audio = result["state_scene_audio"]
        self.assertTrue(audio["host_package_support"])
        self.assertEqual(
            "available_package_streamed_state_sfx", audio["target_playback_status"]
        )
        self.assertEqual("play_sfx", audio["route_action"])
        self.assertTrue(audio["survives_same_package_scene_replacement"])
        self.assertEqual(1, audio["voice_limit"])
        self.assertEqual(64, graph["limits"]["states"])
        self.assertEqual(1, graph["limits"]["render_models"])
        self.assertIn("state.add", graph["state_commands"])
        self.assertNotIn("state.set_render_model", graph["state_commands"])
        self.assertIn("state_placement.set_override", graph["state_placement_commands"])
        self.assertIn("route.guard.move", graph["guard_commands"])
        self.assertIn("route.action.move", graph["action_commands"])
        self.assertIn("scene_exit.add", graph["scene_exit_commands"])
        self.assertIn("route.create_trigger", graph["route_commands"])
        self.assertNotIn("route.add_scene_exit", graph["route_commands"])
        self.assertEqual("reject_if_referenced", graph["generic_delete_policy"])
        waiting_animation = result["state_scene_presentation"]["waiting_animation"]
        self.assertIn("render_element.bind_waiting_animation", waiting_animation["commands"])
        self.assertIn("render_element.clear_waiting_animation", waiting_animation["commands"])

    def test_create_project_is_valid_buildable_previewable_and_reopenable(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "My First Game.peepproj"
            service = AuthoringService()
            created = service.handle(
                request("project.create", {"path": str(project_root)})
            )

            self.assertTrue(created["valid"])
            self.assertFalse(created["dirty"])
            self.assertEqual("My First Game.peepproj", created["source_name"])
            self.assertEqual("My First Game", created["summary"]["project_name"])
            self.assertEqual("main", created["summary"]["entry_scene"])
            self.assertEqual(1, created["summary"]["scene_count"])
            self.assertTrue((project_root / "project.json").is_file())
            self.assertTrue((project_root / "scenes" / "main.state.json").is_file())
            manifest = json.loads(
                (project_root / "project.json").read_text(encoding="utf-8")
            )
            self.assertEqual(["scenes/main.state.json"], manifest["scene_sources"])

            revision = created["project_revision"]
            built = service.handle(
                request("project.build_package", {"project_revision": revision})
            )
            self.assertGreater(built["package"]["size_bytes"], 0)
            preview = service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": revision, "scene_id": "main"},
                )
            )
            ignored = service.handle(
                request(
                    "project.preview_input",
                    {
                        "project_revision": revision,
                        "preview_revision": preview["preview_revision"],
                        "logical_source": "BUTTON_B",
                    },
                )
            )
            self.assertFalse(ignored["input"]["accepted"])
            main_scene = created["document"]["scenes"][0]
            self.assertEqual([], main_scene["input_actions"])
            self.assertEqual([], main_scene["routes"])
            self.assertEqual([], main_scene["scene_exits"])

            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": revision,
                        "commands": [
                            {
                                "kind": "state.rename",
                                "scene_id": "main",
                                "state_id": "start",
                                "display_name": "Ready",
                            }
                        ],
                    },
                )
            )
            self.assertTrue(changed["dirty"])
            saved = service.handle(
                request(
                    "project.save",
                    {"project_revision": changed["project_revision"]},
                )
            )
            self.assertFalse(saved["dirty"])

            reopened = AuthoringService().handle(
                request("project.load", {"path": str(project_root)})
            )
            self.assertTrue(reopened["valid"])
            self.assertEqual(saved["document"], reopened["document"])

    def test_create_project_never_overwrites_an_existing_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "Existing.peepproj"
            project_root.mkdir()
            marker = project_root / "keep.txt"
            marker.write_text("keep", encoding="utf-8")

            with self.assertRaises(ProtocolError) as raised:
                AuthoringService().handle(
                    request("project.create", {"path": str(project_root)})
                )
            self.assertEqual("PROJECT_CREATE_TARGET_EXISTS", raised.exception.code)
            self.assertEqual("keep", marker.read_text(encoding="utf-8"))

    def test_scene_add_creates_blank_relative_source_that_saves_and_reopens(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "Scene Authoring.peepproj"
            service = AuthoringService()
            created = service.handle(
                request("project.create", {"path": str(project_root)})
            )
            added = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": created["project_revision"],
                        "commands": [
                            {
                                "kind": "scene.add",
                                "display_name": "Credits",
                            }
                        ],
                    },
                )
            )

            self.assertTrue(added["valid"])
            self.assertTrue(added["dirty"])
            self.assertEqual(2, added["summary"]["scene_count"])
            applied = added["applied_commands"][0]
            self.assertEqual("credits", applied["scene_id"])
            self.assertEqual("scenes/credits.state.json", applied["source"])
            credits = next(
                scene
                for scene in added["document"]["scenes"]
                if scene["scene_id"] == "credits"
            )
            self.assertEqual("start", credits["entry_state"])
            self.assertEqual([], credits["input_actions"])
            self.assertEqual([], credits["routes"])
            self.assertEqual([], credits["reactive_wait_default"]["event_interests"])

            built = service.handle(
                request(
                    "project.build_package",
                    {"project_revision": added["project_revision"]},
                )
            )
            self.assertEqual(2, built["package"]["scene_count"])
            saved = service.handle(
                request(
                    "project.save",
                    {"project_revision": added["project_revision"]},
                )
            )
            self.assertFalse(saved["dirty"])
            self.assertTrue((project_root / "scenes" / "credits.state.json").is_file())
            manifest = json.loads(
                (project_root / "project.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                ["scenes/main.state.json", "scenes/credits.state.json"],
                manifest["scene_sources"],
            )

            reopened = AuthoringService().handle(
                request("project.load", {"path": str(project_root)})
            )
            self.assertTrue(reopened["valid"])
            self.assertEqual(saved["document"], reopened["document"])

    def test_scene_add_derives_collision_safe_ids(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            service = AuthoringService()
            created = service.handle(
                request(
                    "project.create",
                    {"path": str(Path(temp_dir) / "Scene IDs.peepproj")},
                )
            )
            first = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": created["project_revision"],
                        "commands": [{"kind": "scene.add", "display_name": "Credits"}],
                    },
                )
            )
            second = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": first["project_revision"],
                        "commands": [{"kind": "scene.add", "display_name": "Credits"}],
                    },
                )
            )

            self.assertEqual("credits", first["applied_commands"][0]["scene_id"])
            self.assertEqual("credits_2", second["applied_commands"][0]["scene_id"])
            self.assertEqual(
                "scenes/credits_2.state.json",
                second["applied_commands"][0]["source"],
            )

    def test_state_sfx_package_preview_and_audition_use_compiled_audio(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            service = AuthoringService()
            loaded = service.handle(
                request("project.load", {"path": str(make_audio_project(Path(temp_dir)))})
            )
            revision = loaded["project_revision"]
            self.assertTrue(loaded["valid"])
            self.assertEqual(1, loaded["summary"]["audio_asset_count"])
            self.assertEqual(1, loaded["summary"]["audio_cue_count"])

            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": revision,
                        "commands": [
                            {
                                "kind": "audio_asset.upsert",
                                "audio_asset": {
                                    "asset_id": "ui.select",
                                    "asset_type": "sampled_sfx",
                                    "source_path": "assets/select.wav",
                                    "source_format": "wav",
                                },
                            },
                            {
                                "kind": "audio_cue.upsert",
                                "audio_cue": {
                                    "cue_id": "ui.select.cue",
                                    "asset_ref": "ui.select",
                                    "priority": 96,
                                    "volume": 200,
                                },
                            },
                            {
                                "kind": "route.set_action",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "action_index": 2,
                                "action": {
                                    "kind": "play_sfx",
                                    "cue_ref": "ui.select.cue",
                                },
                            },
                        ],
                    },
                )
            )
            revision = changed["project_revision"]

            built = service.handle(
                request("project.build_package", {"project_revision": revision})
            )
            self.assertEqual(1, built["package"]["audio_asset_count"])
            self.assertEqual(1, built["package"]["audio_cue_count"])
            capabilities = built["compatibility_report"]["capabilities"]
            self.assertIn("audio.sampled_sfx", {item["capability"] for item in capabilities})

            reset = service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": revision, "scene_id": "state_demo"},
                )
            )
            preview = service.handle(
                request(
                    "project.preview_input",
                    {
                        "project_revision": revision,
                        "preview_revision": reset["preview_revision"],
                        "logical_source": "BUTTON_R",
                    },
                )
            )
            self.assertEqual("ui.select.cue", preview["input"]["audio_events"][0]["cue_id"])
            self.assertEqual(200, preview["input"]["audio_events"][0]["volume"])

            audition = service.handle(
                request(
                    "project.audio_audition",
                    {"project_revision": revision, "cue_id": "ui.select.cue"},
                )
            )
            wav_bytes = base64.b64decode(audition["audio"]["wav_base64"])
            with wave.open(io.BytesIO(wav_bytes), "rb") as wav:
                self.assertEqual(16000, wav.getframerate())
                self.assertEqual(1, wav.getnchannels())
                self.assertEqual(1600, wav.getnframes())

    def test_load_validate_and_normalize_share_one_revision(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        self.assertTrue(loaded["valid"])
        self.assertEqual(1, loaded["project_revision"])
        self.assertEqual("test_project.peepproj", loaded["source_name"])
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

    def test_public_example_is_a_menu_selection_flow(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(PUBLIC_EXAMPLE)}))
        self.assertTrue(loaded["valid"])
        self.assertEqual("example.menu_selection", loaded["summary"]["project_id"])
        self.assertEqual("Menu Selection Demo", loaded["summary"]["project_name"])

        package = parse_egg(build_egg(load_project(PUBLIC_EXAMPLE)))
        scenes = {scene["scene_id"]: scene for scene in package.scenes}
        self.assertTrue(
            all(
                scene["graph"]["interaction_mode"] == 1
                and scene["graph"]["inactive_route"] == 0
                for scene in scenes.values()
            )
        )
        for scene_id in ("credits", "settings", "start_game"):
            scene = scenes[scene_id]
            self.assertTrue(
                all(
                    not any(element["focus_role"] for element in model["elements"])
                    for model in scene["render_models"]
                )
            )
            self.assertTrue(all(not visual["elements"] for visual in scene["waiting_visuals"]))
        self.assertTrue(
            all(
                sum(element["focus_role"] for element in model["elements"]) == 1
                for model in scenes["main_menu"]["render_models"]
            )
        )
        self.assertTrue(any(visual["elements"] for visual in scenes["main_menu"]["waiting_visuals"]))

        reset = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": loaded["project_revision"], "scene_id": "main_menu"},
            )
        )
        self.assertEqual("main_menu", reset["scene"]["scene_id"])
        self.assertEqual("select_start", reset["scene"]["state_id"])
        self.assertEqual({}, reset["variables"])

        moved = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "JOY_DOWN",
                },
            )
        )
        self.assertEqual("select_settings", moved["scene"]["state_id"])
        self.assertEqual({}, moved["variables"])

        chosen = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_A",
                },
            )
        )
        self.assertEqual("settings", chosen["scene"]["scene_id"])

        returned = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_B",
                },
            )
        )
        self.assertEqual("main_menu", returned["scene"]["scene_id"])
        self.assertEqual("select_start", returned["scene"]["state_id"])

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

    def test_graph_commands_build_and_compile_a_complete_menu_state(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "state.add",
                            "scene_id": "state_demo",
                            "state": {
                                "state_id": "options",
                                "display_name": "Options",
                                "waiting_visual_ref": "state_wait",
                                "placement_overrides": [{"element_ref": "cursor", "x": 8, "y": 118}],
                            },
                        },
                        {"kind": "state.set_entry", "scene_id": "state_demo", "state_id": "options"},
                        {"kind": "render_model.set_focus_index", "scene_id": "state_demo", "visual_id": "scene_placement", "focus_index": 4},
                        {
                            "kind": "variable.add",
                            "scene_id": "state_demo",
                            "variable": {"variable_id": "option_index", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 1},
                        },
                        {
                            "kind": "variable.update",
                            "scene_id": "state_demo",
                            "variable": {"variable_id": "option_index", "value_type": "int32", "initial": 1, "minimum": 0, "maximum": 2},
                        },
                        {
                            "kind": "input_action.add",
                            "scene_id": "state_demo",
                            "input_action": {"action_id": "open_options", "logical_source": "BUTTON_B"},
                        },
                        {
                            "kind": "input_action.update",
                            "scene_id": "state_demo",
                            "input_action": {"action_id": "open_options", "logical_source": "BUTTON_START"},
                        },
                        {
                            "kind": "route.add",
                            "scene_id": "state_demo",
                            "route": {
                                "route_id": "options_to_center",
                                "action_ref": "open_options",
                                "from_states": ["options"],
                                "guards": [],
                                "actions": [],
                                "target_state": "center",
                            },
                        },
                        {
                            "kind": "route.guard.add",
                            "scene_id": "state_demo",
                            "route_id": "options_to_center",
                            "guard_index": 0,
                            "guard": {"variable_ref": "option_index", "operator": "ge", "value": 0},
                        },
                        {
                            "kind": "route.guard.add",
                            "scene_id": "state_demo",
                            "route_id": "options_to_center",
                            "guard_index": 1,
                            "guard": {"variable_ref": "option_index", "operator": "le", "value": 2},
                        },
                        {"kind": "route.guard.move", "scene_id": "state_demo", "route_id": "options_to_center", "guard_index": 1, "target_index": 0},
                        {
                            "kind": "route.action.add",
                            "scene_id": "state_demo",
                            "route_id": "options_to_center",
                            "action_index": 0,
                            "action": {"kind": "set_variable", "variable_ref": "option_index", "operation": "assign", "value": 0},
                        },
                        {
                            "kind": "route.action.add",
                            "scene_id": "state_demo",
                            "route_id": "options_to_center",
                            "action_index": 1,
                            "action": {"kind": "request_render"},
                        },
                        {"kind": "route.action.move", "scene_id": "state_demo", "route_id": "options_to_center", "action_index": 1, "target_index": 0},
                        {
                            "kind": "scene.set_reactive_wait_default",
                            "scene_id": "state_demo",
                            "reactive_wait_default": {
                                "policy_id": "state_wait_policy",
                                "waiting_visual_ref": "state_wait",
                                "hold_fallback_allowed": True,
                                "event_interests": ["move_left", "move_right", "joy_move_left", "joy_move_right", "open_details", "open_options"],
                            },
                        },
                        {
                            "kind": "scene.set_interaction_policy",
                            "scene_id": "state_demo",
                            "interaction_policy": {
                                "policy_id": "state_interaction",
                                "mode": "timeout",
                                "meaningful_activity_actions": ["move_left", "move_right", "joy_move_left", "joy_move_right", "open_details", "open_options"],
                                "inactive_route": "preserve_scene",
                                "bounded_deferrals": [],
                            },
                        },
                    ],
                },
            )
        )
        scene = changed["document"]["scenes"][0]
        self.assertEqual("options", scene["entry_state"])
        self.assertEqual(4, next(model for model in scene["render_models"] if model["visual_id"] == "scene_placement")["focus_index"])
        options = next(state for state in scene["states"] if state["state_id"] == "options")
        self.assertEqual([{"element_ref": "cursor", "x": 8, "y": 118}], options["placement_overrides"])
        route = next(route for route in scene["routes"] if route["route_id"] == "options_to_center")
        self.assertEqual(["le", "ge"], [guard["operator"] for guard in route["guards"]])
        self.assertEqual(["request_render", "set_variable"], [action["kind"] for action in route["actions"]])
        built = service.handle(request("project.build_package", {"project_revision": changed["project_revision"]}))
        self.assertGreater(built["package"]["size_bytes"], 0)

    def test_graph_deletes_reject_referenced_records(self) -> None:
        cases = [
            ({"kind": "state.delete", "scene_id": "state_demo", "state_id": "center"}, "entry state"),
            ({"kind": "render_model.delete", "scene_id": "state_demo", "visual_id": "scene_placement"}, "one scene placement model"),
            ({"kind": "variable.delete", "scene_id": "state_demo", "variable_id": "selected_index"}, "referenced by route"),
            ({"kind": "input_action.delete", "scene_id": "state_demo", "action_id": "move_right"}, "referenced by route"),
        ]
        for command, message in cases:
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
            with self.subTest(kind=command["kind"]), self.assertRaises(ProtocolError) as raised:
                service.handle(request("project.apply_commands", {"project_revision": loaded["project_revision"], "commands": [command]}))
            self.assertEqual("COMMAND_TARGET_IN_USE", raised.exception.code)
            self.assertIn(message, raised.exception.message)

    def test_graph_delete_batch_succeeds_after_references_are_detached(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        added = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {"kind": "variable.add", "scene_id": "state_demo", "variable": {"variable_id": "unused_variable", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 1}},
                        {"kind": "input_action.add", "scene_id": "state_demo", "input_action": {"action_id": "unused_input", "logical_source": "BUTTON_START"}},
                        {"kind": "state.add", "scene_id": "state_demo", "state": {"state_id": "unused_state", "display_name": "Unused", "waiting_visual_ref": "state_wait"}},
                        {
                            "kind": "route.add",
                            "scene_id": "state_demo",
                            "route": {"route_id": "unused_route", "action_ref": "unused_input", "from_states": ["unused_state"], "guards": [], "actions": [], "target_state": "center"},
                        },
                        {"kind": "route.guard.add", "scene_id": "state_demo", "route_id": "unused_route", "guard_index": 0, "guard": {"variable_ref": "unused_variable", "operator": "eq", "value": 0}},
                        {"kind": "route.guard.delete", "scene_id": "state_demo", "route_id": "unused_route", "guard_index": 0},
                        {"kind": "route.action.add", "scene_id": "state_demo", "route_id": "unused_route", "action_index": 0, "action": {"kind": "request_render"}},
                        {"kind": "route.action.delete", "scene_id": "state_demo", "route_id": "unused_route", "action_index": 0},
                        {"kind": "route.set_sources", "scene_id": "state_demo", "route_id": "unused_route", "from_states": ["left"]},
                        {"kind": "route.set_action_ref", "scene_id": "state_demo", "route_id": "unused_route", "action_ref": "move_left"},
                    ],
                },
            )
        )
        removed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": added["project_revision"],
                    "commands": [
                        {"kind": "route.delete", "scene_id": "state_demo", "route_id": "unused_route"},
                        {"kind": "state.delete", "scene_id": "state_demo", "state_id": "unused_state"},
                        {"kind": "variable.delete", "scene_id": "state_demo", "variable_id": "unused_variable"},
                        {"kind": "input_action.delete", "scene_id": "state_demo", "action_id": "unused_input"},
                    ],
                },
            )
        )
        scene = removed["document"]["scenes"][0]
        self.assertNotIn("unused_variable", {variable["variable_id"] for variable in scene["variables"]})
        self.assertNotIn("unused_input", {action["action_id"] for action in scene["input_actions"]})
        self.assertNotIn("unused_state", {state["state_id"] for state in scene["states"]})
        self.assertNotIn("unused_route", {route["route_id"] for route in scene["routes"]})

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

    def test_scene_exit_add_creates_unwired_semantic_endpoint(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        details_before = next(
            scene for scene in loaded["document"]["scenes"]
            if scene["scene_id"] == "state_details"
        )
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "scene_exit.add",
                            "scene_id": "state_details",
                            "target_scene": "state_demo",
                        }
                    ],
                },
            )
        )

        applied = changed["applied_commands"][0]
        details = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_details")
        self.assertEqual("scene_exit.add", applied["kind"])
        self.assertEqual(
            {
                "scene_exit_id": "to_state_demo_2",
                "display_name": "State Demo",
                "target_scene": "state_demo",
            },
            applied["scene_exit"],
        )
        self.assertIn(applied["scene_exit"], details["scene_exits"])
        self.assertEqual(details_before["input_actions"], details["input_actions"])
        self.assertEqual(details_before["routes"], details["routes"])
        self.assertEqual(
            details_before["reactive_wait_default"],
            details["reactive_wait_default"],
        )
        self.assertTrue(changed["dirty"])
        self.assertTrue(changed["can_undo"])

        positioned = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": changed["project_revision"],
                    "commands": [{
                        "kind": "editor.state_graph.set_node_position",
                        "scene_id": "state_details",
                        "node_id": "scene-exit-to_state_demo_2",
                        "x": 480,
                        "y": 120,
                    }],
                },
            )
        )
        self.assertEqual(
            {"x": 480, "y": 120},
            positioned["document"]["project"]["editor"]["state_graph"]["scenes"]
            ["state_details"]["nodes"]["scene-exit-to_state_demo_2"],
        )

    def test_route_create_trigger_owns_input_policies_and_entry_socket_layout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "Trigger Game.peepproj"
            service = AuthoringService()
            created = service.handle(
                request("project.create", {"path": str(project_root)})
            )
            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": created["project_revision"],
                        "commands": [
                            {
                                "kind": "route.create_trigger",
                                "scene_id": "main",
                                "source_state": "start",
                                "logical_source": "BUTTON_A",
                                "event_kind": "press",
                                "target_state": "start",
                                "target_handle": "entry-bottom-right",
                                "target_side": "right",
                            }
                        ],
                    },
                )
            )

            applied = changed["applied_commands"][0]
            scene = changed["document"]["scenes"][0]
            self.assertTrue(applied["input_action_created"])
            self.assertEqual(
                {
                    "action_id": "button_a_press",
                    "logical_source": "BUTTON_A",
                    "event_kind": "press",
                },
                applied["input_action"],
            )
            self.assertEqual("start_button_a_press", applied["route"]["route_id"])
            self.assertEqual("start", applied["route"]["target_state"])
            self.assertEqual(["button_a_press"], scene["reactive_wait_default"]["event_interests"])
            self.assertEqual(["button_a_press"], scene["interaction_policy"]["meaningful_activity_actions"])
            layout = changed["document"]["project"]["editor"]["state_graph"]["scenes"]["main"]["routes"]
            self.assertEqual(
                {
                    "routing_version": 3,
                    "rails": [],
                    "target_handle": "entry-bottom-right",
                    "target_side": "right",
                },
                layout["start_button_a_press"]["sources"]["start"],
            )
            self.assertTrue(changed["valid"])

    def test_route_create_trigger_reuses_scene_input_and_rejects_duplicate_source_route(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "Trigger Reuse.peepproj"
            service = AuthoringService()
            created = service.handle(
                request("project.create", {"path": str(project_root)})
            )
            with_second_state = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": created["project_revision"],
                        "commands": [
                            {
                                "kind": "state.add",
                                "scene_id": "main",
                                "state": {
                                    "state_id": "second",
                                    "display_name": "Second",
                                    "waiting_visual_ref": "static_wait",
                                },
                            },
                            {
                                "kind": "route.create_trigger",
                                "scene_id": "main",
                                "source_state": "start",
                                "logical_source": "JOY_DOWN",
                                "event_kind": "repeat",
                                "target_state": "second",
                            },
                        ],
                    },
                )
            )
            reused = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": with_second_state["project_revision"],
                        "commands": [
                            {
                                "kind": "route.create_trigger",
                                "scene_id": "main",
                                "source_state": "second",
                                "logical_source": "JOY_DOWN",
                                "event_kind": "repeat",
                                "target_state": "start",
                            }
                        ],
                    },
                )
            )
            self.assertFalse(reused["applied_commands"][0]["input_action_created"])
            self.assertEqual(1, len(reused["document"]["scenes"][0]["input_actions"]))

            with self.assertRaises(ProtocolError) as raised:
                service.handle(
                    request(
                        "project.apply_commands",
                        {
                            "project_revision": reused["project_revision"],
                            "commands": [
                                {
                                    "kind": "route.create_trigger",
                                    "scene_id": "main",
                                    "source_state": "second",
                                    "logical_source": "JOY_DOWN",
                                    "event_kind": "repeat",
                                    "target_state": "second",
                                }
                            ],
                        },
                    )
                )
            self.assertEqual("ROUTE_TRIGGER_IN_USE", raised.exception.code)

    def test_route_create_trigger_wires_a_declared_scene_exit_without_input_prompt(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        details = next(
            scene for scene in loaded["document"]["scenes"]
            if scene["scene_id"] == "state_details"
        )
        scene_exit = details["scene_exits"][0]
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "route.create_trigger",
                            "scene_id": "state_details",
                            "source_state": details["entry_state"],
                            "logical_source": "BUTTON_L",
                            "scene_exit_ref": scene_exit["scene_exit_id"],
                        }
                    ],
                },
            )
        )
        route = changed["applied_commands"][0]["route"]
        self.assertEqual(scene_exit["scene_exit_id"], route["scene_exit_ref"])
        self.assertEqual(scene_exit["target_scene"], route["target_scene"])
        self.assertNotIn("target_state", route)

    def test_scene_exit_play_sfx_compiles_previews_and_can_be_deleted(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_audio_project(Path(temp_dir))
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            changed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "route.action.add",
                                "scene_id": "state_demo",
                                "route_id": "open_details",
                                "action_index": 0,
                                "action": {
                                    "kind": "play_sfx",
                                    "cue_ref": "ui.select.cue",
                                },
                            }
                        ],
                    },
                )
            )
            source = next(
                scene
                for scene in changed["document"]["scenes"]
                if scene["scene_id"] == "state_demo"
            )
            route = next(
                item for item in source["routes"] if item["route_id"] == "open_details"
            )
            self.assertEqual(
                [{"kind": "play_sfx", "cue_ref": "ui.select.cue"}],
                route["actions"],
            )

            service.handle(
                request(
                    "project.build_package",
                    {"project_revision": changed["project_revision"]},
                )
            )
            reset = service.handle(
                request(
                    "project.preview_reset",
                    {
                        "project_revision": changed["project_revision"],
                        "scene_id": "state_demo",
                    },
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
            self.assertEqual("state_details", preview["scene"]["scene_id"])
            self.assertEqual(
                "ui.select.cue",
                preview["input"]["audio_events"][0]["cue_id"],
            )

            deleted = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": changed["project_revision"],
                        "commands": [
                            {
                                "kind": "route.delete",
                                "scene_id": "state_demo",
                                "route_id": "open_details",
                            }
                        ],
                    },
                )
            )
            source = next(
                scene
                for scene in deleted["document"]["scenes"]
                if scene["scene_id"] == "state_demo"
            )
            self.assertNotIn(
                "open_details",
                {route["route_id"] for route in source["routes"]},
            )

    def test_scene_exit_rejects_scene_local_action(self) -> None:
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
                                "kind": "route.action.add",
                                "scene_id": "state_demo",
                                "route_id": "open_details",
                                "action_index": 0,
                                "action": {"kind": "request_render"},
                            }
                        ],
                    },
                )
            )
        self.assertEqual(
            "SCENE_TRANSITION_ACTION_UNSUPPORTED",
            raised.exception.code,
        )

    def test_scene_exit_set_target_updates_referencing_routes(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(PUBLIC_EXAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "scene_exit.set_target",
                            "scene_id": "main_menu",
                            "scene_exit_id": "to_settings",
                            "target_scene": "credits",
                        }
                    ],
                },
            )
        )

        menu = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "main_menu")
        scene_exit = next(item for item in menu["scene_exits"] if item["scene_exit_id"] == "to_settings")
        route = next(item for item in menu["routes"] if item["route_id"] == "choose_settings")
        self.assertEqual("credits", scene_exit["target_scene"])
        self.assertEqual("credits", route["target_scene"])

    def test_scene_exit_delete_rejects_connected_endpoint_and_removes_unused_endpoint(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        with self.assertRaises(ProtocolError) as raised:
            service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [{
                            "kind": "scene_exit.delete",
                            "scene_id": "state_demo",
                            "scene_exit_id": "to_state_details",
                        }],
                    },
                )
            )
        self.assertEqual("COMMAND_TARGET_IN_USE", raised.exception.code)

        added = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [{
                        "kind": "scene_exit.add",
                        "scene_id": "state_details",
                        "target_scene": "state_demo",
                    }],
                },
            )
        )
        added_exit_id = added["applied_commands"][0]["scene_exit"]["scene_exit_id"]
        deleted = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": added["project_revision"],
                    "commands": [{
                        "kind": "scene_exit.delete",
                        "scene_id": "state_details",
                        "scene_exit_id": added_exit_id,
                    }],
                },
            )
        )
        details = next(scene for scene in deleted["document"]["scenes"] if scene["scene_id"] == "state_details")
        self.assertNotIn(added_exit_id, {item["scene_exit_id"] for item in details["scene_exits"]})

    def test_scene_exit_rejects_unknown_target(self) -> None:
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
                                "kind": "scene_exit.add",
                                "scene_id": "state_demo",
                                "target_scene": "missing",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("COMMAND_TARGET_UNKNOWN", raised.exception.code)

    def test_render_element_set_position_updates_existing_element(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "render_element.set_position",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element_id": "cursor",
                            "x": 31,
                            "y": 42,
                        }
                    ],
                },
            )
        )

        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        screen = next(model for model in demo["render_models"] if model["visual_id"] == "scene_placement")
        cursor = next(element for element in screen["elements"] if element["element_id"] == "cursor")
        self.assertEqual(31, cursor["x"])
        self.assertEqual(42, cursor["y"])
        self.assertEqual(
            {
                "kind": "render_element.set_position",
                "scene_id": "state_demo",
                "render_model_id": "scene_placement",
                "element_id": "cursor",
                "previous_x": 8,
                "previous_y": 73,
                "x": 31,
                "y": 42,
            },
            changed["applied_commands"][0],
        )
        self.assertTrue(changed["dirty"])

    def test_render_element_set_position_rejects_out_of_bounds(self) -> None:
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
                                "kind": "render_element.set_position",
                                "scene_id": "state_demo",
                                "render_model_id": "scene_placement",
                                "element_id": "cursor",
                                "x": 168,
                                "y": 0,
                            }
                        ],
                    },
                )
            )
        self.assertEqual("RENDER_BOUNDS_INVALID", raised.exception.code)

    def test_state_placement_override_moves_one_state_without_duplication(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "state_placement.set_override",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "render_model_id": "scene_placement",
                            "element_id": "cursor",
                            "x": 31,
                            "y": 42,
                        }
                    ],
                },
            )
        )

        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        self.assertEqual(["scene_placement"], [model["visual_id"] for model in demo["render_models"]])
        scene_cursor = demo["render_models"][0]["elements"][0]
        self.assertEqual((8, 73), (scene_cursor["x"], scene_cursor["y"]))
        center = next(state for state in demo["states"] if state["state_id"] == "center")
        self.assertEqual([{"element_ref": "cursor", "x": 31, "y": 42}], center["placement_overrides"])

        built = service.handle(request("project.build_package", {"project_revision": changed["project_revision"]}))
        package = parse_egg(base64.b64decode(built["package"]["blob_base64"]))
        package_scene = next(scene for scene in package.scenes if scene["scene_id"] == "state_demo")
        graph_states = package_scene["graph"]["states"]
        center_state = next(state for state in graph_states if state["state_id"] == "center")
        left_state = next(state for state in graph_states if state["state_id"] == "left")
        center_model = package_scene["render_models"][center_state["render_model_index"]]
        left_model = package_scene["render_models"][left_state["render_model_index"]]
        center_cursor = next(element for element in center_model["elements"] if element["element_id"] == "cursor")
        left_cursor = next(element for element in left_model["elements"] if element["element_id"] == "cursor")
        self.assertEqual((31, 42), (center_cursor["x"], center_cursor["y"]))
        self.assertEqual((8, 43), (left_cursor["x"], left_cursor["y"]))

    def test_state_presentation_commands_add_and_edit_retained_element(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "render_element.add",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element": {
                                "element_id": "menu_box",
                                "kind": "outline_rect",
                                "x": 20,
                                "y": 20,
                                "width": 40,
                                "height": 16,
                                "z_order": 2,
                                "layer": "SCENE",
                                "visible": True,
                            },
                        },
                        {
                            "kind": "render_element.set_bounds",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element_id": "menu_box",
                            "x": 24,
                            "y": 22,
                            "width": 48,
                            "height": 18,
                        },
                        {
                            "kind": "render_element.set_layer",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element_id": "menu_box",
                            "layer": "BACKGROUND",
                        },
                        {
                            "kind": "render_element.set_visibility",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element_id": "menu_box",
                            "visible": False,
                        },
                        {
                            "kind": "render_element.set_z_order",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element_id": "menu_box",
                            "z_order": 7,
                        },
                    ],
                },
            )
        )
        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        model = next(item for item in demo["render_models"] if item["visual_id"] == "scene_placement")
        element = next(item for item in model["elements"] if item["element_id"] == "menu_box")
        self.assertEqual((24, 22, 48, 18), (element["x"], element["y"], element["width"], element["height"]))
        self.assertEqual("BACKGROUND", element["layer"])
        self.assertFalse(element["visible"])
        self.assertEqual(7, element["z_order"])

    def test_text_asset_command_rasterizes_and_builds_as_state_sprite(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "asset.upsert",
                            "asset": {
                                "asset_id": "menu_title",
                                "asset_type": "masked_1bpp",
                                "source_format": "system_font_text",
                                "font_id": "peepshow.system.8x8.basic.v1",
                                "text": "EGGLESS",
                                "scale": 1,
                                "frames": [{"frame_id": "menu_title.frame", "pivot_x": 0, "pivot_y": 0}],
                            },
                        },
                        {
                            "kind": "render_element.add",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element": {
                                "element_id": "menu_title",
                                "kind": "sprite",
                                "visual_ref": "menu_title.frame",
                                "x": 48,
                                "y": 20,
                                "width": 56,
                                "height": 8,
                                "z_order": 20,
                                "layer": "UI",
                                "visible": True,
                            },
                        },
                    ],
                },
            )
        )
        built = service.handle(request("project.build_package", {"project_revision": changed["project_revision"]}))
        package = parse_egg(base64.b64decode(built["package"]["blob_base64"]))
        title = next(frame for frame in package.assets if frame["frame_id"] == "menu_title.frame")
        self.assertEqual((56, 8), (title["width"], title["height"]))

    def test_state_sprite_rejects_general_frame_animation_binding(self) -> None:
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
                                "kind": "render_element.set_visual_ref",
                                "scene_id": "state_demo",
                                "render_model_id": "scene_placement",
                                "element_id": "cursor",
                                "visual_ref": "cursor.blink",
                            }
                        ],
                    },
                )
            )
        self.assertEqual("ASSET_FRAME_UNKNOWN", raised.exception.code)

    def test_render_element_bind_waiting_animation_clones_shared_wait(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        added = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "render_element.add",
                            "scene_id": "state_demo",
                            "render_model_id": "scene_placement",
                            "element": {
                                "element_id": "extra_cursor",
                                "kind": "sprite",
                                "visual_ref": "cursor.phase_a",
                                "x": 48,
                                "y": 40,
                                "width": 8,
                                "height": 16,
                                "z_order": 99,
                                "layer": "SCENE",
                                "visible": True,
                            },
                        },
                    ],
                },
            )
        )
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": added["project_revision"],
                    "commands": [
                        {
                            "kind": "render_element.bind_waiting_animation",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "render_model_id": "scene_placement",
                            "element_id": "extra_cursor",
                            "phase_visual_refs": ["cursor.phase_a", "cursor.phase_b"],
                        },
                    ],
                },
            )
        )
        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        center = next(state for state in demo["states"] if state["state_id"] == "center")
        left = next(state for state in demo["states"] if state["state_id"] == "left")
        right = next(state for state in demo["states"] if state["state_id"] == "right")
        self.assertEqual("state_wait", left["waiting_visual_ref"])
        self.assertEqual("state_wait", right["waiting_visual_ref"])
        self.assertNotEqual("state_wait", center["waiting_visual_ref"])
        state_wait = next(wait for wait in demo["waiting_visuals"] if wait["waiting_visual_id"] == "state_wait")
        self.assertEqual(2, len(state_wait["elements"]))
        center_wait = next(wait for wait in demo["waiting_visuals"] if wait["waiting_visual_id"] == center["waiting_visual_ref"])
        self.assertEqual(250, center_wait["phase_quantum_ms"])
        self.assertEqual(6, center_wait["combined_step_count"])
        self.assertEqual(1, center_wait["settled_step"])
        extra = next(item for item in center_wait["elements"] if item["source_element_ref"] == "extra_cursor")
        self.assertEqual(["cursor.phase_a", "cursor.phase_b"], extra["phase_visual_refs"])
        self.assertEqual([1, 0, 1, 0, 1, 0], extra["step_phase_indices"])

    def test_render_element_bind_waiting_animation_rejects_wrong_size_frames(self) -> None:
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
                                "kind": "render_element.bind_waiting_animation",
                                "scene_id": "state_demo",
                                "state_id": "center",
                                "render_model_id": "scene_placement",
                                "element_id": "cursor",
                                "phase_visual_refs": ["marker.phase_a", "marker.phase_b"],
                            },
                        ],
                    },
                )
            )
        self.assertEqual("WAIT_FRAME_SIZE_INVALID", raised.exception.code)

    def test_render_element_clear_waiting_animation_removes_state_binding(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "render_element.clear_waiting_animation",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "render_model_id": "scene_placement",
                            "element_id": "cursor",
                        },
                    ],
                },
            )
        )
        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        center = next(state for state in demo["states"] if state["state_id"] == "center")
        center_wait = next(wait for wait in demo["waiting_visuals"] if wait["waiting_visual_id"] == center["waiting_visual_ref"])
        self.assertTrue(all(item["source_element_ref"] != "cursor" for item in center_wait["elements"]))
        self.assertEqual(3, center_wait["combined_step_count"])

    def test_waiting_visual_upsert_authors_bounded_state_loop(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "waiting_visual.upsert",
                            "scene_id": "state_demo",
                            "waiting_visual": {
                                "waiting_visual_id": "menu_wait",
                                "presentation_id": "menu.presentation",
                                "phase_quantum_ms": 125,
                                "combined_step_count": 4,
                                "settled_step": 1,
                                "cycle_policy": "loop",
                                "elements": [
                                    {
                                        "element_id": "cursor_loop",
                                        "source_element_ref": "cursor",
                                        "phase_visual_refs": ["cursor.phase_a", "cursor.phase_b"],
                                        "step_phase_indices": [0, 1, 0, 1],
                                    }
                                ],
                            },
                        },
                        {
                            "kind": "state.set_waiting_visual",
                            "scene_id": "state_demo",
                            "state_id": "center",
                            "waiting_visual_id": "menu_wait",
                        },
                    ],
                },
            )
        )
        demo = next(scene for scene in changed["document"]["scenes"] if scene["scene_id"] == "state_demo")
        waiting = next(item for item in demo["waiting_visuals"] if item["waiting_visual_id"] == "menu_wait")
        self.assertEqual(125, waiting["phase_quantum_ms"])
        self.assertEqual([0, 1, 0, 1], waiting["elements"][0]["step_phase_indices"])
        center = next(item for item in demo["states"] if item["state_id"] == "center")
        self.assertEqual("menu_wait", center["waiting_visual_ref"])

    def test_animation_catalog_edit_survives_project_save(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "animation_edit.peepproj"
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
                                "kind": "animation.upsert",
                                "animation": {
                                    "animation_id": "cursor.blink",
                                    "frame_refs": ["cursor.phase_a", "cursor.phase_b"],
                                    "frame_duration_ms": [100, 300],
                                    "loop_policy": "loop",
                                },
                            }
                        ],
                    },
                )
            )
            service.handle(request("project.save", {"project_revision": changed["project_revision"]}))
            reloaded = load_project(project_root)
            cursor = next(item for item in reloaded.animations if item["animation_id"] == "cursor.blink")
            self.assertEqual([100, 300], cursor["frame_duration_ms"])

    def test_short_start_can_be_authored_compiled_and_previewed(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        source = next(
            scene for scene in loaded["document"]["scenes"]
            if scene["scene_id"] == "state_demo"
        )
        wait_policy = dict(source["reactive_wait_default"])
        wait_policy["event_interests"] = [*wait_policy["event_interests"], "open_details_start"]
        interaction_policy = dict(source["interaction_policy"])
        interaction_policy["meaningful_activity_actions"] = [
            *interaction_policy["meaningful_activity_actions"],
            "open_details_start",
        ]
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "input_action.add",
                            "scene_id": "state_demo",
                            "input_action": {
                                "action_id": "open_details_start",
                                "logical_source": "BUTTON_START",
                            },
                        },
                        {
                            "kind": "route.add",
                            "scene_id": "state_demo",
                            "route": {
                                "route_id": "open_details_start",
                                "action_ref": "open_details_start",
                                "from_states": ["center"],
                                "guards": [],
                                "actions": [],
                                "scene_exit_ref": "to_state_details",
                                "target_scene": "state_details",
                            },
                        },
                        {
                            "kind": "scene.set_reactive_wait_default",
                            "scene_id": "state_demo",
                            "reactive_wait_default": wait_policy,
                        },
                        {
                            "kind": "scene.set_interaction_policy",
                            "scene_id": "state_demo",
                            "interaction_policy": interaction_policy,
                        },
                    ],
                },
            )
        )
        service.handle(request("project.build_package", {"project_revision": changed["project_revision"]}))
        preview = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": changed["project_revision"], "scene_id": "state_demo"},
            )
        )
        advanced = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": changed["project_revision"],
                    "preview_revision": preview["preview_revision"],
                    "logical_source": "BUTTON_START",
                },
            )
        )
        self.assertEqual("state_details", advanced["scene"]["scene_id"])

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

    def test_state_graph_node_position_is_editor_only_and_persists(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "state_layout.peepproj"
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
                                "kind": "editor.state_graph.set_node_position",
                                "scene_id": "state_demo",
                                "state_id": "right",
                                "x": -256.5,
                                "y": 384.2,
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
                {"x": -256, "y": 384},
                moved["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["nodes"]["right"],
            )
            self.assertTrue(moved["dirty"])
            saved = service.handle(request("project.save", {"project_revision": moved["project_revision"]}))
            self.assertIn("project.json", saved["saved_sources"])

            reloaded = load_project(project_root)
            self.assertEqual(
                {"x": -256, "y": 384},
                reloaded.normalized()["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["nodes"]["right"],
            )

    def test_state_graph_route_waypoints_are_editor_only_and_persist(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "route_layout.peepproj"
            shutil.copytree(SAMPLE, project_root)
            service = AuthoringService()
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            before = service.handle(
                request("project.build_package", {"project_revision": loaded["project_revision"]})
            )
            routed = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": loaded["project_revision"],
                        "commands": [
                            {
                                "kind": "editor.state_graph.set_route_layout",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "source_state": "center",
                                "rails": [
                                    {"axis": "x", "value": 144.4},
                                    {"axis": "y", "value": -64.6},
                                ],
                                "target_handle": "entry-bottom-right",
                                "target_side": "right",
                            }
                        ],
                    },
                )
            )
            expected = [{"axis": "x", "value": 144}, {"axis": "y", "value": -65}]
            route_layout = routed["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["routes"]["center_to_right"]
            self.assertEqual(3, route_layout["sources"]["center"]["routing_version"])
            self.assertEqual(expected, route_layout["sources"]["center"]["rails"])
            self.assertEqual("entry-bottom-right", route_layout["sources"]["center"]["target_handle"])
            self.assertEqual("right", route_layout["sources"]["center"]["target_side"])
            after = service.handle(
                request("project.build_package", {"project_revision": routed["project_revision"]})
            )
            self.assertEqual(before["package"]["sha256"], after["package"]["sha256"])

            undone = service.handle(request("project.undo", {"project_revision": routed["project_revision"]}))
            undone_routes = undone["document"]["project"].get("editor", {}).get("state_graph", {}).get("scenes", {}).get("state_demo", {}).get("routes", {})
            self.assertNotIn("center_to_right", undone_routes)
            redone = service.handle(request("project.redo", {"project_revision": undone["project_revision"]}))
            redone_route = redone["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["routes"]["center_to_right"]
            self.assertEqual(3, redone_route["sources"]["center"]["routing_version"])
            self.assertEqual(expected, redone_route["sources"]["center"]["rails"])
            self.assertEqual("entry-bottom-right", redone_route["sources"]["center"]["target_handle"])
            self.assertEqual("right", redone_route["sources"]["center"]["target_side"])

            saved = service.handle(request("project.save", {"project_revision": redone["project_revision"]}))
            self.assertIn("project.json", saved["saved_sources"])
            reloaded = load_project(project_root)
            persisted = reloaded.normalized()["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["routes"]["center_to_right"]
            self.assertEqual(3, persisted["sources"]["center"]["routing_version"])
            self.assertEqual(expected, persisted["sources"]["center"]["rails"])
            self.assertEqual("entry-bottom-right", persisted["sources"]["center"]["target_handle"])
            self.assertEqual("right", persisted["sources"]["center"]["target_side"])

            reset = service.handle(
                request(
                    "project.apply_commands",
                    {
                        "project_revision": saved["project_revision"],
                        "commands": [
                            {
                                "kind": "editor.state_graph.set_route_layout",
                                "scene_id": "state_demo",
                                "route_id": "center_to_right",
                                "source_state": "center",
                                "rails": [],
                                "target_handle": None,
                                "target_side": None,
                            }
                        ],
                    },
                )
            )
            reset_routes = reset["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"].get("routes", {})
            self.assertNotIn("center_to_right", reset_routes)

    def test_state_graph_target_socket_persists_without_manual_rails(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "editor.state_graph.set_route_layout",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "source_state": "center",
                            "rails": [],
                            "target_handle": "entry-bottom-left",
                            "target_side": "left",
                        }
                    ],
                },
            )
        )
        source_layout = changed["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"]["routes"]["center_to_right"]["sources"]["center"]
        self.assertEqual(3, source_layout["routing_version"])
        self.assertEqual([], source_layout["rails"])
        self.assertEqual("entry-bottom-left", source_layout["target_handle"])
        self.assertEqual("left", source_layout["target_side"])

    def test_state_graph_route_layout_rejects_unknown_routes_and_excess_rails(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        cases = [
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "missing",
                    "source_state": "center",
                    "rails": [{"axis": "x", "value": 0}],
                    "target_handle": "entry-top-left",
                    "target_side": "top",
                },
                "COMMAND_TARGET_UNKNOWN",
            ),
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "center_to_right",
                    "source_state": "left",
                    "rails": [{"axis": "x", "value": 0}],
                    "target_handle": "entry-top-left",
                    "target_side": "top",
                },
                "COMMAND_TARGET_UNKNOWN",
            ),
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "center_to_right",
                    "source_state": "center",
                    "rails": [
                        {"axis": "x" if index % 2 == 0 else "y", "value": index}
                        for index in range(9)
                    ],
                    "target_handle": "entry-top-left",
                    "target_side": "top",
                },
                "PROJECT_LIMIT_EXCEEDED",
            ),
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "center_to_right",
                    "source_state": "center",
                    "rails": [],
                    "target_handle": "entry-left",
                    "target_side": "top",
                },
                "PROJECT_VALUE_INVALID",
            ),
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "center_to_right",
                    "source_state": "center",
                    "rails": [{"axis": "x", "value": 10}, {"axis": "x", "value": 20}],
                    "target_handle": "entry-top-left",
                    "target_side": "top",
                },
                "PROJECT_VALUE_INVALID",
            ),
            (
                {
                    "kind": "editor.state_graph.set_route_layout",
                    "scene_id": "state_demo",
                    "route_id": "center_to_right",
                    "source_state": "center",
                    "rails": [],
                    "target_handle": "entry-top-left",
                    "target_side": "right",
                },
                "PROJECT_VALUE_INVALID",
            ),
        ]
        for command, code in cases:
            with self.subTest(code=code):
                with self.assertRaises(ProtocolError) as raised:
                    service.handle(
                        request(
                            "project.apply_commands",
                            {
                                "project_revision": loaded["project_revision"],
                                "commands": [command],
                            },
                        )
                    )
                self.assertEqual(code, raised.exception.code)

    def test_state_graph_route_layout_is_removed_with_a_route_source(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(SAMPLE)}))
        routed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": loaded["project_revision"],
                    "commands": [
                        {
                            "kind": "editor.state_graph.set_route_layout",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "source_state": "center",
                            "rails": [{"axis": "x", "value": 100}],
                            "target_handle": "entry-top-left",
                            "target_side": "top",
                        }
                    ],
                },
            )
        )
        changed = service.handle(
            request(
                "project.apply_commands",
                {
                    "project_revision": routed["project_revision"],
                    "commands": [
                        {
                            "kind": "route.set_sources",
                            "scene_id": "state_demo",
                            "route_id": "center_to_right",
                            "from_states": ["left"],
                        }
                    ],
                },
            )
        )
        routes = changed["document"]["project"]["editor"]["state_graph"]["scenes"]["state_demo"].get("routes", {})
        self.assertNotIn("center_to_right", routes)

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
            [
                "project.json",
                "scenes/state_demo.state.json",
                "scenes/state_details.state.json",
                "assets/catalog.json",
            ],
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
        self.assertEqual(64, len(first["target_profile"]["profile_hash"]))
        self.assertEqual(
            5242880, first["budgets"]["package_size"]["limit_bytes"]
        )
        self.assertEqual("passed", first["budgets"]["package_size"]["status"])
        self.assertEqual(4194304, first["budgets"]["audio"]["limit_bytes"])
        self.assertEqual("passed", first["budgets"]["audio"]["status"])
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

    def test_preview_state_renders_exact_state_without_touching_live_preview(self) -> None:
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

            selected = service.handle(
                request(
                    "project.preview_state",
                    {
                        "project_revision": loaded["project_revision"],
                        "scene_id": "state_demo",
                        "state_id": "right",
                    },
                )
            )

            self.assertEqual(reset["preview_revision"], selected["preview_revision"])
            self.assertEqual("state_demo", selected["scene"]["scene_id"])
            self.assertEqual("right", selected["scene"]["state_id"])
            self.assertEqual(0, selected["timeline"]["elapsed_ms"])
            self.assertNotEqual(reset["framebuffer"]["sha256"], selected["framebuffer"]["sha256"])

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
            self.assertEqual("center", advanced["scene"]["state_id"])
            self.assertEqual(250, advanced["timeline"]["elapsed_ms"])

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

    def test_preview_applies_state_element_actions_atomically(self) -> None:
        service = AuthoringService()
        loaded = service.handle(request("project.load", {"path": str(TRANSITION_SAMPLE)}))
        reset = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
            )
        )
        baseline_left = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_L",
                },
            )
        )
        self.assertEqual("left", baseline_left["scene"]["state_id"])

        reset = service.handle(
            request(
                "project.preview_reset",
                {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
            )
        )
        moved_right = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_R",
                },
            )
        )
        changed_left = service.handle(
            request(
                "project.preview_input",
                {
                    "project_revision": loaded["project_revision"],
                    "preview_revision": reset["preview_revision"],
                    "logical_source": "BUTTON_R",
                },
            )
        )
        self.assertEqual("right", moved_right["scene"]["state_id"])
        self.assertEqual("left", changed_left["scene"]["state_id"])
        self.assertEqual(0, changed_left["variables"]["selected_index"])
        self.assertNotEqual(
            baseline_left["framebuffer"]["sha256"],
            changed_left["framebuffer"]["sha256"],
        )

    def test_preview_renders_static_primitives_and_rejects_stale_sessions(self) -> None:
        service = AuthoringService()
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_procedural_project(Path(temp_dir))
            loaded = service.handle(request("project.load", {"path": str(project_root)}))
            reset = service.handle(
                request(
                    "project.preview_reset",
                    {"project_revision": loaded["project_revision"], "scene_id": "state_demo"},
                )
            )
            self.assertGreater(reset["framebuffer"]["black_pixel_count"], 0)
            framebuffer = base64.b64decode(reset["framebuffer"]["data_base64"], validate=True)

            def pixel(x: int, y: int) -> int:
                return (framebuffer[y * 21 + x // 8] >> (7 - (x % 8))) & 1

            for point in (
                (20, 20), (28, 26),
                (32, 20), (36, 23), (40, 26),
                (44, 20), (52, 26),
                (60, 20), (64, 24), (60, 28), (56, 24),
                (73, 20), (78, 23), (73, 26), (68, 23),
            ):
                self.assertEqual(1, pixel(*point), point)
            self.assertEqual(0, pixel(48, 23))
            self.assertEqual(0, pixel(60, 24))

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
