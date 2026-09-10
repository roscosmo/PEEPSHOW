"""Exact bounded display schedules compared with the live V2 object engine."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_awake as awake
from build_object_development import DEFAULT_PROJECT, dual_fixture_bundle
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


class ObjectWaitingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        awake.ObjectAwakeTests.setUpClass.__func__(cls)
        cls.exe = cls.work / "waiting.exe"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_waiting.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def bundle(self):
        return load_project(DEFAULT_PROJECT.parent / "native_v2_timer_four_frames.peepproj")

    def check(self, bundle, mode=0):
        blob = build_development_egg_v2(bundle)
        path = self.work / "waiting.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        output = self.work / "pixels.bin"
        result = subprocess.run([str(self.exe), str(path), str(output), str(mode)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        if mode != 1:
            pixels = output.read_bytes()
            self.assertEqual(13 * 2 * 3024, len(pixels))
            for offset in range(0, len(pixels), 6048):
                self.assertEqual(pixels[offset:offset + 3024], pixels[offset + 3024:offset + 6048])

    def test_numbered_frames_partial_interval_wrap_and_large_elapsed(self):
        self.check(self.bundle())

    def test_dual_fixture_continuity_and_awkward_timing_rejection(self):
        bundle = dual_fixture_bundle()
        self.check(bundle, 5)
        animations = deepcopy(list(bundle.animations))
        animations[-1]["frame_duration_ms"] = [700] * 4
        self.check(replace(bundle, animations=animations), 1)

    def test_dual_hidden_indicator_does_not_expand_visible_schedule(self):
        bundle = dual_fixture_bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"][-1]["defaults"]["visible"] = False
        self.check(replace(bundle, scenes=(scene,)))

    def test_full_scene_preserves_static_overlay(self):
        bundle = self.bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"].append({"object_id": "overlay", "kind": "filled_rect",
            "width": 10, "height": 24, "layer": "UI", "z_order": 3,
            "defaults": {"x": 79, "y": 40, "visible": True}})
        self.check(replace(bundle, scenes=(scene,)))

    def test_unequal_frame_durations_preserve_exact_repeated_steps(self):
        bundle = self.bundle()
        animations = deepcopy(list(bundle.animations))
        animations[0]["frame_duration_ms"] = [200, 400, 200, 800]
        self.check(replace(bundle, animations=animations), 4)

    def test_hidden_and_static_masked_clips_require_only_hold(self):
        bundle = self.bundle()
        for override in ({"visible": False}, {"visual_ref": "pulse.c"}):
            with self.subTest(override=override):
                scene = deepcopy(bundle.scenes[0])
                scene["states"][0]["object_overrides"] = [{"object_ref": "continuity_sprite", **override}]
                self.check(replace(bundle, scenes=(scene,)), 2)

    def test_multiple_clips_admit_exactly_twelve_steps_or_reject(self):
        bundle = self.bundle()
        scene = deepcopy(bundle.scenes[0])
        second = deepcopy(scene["objects"][0])
        second.update(object_id="second_sprite", animation_ref="second_loop", z_order=3)
        second["defaults"].update(x=20, y=20)
        scene["objects"].append(second)
        animations = deepcopy(list(bundle.animations))
        animations[0]["frame_duration_ms"] = [200] * 4
        animations.append({"animation_id": "second_loop", "loop_policy": "loop",
                           "frame_refs": ["pulse.a", "pulse.b", "pulse.c"],
                           "frame_duration_ms": [400] * 3})
        self.check(replace(bundle, scenes=(scene,), animations=animations), 3)
        animations[1]["frame_duration_ms"] = [500] * 3
        self.check(replace(bundle, scenes=(scene,), animations=animations), 1)


if __name__ == "__main__":
    unittest.main()
