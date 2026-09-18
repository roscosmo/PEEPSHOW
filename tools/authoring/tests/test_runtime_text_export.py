"""API 49 runtime text uses ordinary authoring and public package export."""
import base64
import hashlib
from pathlib import Path
import tempfile
import unittest

from build_runtime_text_fixture import runtime_text_bundle, text_object
from peepshow_authoring.egg_format import parse_egg
from peepshow_authoring.protocol import ServiceRequest, ProtocolError
from peepshow_authoring.service import AuthoringService


class RuntimeTextExportTests(unittest.TestCase):
    def setUp(self):
        self.service = AuthoringService()
        self.revision = 0

    def call(self, op, **params):
        if op not in ("service.hello", "project.create", "project.load"):
            params.setdefault("project_revision", self.revision)
        result = self.service.handle(ServiceRequest("text", op, params))
        self.revision = result.get("project_revision", self.revision)
        return result

    def test_capabilities_and_exact_hardware_fixture_public_build(self):
        hello = self.call("service.hello")
        self.assertEqual(50, hello["service_api_version"])
        profile = hello["package_export"]["v2_profile"]
        text = hello["state_scene_presentation"]["runtime_text_profile"]
        self.assertTrue(hello["state_scene_presentation"]["runtime_text"])
        self.assertEqual(5, profile["profile_revision"])
        self.assertEqual(text, profile["runtime_text"])
        self.assertEqual(text, hello["scene_object_authoring"]["runtime_text"])
        self.assertFalse(text["baked_assets"])
        self.assertFalse(text["dynamic_content"])
        self.assertEqual(["x", "y", "visible"], text["override_properties"])
        loaded = self.service._activate_bundle(runtime_text_bundle())
        self.revision = loaded["project_revision"]
        self.assertEqual([], loaded["build_issues"])
        self.assertTrue(loaded["scene_capabilities"]["object_awake"]["runtime_text"])
        self.assertTrue(loaded["scene_capabilities"]["object_awake"]["export_ready"])
        built = self.call("project.build_package")
        blob = base64.b64decode(built["package"]["blob_base64"])
        self.assertEqual(2052, len(blob))
        self.assertEqual("14e05d8e70e8478561af202a05f102d81946ac23d7192028254fd708b67eb83d",
                         hashlib.sha256(blob).hexdigest())
        self.assertEqual(4, len(parse_egg(blob).assets))

    def test_public_edit_undo_save_reload_and_unbaked_export(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "Text.peepproj"
            self.call("project.create", path=str(path), scene_schema_version=2)
            obj = text_object("label", "Aa\n12", 0, 0, 120, 32, 2, "center")
            self.call("project.apply_commands", commands=[
                {"kind": "object.add", "scene_id": "main", "object": obj}])
            before = self.service._bundle.canonical_bytes()
            command = {"kind": "object.set_text", "scene_id": "main", "object_id": "label",
                       **{k: obj[k] for k in ("text", "font_id", "scale", "alignment", "width", "height")}}
            command["text"] = "New\nText"
            self.call("project.apply_commands", commands=[command])
            changed = self.service._bundle.canonical_bytes()
            self.call("project.undo")
            self.assertEqual(before, self.service._bundle.canonical_bytes())
            self.call("project.redo")
            self.assertEqual(changed, self.service._bundle.canonical_bytes())
            with self.assertRaises(ProtocolError):
                self.call("project.apply_commands", commands=[{**command, "width": 1}])
            self.assertEqual(changed, self.service._bundle.canonical_bytes())
            self.call("project.save")
            loaded = self.call("project.load", path=str(path))
            self.assertEqual(changed, self.service._bundle.canonical_bytes())
            self.assertEqual([], loaded["build_issues"])
            built = self.call("project.build_package")
            package = parse_egg(base64.b64decode(built["package"]["blob_base64"]))
            self.assertEqual(0, len(package.assets))
            self.assertEqual("New\nText", package.scenes[0]["objects"][0]["text"])
            self.assertEqual(9, package.scenes[0]["objects"][0]["kind"])

    def test_legacy_scene_does_not_gain_runtime_text(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.call("project.create", path=str(Path(temporary) / "Old.peepproj"),
                               scene_schema_version=1)
            self.assertFalse(result["scene_capabilities"]["main"]["runtime_text"])
            self.assertIsNone(result["scene_capabilities"]["main"]["runtime_text_profile"])
