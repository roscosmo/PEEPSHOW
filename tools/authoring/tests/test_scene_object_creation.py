"""Native V2 drafts and state management through the public service boundary."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.service import AuthoringService, SERVICE_API_VERSION


class SceneObjectCreationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "Native Objects.peepproj"
        self.service = AuthoringService()
        self.revision = 0

    def call(self, operation, **params):
        if operation not in ("service.hello", "project.create", "project.load"):
            params["project_revision"] = self.revision
        result = self.service.handle(ServiceRequest("test", operation, params))
        self.revision = result.get("project_revision", self.revision)
        return result

    def create(self, **params):
        return self.call("project.create", path=str(self.root), **params)

    def edit(self, *commands):
        return self.call("project.apply_commands", commands=list(commands))

    @staticmethod
    def command(kind, **params):
        return {"kind": kind, "scene_id": "main", **params}

    def scene(self, scene_id="main"):
        return next(scene for scene in self.service._bundle.scenes if scene["scene_id"] == scene_id)

    def layout(self):
        return self.service._bundle.project["editor"]["state_graph"]["scenes"]["main"]

    def add_state(self, name="Second"):
        return self.edit(self.command("state.create", display_name=name, x=200, y=100))

    def assert_rejected_unchanged(self, *commands, code=None):
        before = self.service._bundle.canonical_bytes()
        revision = self.revision
        with self.assertRaises(ProtocolError) as error:
            self.edit(*commands)
        if code is not None:
            self.assertEqual(code, error.exception.code)
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.assertEqual(revision, self.revision)

    def test_native_create_is_saved_editable_draft_without_legacy_records(self):
        created = self.create(scene_schema_version=2)
        self.assertTrue(created["valid"])
        self.assertFalse(created["dirty"])
        self.assertFalse(created["can_undo"])
        self.assertEqual(1, self.service._bundle.project["schema_version"])
        self.assertEqual(2, self.scene()["schema_version"])
        self.assertEqual([], self.scene()["objects"])
        self.assertEqual([{"state_id": "start", "display_name": "Start", "object_overrides": []}], self.scene()["states"])
        for key in ("routes", "input_actions", "variables", "scene_exits"):
            self.assertEqual([], self.scene()[key])
        for key in ("render_models", "waiting_visuals"):
            self.assertNotIn(key, self.scene())
        self.assertNotIn("waiting_visual_ref", self.scene()["reactive_wait_default"])
        saved = load_project(self.root)
        self.assertTrue(saved.valid, saved.issues)
        self.assertEqual(saved.canonical_bytes(), self.service._bundle.canonical_bytes())
        self.call("project.preview_reset", scene_id="main")
        self.call("project.scene_thumbnails")
        with self.assertRaises(ProtocolError) as error:
            self.call("project.build_package")
        self.assertIn("V2_OBJECTS_EMPTY", [issue["code"] for issue in error.exception.details["issues"]])

    def test_capabilities_distinguish_creation_state_management_and_graphs(self):
        hello = self.call("service.hello")
        self.assertEqual(43, SERVICE_API_VERSION)
        creation = hello["scene_creation"]
        self.assertEqual("project.create", creation["project_operation"])
        self.assertEqual("scene.add", creation["scene_command"])
        self.assertEqual("project.set_entry_scene", creation["entry_scene_command"])
        self.assertEqual("scene_schema_version", creation["version_parameter"])
        self.assertEqual([1, 2], creation["supported_versions"])
        self.assertEqual(1, creation["default_version"])
        self.assertFalse(creation["scene_add_inherits_version"])
        result = self.create(scene_schema_version=2)
        capability = result["scene_capabilities"]["main"]
        self.assertEqual("scene_objects", capability["execution_model"])
        expected = {"state.add", "state.create", "state.delete", "state.rename", "state.set_entry",
                    "editor.state_graph.set_node_position", "editor.state_graph.set_entry_layout"}
        self.assertEqual(expected, set(capability["state_management_commands"]))
        self.assertTrue(expected.issubset(capability["supported_commands"]))
        self.assertIn("project.set_entry_scene", capability["supported_commands"])
        self.assertEqual(expected, set(hello["scene_object_authoring"]["state_management_commands"]))
        self.assertTrue(capability["graph_construction_commands"])
        self.assertTrue(capability["scene_connection_commands"])
        self.assertTrue(capability["egg_export"])
        self.assertFalse(capability["export_ready"])
        self.assertTrue(hello["scene_object_authoring"]["firmware_available"])

    def test_version_selection_is_explicit_for_each_new_scene(self):
        self.create()
        self.assertEqual(1, self.scene()["schema_version"])
        self.edit({"kind": "scene.add", "display_name": "Objects", "scene_schema_version": 2})
        self.assertEqual(2, self.scene("objects")["schema_version"])
        self.edit({"kind": "project.set_entry_scene", "scene_id": "objects"})
        self.assertEqual("objects", self.service._bundle.project["entry_scene"])
        result = self.edit({"kind": "scene.add", "display_name": "Legacy"})
        self.assertEqual(1, self.scene("legacy")["schema_version"])
        self.assertTrue(result["scene_capabilities"]["legacy"]["graph_construction_commands"])
        self.assertNotIn("objects", self.scene("legacy"))

    def test_scene_add_does_not_inherit_v2_and_uses_collision_safe_sources(self):
        self.create(scene_schema_version=2)
        self.edit({"kind": "scene.add", "display_name": "Main", "scene_schema_version": 2},
                  {"kind": "scene.add", "display_name": "Main"})
        self.assertEqual(["main", "main_2", "main_3"], [scene["scene_id"] for scene in self.service._bundle.scenes])
        self.assertEqual([2, 2, 1], [scene["schema_version"] for scene in self.service._bundle.scenes])
        self.assertFalse((self.root / "scenes/main_2.state.json").exists())
        self.call("project.save")
        self.assertEqual([2, 2, 1], [scene["schema_version"] for scene in load_project(self.root).scenes])

    def test_invalid_project_versions_do_not_create_files_or_replace_active_project(self):
        self.create(scene_schema_version=1)
        before = self.service._bundle.canonical_bytes()
        revision = self.revision
        for index, value in enumerate((0, 3, True, 2.0, "2", None)):
            with self.subTest(value=value):
                path = Path(self.temp.name) / f"invalid{index}.peepproj"
                with self.assertRaises(ProtocolError) as error:
                    self.call("project.create", path=str(path), scene_schema_version=value)
                self.assertEqual("SCENE_SCHEMA_VERSION_UNSUPPORTED", error.exception.code)
                self.assertFalse(path.exists())
                self.assertFalse(path.with_name(f".{path.stem}.creating.peepproj").exists())
                self.assertEqual(before, self.service._bundle.canonical_bytes())
                self.assertEqual(revision, self.revision)

    def test_invalid_scene_version_rolls_back_batch_and_history(self):
        self.create(scene_schema_version=2)
        for value in (0, 3, True, 2.0, "2", None):
            with self.subTest(value=value):
                self.assert_rejected_unchanged(
                    self.command("state.create", display_name="Discarded", x=0, y=0),
                    {"kind": "scene.add", "display_name": "Invalid", "scene_schema_version": value},
                    code="SCENE_SCHEMA_VERSION_UNSUPPORTED")
        with self.assertRaises(ProtocolError) as error:
            self.call("project.undo")
        self.assertEqual("UNDO_UNAVAILABLE", error.exception.code)
        self.assertEqual(1, len(load_project(self.root).scenes))

    def test_new_state_has_no_inherited_overrides_and_delete_preserves_objects(self):
        self.create(scene_schema_version=2)
        obj = {"object_id": "panel", "kind": "filled_rect", "width": 8, "height": 8,
               "z_order": 0, "layer": "SCENE", "defaults": {"x": 4, "y": 6, "visible": True}}
        self.edit(self.command("object.add", object=obj, visible_in_states=["start"]))
        self.add_state()
        self.add_state()
        self.assertEqual(["start", "second", "second_2"], [state["state_id"] for state in self.scene()["states"]])
        self.assertEqual([], self.scene()["states"][1]["object_overrides"])
        self.assertEqual([], self.scene()["states"][2]["object_overrides"])
        self.assertFalse(self.scene()["objects"][0]["defaults"]["visible"])
        self.assertEqual({"x": 200, "y": 100}, self.layout()["nodes"]["second"])
        self.edit(self.command("object_override.set", state_id="second", object_id="panel", properties={"x": 20}))
        objects_before = deepcopy(self.scene()["objects"])
        self.edit(self.command("state.delete", state_id="second"))
        self.assertEqual(objects_before, self.scene()["objects"])
        self.assertNotIn("second", self.layout()["nodes"])
        self.call("project.save")
        self.assertEqual(objects_before, load_project(self.root).scenes[0]["objects"])

    def test_entry_rename_layout_and_explicit_state_add(self):
        self.create(scene_schema_version=2)
        self.assert_rejected_unchanged(self.command("state.delete", state_id="start"), code="COMMAND_TARGET_IN_USE")
        self.edit(self.command("state.add", state={"state_id": "ready", "display_name": "Ready", "object_overrides": []}))
        self.assert_rejected_unchanged(self.command("state.delete", state_id="start"), code="COMMAND_TARGET_IN_USE")
        self.edit(self.command("state.set_entry", state_id="ready"),
                  self.command("state.rename", state_id="ready", display_name="Playing"),
                  self.command("editor.state_graph.set_node_position", state_id="ready", x=150, y=-20),
                  self.command("editor.state_graph.set_entry_layout", target_handle="entry-top-left", target_side="top"),
                  self.command("state.delete", state_id="start"))
        self.assertEqual("ready", self.scene()["entry_state"])
        self.assertEqual("Playing", self.scene()["states"][0]["display_name"])
        self.assertEqual({"x": 150, "y": -20}, self.layout()["nodes"]["ready"])
        self.assertEqual({"target_handle": "entry-top-left", "target_side": "top"}, self.layout()["entry"])
        self.assert_rejected_unchanged(self.command("state.set_entry", state_id="missing"))
        self.assert_rejected_unchanged(self.command("state.delete", state_id="ready"), code="COMMAND_TARGET_IN_USE")

    def test_state_shapes_ids_and_bounds_are_validated_atomically(self):
        self.create(scene_schema_version=2)
        for state in (
            {"state_id": "new", "display_name": "New", "waiting_visual_ref": "static_wait"},
            {"state_id": "new", "display_name": "New"},
            {"state_id": "start", "display_name": "Duplicate", "object_overrides": []},
            {"state_id": "new", "display_name": "New", "object_overrides": [{"object_ref": "missing", "x": 2}]},
        ):
            self.assert_rejected_unchanged(self.command("state.add", state=state))
        self.assert_rejected_unchanged(self.command("state.create", display_name="Bad position", x="0", y=0))
        self.edit(*[self.command("state.add", state={"state_id": f"s{n}", "display_name": f"State {n}", "object_overrides": []})
                    for n in range(63)])
        self.assertEqual(64, len(self.scene()["states"]))
        self.assert_rejected_unchanged(self.command("state.create", display_name="Too many", x=0, y=0))

    def test_referenced_state_deletion_checks_routes_and_independent_handlers(self):
        self.create(scene_schema_version=2)
        self.add_state()
        base = deepcopy(self.scene())
        variants = []
        for source, target in (("start", "second"), ("second", "start")):
            scene = deepcopy(base)
            scene["input_actions"] = [{"action_id": "a", "logical_source": "BUTTON_A"}]
            scene["routes"] = [{"route_id": "linked", "action_ref": "a", "from_states": [source],
                                "target_state": target, "guards": [], "actions": []}]
            variants.append(scene)
        scene = deepcopy(base)
        scene["event_bindings"] = [{"binding_id": "tick", "event_type": "time.scene_elapsed",
                                     "configuration": {"delay_ms": 5000, "start_policy": "scene_entry"}}]
        scene["event_handlers"] = [{"handler_id": "timer_target", "event_ref": "tick", "guards": [],
                                     "actions": [], "target_state": "second"}]
        variants.append(scene)
        # Load source fixtures so state deletion is checked independently of graph commands.
        for index, scene in enumerate(variants):
            with self.subTest(reference=index):
                (self.root / "scenes/main.state.json").write_text(json.dumps(scene), encoding="utf-8")
                loaded = self.call("project.load", path=str(self.root))
                self.assertTrue(loaded["valid"], loaded.get("issues"))
                self.assert_rejected_unchanged(self.command("state.delete", state_id="second"), code="COMMAND_TARGET_IN_USE")

    def test_scene_and_state_batch_undo_redo_and_failure_are_atomic(self):
        self.create(scene_schema_version=2)
        before = self.service._bundle.canonical_bytes()
        result = self.edit({"kind": "scene.add", "display_name": "Other", "scene_schema_version": 2},
                           {"kind": "state.create", "scene_id": "other", "display_name": "Waiting", "x": 1, "y": 2})
        after = self.service._bundle.canonical_bytes()
        self.assertIn("other", result["scene_capabilities"])
        undone = self.call("project.undo")
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.assertNotIn("other", undone["scene_capabilities"])
        self.call("project.redo")
        self.assertEqual(after, self.service._bundle.canonical_bytes())
        self.assert_rejected_unchanged({"kind": "scene.add", "display_name": "Discard", "scene_schema_version": 2},
                                      self.command("state.delete", state_id="start"))
        self.call("project.undo")
        self.assertEqual(before, self.service._bundle.canonical_bytes())

    def test_unknown_commands_and_legacy_mutations_remain_blocked(self):
        self.create(scene_schema_version=2)
        for kind in ("route.create", "event_binding.upsert", "event_handler.upsert",
                     "state_placement.clear_override",
                     "render_element.bind_waiting_animation"):
            with self.subTest(kind=kind):
                self.assert_rejected_unchanged(self.command(kind), code="COMMAND_EXECUTION_MODEL_MISMATCH")


if __name__ == "__main__":
    unittest.main()
