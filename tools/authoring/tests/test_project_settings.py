"""Persisted settings must not imply runtime or audio processing support."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from test_authoring_service import make_audio_project, SAMPLE
from peepshow_authoring.compiler import build_egg
from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.service import AuthoringService


class ProjectSettingsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.service = AuthoringService()
        self.revision = 0

    def call(self, operation, **params):
        if operation not in ("project.create", "project.load", "service.hello"):
            params.setdefault("project_revision", self.revision)
        result = self.service.handle(ServiceRequest("settings", operation, params))
        self.revision = result.get("project_revision", self.revision)
        return result

    def set_settings(self, settings):
        return self.call("project.apply_commands", commands=[{"kind": "project.settings.set", "settings": settings}])

    def test_v1_and_v2_roundtrip_undo_and_byte_identical_export(self):
        settings = {"sfx_import": {"normalization": "peak", "target_peak_dbfs": -6},
                    "runtime_preferences": {"inactivity_timeout_ms": 30000}}
        for version in (1, 2):
            with self.subTest(version=version):
                path = self.root / f"Version{version}.peepproj"
                if version == 1:
                    shutil.copytree(SAMPLE, path)
                    self.call("project.load", path=str(path))
                else:
                    self.call("project.create", path=str(path), scene_schema_version=version)
                    self.call("project.apply_commands", commands=[{"kind": "object.add", "scene_id": "main", "object": {
                        "object_id": "marker", "kind": "filled_rect", "width": 8, "height": 8,
                        "layer": "SCENE", "z_order": 0, "defaults": {"x": 10, "y": 20, "visible": True}}}])
                before = self.service._bundle.canonical_bytes()
                egg = build_egg(self.service._bundle)
                self.assertEqual({}, self.call("project.settings.get")["settings"])
                self.set_settings(settings)
                self.assertEqual(settings, self.call("project.settings.get")["settings"])
                self.assertEqual(egg, build_egg(self.service._bundle))
                self.call("project.undo")
                self.assertEqual(before, self.service._bundle.canonical_bytes())
                self.call("project.redo")
                self.call("project.save")
                loaded = load_project(path)
                self.assertTrue(loaded.valid, loaded.issues)
                self.assertEqual(settings, loaded.project["settings"])
                copied = self.root / f"Copy{version}.peepproj"
                shutil.copytree(path, copied)
                self.assertEqual(egg, build_egg(load_project(copied)))
                self.set_settings({"runtime_preferences": {"inactivity_timeout_ms": None}})
                self.assertNotIn("sfx_import", self.call("project.settings.get")["settings"])
                self.set_settings({})
                self.assertEqual({}, self.call("project.settings.get")["settings"])

    def test_audio_is_not_reprocessed(self):
        path = make_audio_project(self.root)
        self.call("project.load", path=str(path))
        before = build_egg(self.service._bundle)
        audio = self.service._bundle.audio_assets
        self.set_settings({"sfx_import": {"normalization": "peak", "target_peak_dbfs": -60}})
        self.call("project.save")
        loaded = load_project(path)
        self.assertEqual(audio, loaded.audio_assets)
        self.assertEqual(before, build_egg(loaded))

    def test_invalid_settings_and_stale_revisions_are_atomic(self):
        path = self.root / "Invalid.peepproj"
        self.call("project.create", path=str(path), scene_schema_version=2)
        invalid = [None, [], {"unknown": 1}, {"sfx_import": {}},
                   {"sfx_import": {"normalization": "lufs", "target_peak_dbfs": -6}},
                   {"sfx_import": {"normalization": "peak", "target_peak_dbfs": True}},
                   {"sfx_import": {"normalization": "peak", "target_peak_dbfs": -61}},
                   {"runtime_preferences": {"inactivity_timeout_ms": False}},
                   {"runtime_preferences": {"inactivity_timeout_ms": 0}},
                   {"runtime_preferences": {"inactivity_timeout_ms": 86400001}}]
        for settings in invalid:
            before = self.service._bundle.canonical_bytes()
            revision = self.revision
            with self.assertRaises(ProtocolError):
                self.set_settings(settings)
            self.assertEqual(before, self.service._bundle.canonical_bytes())
            self.assertEqual(revision, self.revision)
        with self.assertRaises(ProtocolError):
            self.call("project.settings.get", project_revision=self.revision - 1)
        with self.assertRaises(ProtocolError):
            self.call("project.apply_commands", project_revision=self.revision - 1,
                      commands=[{"kind": "project.settings.set", "settings": {}}])
        project_path = path / "project.json"
        project = json.loads(project_path.read_text())
        project["settings"] = invalid[-1]
        project_path.write_text(json.dumps(project))
        self.assertFalse(load_project(path).valid)

    def test_capabilities(self):
        hello = self.call("service.hello")
        self.assertEqual(50, hello["service_api_version"])
        cap = hello["project_settings"]
        self.assertTrue(cap["persisted"])
        self.assertFalse(cap["runtime_encoded"])
        self.assertFalse(cap["sfx_import"]["import_applied"])
        self.assertFalse(cap["runtime_preferences"]["firmware_enforced"])


if __name__ == "__main__":
    unittest.main()
