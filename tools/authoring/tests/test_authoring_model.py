from __future__ import annotations

import copy
import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


TOOL_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.project import load_project  # noqa: E402
from peepshow_authoring.compiler import EggCompileError, build_egg, write_egg  # noqa: E402
from peepshow_authoring.egg_format import (  # noqa: E402
    CHUNK_ENTRY,
    FOOTER,
    HEADER,
    EggFormatError,
    parse_egg,
)


SAMPLE = WORKSPACE_ROOT / "examples" / "authoring" / "state_slice.peepproj"


def resign_package(blob: bytearray) -> None:
    footer_offset = HEADER.unpack_from(blob)[5]
    blob[footer_offset:] = FOOTER.pack(
        b"END1",
        1,
        FOOTER.size,
        hashlib.sha256(blob[:footer_offset]).digest(),
    )


class AuthoringModelTests(unittest.TestCase):
    def test_sample_is_valid_and_normalization_is_deterministic(self) -> None:
        first = load_project(SAMPLE)
        second = load_project(SAMPLE)
        self.assertEqual((), first.issues)
        self.assertEqual(first.canonical_bytes(), second.canonical_bytes())
        normalized = json.loads(first.canonical_bytes())
        self.assertEqual("state_demo", normalized["project"]["entry_scene"])
        self.assertEqual(1, len(normalized["scenes"]))

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
        self.assertEqual(6, len(package.chunks))
        self.assertEqual(3, package.scenes[0]["state_count"])
        self.assertEqual(6, package.scenes[0]["route_count"])
        self.assertNotIn("scenes/state_demo.state.json", package.strings)

    def test_editor_source_path_does_not_change_package_bytes(self) -> None:
        original = build_egg(load_project(SAMPLE))
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "renamed.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["scene_sources"] = ["scenes/renamed.json"]
            scene = (SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8")
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "renamed.json").write_text(scene, encoding="utf-8")
            renamed = build_egg(load_project(project_root))
        self.assertEqual(original, renamed)

    def test_multiple_state_scenes_have_distinct_chunk_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir) / "multiple.peepproj"
            scene_dir = project_root / "scenes"
            scene_dir.mkdir(parents=True)
            project = json.loads((SAMPLE / "project.json").read_text(encoding="utf-8"))
            project["scene_sources"] = ["scenes/state_demo.state.json", "scenes/second.state.json"]
            scene = json.loads((SAMPLE / "scenes" / "state_demo.state.json").read_text(encoding="utf-8"))
            second = copy.deepcopy(scene)
            second["scene_id"] = "second_scene"
            second["display_name"] = "Second Scene"
            (project_root / "project.json").write_text(json.dumps(project), encoding="utf-8")
            (scene_dir / "state_demo.state.json").write_text(json.dumps(scene), encoding="utf-8")
            (scene_dir / "second.state.json").write_text(json.dumps(second), encoding="utf-8")

            package = parse_egg(build_egg(load_project(project_root)))
            self.assertEqual(("second_scene", "state_demo"), tuple(item["scene_id"] for item in package.scenes))
            self.assertEqual(9, len(package.chunks))

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


if __name__ == "__main__":
    unittest.main()
