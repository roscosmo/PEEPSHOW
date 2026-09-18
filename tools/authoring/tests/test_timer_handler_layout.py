"""Timer handler geometry is persisted editor metadata, never runtime content."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from peepshow_authoring.compiler import build_egg, build_preview_package
from peepshow_authoring.preview import StateScenePreview
from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.service import AuthoringService


class TimerHandlerLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "TimerLayout.peepproj"
        self.service = AuthoringService()
        self.revision = 0
        self.scene_id = "main"
        self.call("project.create", path=str(self.root), scene_schema_version=2)
        self.edit(self.command("object.add", object={
            "object_id": "panel", "kind": "filled_rect", "width": 8, "height": 8,
            "layer": "SCENE", "z_order": 0, "defaults": {"x": 10, "y": 20, "visible": True}}))
        self.add_timer()

    def call(self, operation, **params):
        if operation not in ("project.create", "project.load", "service.hello"):
            params["project_revision"] = self.revision
        result = self.service.handle(ServiceRequest("layout", operation, params))
        self.revision = result.get("project_revision", self.revision)
        return result

    def command(self, kind, **values):
        return {"kind": kind, "scene_id": self.scene_id, **values}

    def edit(self, *commands):
        return self.call("project.apply_commands", commands=list(commands))

    def add_timer(self):
        self.edit(self.command("event_binding.add", event_binding={"binding_id": "tick",
                  "event_type": "time.scene_elapsed", "configuration": {"delay_ms": 1000}}),
                  self.command("event_handler.add", event_handler=self.handler()))

    @staticmethod
    def handler(**values):
        return {"handler_id": "tick_handler", "event_ref": "tick", "guards": [],
                "actions": [], **values}

    @staticmethod
    def layout():
        return {"termination": {"x": 240.4, "y": -50.6},
                "rails": [{"axis": "x", "value": 100.4}, {"axis": "y", "value": 250}],
                "token_positions": {"guards": [0.2, 0.4], "actions": [0.7]}}

    def set_layout(self, layout, **values):
        return self.edit(self.command("editor.state_graph.set_handler_layout",
                         handler_id="tick_handler", layout=layout, **values))

    def records(self):
        return self.service._bundle.project.get("editor", {}).get("state_graph", {}).get(
            "scenes", {}).get(self.scene_id, {}).get("handlers", {})

    def test_capabilities_and_export_preview_undo_save_reload(self):
        hello = self.call("service.hello")
        self.assertEqual(50, hello["service_api_version"])
        cap = hello["state_scene_graph"]["scene_timers"]["editor_layout"]
        normalized = self.call("project.normalize")
        scene_cap = normalized["scene_capabilities"][self.scene_id]
        self.assertEqual(cap, scene_cap["timer_handler_layout"])
        self.assertIn("editor.state_graph.set_handler_layout", scene_cap["supported_commands"])
        before = build_egg(self.service._bundle)
        first = StateScenePreview(build_preview_package(self.service._bundle), self.scene_id)
        scenes = deepcopy(self.service._bundle.scenes)
        public_before = self.call("project.build_package")["package"]["sha256"]
        self.set_layout(self.layout())
        expected = deepcopy(self.records())
        self.assertEqual({"x": 240, "y": -51}, expected["tick_handler"]["termination"])
        self.assertEqual(scenes, self.service._bundle.scenes)
        self.assertEqual(before, build_egg(self.service._bundle))
        self.assertEqual(public_before, self.call("project.build_package")["package"]["sha256"])
        second = StateScenePreview(build_preview_package(self.service._bundle), self.scene_id)
        first.advance(1000)
        second.advance(1000)
        self.assertEqual(first.snapshot(), second.snapshot())
        self.call("project.undo")
        self.assertEqual({}, self.records())
        self.call("project.redo")
        self.assertEqual(expected, self.records())
        self.call("project.save")
        self.call("project.load", path=str(self.root))
        self.assertEqual(expected, self.records())
        self.assertEqual(before, build_egg(self.service._bundle))
        self.set_layout(None)
        self.assertEqual({}, self.records())
        self.call("project.undo")
        self.assertEqual(expected, self.records())

    def test_local_target_socket_and_semantic_update_cleanup(self):
        self.set_layout(self.layout())
        self.edit(self.command("event_handler.update", event_handler=self.handler(target_state="start")))
        self.assertNotIn("termination", self.records()["tick_handler"])
        self.set_layout({"target_handle": "entry-bottom-right", "target_side": "right",
                         "token_positions": {"condition": 0.3, "actions": [0.6]}})
        self.call("project.save")
        self.assertTrue(load_project(self.root).valid)
        self.edit(self.command("event_handler.update", event_handler=self.handler(actions=[{"kind": "request_render"}])))
        self.assertNotIn("target_handle", self.records()["tick_handler"])
        self.assertNotIn("token_positions", self.records()["tick_handler"])
        self.set_layout(self.layout())
        saved = deepcopy(self.records())
        self.edit(self.command("event_handler.delete", handler_id="tick_handler"),
                  self.command("event_binding.delete", binding_id="tick"))
        self.assertEqual({}, self.records())
        self.call("project.undo")
        self.assertEqual(saved, self.records())
        self.call("project.save")
        self.assertTrue(load_project(self.root).valid)

    def test_rejects_malformed_geometry_atomically(self):
        invalid = [
            [], {"unknown": 1}, {"routing_version": 2}, {"routing_version": True},
            {"termination": {"x": True, "y": 0}}, {"termination": {"x": 0}},
            {"termination": {"x": float("nan"), "y": 0}},
            {"termination": {"x": 100001, "y": 0}}, {"rails": None},
            {"rails": [{"axis": "x", "value": 0}] * 9},
            {"rails": [{"axis": "x", "value": 0}, {"axis": "x", "value": 1}]},
            {"rails": [{"axis": "z", "value": 0}]},
            {"target_handle": "entry-top-left", "target_side": "left"},
            {"target_side": "right"}, {"target_handle": []},
            {"token_positions": {"condition": 0.5, "guards": [0.2]}},
            {"token_positions": {"guards": [0.5, 0.2]}},
            {"token_positions": {"guards": [0.6], "actions": [0.4]}},
            {"token_positions": {"guards": [0.1] * 9}},
            {"token_positions": {"actions": [0.5] * 9}},
            {"token_positions": {"guards": [True]}},
            {"token_positions": {"actions": [float("inf")]}},
        ]
        for layout in invalid:
            with self.subTest(layout=layout):
                before = self.service._bundle.canonical_bytes()
                revision = self.revision
                with self.assertRaises(ProtocolError):
                    self.set_layout(layout)
                self.assertEqual(before, self.service._bundle.canonical_bytes())
                self.assertEqual(revision, self.revision)
        with self.assertRaises(ProtocolError):
            self.edit(self.command("editor.state_graph.set_handler_layout", handler_id="missing", layout={}))

    def test_load_rejects_dangling_and_malformed_layouts(self):
        self.call("project.save")
        path = self.root / "project.json"
        original = json.loads(path.read_text())
        for handler_id, layout in (("missing", {}), ("tick_handler", {"rails": "bad"}),
                                   ("tick_handler", {"target_handle": "entry-top-left", "target_side": "top"})):
            project = deepcopy(original)
            project.setdefault("editor", {}).setdefault("state_graph", {}).setdefault("scenes", {}).setdefault(
                self.scene_id, {})["handlers"] = {handler_id: layout}
            path.write_text(json.dumps(project))
            self.assertFalse(load_project(self.root).valid)

    def test_replacement_is_not_merge_and_cross_scene_paths_have_no_termination(self):
        self.set_layout(self.layout())
        self.set_layout({"rails": [], "target_handle": None, "target_side": None})
        self.assertEqual({"routing_version": 1, "rails": []}, self.records()["tick_handler"])
        self.edit({"kind": "scene.add", "display_name": "Other", "scene_schema_version": 2})
        self.edit(self.command("event_handler.update", event_handler=self.handler(target_scene="other")))
        self.set_layout({"rails": [{"axis": "y", "value": 400}]})
        before = self.service._bundle.canonical_bytes()
        with self.assertRaises(ProtocolError):
            self.set_layout({"termination": {"x": 0, "y": 0}})
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.call("project.save")
        self.assertTrue(load_project(self.root).valid)

    def test_saved_null_socket_is_valid_for_targetless_handler(self):
        self.set_layout({})
        self.call("project.save")
        path = self.root / "project.json"
        project = json.loads(path.read_text())
        layout = project["editor"]["state_graph"]["scenes"][self.scene_id]["handlers"]["tick_handler"]
        layout.update(target_handle=None, target_side=None)
        path.write_text(json.dumps(project))
        self.assertTrue(load_project(self.root).valid)

    def test_legacy_scene_support_is_also_editor_only(self):
        root = Path(self.temp.name) / "Legacy.peepproj"
        shutil.copytree(Path(__file__).resolve().parents[1] / "peepshow_authoring/test_project.peepproj", root)
        self.call("project.load", path=str(root))
        self.scene_id = "state_demo"
        self.add_timer()
        before = build_egg(self.service._bundle)
        self.set_layout(self.layout())
        self.assertEqual(before, build_egg(self.service._bundle))
        self.call("project.save")
        self.assertTrue(load_project(root).valid)
