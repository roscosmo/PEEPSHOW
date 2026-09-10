"""V2 full-scene rasterization through the production LPBAM payload compiler."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_awake as awake
from test_firmware_package_workflow import firmware_function
from build_object_development import DEFAULT_PROJECT
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


class ObjectLpbamPrepareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        awake.ObjectAwakeTests.setUpClass.__func__(cls)
        renderer = (cls.firmware / "Core/Src/display_renderer.c").read_text(encoding="utf-8")
        owner = (cls.firmware / "Core/Src/ps_hw6_owner_services.c").read_text(encoding="utf-8")
        (cls.work / "object_lpbam_under_test.inc").write_text("\n".join([
            firmware_function(renderer, "DisplayRenderer_DrawSceneModel"),
            firmware_function(renderer, "DisplayRenderer_CopySceneModelFrame"),
            firmware_function(owner, "PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting")]), encoding="ascii")
        (cls.work / "stm32u5xx_hal.h").write_text(
            "#ifndef TEST_HAL_H\n#define TEST_HAL_H\n"
            "typedef enum {HAL_OK=0, HAL_ERROR=1} HAL_StatusTypeDef;\n#endif\n", encoding="ascii")
        cls.exe = cls.work / "lpbam_prepare.exe"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_lpbam_prepare.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def bundle(self):
        return load_project(DEFAULT_PROJECT.parent / "native_v2_timer_four_frames.peepproj")

    def check(self, bundle, mode=0):
        blob = build_development_egg_v2(bundle)
        path = self.work / "prepare.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path), str(mode)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout

    def test_numbered_scene_payload_replay_wrap_and_lease_guards(self):
        self.check(self.bundle())

    def test_overlapping_static_object_survives_every_packed_frame(self):
        bundle = self.bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"].append({"object_id": "overlay", "kind": "filled_rect",
            "width": 10, "height": 24, "layer": "UI", "z_order": 3,
            "defaults": {"x": 79, "y": 40, "visible": True}})
        self.check(replace(bundle, scenes=(scene,)))

    def test_capacity_rejection_does_not_change_live_scene_or_framebuffer(self):
        bundle = self.bundle()
        scene = deepcopy(bundle.scenes[0])
        for index, x in enumerate((2, 30, 58, 114, 142)):
            obj = deepcopy(scene["objects"][0])
            obj["object_id"] = f"extra{index}"
            obj["defaults"].update(x=x, y=70)
            scene["objects"].append(obj)
        self.check(replace(bundle, scenes=(scene,)), 1)

    def test_hidden_animation_packs_an_unchanged_hold(self):
        bundle = self.bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"][0]["defaults"]["visible"] = False
        self.check(replace(bundle, scenes=(scene,)), 2)


if __name__ == "__main__":
    unittest.main()
