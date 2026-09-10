"""Real V2 decoder/graph + extracted production scheduler and render completion."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import re
import subprocess
import unittest

import test_firmware_object_awake as awake
from test_firmware_package_workflow import firmware_function
from test_firmware_shape_primitives import panel_pixels
from build_object_development import DEFAULT_PROJECT, timer_fixture_bundle
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


class ObjectTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        awake.ObjectAwakeTests.setUpClass.__func__(cls)
        source = (cls.firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        start = source.index("typedef struct\n{\n  uint32_t configured;")
        end = source.index("static uint32_t ps_runtime_rtc_wake_source;", start)
        declarations = source[start:end]
        functions = "\n".join(firmware_function(source, "PS_HW6_RTOS_" + name) for name in (
            "ObjectAdvance", "CompleteStateSceneEvent", "RuntimeStateTimersClear",
            "RuntimeStateTimersSync", "RuntimeStateTimerNext", "RuntimeStateTimersPause",
            "RuntimeStateTimersResume", "RuntimeStateTimersService"))
        fields = sorted(set(re.findall(r"g_ps_hw6_rtos_probe\.(\w+)", functions)))
        probe = "static struct {\n" + "".join(f"uint32_t {field};\n" for field in fields)
        probe += "} g_ps_hw6_rtos_probe;\n"
        (cls.work / "object_timer_probe_under_test.inc").write_text(probe, encoding="ascii")
        (cls.work / "object_timers_under_test.inc").write_text(declarations + functions, encoding="ascii")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        cls.timer_exe = cls.work / "object_timers.exe"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_timers.c")),
            "-o", str(cls.timer_exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_timer(self, mode, scene=None, bundle=None):
        if bundle is None:
            bundle = timer_fixture_bundle()
        if scene is not None:
            bundle = replace(bundle, scenes=(scene,))
        blob = build_development_egg_v2(bundle)
        path = self.work / "timers.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        output = self.work / "timer_pixels.bin"
        result = subprocess.run([str(self.timer_exe), str(path), str(output), str(mode)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return output.read_bytes()

    def test_scene_handler_preserves_state_animation_and_draws(self):
        actual = self.run_timer(0)
        frame = timer_fixture_bundle().frames[0]
        logical = bytearray(3024)
        for y in range(144):
            for x in range(168):
                black = (32 <= x < 48 and 104 <= y < 120) or (140 <= x < 152 and 16 <= y < 28)
                if 80 <= x < 88 and 40 <= y < 56:
                    index = (y - 40) * frame.row_stride_bytes
                    bit = 128 >> (x - 80)
                    black = bool(frame.pixels[index] & frame.mask[index] & bit)
                if black:
                    logical[y * 21 + x // 8] |= 128 >> (x % 8)
        self.assertEqual(panel_pixels(logical), actual)

    def test_start_restart_cancel_and_consumed_one_shot(self):
        self.run_timer(1)
        scene = deepcopy(timer_fixture_bundle().scenes[0])
        scene["event_bindings"][0]["configuration"]["start_policy"] = "action"
        for route in scene["routes"]:
            for action in route["actions"]:
                if action["kind"] == "restart_timer":
                    action["kind"] = "start_timer"
        self.run_timer(2, scene)

    def test_exact_gui_scene_timer_reveals_once_without_reentry(self):
        bundle = load_project(DEFAULT_PROJECT.parent / "native_v2_scene_timer.peepproj")
        actual = self.run_timer(11, bundle=bundle)
        frame = bundle.frames[0]
        logical = bytearray(3024)
        for y in range(144):
            for x in range(168):
                black = (120 <= x < 136 and 104 <= y < 120) or (76 <= x < 92 and 80 <= y < 96)
                if 80 <= x < 88 and 40 <= y < 56:
                    index = (y - 40) * frame.row_stride_bytes
                    bit = 128 >> (x - 80)
                    black = bool(frame.pixels[index] & frame.mask[index] & bit)
                if black:
                    logical[y * 21 + x // 8] |= 128 >> (x % 8)
        self.assertEqual(panel_pixels(logical), actual)

    def test_pause_resume_including_already_due_timer(self):
        self.run_timer(3)
        self.run_timer(10)

    def test_state_reentry_and_scene_recreation(self):
        self.run_timer(8)
        self.run_timer(9)

    def test_false_guard_consumes_without_draw_or_retry(self):
        scene = deepcopy(timer_fixture_bundle().scenes[0])
        scene["variables"] = [{"variable_id": "allow", "value_type": "int32", "initial": 0,
                               "minimum": 0, "maximum": 1}]
        scene["event_handlers"][0]["guards"] = [{"variable_ref": "allow", "operator": "eq", "value": 1}]
        self.run_timer(5, scene)

    def test_first_expiry_cancels_second_and_exit_ends_owner(self):
        for mode, action in ((4, {"kind": "cancel_timer", "timer_ref": "second"}),
                             (6, {"kind": "exit_to_shell"})):
            with self.subTest(mode=mode):
                scene = deepcopy(timer_fixture_bundle().scenes[0])
                scene["event_bindings"].append({"binding_id": "second", "event_type": "time.scene_elapsed",
                    "configuration": {"delay_ms": 5000, "start_policy": "scene_entry"}})
                scene["event_handlers"][0]["actions"].append(action)
                scene["event_handlers"].append({"handler_id": "second_handler", "event_ref": "second",
                    "guards": [], "actions": [{"kind": "object.set_position", "object_ref": "position_marker", "y": 64}]})
                scene["reactive_wait_default"]["event_interests"].append("second")
                self.run_timer(mode, scene)

    def test_render_failure_ends_session_and_clears_timers(self):
        self.run_timer(7)


if __name__ == "__main__":
    unittest.main()
