"""Public editing, persistence and host-session lifetime for scoped variables."""
import unittest
import json
from copy import deepcopy
import test_scene_object_connections as connections
from peepshow_authoring.project import load_project
from peepshow_authoring.compiler import build_development_egg_v2, EggCompileError
from peepshow_authoring.protocol import ProtocolError


class ScopedVariableTests(unittest.TestCase):
    def setUp(self):
        self.h = connections.SceneObjectConnectionTests()
        self.h.setUp()
        self.addCleanup(self.h.doCleanups)
        h = self.h
        h.edit({"kind": "project.variables.enable"},
               {"kind": "package_variable.add", "variable": self.integer("count")},
               h.command("variable.add", variable=self.integer("count")),
               {"kind": "package_variable.add", "variable": {
                   "variable_id": "flag", "value_type": "bool", "initial": False}})

    @staticmethod
    def integer(name):
        return {"variable_id": name, "value_type": "int32", "initial": 1, "minimum": 0, "maximum": 3}

    def actions(self, actions):
        h = self.h
        h.edit(h.command("object_actions.set", owner_kind="route", owner_id="start_button_a_press", actions=actions))

    @staticmethod
    def action(scope, operation="add", value=1, name="count"):
        action = {"kind": "set_variable", "variable_scope": scope, "variable_ref": name, "operation": operation}
        if operation != "reset":
            action["value"] = value
        return action

    @staticmethod
    def values(result):
        return {(v["scope"], v["variable_id"]): v["value"] for v in result["scoped_variables"]}

    def test_lifetimes_clamp_reopen_and_export_block(self):
        h = self.h
        self.actions([self.action("package", value=100), self.action("scene", value=-100),
                      self.action("package", "assign", True, "flag")])
        h.wire(h.add_exit())
        exit_id = h.edit(h.command("scene_exit.add", scene_id="settings", target_scene="main"))["applied_commands"][0]["scene_exit"]["scene_exit_id"]
        h.edit(h.command("route.create_trigger", scene_id="settings", source_state="start",
                         logical_source="BUTTON_B", scene_exit_ref=exit_id))
        h.call("project.save")
        h.call("project.load", path=str(h.root))
        self.assertTrue(load_project(h.root).valid)
        with self.assertRaises(ProtocolError):
            h.call("project.build_package")
        with self.assertRaises(EggCompileError):
            build_development_egg_v2(h.service._bundle)
        h.call("project.preview_reset", scene_id="main")
        result = h.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual(3, self.values(result)["package", "count"])
        self.assertEqual(0, self.values(result)["scene", "count"])
        self.assertIs(True, self.values(result)["package", "flag"])
        for operation in ("project.preview_suspend", "project.preview_resume"):
            result = h.call(operation, preview_revision=h.preview_revision)
            self.assertEqual(3, self.values(result)["package", "count"])
            self.assertEqual(0, self.values(result)["scene", "count"])
        h.call("project.preview_input", logical_source="BUTTON_R")
        result = h.call("project.preview_input", logical_source="BUTTON_B")
        self.assertEqual(3, self.values(result)["package", "count"])
        self.assertEqual(1, self.values(result)["scene", "count"])
        result = h.call("project.preview_reset", scene_id="main")
        self.assertEqual(1, self.values(result)["package", "count"])
        self.assertIs(False, self.values(result)["package", "flag"])

    def test_reset_guard_and_reference_protection(self):
        h = self.h
        self.actions([self.action("package", value=2), self.action("package", "reset"),
                      self.action("package", "assign", True, "flag")])
        h.call("project.preview_reset", scene_id="main")
        result = h.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual(1, self.values(result)["package", "count"])
        before = h.service._bundle.canonical_bytes()
        with self.assertRaises(ProtocolError):
            h.edit({"kind": "package_variable.delete", "variable_id": "count"})
        self.assertEqual(before, h.service._bundle.canonical_bytes())
        h.edit(h.command("variable.delete", variable_id="count"))
        h.edit(h.command("route.guard.add", route_id="start_button_a_press", guard_index=0,
                         guard={"variable_scope": "package", "variable_ref": "flag", "operator": "eq", "value": True}))
        h.call("project.preview_reset", scene_id="main")
        result = h.call("project.preview_input", logical_source="BUTTON_A")
        self.assertFalse(result["input"]["accepted"])

    def test_validation_and_undo(self):
        h = self.h
        for action in (self.action("other"), self.action("package", "add", 1, "flag"),
                       self.action("package", "assign", 99), self.action("package", "assign", 1, "flag")):
            with self.subTest(action=action), self.assertRaises(ProtocolError):
                self.actions([action])
        for variable in ({**self.integer("bad"), "initial": 4},
                         {**self.integer("bad"), "maximum": 2147483648}):
            with self.assertRaises(ProtocolError):
                h.edit({"kind": "package_variable.add", "variable": variable})
        original = deepcopy(h.service._bundle.project["package_variables"])
        h.edit({"kind": "package_variable.update", "variable": {**self.integer("count"), "initial": 2}})
        h.call("project.undo")
        self.assertEqual(original, h.service._bundle.project["package_variables"])
        h.call("project.redo")
        self.assertEqual(2, h.service._bundle.project["package_variables"][0]["initial"])

    def test_capabilities_and_budget_are_explicit(self):
        h = self.h
        cap = h.call("service.hello")["scoped_variables"]
        self.assertTrue(cap["host_preview"])
        self.assertFalse(cap["runtime"])
        self.assertFalse(cap["export"])
        self.assertFalse(cap["entry_graphs"])
        before = h.service._bundle.canonical_bytes()
        with self.assertRaises(ProtocolError):
            h.edit(*[{"kind": "package_variable.add", "variable": self.integer(f"extra{i}")} for i in range(32)])
        self.assertEqual(before, h.service._bundle.canonical_bytes())

    def test_boolean_scene_reset_and_duplicate_scope_ids(self):
        h = self.h
        h.edit(h.command("variable.add", variable={"variable_id": "flag", "value_type": "bool", "initial": True}))
        self.actions([self.action("scene", "assign", False, "flag"),
                      self.action("scene", "reset", name="flag")])
        h.call("project.preview_reset", scene_id="main")
        result = h.call("project.preview_input", logical_source="BUTTON_A")
        self.assertIs(True, result["variables"]["flag"])
        self.assertIs(False, result["package_variables"]["flag"])

    def test_scene_exit_variable_writes_still_rejected(self):
        h = self.h
        route = h.wire(h.add_exit())
        with self.assertRaises(ProtocolError):
            h.edit(h.command("object_actions.set", owner_kind="route", owner_id=route,
                             actions=[self.action("package")]))

    def test_malformed_reopened_source_reports_issues(self):
        h = self.h
        h.call("project.save")
        path = h.root / "scenes/main.state.json"
        original = json.loads(path.read_text(encoding="utf-8"))
        for key, value in (("variables", None),):
            changed = {**original, key: value}
            path.write_text(json.dumps(changed), encoding="utf-8")
            self.assertFalse(load_project(h.root).valid)
        path.write_text(json.dumps(original), encoding="utf-8")
        project = json.loads((h.root / "project.json").read_text(encoding="utf-8"))
        project.pop("variable_model")
        (h.root / "project.json").write_text(json.dumps(project), encoding="utf-8")
        self.assertFalse(load_project(h.root).valid)
