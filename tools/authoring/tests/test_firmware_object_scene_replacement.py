"""Real runtime replacement plus private owner raster admission; no export widening."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_candidate_queue as queue
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


def replacement_bundle(timer_exit=False):
    base = load_project(Path(__file__).resolve().parents[3] /
                        "examples/authoring/native_v2_installation.peepproj")
    scenes = [deepcopy(base.scenes[0]), deepcopy(base.scenes[0])]
    for index, scene in enumerate(scenes):
        scene["scene_id"] = "main" if index == 0 else "second"
        scene["variables"] = [{"variable_id": "hits", "value_type": "int32",
                               "initial": (index + 1) * 10, "minimum": 0, "maximum": 100}]
    route = scenes[0]["routes"][0]
    route.pop("target_state")
    route.update(target_scene="second", actions=[])
    route = scenes[1]["routes"][1]
    route.pop("target_state")
    route.update(target_scene="main", from_states=["start", "marker_right"], actions=[])
    scenes[1]["routes"][0]["actions"] = [
        {"kind": "set_variable", "variable_ref": "hits", "operation": "add", "value": 1}]
    if timer_exit:
        scenes[0]["event_handlers"][0].update(target_scene="second", actions=[])
        scenes[0]["event_bindings"].append({"binding_id": "outgoing_second",
            "event_type": "time.scene_elapsed",
            "configuration": {"delay_ms": 2100, "start_policy": "scene_entry"}})
        scenes[0]["event_handlers"].append({"handler_id": "outgoing_second_handler",
            "event_ref": "outgoing_second", "guards": [], "actions": [
                {"kind": "set_variable", "variable_ref": "hits", "operation": "add", "value": 1}]})
        scenes[0]["reactive_wait_default"]["event_interests"].append("outgoing_second")
        scenes[0]["routes"][1].update(from_states=["start", "marker_right"], actions=[
            {"kind": "restart_timer", "timer_ref": "reveal_timer"}])
    return replace(base, scenes=tuple(scenes))


class ObjectSceneReplacementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        queue.ObjectCandidateQueueTests.setUpClass.__func__(cls)
        cls.exe = cls.work / "replacement.exe"
        result = subprocess.run([os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe"),
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffunction-sections",
            "-fdata-sections", "-Wl,--gc-sections", "-I", str(cls.firmware / "Core/Inc"),
            "-I", str(cls.firmware / "Core/Src"), "-I", str(cls.work),
            str(Path(__file__).with_name("native_object_scene_replacement.c")), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_fresh_replacement_and_failed_admission_preserve_source(self):
        blob = build_development_egg_v2(replacement_bundle())
        path = self.work / "replacement.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path)], capture_output=True,
                                text=True, timeout=20, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_development_helpers_match_new_request_api(self):
        header = (self.firmware / "Core/Inc/ps_hw6_object_development.h").read_text()
        self.assertIn("PS_HW6_OBJECT_DEVELOPMENT_API_VERSION (4UL)", header)
        for name in ("__fw0_object_scene_awake_enable.gdb", "__fw0_object_scene_awake_prints.gdb",
                     "__fw0_object_scene_lpbam_enable.gdb"):
            self.assertIn("g_ps_object_development_probe.api_version != 4",
                          (self.firmware / name).read_text())
