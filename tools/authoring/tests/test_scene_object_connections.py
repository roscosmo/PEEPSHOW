"""Public V2 connection commands and fresh host replacement, without export."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.preview import PreviewError, StateScenePreview
from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.scene_object_authoring import SCENE_CONNECTION_COMMANDS
from peepshow_authoring.service import AuthoringService


class SceneObjectConnectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "Connections.peepproj"
        repo = Path(__file__).resolve().parents[3]
        shutil.copytree(repo / "examples/authoring/native_v2_installation.peepproj", self.root)
        self.service = AuthoringService()
        self.revision = self.preview_revision = 0
        self.call("project.load", path=str(self.root))
        self.edit({"kind": "scene.add", "display_name": "Settings", "scene_schema_version": 2},
                  {"kind": "scene.add", "display_name": "Credits", "scene_schema_version": 2})
        for scene_id in ("settings", "credits"):
            self.edit(self.command("object.add", scene_id=scene_id, object={
                "object_id": "panel", "kind": "filled_rect", "width": 8, "height": 8,
                "layer": "SCENE", "z_order": 0,
                "defaults": {"x": 10, "y": 20, "visible": True}}))

    def call(self, operation, **params):
        if operation not in ("project.load", "service.hello"):
            params["project_revision"] = self.revision
        if operation in ("project.preview_input", "project.preview_advance"):
            params["preview_revision"] = self.preview_revision
        result = self.service.handle(ServiceRequest("connections", operation, params))
        self.revision = result.get("project_revision", self.revision)
        self.preview_revision = result.get("preview_revision", self.preview_revision)
        return result

    @staticmethod
    def command(kind, **values):
        return {"kind": kind, "scene_id": "main", **values}

    def edit(self, *commands):
        return self.call("project.apply_commands", commands=list(commands))

    def scene(self, scene_id="main"):
        return next(scene for scene in self.service._bundle.scenes if scene["scene_id"] == scene_id)

    def reject(self, *commands, code=None):
        before = self.service._bundle.canonical_bytes()
        revision = self.revision
        with self.assertRaises(ProtocolError) as error:
            self.edit(*commands)
        if code:
            self.assertEqual(code, error.exception.code)
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.assertEqual(revision, self.revision)

    def add_exit(self, **values):
        result = self.edit(self.command("scene_exit.add", target_scene="settings", **values))
        return result["applied_commands"][0]["scene_exit"]["scene_exit_id"]

    def wire(self, exit_id):
        result = self.edit(self.command("route.create_trigger", source_state="start",
                                        logical_source="BUTTON_R", scene_exit_ref=exit_id))
        route_id = result["applied_commands"][0]["route"]["route_id"]
        self.edit(self.command("route.set_sources", route_id=route_id, from_states=["start", "marker_right"]))
        return route_id

    def timer_exit(self, exit_id):
        handler = deepcopy(self.scene()["event_handlers"][0])
        handler.update(target_scene="settings", scene_exit_ref=exit_id, actions=[])
        self.edit(self.command("event_handler.update", event_handler=handler))
        return handler

    def test_capabilities_and_unwired_named_exit(self):
        before_inputs = deepcopy(self.scene()["input_actions"])
        before_routes = deepcopy(self.scene()["routes"])
        exit_id = self.add_exit(display_name="Open Settings")
        self.assertEqual(before_inputs, self.scene()["input_actions"])
        self.assertEqual(before_routes, self.scene()["routes"])
        self.assertEqual("to_settings", exit_id)
        hello = self.call("service.hello")
        self.assertEqual(43, hello["service_api_version"])
        caps = hello["scene_object_authoring"]
        scene_caps = self.call("project.normalize")["scene_capabilities"]["main"]
        for item in (caps, scene_caps):
            self.assertTrue(item["scene_connection_commands"])
            self.assertEqual(list(SCENE_CONNECTION_COMMANDS), item["connection_commands"])
            self.assertEqual(["fresh_default"], item["scene_entry_modes"])
            self.assertEqual([], item["scene_exit_action_kinds"])
        self.assertFalse(caps["multi_scene_export"])
        self.assertFalse(scene_caps["export_ready"])
        with self.assertRaises(ProtocolError):
            self.call("project.build_package")

    def test_retarget_updates_routes_and_handlers_and_history_atomically(self):
        exit_id = self.add_exit()
        route_id = self.wire(exit_id)
        self.timer_exit(exit_id)
        before = self.service._bundle.canonical_bytes()
        self.edit(self.command("scene_exit.set_target", scene_exit_id=exit_id, target_scene="credits"))
        for owner in [*self.scene()["routes"], *self.scene()["event_handlers"]]:
            if owner.get("scene_exit_ref") == exit_id:
                self.assertEqual("credits", owner["target_scene"])
        after = self.service._bundle.canonical_bytes()
        self.call("project.undo")
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.call("project.redo")
        self.assertEqual(after, self.service._bundle.canonical_bytes())
        self.reject(self.command("scene_exit.set_target", scene_exit_id=exit_id, target_scene="main"))
        self.reject(self.command("scene_exit.set_target", scene_exit_id=exit_id, target_scene="missing"))
        self.edit(self.command("route.set_target", route_id=route_id, target_state="start"))
        self.assertNotIn("scene_exit_ref", next(r for r in self.scene()["routes"] if r["route_id"] == route_id))
        self.reject(self.command("scene_exit.delete", scene_exit_id=exit_id), code="COMMAND_TARGET_IN_USE")

    def test_detach_then_delete_keeps_timer_and_cleans_layouts(self):
        exit_id = self.add_exit()
        route_id = self.wire(exit_id)
        handler = self.timer_exit(exit_id)
        self.edit(self.command("editor.state_graph.set_node_position", node_id=f"scene-exit-{exit_id}", x=200, y=100),
                  self.command("editor.scene_flow.set_route_layout", endpoint_kind="scene_exit", endpoint_id=exit_id,
                               rails=[{"axis": "x", "value": 200}]))
        self.reject(self.command("scene_exit.delete", scene_exit_id=exit_id), code="COMMAND_TARGET_IN_USE")
        self.edit(self.command("route.delete", route_id=route_id))
        self.reject(self.command("scene_exit.delete", scene_exit_id=exit_id), code="COMMAND_TARGET_IN_USE")
        handler.pop("scene_exit_ref")
        handler.pop("target_scene")
        self.edit(self.command("event_handler.update", event_handler=handler),
                  self.command("scene_exit.delete", scene_exit_id=exit_id))
        self.assertEqual([], self.scene()["scene_exits"])
        self.assertEqual(1, len(self.scene()["event_bindings"]))
        editor = self.service._bundle.project["editor"]
        self.assertNotIn(f"scene-exit-{exit_id}", editor["state_graph"]["scenes"]["main"]["nodes"])
        self.assertNotIn("main", editor["scene_flow"].get("routes", {}))

    def test_go_to_alias_retargets_shared_exit_but_deletion_keeps_semantic_links(self):
        result = self.edit({"kind": "editor.scene_flow.add_reference", "target_scene": "settings", "x": 400, "y": 0})
        reference_id = result["applied_commands"][0]["reference_id"]
        exit_id = self.add_exit(scene_flow_reference_id=reference_id)
        self.wire(exit_id)
        self.timer_exit(exit_id)
        self.edit({"kind": "editor.scene_flow.set_reference_target", "reference_id": reference_id, "target_scene": "credits"})
        for owner in [*self.scene()["routes"], *self.scene()["event_handlers"]]:
            if owner.get("scene_exit_ref") == exit_id:
                self.assertEqual("credits", owner["target_scene"])
        self.edit({"kind": "editor.scene_flow.set_reference_position", "reference_id": reference_id, "x": 410, "y": 20},
                  {"kind": "editor.scene_flow.set_package_entry_position", "x": -100, "y": 0},
                  self.command("editor.scene_flow.set_node_position", x=20, y=30))
        self.call("project.save")
        self.assertEqual(self.service._bundle.canonical_bytes(), load_project(self.root).canonical_bytes())
        self.edit({"kind": "editor.scene_flow.delete_reference", "reference_id": reference_id})
        self.assertEqual("credits", self.scene()["scene_exits"][0]["target_scene"])
        self.assertEqual("credits", self.scene()["event_handlers"][0]["target_scene"])

    def test_exit_actions_and_inconsistent_handler_references_are_rejected(self):
        exit_id = self.add_exit()
        route_id = self.wire(exit_id)
        handler = self.timer_exit(exit_id)
        actions = [
            {"kind": "object.move_by", "object_ref": "position_marker", "dx": 1},
            {"kind": "set_variable", "variable_ref": "missing", "operation": "set", "value": 1},
            {"kind": "restart_timer", "timer_ref": "reveal_timer"},
            {"kind": "play_sfx", "cue_ref": "missing"},
        ]
        for action in actions:
            for kind, owner_id in (("route", route_id), ("handler", handler["handler_id"])):
                self.reject(self.command("object_actions.set", owner_kind=kind, owner_id=owner_id, actions=[action]),
                            code="SCENE_TRANSITION_ACTION_UNSUPPORTED")
        for change in ({"target_scene": "credits"}, {"target_scene": "main"}, {"scene_exit_ref": "missing"}):
            self.reject(self.command("event_handler.update", event_handler={**handler, **change}))
        handler.pop("target_scene")
        self.reject(self.command("event_handler.update", event_handler=handler), code="SCENE_EXIT_TARGET_MISMATCH")

    def test_input_replacement_enters_default_and_return_is_fresh(self):
        exit_id = self.add_exit()
        self.wire(exit_id)
        self.edit(self.command("variable.add", variable={"variable_id": "count", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 10}),
                  self.command("object_actions.set", owner_kind="route", owner_id="start_button_a_press", actions=[
                      {"kind": "set_variable", "variable_ref": "count", "operation": "add", "value": 1},
                      {"kind": "object.move_by", "object_ref": "position_marker", "dy": 5}]),
                  self.command("state.create", scene_id="settings", display_name="Selected", x=150, y=0),
                  self.command("state.set_entry", scene_id="settings", state_id="selected"))
        result = self.edit(self.command("scene_exit.add", scene_id="settings", target_scene="main"))
        back = result["applied_commands"][0]["scene_exit"]["scene_exit_id"]
        self.edit(self.command("route.create_trigger", scene_id="settings", source_state="selected", logical_source="BUTTON_R", scene_exit_ref=back))
        self.call("project.preview_reset", scene_id="main")
        self.call("project.preview_advance", elapsed_ms=950)
        self.call("project.preview_input", logical_source="BUTTON_A")
        destination = self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual("settings", destination["scene"]["scene_id"])
        self.assertEqual("selected", destination["scene"]["state_id"])
        restored = self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual("main", restored["scene"]["scene_id"])
        self.assertEqual("start", restored["scene"]["state_id"])
        self.assertEqual(0, restored["variables"]["count"])
        objects = {obj["object_id"]: obj for obj in restored["objects"]}
        self.assertEqual(68, objects["position_marker"]["underlying"]["y"])
        self.assertFalse(objects["timer_marker"]["effective"]["visible"])
        self.assertEqual(0, self.service._preview._elapsed_ms)
        self.assertEqual([2000], list(self.service._preview._scene_timer_deadlines.values()))
        self.assertEqual("pulse.a", objects["continuity_sprite"]["effective"]["visual_ref"])

    def test_timer_replacement_drops_other_source_expiries(self):
        exit_id = self.add_exit()
        self.timer_exit(exit_id)
        self.edit(self.command("event_binding.add", event_binding={"binding_id": "later", "event_type": "time.scene_elapsed", "configuration": {"delay_ms": 2100, "start_policy": "scene_entry"}}),
                  self.command("event_handler.add", event_handler={"handler_id": "later_handler", "event_ref": "later", "guards": [], "actions": [
                      {"kind": "object.move_by", "object_ref": "position_marker", "dx": 1}]}))
        self.call("project.preview_reset", scene_id="main")
        result = self.call("project.preview_advance", elapsed_ms=2500)
        self.assertEqual("settings", result["scene"]["scene_id"])
        self.assertEqual(1, len(result["timer_events"]))
        self.assertEqual({}, self.service._preview._scene_timer_deadlines)
        self.assertEqual(500, self.service._preview._elapsed_ms)

    def test_direct_route_guard_and_named_exit_attachment(self):
        exit_id = self.add_exit()
        self.edit(self.command("variable.add", variable={"variable_id": "allowed", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 1}),
                  self.command("route.add", route={"route_id": "direct", "action_ref": "button_a_press", "from_states": ["marker_right"],
                      "target_scene": "settings", "guards": [{"variable_ref": "allowed", "operator": "eq", "value": 1}], "actions": []}))
        self.edit(self.command("route.set_target", route_id="direct", target_scene="settings", scene_exit_ref=exit_id))
        self.call("project.preview_reset", scene_id="main", state_id="marker_right")
        rejected = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertFalse(rejected["input"]["accepted"])
        self.assertEqual("main", rejected["scene"]["scene_id"])
        self.edit(self.command("variable.update", variable={"variable_id": "allowed", "value_type": "int32", "initial": 1, "minimum": 0, "maximum": 1}))
        self.call("project.preview_reset", scene_id="main", state_id="marker_right")
        accepted = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual("settings", accepted["scene"]["scene_id"])

    def test_failed_destination_raster_does_not_replace_source(self):
        self.wire(self.add_exit())
        self.call("project.preview_reset", scene_id="main")
        preview = self.service._preview
        before = preview.snapshot()
        render = StateScenePreview._render_framebuffer

        def fail_destination(candidate):
            if candidate.scene_id == "settings":
                raise PreviewError("injected destination raster failure")
            render(candidate)

        with patch.object(StateScenePreview, "_render_framebuffer", fail_destination):
            with self.assertRaises(ProtocolError):
                self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual(before, preview.snapshot())
        result = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual("marker_right", result["scene"]["state_id"])

    def test_rejected_destination_preserves_source_and_pending_expiry(self):
        exit_id = self.add_exit()
        self.wire(exit_id)
        self.timer_exit(exit_id)
        self.call("project.preview_reset", scene_id="main")
        preview = self.service._preview
        before = preview.snapshot()
        validate = StateScenePreview._validate_scene_subset

        def reject_destination(candidate):
            if candidate.scene_id == "settings":
                raise PreviewError("injected destination admission failure")
            validate(candidate)

        with patch.object(StateScenePreview, "_validate_scene_subset", reject_destination):
            with self.assertRaises(ProtocolError):
                self.call("project.preview_input", logical_source="BUTTON_R")
            self.assertEqual(before, preview.snapshot())
            with self.assertRaises(ProtocolError):
                self.call("project.preview_advance", elapsed_ms=2000)
            self.assertEqual("main", preview.scene_id)
            self.assertEqual([2000], list(preview._scene_timer_deadlines.values()))
            self.assertEqual(set(), preview._fired_timer_bindings)
        result = self.call("project.preview_advance", elapsed_ms=0)
        self.assertEqual("settings", result["scene"]["scene_id"])

    def test_schema_and_direct_document_validation_agree_on_handler_links(self):
        exit_id = self.add_exit()
        self.timer_exit(exit_id)
        self.call("project.save")
        self.assertTrue(load_project(self.root).valid)
        schema = Path(__file__).resolve().parents[3] / "schemas/authoring/state-scene-v2.schema.json"
        definitions = json.loads(schema.read_text(encoding="utf-8"))["$defs"]
        self.assertIn("scene_exit_ref", definitions["handler"]["properties"])
        self.assertEqual(0, definitions["scene_replacement"]["then"]["properties"]["actions"]["maxItems"])
        invalid = deepcopy(self.scene())
        invalid["event_handlers"][0]["actions"] = [{"kind": "restart_timer", "timer_ref": "reveal_timer"}]
        (self.root / "scenes/main.state.json").write_text(json.dumps(invalid), encoding="utf-8")
        rejected = load_project(self.root)
        self.assertFalse(rejected.valid)
        self.assertIn("SCENE_TRANSITION_ACTION_UNSUPPORTED", [issue.code for issue in rejected.issues])


if __name__ == "__main__":
    unittest.main()
