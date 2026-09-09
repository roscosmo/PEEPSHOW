from copy import deepcopy
from dataclasses import replace
from pathlib import Path
import json
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.compiler import EggCompileError, build_egg, build_preview_package
from peepshow_authoring.preview import PreviewError, StateScenePreview
from peepshow_authoring.project import SCENE_KEYS, SCENE_OPTIONAL_KEYS, load_project
from peepshow_authoring.protocol import ProtocolError, ServiceRequest
from peepshow_authoring.service import AuthoringService


FIXTURE = Path(__file__).resolve().parents[1] / "peepshow_authoring/test_project.peepproj"


class SceneObjectIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "objects.peepproj"
        shutil.copytree(FIXTURE, self.root)
        self.service = AuthoringService()
        self.revision = 0
        self.preview_revision = 0
        self.call("project.load", path=str(self.root))

    def call(self, operation, **params):
        if operation not in ("project.load", "service.hello"):
            params["project_revision"] = self.revision
        if operation in ("project.preview_input", "project.preview_advance"):
            params["preview_revision"] = self.preview_revision
        result = self.service.handle(ServiceRequest("test", operation, params))
        self.revision = result.get("project_revision", self.revision)
        self.preview_revision = result.get("preview_revision", self.preview_revision)
        return result

    def migrate(self):
        result = self.call("project.object_migration_preview", scene_id="state_demo", accept_continuous_animation=True)
        self.assertTrue(result["can_apply"], result)
        return self.call("project.object_migration_apply", scene_id="state_demo", accept_continuous_animation=True,
                         source_revision=result["plan"]["source_revision"])

    def command(self, kind, **fields):
        return {"kind": kind, "scene_id": "state_demo", **fields}

    def edit(self, *commands):
        return self.call("project.apply_commands", commands=list(commands))

    def scene(self):
        return next(scene for scene in self.service._bundle.scenes if scene["scene_id"] == "state_demo")

    @staticmethod
    def cursor(snapshot):
        return next(obj for obj in snapshot["objects"] if obj["object_id"] == "cursor")

    def test_migration_is_one_undo_and_redo_including_clip_catalog(self):
        before = self.service._bundle.canonical_bytes()
        result = self.migrate()
        after = self.service._bundle.canonical_bytes()
        self.assertNotEqual(before, after)
        self.assertTrue(self.service._bundle.valid)
        self.assertEqual(2, self.scene()["schema_version"])
        self.assertIn("scene_capabilities", result)
        self.assertEqual("scene_objects", result["scene_capabilities"]["state_demo"]["execution_model"])
        self.assertEqual("legacy_state_bindings", result["scene_capabilities"]["state_details"]["execution_model"])
        normalized = self.call("project.normalize")
        self.assertEqual(result["scene_capabilities"], normalized["scene_capabilities"])
        self.call("project.undo")
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.call("project.redo")
        self.assertEqual(after, self.service._bundle.canonical_bytes())

    def test_migration_requires_explicit_choice_and_fresh_revision(self):
        plan = self.call("project.object_migration_preview", scene_id="state_demo", accept_continuous_animation=False)
        self.assertFalse(plan["can_apply"])
        before = self.service._bundle.canonical_bytes()
        with self.assertRaises(ProtocolError):
            self.call("project.object_migration_apply", scene_id="state_demo", accept_continuous_animation=False,
                      source_revision=plan["plan"]["source_revision"])
        with self.assertRaises(ProtocolError):
            self.call("project.object_migration_apply", scene_id="state_demo", accept_continuous_animation=True,
                      source_revision="stale")
        self.assertEqual(before, self.service._bundle.canonical_bytes())

    def test_save_reload_valid_but_egg_export_blocked(self):
        self.migrate()
        self.call("project.save")
        bundle = load_project(self.root)
        self.assertTrue(bundle.valid, bundle.issues)
        self.assertEqual(self.service._bundle.canonical_bytes(), bundle.canonical_bytes())
        with self.assertRaisesRegex(EggCompileError, "firmware support is not implemented"):
            build_egg(bundle)
        hello = self.call("service.hello")
        self.assertFalse(hello["scene_object_authoring"]["egg_export"])
        with self.assertRaises(ProtocolError) as failure:
            self.call("project.build_package")
        self.assertEqual("SCENE_OBJECT_EXECUTABLE_UNAVAILABLE", failure.exception.details["issues"][0]["code"])
        self.call("project.compatibility_report")
        self.call("project.scene_thumbnails")

    def test_mixed_models_reject_wrong_command_and_rollback_entire_batch(self):
        with self.assertRaises(ProtocolError):
            self.edit(self.command("object.set_defaults", object_id="cursor", properties={"x": 20}))
        self.migrate()
        before = self.service._bundle.canonical_bytes()
        revision = self.revision
        with self.assertRaises(ProtocolError):
            self.edit(self.command("object.set_defaults", object_id="cursor", properties={"x": 20}),
                      self.command("state_placement.clear_override", state_id="center", element_ref="cursor", properties=["position"]))
        self.assertEqual(before, self.service._bundle.canonical_bytes())
        self.assertEqual(revision, self.revision)

    def test_independent_axis_clear_preserves_y(self):
        self.migrate()
        self.edit(self.command("object_override.clear", state_id="center", object_id="cursor", properties=["x"]))
        state = next(state for state in self.scene()["states"] if state["state_id"] == "center")
        override = next(item for item in state["object_overrides"] if item["object_ref"] == "cursor")
        self.assertNotIn("x", override)
        self.assertEqual(73, override["y"])
        with self.assertRaises(ProtocolError):
            self.edit(self.command("object_override.clear", state_id="center", object_id="cursor", properties=["position"]))

    def test_add_exact_visibility_set_and_reference_safe_delete(self):
        self.migrate()
        obj = deepcopy(self.scene()["objects"][0])
        obj["object_id"] = "extra"
        self.edit(self.command("object.add", object=obj, visible_in_states=["left", "right"]))
        added = next(item for item in self.scene()["objects"] if item["object_id"] == "extra")
        self.assertFalse(added["defaults"]["visible"])
        self.assertEqual(["left", "right"], [state["state_id"] for state in self.scene()["states"]
                         if any(item.get("visible") and item["object_ref"] == "extra" for item in state["object_overrides"])])
        self.edit(self.command("object_actions.set", owner_kind="route", owner_id="center_to_right",
                               actions=[{"kind": "object.move_by", "object_ref": "extra", "dx": 2}]))
        with self.assertRaises(ProtocolError):
            self.edit(self.command("object.delete", object_id="extra"))

    def test_clip_reference_edits_are_atomic(self):
        self.migrate()
        before = self.service._bundle.canonical_bytes()
        with self.assertRaises(ProtocolError):
            self.edit(self.command("object.bind_animation", object_id="cursor", animation_ref="missing"))
        clip = self.scene()["objects"][0]["animation_ref"]
        with self.assertRaises(ProtocolError):
            self.edit({"kind": "animation.delete", "animation_id": clip})
        self.assertEqual(before, self.service._bundle.canonical_bytes())

    def test_state_change_preserves_phase_and_residual_interval(self):
        self.migrate()
        self.call("project.preview_reset", scene_id="state_demo")
        before = self.call("project.preview_advance", elapsed_ms=375)
        changed = self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertTrue(changed["input"]["accepted"])
        self.assertEqual("right", changed["scene"]["state_id"])
        self.assertEqual(self.cursor(before)["playback"], self.cursor(changed)["playback"])
        self.assertEqual(125, self.cursor(changed)["playback"]["remaining_ms"])
        advanced = self.call("project.preview_advance", elapsed_ms=125)
        self.assertEqual(250, self.cursor(advanced)["playback"]["remaining_ms"])
        self.assertNotEqual(self.cursor(changed)["effective"]["visual_ref"], self.cursor(advanced)["effective"]["visual_ref"])

    def test_underlying_actions_accumulate_under_position_override(self):
        self.migrate()
        self.edit(self.command("object_actions.set", owner_kind="route", owner_id="center_to_right", actions=[
            {"kind": "object.set_position", "object_ref": "cursor", "x": 20, "y": 40},
            {"kind": "object.move_by", "object_ref": "cursor", "dx": 5, "dy": 10},
            {"kind": "object.move_by", "object_ref": "cursor", "dx": 7, "dy": 3},
        ]))
        self.call("project.preview_reset", scene_id="state_demo")
        snapshot = self.call("project.preview_input", logical_source="BUTTON_R")
        cursor = self.cursor(snapshot)
        self.assertEqual((32, 27), (cursor["underlying"]["x"], cursor["underlying"]["y"]))
        self.assertEqual((8, 103), (cursor["effective"]["x"], cursor["effective"]["y"]))

    def test_hidden_playback_and_suspension(self):
        self.migrate()
        self.edit(self.command("object_override.set", object_id="cursor", state_id="center", properties={"visible": False}))
        preview = StateScenePreview(build_preview_package(self.service._bundle), "state_demo")
        preview.advance(375)
        before = preview.snapshot()
        self.assertFalse(self.cursor(before)["effective"]["visible"])
        preview.suspend()
        preview.advance(2000)
        self.assertEqual(before, preview.snapshot())
        preview.resume()
        preview.apply_input("BUTTON_R")
        self.assertTrue(self.cursor(preview.snapshot())["effective"]["visible"])
        self.assertEqual(self.cursor(before)["playback"], self.cursor(preview.snapshot())["playback"])

    def test_scene_timer_handler_moves_underlying_object_without_changing_state(self):
        # Exercise the shared timer compiler/executor, not a hand-invoked object action.
        bundle = self.service._bundle
        scene = deepcopy(self.scene())
        scene["event_bindings"] = [{"binding_id": "tick", "event_type": "time.scene_elapsed",
                                    "configuration": {"delay_ms": 500, "start_policy": "scene_entry"}}]
        scene["event_handlers"] = [{"handler_id": "tick_handler", "event_ref": "tick", "guards": [],
                                    "actions": [{"kind": "request_render"}]}]
        self.service._bundle = replace(bundle, scenes=tuple(scene if item["scene_id"] == scene["scene_id"] else item for item in bundle.scenes))
        self.migrate()
        self.edit(self.command("object_actions.set", owner_kind="handler", owner_id="tick_handler", actions=[
            {"kind": "object.set_position", "object_ref": "cursor", "x": 30}]))
        self.call("project.preview_reset", scene_id="state_demo")
        self.call("project.preview_advance", elapsed_ms=250)
        self.call("project.preview_input", logical_source="BUTTON_R")
        snapshot = self.call("project.preview_advance", elapsed_ms=250)
        self.assertEqual("right", snapshot["scene"]["state_id"])
        self.assertEqual(1, len(snapshot["timer_events"]))
        self.assertEqual(30, self.cursor(snapshot)["underlying"]["x"])
        self.assertEqual(8, self.cursor(snapshot)["effective"]["x"])
        self.assertEqual([], self.call("project.preview_advance", elapsed_ms=1000)["timer_events"])

    def test_frame_masks_do_not_restart_animation_and_clear_reveals_current_phase(self):
        self.migrate()
        frame = self.scene()["objects"][0]["defaults"]["visual_ref"]
        self.edit(self.command("object_actions.set", owner_kind="route", owner_id="center_to_right", actions=[
            {"kind": "object.set_frame", "object_ref": "cursor", "frame_ref": frame},
            {"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": 2}]),
            self.command("object_actions.set", owner_kind="route", owner_id="right_to_left", actions=[
                {"kind": "object.clear_frame", "object_ref": "cursor"}]))
        self.call("project.preview_reset", scene_id="state_demo")
        snapshot = self.call("project.preview_advance", elapsed_ms=125)
        animated_frame = self.cursor(snapshot)["effective"]["visual_ref"]
        self.assertNotEqual(frame, animated_frame)
        masked = self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual(frame, self.cursor(masked)["effective"]["visual_ref"])
        unmasked = self.call("project.preview_input", logical_source="BUTTON_R")
        self.assertEqual(animated_frame, self.cursor(unmasked)["effective"]["visual_ref"])
        self.assertEqual(self.cursor(snapshot)["playback"], self.cursor(unmasked)["playback"])

    def test_late_variable_failure_does_not_commit_earlier_object_write(self):
        self.migrate()
        self.edit(self.command("object_actions.set", owner_kind="route", owner_id="center_to_right", actions=[
            {"kind": "object.move_by", "object_ref": "cursor", "dx": 10},
            {"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 2}]))
        preview = StateScenePreview(build_preview_package(self.service._bundle), "state_demo")
        before = preview.snapshot()
        with self.assertRaises(PreviewError):
            preview.apply_input("BUTTON_R")
        self.assertEqual(before, preview.snapshot())

    def test_scene_recreation_resets_playback_and_mutable_properties(self):
        self.migrate()
        self.call("project.preview_reset", scene_id="state_demo")
        self.call("project.preview_advance", elapsed_ms=375)
        departed = self.call("project.preview_input", logical_source="BUTTON_A")
        self.assertEqual("state_details", departed["scene"]["scene_id"])
        self.assertNotIn("objects", departed)
        # The existing example's B route re-enters the original scene.
        returned = self.call("project.preview_input", logical_source="BUTTON_B")
        self.assertEqual("state_demo", returned["scene"]["scene_id"])
        self.assertEqual(0, returned["timeline"]["elapsed_ms"])
        self.assertEqual(0, self.cursor(returned)["playback"]["phase_index"])

    def test_filled_primitives_use_existing_rasterizer(self):
        self.migrate()
        for kind in ("filled_circle", "filled_ellipse", "line"):
            with self.subTest(kind=kind):
                obj = {"object_id": kind, "kind": kind, "width": 9, "height": 9, "z_order": 3,
                       "layer": "SCENE", "defaults": {"x": 100, "y": 100, "visible": True}}
                self.edit(self.command("object.add", object=obj))
        snapshot = self.call("project.preview_reset", scene_id="state_demo")
        self.assertGreater(snapshot["framebuffer"]["black_pixel_count"], 0)

    def test_full_source_schema_matches_root_validator_fields(self):
        path = Path(__file__).resolve().parents[3] / "schemas/authoring/state-scene-v2.schema.json"
        schema = json.loads(path.read_text(encoding="utf-8"))
        required = (SCENE_KEYS - {"render_models", "waiting_visuals"}) | {"objects"}
        self.assertEqual(required, set(schema["required"]))
        self.assertEqual(required | SCENE_OPTIONAL_KEYS, set(schema["properties"]))

    def test_malformed_object_commands_leave_document_unchanged(self):
        self.migrate()
        before = self.service._bundle.canonical_bytes()
        cases = [
            self.command("object.add", object=deepcopy(self.scene()["objects"][0]), visible_in_states=None),
            self.command("object_override.clear", object_id="cursor", state_id="center", properties=[[]]),
            self.command("object_actions.set", owner_kind="route", owner_id="center_to_right", actions=[{"kind": "object.unknown"}]),
            self.command("object.set_defaults", object_id="cursor", properties={"x": 1000}),
            {"kind": []},
        ]
        for command in cases:
            with self.subTest(command=command):
                with self.assertRaises(ProtocolError):
                    self.edit(command)
                self.assertEqual(before, self.service._bundle.canonical_bytes())

    def test_empty_scene_objects_remain_a_previewable_draft(self):
        self.migrate()
        for obj in list(self.scene()["objects"]):
            self.edit(self.command("object.delete", object_id=obj["object_id"]))
        snapshot = self.call("project.preview_reset", scene_id="state_demo")
        self.assertEqual([], snapshot["objects"])
        self.assertEqual(0, snapshot["framebuffer"]["black_pixel_count"])
        self.call("project.save")
        self.assertTrue(load_project(self.root).valid)


if __name__ == "__main__":
    unittest.main()
