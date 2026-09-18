"""Names and tags are persisted authoring data, not package content."""
from copy import deepcopy
from pathlib import Path
import shutil
import json
import tempfile
import unittest

from test_authoring_service import make_audio_project
from peepshow_authoring.compiler import build_egg, build_preview_package
from peepshow_authoring.project import load_project
from peepshow_authoring.preview import StateScenePreview
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.service import AuthoringService


class AuthoringMetadataTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.service = AuthoringService()
        self.revision = 0

    def call(self, operation, **params):
        if operation not in ("project.create", "project.load", "service.hello"):
            params["project_revision"] = self.revision
        result = self.service.handle(ServiceRequest("metadata", operation, params))
        self.revision = result.get("project_revision", self.revision)
        return result

    def edit(self, command):
        return self.call("project.apply_commands", commands=[command])

    def test_object_names_preserve_export_and_references(self):
        path = self.root / "Names.peepproj"
        self.call("project.create", path=str(path), scene_schema_version=2)
        self.edit({"kind": "object.add", "scene_id": "main", "object": {
            "object_id": "marker", "kind": "filled_rect", "width": 8, "height": 8,
            "layer": "SCENE", "z_order": 0, "defaults": {"x": 10, "y": 20, "visible": True}}})
        original = deepcopy(self.service._bundle.scenes)
        egg = build_egg(self.service._bundle)
        preview = StateScenePreview(build_preview_package(self.service._bundle), "main").snapshot()
        command = {"kind": "object.rename", "scene_id": "main", "object_id": "marker", "display_name": "Selection marker"}
        self.edit(command)
        self.assertEqual("Selection marker", self.service._bundle.scenes[0]["objects"][0]["display_name"])
        self.assertEqual(egg, build_egg(self.service._bundle))
        self.assertEqual(preview, StateScenePreview(build_preview_package(self.service._bundle), "main").snapshot())
        self.call("project.undo")
        self.assertEqual(original, self.service._bundle.scenes)
        self.call("project.redo")
        self.call("project.save")
        loaded = load_project(path)
        self.assertTrue(loaded.valid, loaded.issues)
        self.assertEqual(self.service._bundle.scenes, loaded.scenes)
        for value in (None, "", "   ", 12, "x" * 97):
            before = self.service._bundle.canonical_bytes()
            with self.assertRaises(ProtocolError):
                self.edit({**command, "display_name": value})
            self.assertEqual(before, self.service._bundle.canonical_bytes())
        caps = self.call("project.normalize")["scene_capabilities"]["main"]
        self.assertTrue(caps["object_display_names"])
        self.assertIn("object.rename", caps["supported_commands"])

    def test_sprite_and_audio_tags_roundtrip_and_export(self):
        path = make_audio_project(self.root)
        self.call("project.load", path=str(path))
        egg = build_egg(self.service._bundle)
        sprite = self.service._bundle.assets[0]["asset_id"]
        for kind, asset_id in (("asset.set_tags", sprite), ("audio_asset.set_tags", "ui.select")):
            command = {"kind": kind, "asset_id": asset_id, "tags": ["Menu", "Selection"]}
            before = self.service._bundle.canonical_bytes()
            self.edit(command)
            self.assertEqual(egg, build_egg(self.service._bundle))
            self.call("project.undo")
            self.assertEqual(before, self.service._bundle.canonical_bytes())
            self.call("project.redo")
            for tags in (None, "Menu", [""], [" a"], ["a", "a"], [3], [{}], ["x" * 33], [str(n) for n in range(17)]):
                before = self.service._bundle.canonical_bytes()
                with self.assertRaises(ProtocolError):
                    self.edit({**command, "tags": tags})
                self.assertEqual(before, self.service._bundle.canonical_bytes())
            with self.assertRaises(ProtocolError):
                self.edit({**command, "asset_id": "missing"})
        normalized = self.service._bundle.normalized()
        self.assertEqual(["Menu", "Selection"], normalized["assets"][0]["tags"])
        self.assertEqual(["Menu", "Selection"], normalized["audio_assets"][0]["tags"])
        self.call("project.save")
        loaded = load_project(path)
        self.assertTrue(loaded.valid, loaded.issues)
        self.assertEqual(self.service._bundle.canonical_bytes(), loaded.canonical_bytes())
        copied = self.root / "Copy.peepproj"
        shutil.copytree(path, copied)
        self.assertEqual(loaded.canonical_bytes(), load_project(copied).canonical_bytes())
        self.assertEqual(egg, build_egg(load_project(copied)))
        self.edit({"kind": "audio_asset.set_tags", "asset_id": "ui.select", "tags": []})
        self.assertEqual([], self.service._bundle.normalized()["audio_assets"][0]["tags"])

    def test_invalid_saved_tags_and_upsert_are_rejected(self):
        path = make_audio_project(self.root)
        self.call("project.load", path=str(path))
        for collection, kind, field in (("assets", "asset.upsert", "asset"),
                                        ("audio_assets", "audio_asset.upsert", "audio_asset")):
            record = deepcopy(self.service._bundle.asset_catalogs[0][collection][0])
            record["tags"] = ["bad", "bad"]
            before = self.service._bundle.canonical_bytes()
            with self.assertRaises(ProtocolError):
                self.edit({"kind": kind, field: record})
            self.assertEqual(before, self.service._bundle.canonical_bytes())
        catalog_path = path / "assets/catalog.json"
        catalog = json.loads(catalog_path.read_text())
        catalog["audio_assets"][0]["tags"] = [None]
        catalog_path.write_text(json.dumps(catalog))
        self.assertIn("ASSET_TAGS_INVALID", {issue.code for issue in load_project(path).issues})

    def test_capabilities(self):
        hello = self.call("service.hello")
        self.assertEqual(49, hello["service_api_version"])
        self.assertFalse(hello["asset_metadata"]["tags"]["runtime_encoded"])
        self.assertEqual(96, hello["scene_object_authoring"]["display_names"]["maximum_length"])


if __name__ == "__main__":
    unittest.main()
