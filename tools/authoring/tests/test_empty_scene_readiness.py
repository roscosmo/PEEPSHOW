from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.cli import _build, _embed, _inspect
from peepshow_authoring.compatibility import build_compatibility_report
from peepshow_authoring.compiler import (
    EggCompileError, _build_egg, build_egg, build_preview_package,
    build_readiness_issues, write_egg, write_embedded_egg_c,
)
from peepshow_authoring.egg_format import EggFormatError, parse_egg
from peepshow_authoring.project import create_project, load_project
from peepshow_authoring.protocol import ServiceRequest
from peepshow_authoring.service import AuthoringService


class EmptySceneReadinessTests(unittest.TestCase):
    def test_static_primitive_without_assets_focus_or_routes_can_build(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "primitive.peepproj"
            create_project(root)
            path = root / "scenes/main.state.json"
            scene = json.loads(path.read_text())
            scene["render_models"][0]["elements"] = [{
                "element_id": "rectangle", "kind": "filled_rect",
                "x": 0, "y": 0, "width": 8, "height": 8, "z_order": 1,
            }]
            path.write_text(json.dumps(scene))
            bundle = load_project(root)
            self.assertTrue(bundle.valid, bundle.issues)
            self.assertEqual([], build_readiness_issues(bundle))
            self.assertEqual(1, len(parse_egg(build_egg(bundle)).scenes))

    def test_unreachable_empty_scenes_report_every_state_in_stable_order(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "draft.peepproj"
            service = AuthoringService()
            created = service.handle(ServiceRequest("create", "project.create", {"path": str(root)}))
            result = service.handle(ServiceRequest("add", "project.apply_commands", {
                "project_revision": created["project_revision"],
                "commands": [
                    {"kind": "scene.add", "display_name": "Settings"},
                    {"kind": "scene.add", "display_name": "Credits"},
                ],
            }))
            self.assertTrue(result["valid"])
            self.assertEqual(["credits", "main", "settings"], [i["scene_id"] for i in result["build_issues"]])
            for issue in result["build_issues"]:
                self.assertEqual("start", issue["state_id"])
                self.assertTrue(issue["render_model_ref"])
            thumbs = service.handle(ServiceRequest("thumbs", "project.scene_thumbnails", {
                "project_revision": result["project_revision"],
            }))
            self.assertEqual(3, len(thumbs["thumbnails"]))

    def test_old_empty_binary_is_rejected_but_draft_preview_is_allowed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "draft.peepproj"
            create_project(root)
            bundle = load_project(root)
            self.assertTrue(bundle.valid)
            report = build_compatibility_report(bundle)
            readiness = [item for item in report["validation_results"]
                         if item["internal_detail_ref"] == "RENDER_MODEL_EMPTY"]
            self.assertEqual(1, len(readiness))
            self.assertTrue(readiness[0]["blocks_dev_package"])
            self.assertFalse(readiness[0]["blocks_authoring_preview"])
            self.assertEqual(1, len(build_preview_package(bundle).scenes))
            # Reproduce the pre-fix compiler output without weakening public export.
            blob = _build_egg(bundle, _draft=True)
            with self.assertRaisesRegex(EggFormatError, "RENDER_MODEL_EMPTY"):
                parse_egg(blob)
            old_export = Path(temp) / "old.egg"
            old_export.write_bytes(blob)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(1, _inspect(str(old_export)))

    def test_failed_build_and_embed_preserve_existing_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "draft.peepproj"
            create_project(root)
            bundle = load_project(root)
            for writer, name in [(write_egg, "previous.egg"), (write_embedded_egg_c, "previous.c")]:
                output = Path(temp) / name
                output.write_bytes(b"previous successful output")
                with self.assertRaises(EggCompileError):
                    writer(bundle, output)
                self.assertEqual(b"previous successful output", output.read_bytes())
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(1, _build(str(root), str(Path(temp) / "previous.egg")))
                self.assertEqual(1, _embed(str(root), str(Path(temp) / "previous.c"), "g_ps_embedded_egg"))
            for name in ["previous.egg", "previous.c"]:
                self.assertEqual(b"previous successful output", (Path(temp) / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
