"""Candidate pixels and exact resource checks must leave live display data alone."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_awake as awake
from test_firmware_package_workflow import firmware_function
from build_object_development import DEFAULT_PROJECT, structured_fixture_bundle, dual_fixture_bundle
from peepshow_authoring.project import load_project
from peepshow_authoring.compiler import build_development_egg_v2


class ObjectDisplayAdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        awake.ObjectAwakeTests.setUpClass.__func__(cls)
        renderer = (cls.firmware / "Core/Src/display_renderer.c").read_text(encoding="utf-8")
        (cls.work / "object_display_under_test.inc").write_text("\n".join(
            firmware_function(renderer, name) for name in (
                "DisplayRenderer_DrawSceneModel", "DisplayRenderer_CopySceneModelFrame",
                "DisplayRenderer_CopyCandidateSceneFrame")), encoding="ascii")
        (cls.work / "stm32u5xx_hal.h").write_text(
            "#ifndef TEST_HAL_H\n#define TEST_HAL_H\n"
            "typedef enum {HAL_OK=0, HAL_ERROR=1} HAL_StatusTypeDef;\n#endif\n", encoding="ascii")
        cls.exe = cls.work / "display_admission.exe"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_display_admission.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def check(self, bundle, failure=0):
        # Matching dimensions and IDs, deliberately different pixel content.
        baseline = load_project(DEFAULT_PROJECT.parent / "native_v2_timer_four_frames.peepproj")
        baseline = replace(baseline, frames=tuple(replace(frame, pixels=bytes(b ^ 255 for b in frame.pixels))
                                                 for frame in baseline.frames))
        paths = []
        for name, source in (("baseline", baseline), ("candidate", bundle)):
            blob = build_development_egg_v2(source)
            path = self.work / f"{name}.egg"
            path.write_bytes(blob)
            path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
            paths.append(str(path))
        result = subprocess.run([str(self.exe), *paths, str(failure)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout

    def test_structured_and_dual_all_authored_states_and_timer_visibility(self):
        for factory, steps, chunks, size in ((structured_fixture_bundle, 4, 8, 4672),
                                             (dual_fixture_bundle, 8, 16, 9344)):
            bundle = factory()
            for state in ("start", "marker_right"):
                for visible in (False, True):
                    with self.subTest(steps=steps, state=state, visible=visible):
                        scene = deepcopy(bundle.scenes[0])
                        scene["entry_state"] = state
                        scene["objects"][2]["defaults"]["visible"] = visible
                        output = self.check(replace(bundle, scenes=(scene,)))
                        self.assertIn(f"steps={steps} chunks={chunks} bytes={size}", output)

    def test_hold_and_shared_payloads(self):
        bundle = structured_fixture_bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"][0]["defaults"]["visible"] = False
        self.assertIn("steps=1 chunks=1 bytes=584", self.check(replace(bundle, scenes=(scene,))))

    def test_combined_schedule_rejection_isolated(self):
        bundle = dual_fixture_bundle()
        clips = deepcopy(bundle.animations)
        clips[1]["frame_duration_ms"] = [700] * 4
        self.assertIn("schedule rejection isolated", self.check(replace(bundle, animations=clips), 2))

    def test_payload_overflow_does_not_disturb_live_data(self):
        bundle = structured_fixture_bundle()
        scene = deepcopy(bundle.scenes[0])
        # Spread digit changes over every native band, exceeding 18 transactions.
        for index, x in enumerate((0, 28, 104, 140)):
            obj = deepcopy(scene["objects"][0])
            obj["object_id"] = f"extra{index}"
            obj["defaults"].update(x=x, y=90)
            scene["objects"].append(obj)
        self.check(replace(bundle, scenes=(scene,)), 1)


if __name__ == "__main__":
    unittest.main()
