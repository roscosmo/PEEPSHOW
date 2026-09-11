"""Build local V2 graphs natively, without migration or direct bundle mutation."""
from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.scene_object_authoring import LOCAL_GRAPH_COMMANDS
from peepshow_authoring.service import AuthoringService


class SceneObjectGraphAuthoringTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "Graph.peepproj"
        self.service = AuthoringService()
        self.revision = self.preview_revision = 0
        self.call("project.create", path=str(self.root), scene_schema_version=2)
        self.edit(self.command("state.create", display_name="Other", x=200, y=0),
                  self.command("variable.add", variable=self.variable()),
                  self.command("object.add", object={
                      "object_id": "panel", "kind": "filled_rect", "width": 8, "height": 8,
                      "z_order": 0, "layer": "SCENE", "defaults": {"x": 10, "y": 20, "visible": True}}))

    def call(self, operation, **params):
        if operation not in ("project.create", "project.load", "service.hello"):
            params["project_revision"] = self.revision
        if operation in ("project.preview_input", "project.preview_advance"):
            params["preview_revision"] = self.preview_revision
        result = self.service.handle(ServiceRequest("test", operation, params))
        self.revision = result.get("project_revision", self.revision)
        self.preview_revision = result.get("preview_revision", self.preview_revision)
        return result

    @staticmethod
    def command(kind, **values):
        return {"kind": kind, "scene_id": "main", **values}

    @staticmethod
    def variable(**values):
        return {"variable_id": "count", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 10, **values}

    @staticmethod
    def route(route_id="next", **values):
        return {"route_id": route_id, "action_ref": "a", "from_states": ["start"],
                "target_state": "other", "guards": [], "actions": [], **values}

    @staticmethod
    def binding(binding_id="tick", event_type="time.scene_elapsed", **configuration):
        return {"binding_id": binding_id, "event_type": event_type, "configuration": {"delay_ms": 500, **configuration}}

    @staticmethod
    def handler(**values):
        return {"handler_id": "tick_handler", "event_ref": "tick", "guards": [], "actions": [], **values}

    def edit(self, *commands):
        return self.call("project.apply_commands", commands=list(commands))

    def scene(self):
        return self.service._bundle.scenes[0]

    def graph(self):
        self.edit(self.command("input_action.add", input_action={"action_id": "a", "logical_source": "BUTTON_A"}),
                  self.command("route.add", route=self.route()))

    def timer(self, **configuration):
        self.edit(self.command("event_binding.add", event_binding=self.binding(**configuration)),
                  self.command("event_handler.add", event_handler=self.handler()))

    def reject(self, *commands, code=None):
        before = self.service._bundle.canonical_bytes()
        revision = self.revision
        with self.assertRaises(ProtocolError) as error:
            self.edit(*commands)
        if code is not None:
            self.assertEqual(code, error.exception.code)
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.assertEqual(revision, self.revision)

    def test_hello_and_scene_advertise_exact_local_scope(self):
        hello = self.call("service.hello")
        self.assertEqual(42, hello["service_api_version"])
        scene_caps = self.call("project.normalize")["scene_capabilities"]["main"]
        for caps, command_key in ((hello["scene_object_authoring"], "commands"), (scene_caps, "supported_commands")):
            self.assertTrue(caps["graph_construction_commands"])
            self.assertFalse(caps["scene_connection_commands"])
            self.assertTrue(caps["egg_export"])
            self.assertEqual(["state", "system_exit"], caps["route_destination_kinds"])
            self.assertEqual(list(LOCAL_GRAPH_COMMANDS), caps["local_graph_commands"])
            self.assertTrue(set(LOCAL_GRAPH_COMMANDS).issubset(caps[command_key]))
            self.assertNotIn("route.action.add", caps[command_key])
            self.assertNotIn("scene_exit.add", caps[command_key])

    def test_native_input_guard_object_actions_save_reload_and_preview(self):
        self.graph()
        self.edit(self.command("route.guard.add", route_id="next", guard_index=0,
                               guard={"variable_ref": "count", "operator": "eq", "value": 0}),
                  self.command("object_actions.set", owner_kind="route", owner_id="next", actions=[
                      {"kind": "object.move_by", "object_ref": "panel", "dx": 5},
                      {"kind": "set_variable", "variable_ref": "count", "operation": "add", "value": 1}]),
                  self.command("object_override.set", state_id="other", object_id="panel", properties={"x": 80}))
        self.call("project.save")
        saved = load_project(self.root)
        self.assertTrue(saved.valid, saved.issues)
        self.assertEqual(saved.canonical_bytes(), self.service._bundle.canonical_bytes())
        self.call("project.load", path=str(self.root))
        self.call("project.preview_reset", scene_id="main")
        snapshot = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertTrue(snapshot["input"]["accepted"])
        self.assertEqual("other", snapshot["scene"]["state_id"])
        self.assertEqual(1, snapshot["variables"]["count"])
        self.assertEqual(15, snapshot["objects"][0]["underlying"]["x"])
        self.assertEqual(80, snapshot["objects"][0]["effective"]["x"])
        self.assertNotIn("waiting_visuals", self.scene())
        built = self.call("project.build_package")
        self.assertEqual(2, built["package"]["container_version"])

    def test_guard_edits_order_and_rejection_do_not_run_actions(self):
        self.graph()
        self.edit(self.command("route.guard.add", route_id="next", guard_index=0,
                               guard={"variable_ref": "count", "operator": "ge", "value": 0}),
                  self.command("route.guard.add", route_id="next", guard_index=1,
                               guard={"variable_ref": "count", "operator": "lt", "value": 10}),
                  self.command("route.guard.move", route_id="next", guard_index=1, target_index=0),
                  self.command("route.set_guard", route_id="next", guard_index=0, variable_ref="count", operator="eq", value=1),
                  self.command("route.guard.delete", route_id="next", guard_index=1))
        self.assertEqual([{"variable_ref": "count", "operator": "eq", "value": 1}], self.scene()["routes"][0]["guards"])
        self.call("project.preview_reset", scene_id="main")
        rejected = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertFalse(rejected["input"]["accepted"])
        self.assertEqual("start", rejected["scene"]["state_id"])
        self.reject(self.command("variable.delete", variable_id="count"), code="COMMAND_TARGET_IN_USE")
        self.reject(self.command("route.set_guard", route_id="next", guard_index=4, variable_ref="count", operator="eq", value=0))

    def test_generated_trigger_rebind_layout_and_reference_cleanup(self):
        result = self.edit(self.command("route.create_trigger", source_state="start", logical_source="BUTTON_A",
                                        target_state="other", target_handle="entry-top-left", target_side="left"))
        route_id = result["applied_commands"][0]["route"]["route_id"]
        action_id = result["applied_commands"][0]["input_action"]["action_id"]
        self.assertIn(action_id, self.scene()["reactive_wait_default"]["event_interests"])
        self.reject(self.command("input_action.delete", action_id=action_id), code="COMMAND_TARGET_IN_USE")
        result = self.edit(self.command("route.rebind_trigger", route_id=route_id, logical_source="BUTTON_B"))
        self.assertTrue(result["applied_commands"][0]["previous_action_removed"])
        action_id = result["applied_commands"][0]["input_action"]["action_id"]
        self.edit(self.command("editor.state_graph.set_route_layout", route_id=route_id, source_state="start",
                               rails=[{"axis": "x", "value": 100}], target_handle="entry-top-right", target_side="right"),
                  self.command("route.set_sources", route_id=route_id, from_states=["other"]),
                  self.command("route.set_target", route_id=route_id, target_state="start"))
        layout = self.service._bundle.project["editor"]["state_graph"]["scenes"]["main"]
        self.assertNotIn(route_id, layout.get("routes", {}))
        interaction = deepcopy(self.scene()["interaction_policy"])
        interaction["meaningful_activity_actions"] = []
        self.edit(self.command("route.delete", route_id=route_id),
                  self.command("scene.set_reactive_wait_default", reactive_wait_default={
                      "policy_id": "main_wait", "hold_fallback_allowed": True, "event_interests": []}),
                  self.command("scene.set_interaction_policy", interaction_policy=interaction),
                  self.command("input_action.delete", action_id=action_id))
        self.assertEqual([], self.scene()["routes"])
        self.assertEqual([], self.scene()["input_actions"])

    def test_rebinding_keeps_input_still_referenced_through_event_ref(self):
        result = self.edit(self.command("route.create_trigger", source_state="start", logical_source="BUTTON_A", target_state="other"))
        action_id = result["applied_commands"][0]["input_action"]["action_id"]
        route_id = result["applied_commands"][0]["route"]["route_id"]
        second = self.route("back", from_states=["other"], target_state="start")
        second.pop("action_ref")
        second["event_ref"] = action_id
        self.edit(self.command("route.add", route=second))
        result = self.edit(self.command("route.rebind_trigger", route_id=route_id, logical_source="BUTTON_B"))
        self.assertFalse(result["applied_commands"][0]["previous_action_removed"])
        self.assertIn(action_id, [action["action_id"] for action in self.scene()["input_actions"]])

    def test_scene_timer_is_independent_and_fires_once_across_state_change(self):
        self.graph()
        self.timer(start_policy="scene_entry")
        self.edit(self.command("event_handler.update", event_handler=self.handler(actions=[
            {"kind": "object.move_by", "object_ref": "panel", "dx": 7},
            {"kind": "set_variable", "variable_ref": "count", "operation": "add", "value": 1}])))
        self.call("project.preview_reset", scene_id="main")
        self.call("project.preview_advance", elapsed_ms=250)
        self.call("project.preview_input", logical_source="BUTTON_A")
        snapshot = self.call("project.preview_advance", elapsed_ms=250)
        self.assertEqual("other", snapshot["scene"]["state_id"])
        self.assertEqual(1, len(snapshot["timer_events"]))
        self.assertEqual(17, snapshot["objects"][0]["underlying"]["x"])
        self.assertEqual(1, snapshot["variables"]["count"])
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=1000)["timer_events"])

    def test_handler_variable_and_timer_references_block_deletion(self):
        self.timer()
        for values in ({"guards": [{"variable_ref": "count", "operator": "eq", "value": 0}]},
                       {"actions": [{"kind": "set_variable", "variable_ref": "count", "operation": "add", "value": 1}]}):
            self.edit(self.command("event_handler.update", event_handler=self.handler(**values)))
            self.reject(self.command("variable.delete", variable_id="count"), code="COMMAND_TARGET_IN_USE")
        self.reject(self.command("event_binding.delete", binding_id="tick"), code="COMMAND_TARGET_IN_USE")
        self.reject(self.command("event_handler.delete", handler_id="tick_handler"))
        self.edit(self.command("event_handler.delete", handler_id="tick_handler"),
                  self.command("event_binding.delete", binding_id="tick"), self.command("variable.delete", variable_id="count"))
        self.assertEqual([], self.scene()["variables"])

    def test_action_started_timer_restart_cancel_and_binding_update(self):
        self.graph()
        self.timer(start_policy="action")
        self.edit(self.command("event_binding.update", event_binding=self.binding(delay_ms=600, start_policy="action")),
                  self.command("object_actions.set", owner_kind="route", owner_id="next", actions=[{"kind": "start_timer", "timer_ref": "tick"}]))
        self.call("project.preview_reset", scene_id="main")
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=1000)["timer_events"])
        self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual(1, len(self.call("project.preview_advance", elapsed_ms=600)["timer_events"]))
        self.edit(self.command("route.set_target", route_id="next", target_state="start"),
                  self.command("object_actions.set", owner_kind="route", owner_id="next", actions=[{"kind": "restart_timer", "timer_ref": "tick"}]))
        self.call("project.preview_reset", scene_id="main")
        self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=300)["timer_events"])
        self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=300)["timer_events"])
        self.assertEqual(1, len(self.call("project.preview_advance", elapsed_ms=300)["timer_events"]))
        self.edit(self.command("object_actions.set", owner_kind="route", owner_id="next", actions=[
            {"kind": "start_timer", "timer_ref": "tick"}, {"kind": "cancel_timer", "timer_ref": "tick"}]))
        self.call("project.preview_reset", scene_id="main")
        self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=1000)["timer_events"])
        self.reject(self.command("event_handler.delete", handler_id="tick_handler"),
                    self.command("event_binding.delete", binding_id="tick"), code="COMMAND_TARGET_IN_USE")

    def test_state_timer_route_and_trigger_binding_changes(self):
        self.graph()
        self.edit(self.command("event_binding.add", event_binding=self.binding(event_type="time.state_entry_elapsed")),
                  self.command("route.set_event_ref", route_id="next", event_ref="tick"))
        self.call("project.preview_reset", scene_id="main")
        snapshot = self.call("project.preview_advance", elapsed_ms=500)
        self.assertEqual("other", snapshot["scene"]["state_id"])
        self.assertEqual(1, len(snapshot["timer_events"]))
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=500)["timer_events"])
        self.edit(self.command("route.set_action_ref", route_id="next", action_ref="a"),
                  self.command("input_action.update", input_action={"action_id": "a", "logical_source": "BUTTON_R", "event_kind": "release"}),
                  self.command("event_binding.delete", binding_id="tick"))
        self.call("project.preview_reset", scene_id="main")
        self.assertFalse(self.call("project.preview_input", logical_source="BUTTON_R")["input"]["accepted"])
        self.assertTrue(self.call("project.preview_input", logical_source="BUTTON_R", event_kind="release")["input"]["accepted"])

    def test_scope_and_unavailable_events_are_rejected(self):
        self.graph()
        self.reject(self.command("event_binding.add", event_binding=self.binding()))
        for event_type in ("battery.soc", "peripheral.tilt", "unavailable"):
            self.reject(self.command("event_binding.add", event_binding=self.binding(event_type=event_type)))
        self.timer()
        self.reject(self.command("route.set_event_ref", route_id="next", event_ref="tick"))
        self.reject(self.command("event_handler.update", event_handler=self.handler(event_ref="a")))
        self.reject(self.command("event_binding.update", event_binding=self.binding(delay_ms=0)))

    def test_diagonals_require_explicit_policy_and_shell_exit_is_local(self):
        command = self.command("route.create_trigger", source_state="start", logical_source="JOY_UP_LEFT", target_state="other")
        self.reject(command, code="JOYSTICK_POLICY_REQUIRED")
        self.edit(self.command("scene.set_joystick_policy", joystick_policy="eight_way"), command)
        self.call("project.preview_reset", scene_id="main")
        self.assertTrue(self.call("project.preview_input", logical_source="JOY_UP_LEFT")["input"]["accepted"])
        result = self.edit(self.command("route.create_trigger", source_state="start", logical_source="BUTTON_B", system_exit=True))
        route_id = result["applied_commands"][0]["route"]["route_id"]
        self.assertEqual([{"kind": "exit_to_shell"}], result["applied_commands"][0]["route"]["actions"])
        self.reject(self.command("editor.state_graph.delete_system_exit"), code="COMMAND_TARGET_IN_USE")
        self.edit(self.command("route.delete", route_id=route_id), self.command("editor.state_graph.delete_system_exit"))

    def test_connection_payloads_and_legacy_actions_stay_blocked(self):
        self.graph()
        self.timer()
        self.edit({"kind": "scene.add", "display_name": "Destination", "scene_schema_version": 2})
        remote = self.route("remote", target_scene="destination")
        remote.pop("target_state")
        for command in (
            self.command("route.add", route=remote),
            self.command("route.set_target", route_id="next", target_scene="destination"),
            self.command("route.create_trigger", source_state="start", logical_source="BUTTON_R", scene_exit_ref="exit"),
            self.command("event_handler.add", event_handler=self.handler(handler_id="remote", target_scene="destination")),
            self.command("event_handler.update", event_handler=self.handler(target_scene="destination")),
        ):
            self.reject(command, code="SCENE_OBJECT_CONNECTION_UNAVAILABLE")
        for kind in ("scene_exit.add", "editor.scene_flow.add_reference", "route.action.add", "route.set_action"):
            self.reject(self.command(kind), code="COMMAND_EXECUTION_MODEL_MISMATCH")
        invalid = self.route("legacy", actions=[{"kind": "set_element_position", "element_ref": "panel", "x": 1, "y": 2}])
        self.reject(self.command("route.add", route=invalid))
        self.reject(self.command("event_handler.update", event_handler=self.handler(actions=invalid["actions"])))

    def test_graph_batch_undo_redo_and_invalid_batch_preserve_history(self):
        before = self.service._bundle.canonical_bytes()
        self.graph()
        after = self.service._bundle.canonical_bytes()
        self.reject(self.command("variable.update", variable=self.variable(initial=3)),
                    self.command("route.set_target", route_id="next", target_state="missing"))
        self.call("project.undo")
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.call("project.redo")
        self.assertEqual(after, self.service._bundle.canonical_bytes())
        self.edit(self.command("variable.update", variable=self.variable(initial=3)))
        self.assertEqual(3, self.scene()["variables"][0]["initial"])
        self.reject(self.command("variable.update", variable=self.variable(initial=11)))

    def test_duplicate_ids_and_bounded_graph_data_are_rejected(self):
        self.graph()
        self.reject(self.command("input_action.add", input_action={"action_id": "a", "logical_source": "BUTTON_B"}))
        self.reject(self.command("event_binding.add", event_binding=self.binding(binding_id="a", event_type="time.state_entry_elapsed")))
        self.reject(self.command("route.add", route=self.route()))
        self.reject(self.command("route.add", route=self.route("bad", guards=[{"variable_ref": "count", "operator": "eq", "value": 0}] * 9)))
        self.reject(self.command("route.add", route=self.route("bad", actions=[{"kind": "request_render"}] * 9)))
        self.reject(self.command("route.set_sources", route_id="next", from_states=["missing"]))


if __name__ == "__main__":
    unittest.main()
