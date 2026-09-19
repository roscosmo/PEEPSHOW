"""Restricted public build/parser/service boundary, independent of draft preview."""
import base64
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from build_object_development import dual_fixture_bundle, sfx_fixture_bundle, scene_exit_fixture_bundle
from peepshow_authoring.compiler import build_egg, build_development_egg_v2, build_readiness_issues, EggCompileError
from peepshow_authoring.egg_format import parse_egg, EggFormatError
from peepshow_authoring.project import load_project
from peepshow_authoring.protocol import ServiceRequest, ProtocolError
from peepshow_authoring.service import AuthoringService
from peepshow_authoring.v2_export import LIMITS, PROFILE_ID, package_issues, package_admission

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "examples/authoring/native_v2_installation.peepproj"


class V2ExportTests(unittest.TestCase):
    def setUp(self):
        self.bundle = load_project(FIXTURE)
        self.blob = build_development_egg_v2(self.bundle)
        self.package = parse_egg(self.blob, _development_v2=True)

    def codes(self, package=None, size=None):
        return {issue["code"] for issue in package_issues(
            package or self.package, len(self.blob) if size is None else size)}

    def edited_scene(self):
        scene = deepcopy(self.package.scenes[0])
        return scene, replace(self.package, scenes=(scene,))

    def test_service_build_exact_hardware_fixture_and_capabilities(self):
        service = AuthoringService()
        hello = service.handle(ServiceRequest("hello", "service.hello", {}))
        self.assertEqual(53, hello["service_api_version"])
        self.assertTrue(hello["scene_object_authoring"]["multi_scene_export"])
        self.assertEqual(6, hello["package_export"]["v2_profile"]["profile_revision"])
        self.assertEqual(PROFILE_ID, hello["package_export"]["v2_profile"]["profile_id"])
        loaded = service.handle(ServiceRequest("load", "project.load", {"path": str(FIXTURE)}))
        self.assertEqual([], loaded["build_issues"])
        self.assertTrue(loaded["scene_capabilities"]["main"]["export_ready"])
        params = {"project_revision": loaded["project_revision"]}
        before = service._bundle.canonical_bytes()
        result = service.handle(ServiceRequest("build", "project.build_package", params))
        blob = base64.b64decode(result["package"]["blob_base64"])
        self.assertEqual(self.blob, blob)
        self.assertEqual(2196, len(blob))
        self.assertEqual(2, result["package"]["container_version"])
        self.assertEqual(PROFILE_ID, result["package"]["export_profile_id"])
        self.assertEqual(hashlib.sha256(blob).hexdigest(), result["package"]["sha256"])
        report = result["compatibility_report"]
        self.assertEqual(2, report["package"]["package_container_version"])
        self.assertIn("peepshow.authoring.state_scene:2", report["generated_by"]["schema_versions"])
        self.assertEqual(65536, report["budgets"]["package_size"]["limit_bytes"])
        budget = report["budgets"]["waiting_visual_sequences"]["analysis"]
        self.assertEqual((4, 400, 8, 4672), tuple(budget[k] for k in (
            "combined_steps", "quantum_ms", "chunks_upper_bound", "payload_bytes_upper_bound")))
        self.assertEqual(before, service._bundle.canonical_bytes())
        self.assertEqual(2, parse_egg(blob).scenes[0]["execution_model"])
        self.assertEqual(result, service.handle(ServiceRequest("again", "project.build_package", params)))
        with self.assertRaises(ProtocolError):
            service.handle(ServiceRequest("stale", "project.build_package", {"project_revision": params["project_revision"] - 1}))

    def test_audio_does_not_admit_shell_actions_or_mixed_projects(self):
        audio = sfx_fixture_bundle()
        self.assertIn("V2_ACTION_UNSUPPORTED", {i["code"] for i in build_readiness_issues(audio)})
        mixed = replace(self.bundle, scenes=(*self.bundle.scenes, *self.bundle.scenes))
        self.assertIn("V2_SCENE_PROFILE", {i["code"] for i in build_readiness_issues(mixed)})
        with self.assertRaises(EggCompileError):
            build_egg(mixed)
        with self.assertRaises(EggFormatError):
            parse_egg(build_development_egg_v2(audio))

    def scene_set(self, count=2):
        scenes = []
        for index in range(count):
            scene = deepcopy(self.bundle.scenes[0])
            scene["scene_id"] = f"scene_{index}"
            if count > 1:
                route = scene["routes"][0]
                route.pop("target_state")
                route["target_scene"] = f"scene_{(index + 1) % count}"
            scenes.append(scene)
        project = deepcopy(self.bundle.project)
        project.update(entry_scene=scenes[-1]["scene_id"],
                       scene_sources=[f"scenes/{s['scene_id']}.state.json" for s in scenes])
        return replace(self.bundle, project=project, scenes=tuple(scenes))

    def test_multiscene_public_build_roundtrip_and_nonfirst_entry(self):
        for bundle in (self.scene_set(8), scene_exit_fixture_bundle()):
            with self.subTest(entry=bundle.project["entry_scene"]):
                self.assertEqual([], build_readiness_issues(bundle))
                blob = build_egg(bundle)
                self.assertEqual(build_development_egg_v2(bundle), blob)
                package = parse_egg(blob)
                self.assertEqual(bundle.project["entry_scene"], package.manifest["entry_scene"])
                admission = package_admission(package, len(blob))
                self.assertEqual([], admission["issues"])
                self.assertIsNone(admission["animation_budget"])
                self.assertEqual({s["scene_id"] for s in bundle.scenes}, set(admission["scene_animation_budgets"]))
        self.assertIn("V2_SCENE_PROFILE", {i["code"] for i in build_readiness_issues(self.scene_set(9))})
        self.assertIn("V2_CAPACITY", {i["code"] for i in package_issues(
            parse_egg(build_development_egg_v2(self.scene_set(9)), _development_v2=True), 65536)})
        mixed = deepcopy(list(self.scene_set().scenes))
        mixed[1]["schema_version"] = 1
        self.assertIn("V2_SCENE_PROFILE", {i["code"] for i in build_readiness_issues(
            replace(self.scene_set(), scenes=tuple(mixed)))})

    def test_every_scene_has_independent_animation_and_graph_budgets(self):
        blob = build_egg(self.scene_set())
        package = parse_egg(blob)
        scenes = deepcopy(package.scenes)
        clips = deepcopy(list(package.animations))
        clips.append(deepcopy(clips[0]))
        clips[1]["frame_duration_ms"] = (250,) * 4
        scenes[1]["objects"][0]["clip_index"] = 1
        package = replace(package, scenes=scenes, animations=tuple(clips))
        admission = package_admission(package, len(blob))
        self.assertEqual([], admission["issues"])
        self.assertEqual([400, 250], [b["quantum_ms"] for b in admission["scene_animation_budgets"].values()])
        # Remove incoming edges: an unreachable bad scene must still block export.
        scenes[0]["graph"]["routes"][0].update(target_scene=None, target_state_index=0)
        for field, limit in (("state_count", 8), ("variable_count", 8), ("binding_count", 16)):
            with self.subTest(field=field):
                altered = deepcopy(scenes)
                altered[1]["graph"][field] = limit + 1
                issues = package_issues(replace(package, scenes=altered), len(blob))
                self.assertTrue(any(i["code"] == "V2_CAPACITY" and i["scene_id"] == "scene_1" for i in issues))
        clips[1]["frame_duration_ms"] = (251,) * 4
        issues = package_issues(replace(package, animations=tuple(clips)), len(blob))
        self.assertTrue(any(i["code"] == "V2_ANIMATION_TIMING" and i["scene_id"] == "scene_1" for i in issues))

    def test_cross_scene_exits_reject_self_targets_and_non_sfx_actions(self):
        for target, actions in (("scene_0", []), ("missing", []),
                                ("scene_1", [{"kind": 1}]), ("scene_1", [{"kind": 8}]),
                                ("scene_1", [{"kind": 9}]), ("scene_1", [{"kind": 10}]),
                                ("scene_1", [{"kind": 11}]), ("scene_1", [{"kind": 12}]),
                                ("scene_1", [{"kind": 7}, {"kind": 1}])):
            package = parse_egg(build_egg(self.scene_set()))
            scenes = deepcopy(package.scenes)
            scenes[0]["graph"]["routes"][0].update(target_scene=target, operations=actions)
            self.assertIn("V2_SCENE_EXIT_UNSUPPORTED", {i["code"] for i in package_issues(
                replace(package, scenes=scenes), 65536)})

    def test_multiscene_service_readiness_and_report_are_whole_project(self):
        service = AuthoringService()
        loaded = service.handle(ServiceRequest("load", "project.load", {"path": str(FIXTURE)}))
        service._bundle = self.scene_set()
        params = {"project_revision": loaded["project_revision"]}
        normalized = service.handle(ServiceRequest("normalize", "project.normalize", params))
        for caps in normalized["scene_capabilities"].values():
            self.assertTrue(caps["multi_scene_export"] and caps["export_ready"])
            self.assertEqual("whole_project", caps["export_readiness_scope"])
        built = service.handle(ServiceRequest("build", "project.build_package", params))
        self.assertEqual(build_egg(service._bundle), base64.b64decode(built["package"]["blob_base64"]))
        report = built["compatibility_report"]["budgets"]["waiting_visual_sequences"]
        self.assertEqual("per_scene", report["scope"])
        self.assertIsNone(report["analysis"])
        self.assertEqual({"scene_0", "scene_1"}, set(report["scene_analyses"]))
        scenes = deepcopy(service._bundle.scenes)
        scenes[1]["objects"].extend([{**deepcopy(scenes[1]["objects"][1]), "object_id": f"extra_{i}"} for i in range(13)])
        service._bundle = replace(service._bundle, scenes=scenes)
        normalized = service.handle(ServiceRequest("bad", "project.normalize", params))
        self.assertTrue(build_readiness_issues(service._bundle))
        self.assertTrue(all(not caps["export_ready"] for caps in normalized["scene_capabilities"].values()))
        with self.assertRaises(ProtocolError):
            service.handle(ServiceRequest("blocked", "project.build_package", params))

    def test_shipping_block_does_not_change_draft_validity(self):
        project = deepcopy(self.bundle.project)
        project.setdefault("validation", {})["build_profile"] = "shipping"
        bundle = replace(self.bundle, project=project)
        self.assertTrue(bundle.valid)
        self.assertIn("V2_SHIPPING_UNAVAILABLE", {i["code"] for i in build_readiness_issues(bundle)})
        with self.assertRaises(EggCompileError):
            build_egg(bundle)

    def test_capacity_includes_footer(self):
        self.assertEqual(set(), self.codes(size=65536))
        self.assertIn("V2_CAPACITY", self.codes(size=65537))

    def test_graph_budgets_count_expanded_sources(self):
        for field, maximum in (("state_count", 8), ("variable_count", 8), ("binding_count", 16)):
            with self.subTest(field=field):
                scene, package = self.edited_scene()
                scene["graph"][field] = maximum
                self.assertNotIn("V2_CAPACITY", self.codes(package))
                scene["graph"][field] += 1
                self.assertIn("V2_CAPACITY", self.codes(package))
        scene, package = self.edited_scene()
        scene["graph"]["routes"][0]["source_state_indexes"] = tuple(range(16))
        self.assertIn("V2_CAPACITY", self.codes(package))
        for field, count, record in (("guards", 17, {}), ("operations", 33, {"kind": 1})):
            scene, package = self.edited_scene()
            scene["graph"]["routes"][0][field] = (record,) * count
            self.assertIn("V2_CAPACITY", self.codes(package))

    def test_reject_actions_interaction_and_events(self):
        for kind in (2, 8):
            scene, package = self.edited_scene()
            scene["graph"]["routes"][0]["operations"] = ({"kind": kind},)
            self.assertIn("V2_ACTION_UNSUPPORTED", self.codes(package))
        scene, package = self.edited_scene()
        scene["graph"]["interaction_mode"] = 2
        self.assertIn("V2_INTERACTION_UNSUPPORTED", self.codes(package))
        scene, package = self.edited_scene()
        scene["graph"]["bindings"][0]["event_class"] = 3
        self.assertIn("V2_EVENT_UNSUPPORTED", self.codes(package))

    def test_hidden_clip_and_static_masks_do_not_evade_budget(self):
        scene, package = self.edited_scene()
        scene["objects"][0]["flags"] = 0
        scene["state_overrides"] = (({"object_index": 0, "mask": 4, "frame_index": 0},),)
        animations = deepcopy(package.animations)
        animations[0]["frame_indexes"] = (0, 1, 2, 3) * 4
        animations[0]["frame_duration_ms"] = (400,) * 16
        package = replace(package, animations=animations)
        self.assertIn("V2_CAPACITY", self.codes(package))

    def test_distinct_frames_and_object_capacity(self):
        scene, package = self.edited_scene()
        scene["objects"] = scene["objects"] * 2
        self.assertIn("V2_CAPACITY", self.codes(package))
        clips = deepcopy(self.package.animations)
        clips[0]["frame_indexes"] = (0, 1, 2, 3, 4)
        clips[0]["frame_duration_ms"] = (400,) * 5
        self.assertIn("V2_CAPACITY", self.codes(replace(self.package, animations=clips)))

    def test_timing_is_not_rounded(self):
        for duration in (251, 2001, 60000):
            clips = deepcopy(self.package.animations)
            clips[0]["frame_duration_ms"] = (duration,) * 4
            self.assertIn("V2_ANIMATION_TIMING", self.codes(replace(self.package, animations=clips)))

    def test_dual_fixture_budget_and_awkward_mix(self):
        blob = build_egg(dual_fixture_bundle())
        package = parse_egg(blob)
        budget = package_admission(package, len(blob))["animation_budget"]
        self.assertEqual((8, 400, 16, 9344), tuple(budget[k] for k in (
            "combined_steps", "quantum_ms", "chunks_upper_bound", "payload_bytes_upper_bound")))
        clips = deepcopy(package.animations)
        clips[0]["frame_duration_ms"] = (250,) * 4
        clips[1]["frame_duration_ms"] = (400,) * 4
        self.assertIn("V2_LPBAM_BUDGET", self.codes(replace(package, animations=clips)))

    def test_horizontal_motion_expands_bounds_vertical_motion_does_not(self):
        scene, package = self.edited_scene()
        baseline = package_admission(package, len(self.blob))["animation_budget"]
        scene["object_operations"] = ({"object_index": 0, "axis_mask": 2, "opcode": 2, "x": 0, "y": 1},)
        self.assertEqual(baseline, package_admission(package, len(self.blob))["animation_budget"])
        scene["object_operations"] = ({"object_index": 0, "axis_mask": 1, "opcode": 2, "x": 1, "y": 0},)
        self.assertIn("V2_LPBAM_BUDGET", self.codes(package))
        scene["object_operations"] = ({"object_index": 0, "axis_mask": 1, "opcode": 1, "x": 999, "y": 0},)
        budget = package_admission(package, len(self.blob))["animation_budget"]
        self.assertEqual(3, budget["panel_bands"])

    def test_firmware_capacity_constants_match_host_subset(self):
        inc = ROOT / "firmware/peepshow_hw6_fw0/Core/Inc"
        for file, macro, key in (
            ("ps_egg_state_loader.h", "PS_EGG_STATE_LOADER_SCENE_MAX", "scenes"),
            ("ps_scene_runtime.h", "PS_SCENE_RUNTIME_STATE_MAX", "states"),
            ("ps_scene_runtime.h", "PS_SCENE_RUNTIME_VARIABLE_MAX", "variables"),
            ("ps_scene_runtime.h", "PS_SCENE_RUNTIME_GUARD_MAX", "guards"),
            ("ps_scene_runtime.h", "PS_SCENE_RUNTIME_ACTION_MAX", "actions"),
            ("ps_scene_runtime.h", "PS_SCENE_RUNTIME_TRANSITION_MAX", "transitions"),
            ("ps_scene_render_model.h", "PS_SCENE_RENDER_MODEL_ELEMENT_MAX", "objects"),
            ("ps_scene_waiting_visual.h", "PS_SCENE_WAITING_VISUAL_PHASE_MAX", "clip_distinct_frames"),
            ("ps_scene_waiting_visual.h", "PS_SCENE_WAITING_VISUAL_ELEMENT_MAX", "animated_objects"),
            ("ps_scene_waiting_visual.h", "PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX", "combined_steps"),
            ("ps_lpbam_display_buffers.h", "PS_LPBAM_DISPLAY_MAX_CHUNKS", "chunks"),
            ("ps_lpbam_display_buffers.h", "PS_LPBAM_DISPLAY_SPATIAL_ROWS", "band_rows"),
        ):
            value = re.search(r"#define\s+" + macro + r"\s+\(?(\d+)U", (inc / file).read_text())[1]
            self.assertEqual(LIMITS[key], int(value), macro)


if __name__ == "__main__":
    unittest.main()
