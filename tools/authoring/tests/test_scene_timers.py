from __future__ import annotations

import copy
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOL_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_ROOT))

from peepshow_authoring.compiler import _compile_graph, build_egg  # noqa: E402
from peepshow_authoring.egg_format import (  # noqa: E402
    EVENT_RECORD, GRAPH_HEADER, ROUTE_RECORD_V2,
    STATE_RECORD, EggFormatError, _parse_graph, parse_egg,
)
from peepshow_authoring.preview import StateScenePreview  # noqa: E402
from peepshow_authoring.project import ProjectCommandError, apply_project_commands, load_project  # noqa: E402
from peepshow_authoring.protocol import ServiceRequest  # noqa: E402
from peepshow_authoring.service import AuthoringService  # noqa: E402


class SceneTimerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "timers.peepproj"
        shutil.copytree(TOOL_ROOT / "peepshow_authoring/test_project.peepproj", self.root)
        self.path = self.root / "scenes/state_demo.state.json"
        self.scene = json.loads(self.path.read_text(encoding="utf-8"))
        self.scene["variables"].append({"variable_id": "hits", "value_type": "int32",
                                        "initial": 0, "minimum": 0, "maximum": 100})
        self.scene["event_bindings"] = []
        self.scene["event_handlers"] = []
        self.add_timer("scene_delay", 1000)

    def add_timer(self, name: str, delay: int, start: str = "scene_entry") -> None:
        self.scene["event_bindings"].append({
            "binding_id": name, "event_type": "time.scene_elapsed",
            "configuration": {"delay_ms": delay, "start_policy": start},
        })
        self.scene["event_handlers"].append({
            "handler_id": name + "_handler", "event_ref": name, "guards": [],
            "actions": [{"kind": "set_variable", "variable_ref": "hits",
                         "operation": "add", "value": 1}],
        })
        self.scene["reactive_wait_default"]["event_interests"].append(name)

    def bundle(self):
        self.path.write_text(json.dumps(self.scene), encoding="utf-8")
        return load_project(self.root)

    def preview(self) -> StateScenePreview:
        bundle = self.bundle()
        self.assertEqual((), bundle.issues)
        return StateScenePreview(parse_egg(build_egg(bundle)), "state_demo")

    def hits(self, preview: StateScenePreview) -> int:
        return preview.snapshot()["variables"]["hits"]

    def set_route_command(self, kind: str) -> None:
        for route in self.scene["routes"]:
            if route.get("action_ref") == "move_right":
                route["actions"].append({"kind": kind, "timer_ref": "scene_delay"})

    def test_scene_deadline_survives_selection_and_action_only_does_not_reenter(self) -> None:
        preview = self.preview()
        self.assertEqual(6, preview._graph["format_version"])
        handler = preview._graph["routes"][-1]
        self.assertEqual((), handler["source_state_indexes"])
        self.assertIsNone(handler["target_state_index"])
        preview.advance(400)
        self.assertTrue(preview.apply_input("BUTTON_R").accepted)
        preview.advance(599)
        self.assertEqual(0, self.hits(preview))
        events = preview.advance(1)
        self.assertEqual(1, len(events))
        self.assertTrue(events[0].accepted)
        self.assertEqual("time.scene_elapsed", events[0].logical_source)
        self.assertEqual(1, self.hits(preview))
        self.assertEqual("right", preview.snapshot()["scene"]["state_id"])
        self.assertEqual(600, preview._state_elapsed_ms)
        preview.advance(5000)
        self.assertEqual(1, self.hits(preview))

    def test_start_idle_and_start_active_are_distinct(self) -> None:
        self.scene["event_bindings"][0]["configuration"]["start_policy"] = "action"
        self.set_route_command("start_timer")
        preview = self.preview()
        preview.advance(2000)
        self.assertEqual(0, self.hits(preview))
        preview.apply_input("BUTTON_R")
        preview.advance(600)
        preview.apply_input("BUTTON_R")
        preview.advance(400)
        self.assertEqual(1, self.hits(preview))
        preview.apply_input("BUTTON_R")
        preview.advance(1000)
        self.assertEqual(2, self.hits(preview))

    def test_restart_moves_deadline_and_cancel_removes_it(self) -> None:
        self.set_route_command("restart_timer")
        preview = self.preview()
        preview.advance(900)
        preview.apply_input("BUTTON_R")
        preview.advance(999)
        self.assertEqual(0, self.hits(preview))
        preview.advance(1)
        self.assertEqual(1, self.hits(preview))
        for route in self.scene["routes"]:
            for action in route["actions"]:
                if action["kind"] == "restart_timer":
                    action["kind"] = "cancel_timer"
        preview = self.preview()
        preview.advance(999)
        preview.apply_input("BUTTON_R")
        preview.advance(5000)
        self.assertEqual(0, self.hits(preview))

    def test_equal_deadline_cancel_and_restart_invalidate_next_due(self) -> None:
        self.add_timer("second", 1000)
        for command in ("cancel_timer", "restart_timer", "start_timer"):
            with self.subTest(command=command):
                self.scene["event_handlers"][0]["actions"] = [
                    {"kind": command, "timer_ref": "second"}]
                preview = self.preview()
                preview.advance(1000)
                self.assertEqual(1 if command == "start_timer" else 0, self.hits(preview))
                preview.advance(1000)
                self.assertEqual(0 if command == "cancel_timer" else 1, self.hits(preview))

    def test_false_guard_consumes_one_shot(self) -> None:
        self.scene["event_handlers"][0]["guards"] = [
            {"variable_ref": "selected_index", "operator": "eq", "value": 2}]
        preview = self.preview()
        preview.advance(1000)
        self.assertEqual(0, self.hits(preview))
        preview.apply_input("BUTTON_R")
        preview.advance(5000)
        self.assertEqual(0, self.hits(preview))

    def test_handler_can_transition_and_old_state_timer_still_cancels(self) -> None:
        self.scene["event_handlers"][0]["target_state"] = "right"
        self.scene["event_bindings"].append({
            "binding_id": "state_delay", "event_type": "time.state_entry_elapsed",
            "configuration": {"delay_ms": 1500}})
        self.scene["routes"].append({
            "route_id": "state_delay_route", "event_ref": "state_delay",
            "from_states": ["center"], "guards": [], "actions": [], "target_state": "left"})
        preview = self.preview()
        preview.advance(1600)
        self.assertEqual("right", preview.snapshot()["scene"]["state_id"])
        preview.select_state("center")
        preview.advance(1500)
        self.assertEqual("left", preview.snapshot()["scene"]["state_id"])

    def test_action_only_handler_does_not_restart_state_timer(self) -> None:
        self.scene["event_bindings"].append({
            "binding_id": "state_delay", "event_type": "time.state_entry_elapsed",
            "configuration": {"delay_ms": 1500}})
        self.scene["routes"].append({
            "route_id": "state_delay_route", "event_ref": "state_delay",
            "from_states": ["center"], "guards": [], "actions": [], "target_state": "left"})
        preview = self.preview()
        preview.advance(1500)
        self.assertEqual(1, self.hits(preview))
        self.assertEqual("left", preview.snapshot()["scene"]["state_id"])

    def test_suspension_pauses_relative_time(self) -> None:
        preview = self.preview()
        preview.advance(400)
        preview.suspend()
        preview.advance(20000)
        self.assertFalse(preview.apply_input("BUTTON_R").accepted)
        preview.resume()
        preview.advance(599)
        self.assertEqual(0, self.hits(preview))
        preview.advance(1)
        self.assertEqual(1, self.hits(preview))

    def test_recreated_scene_gets_new_timer_not_old_deadline(self) -> None:
        preview = self.preview()
        preview.advance(900)
        preview.apply_input("BUTTON_A")
        self.assertNotEqual("state_demo", preview.scene_id)
        self.assertEqual({}, preview._scene_timer_deadlines)
        preview.apply_input("BUTTON_B")
        self.assertEqual("state_demo", preview.scene_id)
        preview.advance(999)
        self.assertEqual(0, self.hits(preview))
        preview.advance(1)
        self.assertEqual(1, self.hits(preview))

    def test_source_rejects_invalid_handlers_and_timer_actions(self) -> None:
        original = copy.deepcopy(self.scene)
        cases = [
            (lambda: self.scene["event_handlers"].clear(), "EVENT_HANDLER_REQUIRED"),
            (lambda: self.scene["event_handlers"].append(copy.deepcopy(self.scene["event_handlers"][0]) | {"handler_id": "duplicate"}), "EVENT_HANDLER_REQUIRED"),
            (lambda: self.scene["event_handlers"][0].update(event_ref="move_left"), "EVENT_HANDLER_SCOPE_INVALID"),
            (lambda: self.scene["event_handlers"][0].update(from_states=["center"]), "PROJECT_FIELD_UNKNOWN"),
            (lambda: self.scene["event_handlers"][0].update(target_scene="absent"), "SCENE_TRANSITION_TARGET_UNKNOWN"),
            (lambda: self.scene["event_handlers"][0].update(actions=[{"kind": "start_timer", "timer_ref": "missing"}]), "ACTION_TIMER_UNKNOWN"),
            (lambda: self.scene["event_bindings"][0]["configuration"].update(start_policy="random"), "EVENT_TIMER_START_INVALID"),
        ]
        for mutation, expected in cases:
            with self.subTest(expected=expected):
                self.scene = copy.deepcopy(original)
                mutation()
                self.assertIn(expected, {issue.code for issue in self.bundle().issues})

    def test_handler_commands_add_update_delete_atomically(self) -> None:
        binding = self.scene["event_bindings"].pop()
        handler = self.scene["event_handlers"].pop()
        self.scene["reactive_wait_default"]["event_interests"].remove("scene_delay")
        bundle = self.bundle()
        added, results = apply_project_commands(bundle, [
            {"kind": "event_binding.add", "scene_id": "state_demo", "event_binding": binding},
            {"kind": "event_handler.add", "scene_id": "state_demo", "event_handler": handler},
        ])
        self.assertTrue(added.valid)
        self.assertEqual(2, len(results))
        handler["actions"][0]["value"] = 2
        updated, _ = apply_project_commands(added, [
            {"kind": "event_handler.update", "scene_id": "state_demo", "event_handler": handler}])
        preview = StateScenePreview(parse_egg(build_egg(updated)), "state_demo")
        preview.advance(1000)
        self.assertEqual(2, self.hits(preview))
        with self.assertRaises(ProjectCommandError) as error:
            apply_project_commands(updated, [
                {"kind": "event_binding.delete", "scene_id": "state_demo", "binding_id": "scene_delay"}])
        self.assertEqual("COMMAND_TARGET_IN_USE", error.exception.code)
        removed, _ = apply_project_commands(updated, [
            {"kind": "event_handler.delete", "scene_id": "state_demo", "handler_id": "scene_delay_handler"},
            {"kind": "event_binding.delete", "scene_id": "state_demo", "binding_id": "scene_delay"},
        ])
        self.assertTrue(removed.valid)

    def test_service_returns_expiry_results_once(self) -> None:
        self.bundle()
        service = AuthoringService()
        loaded = service.handle(ServiceRequest("test", "project.load", {"path": str(self.root)}))
        reset = service.handle(ServiceRequest("test", "project.preview_reset", {
            "project_revision": loaded["project_revision"], "scene_id": "state_demo"}))
        params = {"project_revision": loaded["project_revision"],
                  "preview_revision": reset["preview_revision"], "elapsed_ms": 1000}
        expired = service.handle(ServiceRequest("test", "project.preview_advance", params))
        self.assertEqual(1, expired["variables"]["hits"])
        self.assertTrue(expired["timer_events"][0]["accepted"])
        self.assertEqual("scene_delay_handler", expired["timer_events"][0]["route_id"])
        later = service.handle(ServiceRequest("test", "project.preview_advance", params))
        self.assertEqual([], later["timer_events"])

    def test_binary_rejects_wrong_version_scope_and_timer_reference(self) -> None:
        scene = {
            "variables": [], "input_actions": [], "entry_state": "s",
            "event_bindings": [{"binding_id": "t", "event_type": "time.scene_elapsed",
                                "configuration": {"delay_ms": 1000}}],
            "states": [{"state_id": "s", "display_name": "s", "render_model_ref": "r", "waiting_visual_ref": "w"}],
            "render_models": [{"visual_id": "r", "elements": []}],
            "waiting_visuals": [{"waiting_visual_id": "w"}], "routes": [],
            "event_handlers": [{"handler_id": "h", "event_ref": "t", "guards": [],
                                "actions": [{"kind": "start_timer", "timer_ref": "t"}]}],
            "reactive_wait_default": {"policy_id": "p", "waiting_visual_ref": "w",
                                      "hold_fallback_allowed": True, "event_interests": ["t"]},
            "interaction_policy": {"policy_id": "i", "mode": "continuous",
                                   "meaningful_activity_actions": []},
        }
        strings = ("s", "t", "r", "w", "h", "p", "i")
        blob = _compile_graph(scene, {value: index for index, value in enumerate(strings)}, {})
        self.assertEqual(6, _parse_graph(blob, strings, 1, 1, 0)["format_version"])
        route_offset = GRAPH_HEADER.size + EVENT_RECORD.size + STATE_RECORD.size
        operation_offset = route_offset + ROUTE_RECORD_V2.size
        mutations = [(4, b"\x05\x00"),  # v5 cannot express scene timers
                     (GRAPH_HEADER.size + 2, b"\x01"),  # input class without source states
                     (operation_offset + 2, b"\x01\x00"),  # missing timer binding
                     (operation_offset + 4, b"\x01\x00")]  # reserved action field
        for offset, value in mutations:
            with self.subTest(offset=offset):
                broken = bytearray(blob)
                broken[offset:offset + len(value)] = value
                with self.assertRaises(EggFormatError):
                    _parse_graph(bytes(broken), strings, 1, 1, 0)


if __name__ == "__main__":
    unittest.main()
