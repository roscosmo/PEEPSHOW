"""Public entry-graph editing, staged preview execution and persistence."""
from copy import deepcopy
from unittest.mock import patch
import unittest

import test_scoped_variables as scoped
from peepshow_authoring.preview import PreviewError, StateScenePreview
from peepshow_authoring.protocol import ProtocolError
from peepshow_authoring.project import load_project
from peepshow_authoring.compiler import build_development_egg_v2, EggCompileError
from peepshow_authoring.scene_entry import validate
from peepshow_authoring.project import ProjectCommandError


class SceneEntryTests(unittest.TestCase):
    def setUp(self):
        self.base = scoped.ScopedVariableTests()
        self.base.setUp()
        self.addCleanup(self.base.doCleanups)
        self.h = self.base.h

    def graph(self):
        action = self.base.action
        return {"root": "before", "nodes": [
            {"node_id": "before", "kind": "actions", "actions": [action("package")], "next": "choose"},
            {"node_id": "choose", "kind": "decision", "branches": [
                {"guards": [{"variable_scope": "package", "variable_ref": "count", "operator": "eq", "value": 2}], "next": "right"},
                {"guards": [{"variable_scope": "package", "variable_ref": "count", "operator": "ge", "value": 2}], "next": "left"}
            ], "default": "left"},
            {"node_id": "right", "kind": "state", "state_ref": "marker_right"},
            {"node_id": "left", "kind": "state", "state_ref": "start"}
        ], "layout": {"choose": {"x": 20, "y": 40}}}

    def install(self, graph=None, scene="main"):
        self.h.edit(self.h.command("scene.entry_graph.set", scene_id=scene, graph=graph or self.graph()))

    def test_order_first_match_persistence_clear_undo_and_capability(self):
        h = self.h
        graph = self.graph()
        self.install(graph)
        cap = h.call("service.hello")["entry_graph"]
        self.assertTrue(cap["host_preview"])
        self.assertFalse(cap["runtime"])
        h.call("project.save")
        h.call("project.load", path=str(h.root))
        self.assertEqual(graph, h.scene()["entry_graph"])
        self.assertTrue(load_project(h.root).valid)
        result = h.call("project.preview_reset", scene_id="main")
        self.assertEqual("marker_right", result["scene"]["state_id"])
        self.assertEqual(2, result["package_variables"]["count"])
        self.assertEqual(["before", "choose", "right"], result["entry_trace"])
        self.assertEqual([], result["entry_events"])
        with self.assertRaises(ProtocolError):
            h.call("project.build_package")
        with self.assertRaises(EggCompileError):
            build_development_egg_v2(h.service._bundle)
        h.edit(h.command("scene.entry_graph.clear"))
        self.assertNotIn("entry_graph", h.scene())
        h.call("project.undo")
        self.assertEqual(graph, h.scene()["entry_graph"])
        h.call("project.redo")
        self.assertNotIn("entry_graph", h.scene())

    def test_fresh_reentry_only_and_explicit_state_bypass(self):
        h = self.h
        self.install()
        h.wire(h.add_exit())
        exit_id = h.edit(h.command("scene_exit.add", scene_id="settings", target_scene="main"))["applied_commands"][0]["scene_exit"]["scene_exit_id"]
        h.edit(h.command("route.create_trigger", scene_id="settings", source_state="start", logical_source="BUTTON_B", scene_exit_ref=exit_id))
        result = h.call("project.preview_reset", scene_id="main", state_id="start")
        self.assertEqual(1, result["package_variables"]["count"])
        self.assertEqual([], result["entry_trace"])
        h.call("project.preview_reset", scene_id="main")
        for op in ("project.preview_suspend", "project.preview_resume"):
            result = h.call(op, preview_revision=h.preview_revision)
            self.assertEqual(2, result["package_variables"]["count"])
            self.assertNotIn("entry_events", result)
        h.call("project.preview_input", logical_source="BUTTON_R")
        result = h.call("project.preview_input", logical_source="BUTTON_B")
        self.assertEqual(3, result["package_variables"]["count"])
        self.assertEqual("start", result["scene"]["state_id"])
        result = h.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual(3, result["package_variables"]["count"])
        self.assertEqual("marker_right", result["scene"]["state_id"])

    def test_default_merge_and_scoped_reset(self):
        action = self.base.action
        graph = self.graph()
        graph["nodes"][0]["actions"] = [action("package", "reset"), action("scene", value=100)]
        graph["nodes"][1]["branches"] = [{"guards": [{"variable_scope": "package", "variable_ref": "flag", "operator": "eq", "value": True}], "next": "shared"}]
        graph["nodes"][1]["default"] = "shared"
        graph["nodes"] = graph["nodes"][:2] + [
            {"node_id": "shared", "kind": "actions", "actions": [action("package"), action("scene", "reset")], "next": "left"},
            graph["nodes"][3]]
        self.install(graph)
        result = self.h.call("project.preview_reset", scene_id="main")
        self.assertEqual(["before", "choose", "shared", "left"], result["entry_trace"])
        self.assertEqual(2, result["package_variables"]["count"])
        self.assertEqual(1, self.base.values(result)["scene", "count"])
        for command in ({"kind": "package_variable.delete", "variable_id": "count"}, self.h.command("variable.delete", variable_id="count")):
            self.h.reject(command, code="COMMAND_TARGET_IN_USE")

    def test_invalid_graphs_reject_atomically(self):
        mutations = [
            lambda g: g.update(root="missing"),
            lambda g: g["nodes"][1].pop("default"),
            lambda g: g["nodes"][0].update(next="before"),
            lambda g: g["nodes"][2].update(state_ref="missing"),
            lambda g: g["nodes"][0].update(actions=[{"kind": "restart_timer", "timer_ref": "timer"}]),
            lambda g: g["nodes"][0].update(actions=[self.base.action("package", "add", 1, "flag")]),
            lambda g: g["nodes"][0].update(actions=[self.base.action("other")]),
            lambda g: g["nodes"][0].update(actions=[self.base.action("package")] * 9),
            lambda g: g["nodes"].append({"node_id": "unused", "kind": "state", "state_ref": "start"}),
            lambda g: g["layout"].update(missing={"x": 0, "y": 0}),
            lambda g: g["nodes"][1]["branches"][0].update(guards=[]),
            lambda g: g["nodes"][0].update(actions=[{"kind": "play_sfx", "cue_ref": "missing"}]),
        ]
        for mutate in mutations:
            graph = self.graph()
            mutate(graph)
            with self.subTest(graph=graph):
                self.h.reject(self.h.command("scene.entry_graph.set", graph=graph))

    def test_entry_audio_and_failed_admission_rollback(self):
        h = self.h
        h.add_audio()
        cue = {"kind": "play_sfx", "cue_ref": "ui.select.cue"}
        graph = {"root": "effects", "nodes": [
            {"node_id": "effects", "kind": "actions", "actions": [self.base.action("package"), cue], "next": "end"},
            {"node_id": "end", "kind": "state", "state_ref": "start"}]}
        self.install(graph, "settings")
        result = h.call("project.preview_reset", scene_id="settings")
        self.assertEqual("ui.select.cue", result["entry_events"][0]["cue_id"])
        route = h.wire(h.add_exit())
        h.edit(h.command("object_actions.set", owner_kind="route", owner_id=route, actions=[cue]))
        h.call("project.preview_reset", scene_id="main")
        old = h.service._preview.snapshot()
        original = StateScenePreview._validate_scene_subset

        def reject_target(preview):
            if preview.scene_id == "settings":
                raise PreviewError("test destination admission failure")
            return original(preview)

        with patch.object(StateScenePreview, "_validate_scene_subset", reject_target), self.assertRaises(ProtocolError):
            h.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual(old, h.service._preview.snapshot())
        result = h.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual(2, result["package_variables"]["count"])
        self.assertEqual(2, len(result["input"]["audio_events"]))

    def test_maximum_depth_and_host_opt_in(self):
        graph = {"root": "n0", "nodes": [
            {"node_id": f"n{i}", "kind": "actions", "actions": [self.base.action("package")], "next": f"n{i + 1}"}
            for i in range(31)] + [{"node_id": "n31", "kind": "state", "state_ref": "start"}]}
        self.install(graph)
        result = self.h.call("project.preview_reset", scene_id="main")
        self.assertEqual(32, len(result["entry_trace"]))
        self.assertEqual(3, result["package_variables"]["count"])
        oversized = deepcopy(graph)
        oversized["nodes"].append({"node_id": "extra", "kind": "state", "state_ref": "start"})
        self.h.reject(self.h.command("scene.entry_graph.set", graph=oversized))
        with self.assertRaises(ProjectCommandError):
            validate({}, self.h.scene(), set())

    def test_timer_replacement_includes_entry_audio_once(self):
        h = self.h
        h.add_audio()
        graph = {"root": "cue", "nodes": [
            {"node_id": "cue", "kind": "actions", "actions": [{"kind": "play_sfx", "cue_ref": "ui.select.cue"}], "next": "end"},
            {"node_id": "end", "kind": "state", "state_ref": "start"}]}
        self.install(graph, "settings")
        h.timer_exit(h.add_exit())
        h.call("project.preview_reset", scene_id="main")
        result = h.call("project.preview_advance", elapsed_ms=2000)
        self.assertEqual("settings", result["scene"]["scene_id"])
        self.assertEqual(1, len(result["timer_events"][0]["audio_events"]))
        result = h.call("project.preview_advance", elapsed_ms=2000)
        self.assertEqual([], result["timer_events"])
        self.assertNotIn("entry_events", result)

    def test_render_failure_and_placement_preview_have_no_entry_effects(self):
        h = self.h
        self.install()
        h.call("project.preview_reset", scene_id="settings")
        original = h.service._preview
        with patch.object(StateScenePreview, "_render_framebuffer", side_effect=PreviewError("render rejected")), self.assertRaises(ProtocolError):
            h.call("project.preview_reset", scene_id="main")
        self.assertIs(original, h.service._preview)
        result = h.call("project.preview_scene_base", scene_id="main")
        self.assertNotIn("entry_events", result)
        self.assertIs(original, h.service._preview)


if __name__ == "__main__":
    unittest.main()
