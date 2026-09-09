from __future__ import annotations

from copy import deepcopy
from dataclasses import replace
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.compiler import EggCompileError, build_egg
from peepshow_authoring.project import load_project
from peepshow_authoring.scene_objects import (
    OBJECT_ACTION_FIELDS, OBJECT_KINDS, OBJECT_OPTIONAL, OBJECT_REQUIRED,
    PROPERTY_KEYS, SceneObjectError, apply_object_actions, clear_object_override,
    initialize_objects, materialize_object_migration, plan_object_migration,
    resolve_object, validate_object_model,
)
from peepshow_authoring.service import SERVICE_API_VERSION


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tools/authoring/peepshow_authoring/test_project.peepproj"


class SceneObjectModelTests(unittest.TestCase):
    def setUp(self):
        self.frames = {f"pet.f{i}": (8, 16) for i in range(4)}
        self.clips = {"pet.idle": {"animation_id": "pet.idle", "frame_refs": list(self.frames),
                                   "frame_duration_ms": [250] * 4, "loop_policy": "loop"}}
        self.obj = {"object_id": "pet", "kind": "sprite", "width": 8, "height": 16,
                    "z_order": 1, "layer": "SCENE", "animation_ref": "pet.idle",
                    "defaults": {"x": 10, "y": 20, "visible": True, "visual_ref": "pet.f0"}}
        self.model = {"objects": [self.obj], "states": [
            {"state_id": "a", "display_name": "A", "object_overrides": []},
            {"state_id": "b", "display_name": "B", "object_overrides": [{"object_ref": "pet", "x": 100}]},
        ]}
        self.live = initialize_objects(self.model["objects"])

    def run_actions(self, actions, live=None):
        return apply_object_actions(self.model["objects"], self.live if live is None else live, actions, self.frames)

    def action(self, kind, **values):
        return {"kind": kind, "object_ref": "pet", **values}

    def test_valid_model_and_empty_draft_objects(self):
        self.assertEqual((), validate_object_model(self.model, self.frames, self.clips))
        draft = {"objects": [], "states": [self.model["states"][0]]}
        self.assertEqual((), validate_object_model(draft, {}, {}))

    def test_default_frame_does_not_mask_clip_and_overrides_do_not_reset_it(self):
        obj, live = self.obj, self.live["pet"]
        for elapsed, phase in [(375, 1), (425, 1), (500, 2)]:
            visual = resolve_object(obj, live, {"x": 100}, self.clips, elapsed)
            self.assertEqual(f"pet.f{phase}", visual["visual_ref"])
            self.assertEqual(100, visual["x"])
        self.assertEqual(10, live["x"])
        self.assertIsNone(live["static_frame_ref"])

    def test_persistent_mask_and_temporary_mask_are_separate(self):
        staged = self.run_actions([self.action("object.set_frame", frame_ref="pet.f0")])
        self.assertEqual("pet.f0", resolve_object(self.obj, staged["pet"], {}, self.clips, 875)["visual_ref"])
        self.assertEqual("pet.f2", resolve_object(self.obj, staged["pet"], {"visual_ref": "pet.f2"}, self.clips, 875)["visual_ref"])
        cleared = self.run_actions([self.action("object.clear_frame")], staged)
        self.assertEqual("pet.f3", resolve_object(self.obj, cleared["pet"], {}, self.clips, 875)["visual_ref"])
        self.assertEqual("pet.f0", staged["pet"]["static_frame_ref"])

    def test_static_object_clear_reveals_authored_default(self):
        self.obj.pop("animation_ref")
        staged = self.run_actions([self.action("object.set_frame", frame_ref="pet.f2"), self.action("object.clear_frame")])
        self.assertEqual("pet.f0", resolve_object(self.obj, staged["pet"], {}, self.clips, 875)["visual_ref"])

    def test_relative_actions_accumulate_underneath_override(self):
        staged = self.run_actions([self.action("object.move_by", dx=5), self.action("object.move_by", dx=7, dy=3)])
        self.assertEqual((22, 17), (staged["pet"]["x"], staged["pet"]["y"]))
        effective = resolve_object(self.obj, staged["pet"], {"x": 100}, self.clips, 375)
        self.assertEqual((100, 17), (effective["x"], effective["y"]))
        self.assertEqual(22, resolve_object(self.obj, staged["pet"], {}, self.clips, 375)["x"])
        self.assertEqual((10, 20), (self.live["pet"]["x"], self.live["pet"]["y"]))

    def test_absolute_y_stays_down_and_relative_y_is_up(self):
        staged = self.run_actions([self.action("object.set_position", y=80), self.action("object.move_by", dy=5)])
        self.assertEqual((10, 75), (staged["pet"]["x"], staged["pet"]["y"]))

    def test_clamp_whole_object_after_each_ordered_write(self):
        staged = self.run_actions([self.action("object.move_by", dx=2147483647, dy=-2147483648)])
        self.assertEqual((160, 128), (staged["pet"]["x"], staged["pet"]["y"]))
        staged = self.run_actions([self.action("object.move_by", dx=-5, dy=7)], staged)
        self.assertEqual((155, 121), (staged["pet"]["x"], staged["pet"]["y"]))
        staged = self.run_actions([self.action("object.set_position", x=-100, y=-100)], staged)
        self.assertEqual((0, 0), (staged["pet"]["x"], staged["pet"]["y"]))

    def test_invalid_second_action_does_not_commit_first(self):
        before = deepcopy(self.live)
        with self.assertRaises(SceneObjectError):
            self.run_actions([self.action("object.move_by", dx=9), self.action("object.set_frame", frame_ref="missing")])
        self.assertEqual(before, self.live)

    def test_invalid_actions_and_legacy_actions_are_rejected(self):
        invalid = [
            self.action("object.move_by"), self.action("object.move_by", dx=True),
            self.action("object.move_by", dx=2147483648),
            self.action("object.move_by", dx=1, target_state="b"),
            self.action("object.set_visibility", visible=1),
            self.action("object.set_frame", frame_ref=None),
            {"kind": "object.clear_frame", "object_ref": "other_scene.pet"},
            {"kind": "set_element_position", "element_ref": "pet", "x": 1, "y": 2},
            self.action("object.restart"), self.action("object.set_clip", animation_ref="pet.idle"),
        ]
        for action in invalid:
            with self.subTest(action=action), self.assertRaises(SceneObjectError):
                self.run_actions([action])

    def test_independent_axis_clear_preserves_other_properties(self):
        state = deepcopy(self.model["states"][1])
        state["object_overrides"][0].update(y=80, visible=False)
        cleared = clear_object_override(state, "pet", ["x"])
        self.assertEqual([{"object_ref": "pet", "y": 80, "visible": False}], cleared["object_overrides"])
        self.assertEqual(100, state["object_overrides"][0]["x"])
        self.assertEqual([], clear_object_override(cleared, "pet", ["y", "visible"])["object_overrides"])
        with self.assertRaises(SceneObjectError):
            clear_object_override(state, "pet", ["position"])

    def test_visibility_changes_do_not_pause_hidden_clip(self):
        hidden = self.run_actions([self.action("object.set_visibility", visible=False)])
        shown = self.run_actions([self.action("object.set_visibility", visible=True)], hidden)
        self.assertEqual("pet.f3", resolve_object(self.obj, shown["pet"], {}, self.clips, 875)["visual_ref"])

    def test_shared_asset_instances_do_not_share_mutable_state(self):
        other = deepcopy(self.obj)
        other["object_id"] = "other"
        self.model["objects"].append(other)
        self.live = initialize_objects(self.model["objects"])
        staged = self.run_actions([self.action("object.move_by", dx=5), self.action("object.set_frame", frame_ref="pet.f0")])
        self.assertEqual(10, staged["other"]["x"])
        self.assertEqual("pet.f1", resolve_object(other, staged["other"], {}, self.clips, 375)["visual_ref"])

    def test_validation_rejects_bad_references_geometry_and_sparse_records(self):
        mutations = [
            lambda m: m["objects"].append(deepcopy(m["objects"][0])),
            lambda m: m["objects"][0]["defaults"].update(x=True),
            lambda m: m["objects"][0]["defaults"].update(y=129),
            lambda m: m["objects"][0].update(animation_ref="missing"),
            lambda m: m["objects"][0].update(width=7),
            lambda m: m["objects"][0].update(kind="filled_circle", width=8, height=8),
            lambda m: m["states"][1]["object_overrides"].append({"object_ref": "pet", "y": 5}),
            lambda m: m["states"][1]["object_overrides"].append({"object_ref": "missing", "x": 5}),
            lambda m: m["states"][1]["object_overrides"][0].update(animation_ref="pet.idle"),
            lambda m: m["states"][1]["object_overrides"][0].pop("x"),
            lambda m: m.update(render_models=[]),
        ]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                model = deepcopy(self.model)
                mutate(model)
                self.assertTrue(validate_object_model(model, self.frames, self.clips))

    def test_schema_definition_field_sets_match_backend(self):
        schema = json.loads((ROOT / "schemas/authoring/scene-object-model-v2.schema.json").read_text())
        definitions = schema["$defs"]
        self.assertEqual(OBJECT_REQUIRED, set(definitions["object"]["required"]))
        self.assertEqual(OBJECT_REQUIRED | OBJECT_OPTIONAL, set(definitions["object"]["properties"]))
        self.assertEqual(OBJECT_KINDS, set(definitions["object"]["properties"]["kind"]["enum"]))
        self.assertEqual(PROPERTY_KEYS, set(definitions["defaults"]["properties"]))
        self.assertEqual(PROPERTY_KEYS | {"object_ref"}, set(definitions["override"]["properties"]))
        action_schemas = {item["properties"]["kind"]["const"]: item for item in definitions["action"]["oneOf"]}
        self.assertEqual(set(OBJECT_ACTION_FIELDS), set(action_schemas))
        for kind, (required, optional) in OBJECT_ACTION_FIELDS.items():
            self.assertEqual(required | {"kind", "object_ref"}, set(action_schemas[kind]["required"]))
            self.assertEqual(required | optional | {"kind", "object_ref"}, set(action_schemas[kind]["properties"]))

    def test_malformed_json_types_report_issues_without_exceptions(self):
        for field in ("kind", "layer", "focus_role", "line_direction", "animation_ref", "defaults", "width"):
            for value in ([], {}, None):
                with self.subTest(field=field, value=value):
                    model = deepcopy(self.model)
                    model["objects"][0][field] = value
                    self.assertTrue(validate_object_model(model, self.frames, self.clips))
        for field in ("objects", "states"):
            for value in (None, {}, [None], [{}]):
                model = deepcopy(self.model)
                model[field] = value
                self.assertTrue(validate_object_model(model, self.frames, self.clips))


class SceneObjectMigrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bundle = load_project(FIXTURE)
        if not cls.bundle.valid:
            raise AssertionError(cls.bundle.issues)

    def modified_bundle(self, mutate):
        scenes = deepcopy(self.bundle.scenes)
        scene = next(scene for scene in scenes if scene["scene_id"] == "state_demo")
        mutate(scene)
        return replace(self.bundle, scenes=scenes)

    def test_migration_requires_explicit_continuity_choice_and_never_writes(self):
        before = self.bundle.canonical_bytes()
        plan = plan_object_migration(self.bundle, "state_demo")
        self.assertIn("MIGRATION_CONTINUITY_CHOICE_REQUIRED", {issue.code for issue in plan.issues})
        with self.assertRaises(SceneObjectError):
            materialize_object_migration(self.bundle, plan)
        accepted = plan_object_migration(self.bundle, "state_demo", accept_continuous_animation=True)
        self.assertEqual((), accepted.issues)
        candidate, clips = materialize_object_migration(self.bundle, accepted, accept_continuous_animation=True)
        self.assertEqual(2, candidate["schema_version"])
        self.assertNotIn("render_models", candidate)
        self.assertNotIn("waiting_visuals", candidate)
        self.assertEqual(["cursor", "marker"], [obj["object_id"] for obj in candidate["objects"]])
        self.assertTrue(clips)
        self.assertEqual({"object_ref": "cursor", "x": 8, "y": 43}, candidate["states"][0]["object_overrides"][0])
        self.assertNotIn("waiting_visual_ref", candidate["reactive_wait_default"])
        self.assertEqual(before, self.bundle.canonical_bytes())

    def test_stale_source_or_modified_plan_cannot_apply_unreviewed_content(self):
        plan = plan_object_migration(self.bundle, "state_demo", accept_continuous_animation=True)
        modified = self.modified_bundle(lambda scene: scene.update(display_name="Changed"))
        with self.assertRaisesRegex(SceneObjectError, "MIGRATION_SOURCE_CHANGED"):
            materialize_object_migration(modified, plan, accept_continuous_animation=True)
        plan.scene["objects"].clear()
        candidate, _ = materialize_object_migration(self.bundle, plan, accept_continuous_animation=True)
        self.assertEqual(2, len(candidate["objects"]))

    def test_different_state_clip_is_not_silently_collapsed(self):
        def change(scene):
            wait = deepcopy(scene["waiting_visuals"][0])
            wait["waiting_visual_id"] = "different"
            wait["elements"][0]["step_phase_indices"] = [0] * 6
            scene["waiting_visuals"].append(wait)
            scene["states"][0]["waiting_visual_ref"] = "different"
        bundle = self.modified_bundle(change)
        plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
        self.assertIn("MIGRATION_ANIMATED_OVERRIDE_UNSUPPORTED", {issue.code for issue in plan.issues})
        with self.assertRaises(SceneObjectError):
            materialize_object_migration(bundle, plan, accept_continuous_animation=True)

    def test_destination_binding_actions_need_separate_migration(self):
        def change(scene):
            scene["routes"][0]["actions"].append({"kind": "set_element_position", "element_ref": "cursor", "x": 1, "y": 2})
        bundle = self.modified_bundle(change)
        plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
        self.assertIn("MIGRATION_ACTION_CHOICE_REQUIRED", {issue.code for issue in plan.issues})
        with self.assertRaises(SceneObjectError):
            materialize_object_migration(bundle, plan, accept_continuous_animation=True)

    def test_existing_egg_output_is_unchanged_by_migration_planning(self):
        before = build_egg(self.bundle)
        plan_object_migration(self.bundle, "state_demo", accept_continuous_animation=True)
        self.assertEqual(before, build_egg(self.bundle))
        self.assertEqual(39, SERVICE_API_VERSION)

    def test_multiple_tracks_for_one_object_require_a_choice(self):
        def change(scene):
            track = deepcopy(scene["waiting_visuals"][0]["elements"][0])
            track["element_id"] = "second_track"
            scene["waiting_visuals"][0]["elements"].append(track)
        bundle = self.modified_bundle(change)
        plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
        self.assertIn("MIGRATION_MULTIPLE_TRACKS_UNSUPPORTED", {issue.code for issue in plan.issues})
        with self.assertRaises(SceneObjectError):
            materialize_object_migration(bundle, plan, accept_continuous_animation=True)

    def test_empty_legacy_override_does_not_become_an_active_controller(self):
        def change(scene):
            scene["states"][0]["placement_overrides"] = [{"element_ref": "cursor"}]
        bundle = self.modified_bundle(change)
        plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
        self.assertEqual((), plan.issues)
        candidate, _ = materialize_object_migration(bundle, plan, accept_continuous_animation=True)
        self.assertEqual([], candidate["states"][0]["object_overrides"])

    def test_candidate_without_its_migrated_clips_is_invalid(self):
        plan = plan_object_migration(self.bundle, "state_demo", accept_continuous_animation=True)
        candidate, _ = materialize_object_migration(self.bundle, plan, accept_continuous_animation=True)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "candidate.peepproj"
            shutil.copytree(FIXTURE, root)
            (root / "scenes/state_demo.state.json").write_text(json.dumps(candidate), encoding="utf-8")
            bundle = load_project(root)
            self.assertFalse(bundle.valid)
            self.assertIn("OBJECT_ANIMATION_INVALID", {issue.code for issue in bundle.issues})
            with self.assertRaises(EggCompileError):
                build_egg(bundle)

    def test_static_migration_preserves_exact_visibility_set_without_consent(self):
        def change(scene):
            for wait in scene["waiting_visuals"]:
                wait["elements"] = []
            scene["render_models"][0]["elements"][0].update(visible=False, focus_role="none")
            scene["states"][0]["placement_overrides"][0]["visible"] = True
            scene["states"][2]["placement_overrides"][0]["visible"] = True
        bundle = self.modified_bundle(change)
        plan = plan_object_migration(bundle, "state_demo")
        self.assertEqual((), plan.issues)
        candidate, clips = materialize_object_migration(bundle, plan)
        self.assertEqual((), clips)
        self.assertFalse(candidate["objects"][0]["defaults"]["visible"])
        self.assertEqual([True, None, True], [state["object_overrides"][0].get("visible") for state in candidate["states"]])


if __name__ == "__main__":
    unittest.main()
