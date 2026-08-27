from __future__ import annotations

import copy
import hashlib
import json
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

from PIL import Image


TOOL_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.project import load_project  # noqa: E402
from peepshow_authoring.system_fonts import (  # noqa: E402
    SYSTEM_FONT_8X8_BASIC_ID,
    SystemFontError,
    rasterize_system_font_text,
)
from peepshow_authoring.compiler import (  # noqa: E402
    EggCompileError,
    build_egg,
    write_egg,
    write_embedded_egg_c,
)
from peepshow_authoring.egg_format import (  # noqa: E402
    ASSET_HEADER,
    ASSET_RECORD,
    CHUNK_ANIMATION_TABLE,
    CHUNK_ASSET_TABLE,
    CHUNK_ENTRY,
    CHUNK_MASKED_1BPP_SPRITE_BANK,
    FOOTER,
    HEADER,
    RENDER_ELEMENT_RECORD_V1,
    RENDER_HEADER,
    RENDER_MODEL_RECORD,
    EggFormatError,
    _parse_render,
    parse_egg,
)


SAMPLE = WORKSPACE_ROOT / "examples" / "authoring" / "state_slice.peepproj"
EMBEDDED_PROJECT = (
    WORKSPACE_ROOT
    / "examples"
    / "authoring"
    / "state_transition_slice.peepproj"
)
EMBEDDED_SAMPLE = (
    WORKSPACE_ROOT
    / "firmware"
    / "peepshow_hw6_fw0"
    / "Core"
    / "Src"
    / "ps_embedded_egg_autogen.c"
)


def resign_package(blob: bytearray) -> None:
    footer_offset = HEADER.unpack_from(blob)[5]
    blob[footer_offset:] = FOOTER.pack(
        b"END1",
        1,
        FOOTER.size,
        hashlib.sha256(blob[:footer_offset]).digest(),
    )


def make_asset_project(parent: Path, *, invalid_pixel: bool = False) -> Path:
    project_root = parent / "asset_slice.peepproj"
    shutil.copytree(SAMPLE, project_root)
    asset_dir = project_root / "assets"
    shutil.rmtree(asset_dir)
    asset_dir.mkdir()

    project = json.loads((project_root / "project.json").read_text(encoding="utf-8"))
    project["asset_sources"] = ["assets/catalog.json"]
    (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")

    image = Image.new("RGBA", (16, 16), (255, 255, 255, 0))
    for y in range(16):
        for x in range(8):
            if x in {0, 7} or y in {0, 15}:
                image.putpixel((x, y), (0, 0, 0, 255))
        for x in range(8, 16):
            color = (0, 0, 0, 255) if x in {11, 12} else (255, 255, 255, 255)
            image.putpixel((x, y), color)
    if invalid_pixel:
        image.putpixel((1, 1), (127, 127, 127, 255))
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


class AuthoringModelTests(unittest.TestCase):
    def test_sample_is_valid_and_normalization_is_deterministic(self) -> None:
        first = load_project(SAMPLE)
        second = load_project(SAMPLE)
        self.assertEqual((), first.issues)
        self.assertEqual(first.canonical_bytes(), second.canonical_bytes())
        normalized = json.loads(first.canonical_bytes())
        self.assertEqual("state_demo", normalized["project"]["entry_scene"])
        self.assertEqual(2, len(normalized["scenes"]))

    def test_joystick_direction_is_a_stable_logical_source(self) -> None:
        bundle = load_project(SAMPLE)
        self.assertEqual((), bundle.issues)
        package = parse_egg(build_egg(bundle))
        scene = package.scenes[0]
        joy_input = next(
            item
            for item in scene["graph"]["inputs"]
            if item["action_id"] == "joy_move_left"
        )
        self.assertEqual(6, joy_input["logical_source"])

    def test_state_element_actions_compile_and_round_trip(self) -> None:
        package = parse_egg(build_egg(load_project(EMBEDDED_PROJECT)))
        scene = next(item for item in package.scenes if item["scene_id"] == "state_demo")
        routes = {item["route_id"]: item for item in scene["graph"]["routes"]}

        center_to_right = routes["center_to_right"]["operations"]
        moved = next(item for item in center_to_right if item["kind"] == 4)
        selected_waiting = next(
            item for item in center_to_right if item["kind"] == 6
        )
        self.assertEqual((120, 0), (moved["x"], moved["y"]))
        self.assertEqual(1, selected_waiting["timeline_policy"])
        self.assertEqual("marker_hold", selected_waiting["waiting_element_ref"])

        right_to_left = routes["right_to_left"]["operations"]
        restored_waiting = next(
            item for item in right_to_left if item["kind"] == 6
        )
        self.assertEqual(2, restored_waiting["timeline_policy"])
        self.assertEqual("marker_wait", restored_waiting["waiting_element_ref"])

        right_to_left = routes["right_to_left"]["operations"]
        hidden = next(item for item in right_to_left if item["kind"] == 3)
        framed = next(item for item in right_to_left if item["kind"] == 5)
        self.assertEqual(0, hidden["visible"])
        self.assertEqual("marker.phase_c", framed["frame_ref"])

    def test_unknown_transition_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "broken.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            scene = json.loads((SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8"))
            broken = copy.deepcopy(scene)
            broken["routes"][0]["target_state"] = "missing"
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "state_demo.state.json").write_text(json.dumps(broken), encoding="utf-8")

            bundle = load_project(project_root)
            self.assertIn("GRAPH_TRANSITION_TARGET_UNKNOWN", {issue.code for issue in bundle.issues})

    def test_cross_scene_route_compiles_and_round_trips(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "scene_transition.peepproj"
            shutil.copytree(SAMPLE, project_root)
            project_path = project_root / "project.json"
            source_path = project_root / "scenes" / "state_demo.state.json"
            target_path = project_root / "scenes" / "state_target.state.json"
            project = json.loads(project_path.read_text(encoding="utf-8"))
            source = json.loads(source_path.read_text(encoding="utf-8"))
            target = copy.deepcopy(source)

            project["scene_sources"].append("scenes/state_target.state.json")
            source["input_actions"].append(
                {"action_id": "enter_target", "logical_source": "BUTTON_B"}
            )
            source["routes"].append(
                {
                    "route_id": "enter_target",
                    "action_ref": "enter_target",
                    "from_states": ["left", "center", "right"],
                    "guards": [],
                    "actions": [],
                    "target_scene": "state_target",
                }
            )
            source["reactive_wait_default"]["event_interests"].append("enter_target")
            source["interaction_policy"]["meaningful_activity_actions"].append("enter_target")

            target["scene_id"] = "state_target"
            target["display_name"] = "State Target"
            target["input_actions"] = [
                {"action_id": "return_source", "logical_source": "BUTTON_A"}
            ]
            target["routes"] = [
                {
                    "route_id": "return_source",
                    "action_ref": "return_source",
                    "from_states": ["left", "center", "right"],
                    "guards": [],
                    "actions": [],
                    "target_scene": "state_demo",
                }
            ]
            target["reactive_wait_default"]["event_interests"] = ["return_source"]
            target["interaction_policy"]["meaningful_activity_actions"] = ["return_source"]

            project_path.write_text(json.dumps(project), encoding="utf-8")
            source_path.write_text(json.dumps(source), encoding="utf-8")
            target_path.write_text(json.dumps(target), encoding="utf-8")

            bundle = load_project(project_root)
            self.assertEqual((), bundle.issues)
            package = parse_egg(build_egg(bundle))
            self.assertEqual(3, package.manifest["scene_count"])
            self.assertEqual(3, len(package.scenes))
            self.assertTrue(all(scene["graph"]["format_version"] == 3 for scene in package.scenes))
            targets = {
                route["target_scene"]
                for scene in package.scenes
                for route in scene["graph"]["routes"]
                if route["target_scene"] is not None
            }
            self.assertEqual({"state_demo", "state_details", "state_target"}, targets)

    def test_unknown_scene_transition_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "broken_scene_transition.peepproj"
            shutil.copytree(SAMPLE, project_root)
            scene_path = project_root / "scenes" / "state_demo.state.json"
            scene = json.loads(scene_path.read_text(encoding="utf-8"))
            scene["routes"][0]["target_scene"] = "missing_scene"
            del scene["routes"][0]["target_state"]
            scene["routes"][0]["actions"] = []
            scene_path.write_text(json.dumps(scene), encoding="utf-8")

            bundle = load_project(project_root)
            self.assertIn("SCENE_TRANSITION_TARGET_UNKNOWN", {issue.code for issue in bundle.issues})

    def test_source_path_cannot_escape_project(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "broken.peepproj"
            project_root.mkdir()
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["scene_sources"] = ["../outside.json"]
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")

            bundle = load_project(project_root)
            self.assertIn("SCENE_SOURCE_INVALID", {issue.code for issue in bundle.issues})

    def test_malformed_compiler_value_is_reported_before_packing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "broken.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["package"]["display_name"] = 42
            scene = (SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8")
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "state_demo.state.json").write_text(scene, encoding="utf-8")

            bundle = load_project(project_root)
            self.assertIn("PROJECT_TEXT_INVALID", {issue.code for issue in bundle.issues})
            with self.assertRaises(EggCompileError):
                build_egg(bundle)

    def test_egg_build_is_deterministic_and_round_trips(self) -> None:
        bundle = load_project(SAMPLE)
        first = build_egg(bundle)
        second = build_egg(bundle)
        self.assertEqual(first, second)

        package = parse_egg(first)
        self.assertEqual("dev.peepshow.state_slice", package.manifest["package_id"])
        self.assertEqual("state_demo", package.manifest["entry_scene"])
        self.assertEqual(12, len(package.chunks))
        self.assertEqual(5, len(package.assets))
        self.assertEqual(2, len(package.scenes))
        self.assertEqual(3, package.scenes[0]["state_count"])
        self.assertEqual(13, package.scenes[0]["route_count"])
        self.assertEqual(1, package.scenes[1]["state_count"])
        self.assertEqual(1, package.scenes[1]["route_count"])
        self.assertEqual(2, package.scenes[0]["render_format_version"])
        first_model = package.scenes[0]["render_models"][0]
        self.assertEqual(2, first_model["elements"][0]["layer"])
        self.assertTrue(first_model["elements"][0]["visible"])
        self.assertEqual(1, first_model["elements"][0]["kind"])
        self.assertEqual(1, first_model["elements"][1]["layer"])
        self.assertNotIn("scenes/state_demo.state.json", package.strings)

    def test_rnd1_render_payload_remains_load_compatible(self) -> None:
        strings = ("view", "cursor", "cursor.phase_a")
        payload = b"".join(
            (
                RENDER_HEADER.pack(b"RND1", 1, RENDER_HEADER.size, 1, 1, 0, 0),
                RENDER_MODEL_RECORD.pack(0, 1, 0, 1),
                RENDER_ELEMENT_RECORD_V1.pack(1, 2, 1, 1, 8, 12, 8, 16, 3),
            )
        )
        parsed = _parse_render(payload, strings)
        element = parsed["models"][0]["elements"][0]
        self.assertEqual(1, parsed["format_version"])
        self.assertEqual(2, element["layer"])
        self.assertEqual(1, element["visible"])
        self.assertEqual(3, element["z_order"])

    def test_editor_source_path_does_not_change_package_bytes(self) -> None:
        original = build_egg(load_project(SAMPLE))
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "renamed.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            shutil.copytree(SAMPLE / "assets", project_root / "assets")
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["scene_sources"] = ["scenes/renamed.json", "scenes/state_details.state.json"]
            scene = (SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8")
            details = (SAMPLE / "scenes" / "state_details.state.json").read_text(encoding="utf-8")
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "renamed.json").write_text(scene, encoding="utf-8")
            (scene_dir / "state_details.state.json").write_text(details, encoding="utf-8")
            renamed = build_egg(load_project(project_root))
        self.assertEqual(original, renamed)

    def test_system_font_text_rasterizes_exact_hw4_glyph_bits(self) -> None:
        frame = rasterize_system_font_text(
            {
                "asset_id": "label",
                "font_id": SYSTEM_FONT_8X8_BASIC_ID,
                "text": "A",
                "scale": 1,
                "frames": [{"frame_id": "label.a", "pivot_x": 0, "pivot_y": 0}],
            }
        )
        self.assertEqual((8, 8, 1), (frame.width, frame.height, frame.row_stride_bytes))
        self.assertEqual(bytes.fromhex("30 78 cc cc fc cc cc 00"), frame.pixels)
        self.assertEqual(frame.pixels, frame.mask)
        self.assertFalse(frame.opaque)

    def test_system_font_text_supports_newlines_and_integer_scaling(self) -> None:
        frame = rasterize_system_font_text(
            {
                "asset_id": "label",
                "font_id": SYSTEM_FONT_8X8_BASIC_ID,
                "text": "A\nB",
                "scale": 2,
                "frames": [{"frame_id": "label.ab", "pivot_x": -1, "pivot_y": 2}],
            }
        )
        self.assertEqual((16, 32, 2), (frame.width, frame.height, frame.row_stride_bytes))
        self.assertEqual((-1, 2), (frame.pivot_x, frame.pivot_y))

    def test_system_font_text_rejects_unsupported_or_oversized_content(self) -> None:
        base = {
            "asset_id": "label",
            "font_id": SYSTEM_FONT_8X8_BASIC_ID,
            "text": "A",
            "scale": 1,
            "frames": [{"frame_id": "label.a", "pivot_x": 0, "pivot_y": 0}],
        }
        cases = [
            ({**base, "text": "caf\u00e9"}, "printable ASCII"),
            ({**base, "text": "A" * 22}, "must fit 168x144"),
            ({**base, "font_id": "unknown"}, "font_id must be"),
            ({**base, "scale": 0}, "scale must be"),
            ({**base, "text": "   "}, "visible glyph"),
        ]
        for asset, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(SystemFontError, message):
                rasterize_system_font_text(asset)

    def test_system_font_text_compiles_as_an_ordinary_sprite_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "text.peepproj"
            shutil.copytree(SAMPLE, project_root)
            catalog_path = project_root / "assets" / "catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["assets"].append(
                {
                    "asset_id": "eggless_label",
                    "asset_type": "masked_1bpp",
                    "source_format": "system_font_text",
                    "font_id": SYSTEM_FONT_8X8_BASIC_ID,
                    "text": "EGGLESS",
                    "scale": 1,
                    "frames": [{"frame_id": "eggless_label.frame", "pivot_x": 0, "pivot_y": 0}],
                }
            )
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            scene_path = project_root / "scenes" / "state_demo.state.json"
            scene = json.loads(scene_path.read_text(encoding="utf-8"))
            scene["render_models"][0]["elements"].append(
                {
                    "element_id": "eggless_label",
                    "kind": "sprite",
                    "visual_ref": "eggless_label.frame",
                    "x": 48,
                    "y": 20,
                    "width": 56,
                    "height": 8,
                    "z_order": 1,
                    "layer": "UI",
                }
            )
            scene_path.write_text(json.dumps(scene), encoding="utf-8")

            bundle = load_project(project_root)
            self.assertEqual((), bundle.issues)
            package = parse_egg(build_egg(bundle))
        label = next(asset for asset in package.assets if asset["frame_id"] == "eggless_label.frame")
        self.assertEqual((56, 8), (label["width"], label["height"]))
        self.assertFalse(label["opaque"])

    def test_masked_1bpp_assets_compile_and_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_asset_project(Path(temp_dir))
            bundle = load_project(project_root)
            self.assertEqual((), bundle.issues)
            self.assertEqual(5, len(bundle.frames))

            package = parse_egg(build_egg(bundle))
            self.assertEqual(12, len(package.chunks))
            self.assertEqual(
                [CHUNK_ASSET_TABLE, CHUNK_MASKED_1BPP_SPRITE_BANK, CHUNK_ANIMATION_TABLE],
                [chunk.chunk_type for chunk in package.chunks[-3:]],
            )
            self.assertEqual(("cursor.phase_a", "cursor.phase_b"), tuple(asset["frame_id"] for asset in package.assets[:2]))
            self.assertEqual(0xFF, package.assets[0]["pixels"][0])
            self.assertEqual(0x81, package.assets[0]["pixels"][1])
            self.assertEqual(0x81, package.assets[0]["mask"][1])
            self.assertFalse(package.assets[0]["opaque"])
            self.assertTrue(package.assets[1]["opaque"])
            self.assertEqual(b"", package.assets[1]["mask"])
            self.assertEqual((0, 1), package.animations[0]["frame_indexes"])
            self.assertEqual((250, 250), package.animations[0]["frame_duration_ms"])

    def test_asset_build_is_independent_of_project_location(self) -> None:
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first = build_egg(load_project(make_asset_project(Path(first_dir))))
            second = build_egg(load_project(make_asset_project(Path(second_dir))))
        self.assertEqual(first, second)

    def test_normalized_hash_changes_when_compiled_pixels_change(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_asset_project(Path(temp_dir))
            before = load_project(project_root).canonical_bytes()
            image_path = project_root / "assets" / "cursor.png"
            with Image.open(image_path) as opened:
                image = opened.convert("RGBA")
            image.putpixel((1, 1), (0, 0, 0, 255))
            image.save(image_path, format="PNG")
            after = load_project(project_root).canonical_bytes()
        self.assertNotEqual(before, after)

    def test_masked_1bpp_import_rejects_visible_gray_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_asset_project(Path(temp_dir), invalid_pixel=True)
            bundle = load_project(project_root)
        self.assertIn("ASSET_SOURCE_INVALID", {issue.code for issue in bundle.issues})

    def test_asset_record_size_is_checked_after_container_integrity(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            blob = bytearray(build_egg(load_project(make_asset_project(Path(temp_dir)))))
        chunk_count = HEADER.unpack_from(blob)[6]
        for index in range(chunk_count):
            entry_offset = HEADER.size + index * CHUNK_ENTRY.size
            entry = list(CHUNK_ENTRY.unpack_from(blob, entry_offset))
            if entry[0] != CHUNK_ASSET_TABLE:
                continue
            record_offset = entry[4] + ASSET_HEADER.size
            record = list(ASSET_RECORD.unpack_from(blob, record_offset))
            record[9] += 1
            ASSET_RECORD.pack_into(blob, record_offset, *record)
            payload = blob[entry[4] : entry[4] + entry[5]]
            entry[6] = zlib.crc32(payload) & 0xFFFFFFFF
            CHUNK_ENTRY.pack_into(blob, entry_offset, *entry)
            break
        resign_package(blob)
        with self.assertRaisesRegex(EggFormatError, "asset pixel size is invalid"):
            parse_egg(bytes(blob))

    def test_asset_source_path_cannot_escape_project(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = make_asset_project(Path(temp_dir))
            catalog_path = project_root / "assets" / "catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["assets"][0]["source_path"] = "../outside.png"
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            bundle = load_project(project_root)
        self.assertIn("ASSET_SOURCE_INVALID", {issue.code for issue in bundle.issues})

    def test_multiple_state_scenes_have_distinct_chunk_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "multiple.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            shutil.copytree(SAMPLE / "assets", project_root / "assets")
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["scene_sources"] = [
                "scenes/state_demo.state.json",
                "scenes/state_details.state.json",
                "scenes/second.state.json",
            ]
            scene = json.loads((SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8"))
            details = json.loads((SAMPLE / "scenes" / "state_details.state.json").read_text(encoding="utf-8"))
            second = copy.deepcopy(scene)
            second["scene_id"] = "second_scene"
            second["display_name"] = "Second Scene"
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "state_demo.state.json").write_text(json.dumps(scene), encoding="utf-8")
            (scene_dir / "state_details.state.json").write_text(json.dumps(details), encoding="utf-8")
            (scene_dir / "second.state.json").write_text(json.dumps(second), encoding="utf-8")

            package = parse_egg(build_egg(load_project(project_root)))
            self.assertEqual(
                ("second_scene", "state_demo", "state_details"),
                tuple(item["scene_id"] for item in package.scenes),
            )
            self.assertEqual(15, len(package.chunks))

    def test_egg_corruption_and_truncation_are_rejected(self) -> None:
        blob = bytearray(build_egg(load_project(SAMPLE)))
        blob[len(blob) // 2] ^= 0x01
        with self.assertRaises(EggFormatError):
            parse_egg(bytes(blob))
        with self.assertRaises(EggFormatError):
            parse_egg(bytes(blob[:32]))

    def test_chunk_crc_is_checked_independently_of_package_digest(self) -> None:
        blob = bytearray(build_egg(load_project(SAMPLE)))
        first_entry = CHUNK_ENTRY.unpack_from(blob, HEADER.size)
        blob[first_entry[4]] ^= 0x01
        resign_package(blob)
        with self.assertRaisesRegex(EggFormatError, "chunk 0 CRC mismatch"):
            parse_egg(bytes(blob))

    def test_chunk_bounds_are_checked_independently_of_package_digest(self) -> None:
        blob = bytearray(build_egg(load_project(SAMPLE)))
        values = list(CHUNK_ENTRY.unpack_from(blob, HEADER.size))
        values[4] = HEADER.unpack_from(blob)[5]
        CHUNK_ENTRY.pack_into(blob, HEADER.size, *values)
        resign_package(blob)
        with self.assertRaisesRegex(EggFormatError, "outside payload bounds"):
            parse_egg(bytes(blob))

    def test_egg_output_suffix_is_required(self) -> None:
        bundle = load_project(SAMPLE)
        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaises(EggCompileError):
                write_egg(bundle, Path(temp_dir) / "state_slice.bin")

    def test_embedded_c_is_deterministic_and_contains_exact_blob(self) -> None:
        bundle = load_project(SAMPLE)
        blob = build_egg(bundle)
        with tempfile.TemporaryDirectory() as temp_dir:
            first = write_embedded_egg_c(bundle, Path(temp_dir) / "first.c")
            second = write_embedded_egg_c(bundle, Path(temp_dir) / "second.c")
            first_text = first.read_text(encoding="ascii")
            second_text = second.read_text(encoding="ascii")
        self.assertEqual(first_text, second_text)
        encoded = bytes(
            int(token[2:4], 16)
            for token in first_text.replace(",", " ").split()
            if token.startswith("0x")
        )
        self.assertEqual(blob, encoded)

    def test_embedded_c_rejects_invalid_symbol(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaises(EggCompileError):
                write_embedded_egg_c(
                    load_project(SAMPLE),
                    Path(temp_dir) / "bad.c",
                    "not-valid",
                )

    def test_committed_embedded_package_is_current(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            generated = write_embedded_egg_c(
                load_project(EMBEDDED_PROJECT),
                Path(temp_dir) / "embedded.c",
            )
            expected = generated.read_text(encoding="ascii")
        self.assertEqual(expected, EMBEDDED_SAMPLE.read_text(encoding="ascii"))


if __name__ == "__main__":
    unittest.main()
