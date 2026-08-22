from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


TOOL_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.project import load_project  # noqa: E402


SAMPLE = WORKSPACE_ROOT / "examples" / "authoring" / "state_slice.peepproj"


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


if __name__ == "__main__":
    unittest.main()
