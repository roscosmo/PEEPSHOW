"""Development egg -> real C activation/graph/projection -> production pixels."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import TOOL_ROOT, firmware_function
from test_firmware_shape_primitives import panel_pixels
from build_object_development import DEFAULT_PROJECT, fixture_bundle, render_c
from peepshow_authoring.project import load_project
from peepshow_authoring.compiler import build_development_egg_v2, build_egg, EggCompileError


class ObjectAwakeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        if not Path(compiler).is_file():
            raise unittest.SkipTest("native GCC required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        source = (cls.firmware / "Core/Src/display_renderer.c").read_text(encoding="utf-8")
        names = ["SetBlack", "HorizontalLine", "VerticalLine", "Line", "FilledRect",
                 "EllipseRow", "RasterEllipse", "SetPanelPixelWhiteInBuffer",
                 "SetPanelPixelBlackInBuffer", "SetLogicalPixelInBuffer", "ApplyPackageSprite",
                 "ValidateSceneModel", "DrawSceneElement"]
        (cls.work / "object_raster_under_test.inc").write_text(
            "\n".join(firmware_function(source, "DisplayRenderer_" + name) for name in names), encoding="ascii")
        cls.exe = cls.work / "awake.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_awake.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_bundle(self, bundle, reject=False, gui=False):
        blob = build_development_egg_v2(bundle)
        path = self.work / "fixture.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        output = self.work / "pixels.bin"
        result = subprocess.run([str(self.exe), str(path), str(output), str(2 if gui else int(reject))],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return output.read_bytes() if not reject else None

    def test_real_runtime_animation_continuity_and_pixels(self):
        bundle = fixture_bundle()
        actual = self.run_bundle(bundle)
        expected = bytearray()
        for phase, marker_x in [(0, 24), (1, 24), (1, 128), (2, 128), (2, 24), (3, 24), (0, 24)]:
            logical = bytearray(3024)
            frame = bundle.frames[phase]
            for y in range(144):
                for x in range(168):
                    black = marker_x <= x < marker_x + 16 and 100 <= y < 116
                    if 68 <= x < 100 and 28 <= y < 60:
                        black = bool(frame.pixels[(y - 28) * 4 + (x - 68) // 8] & (128 >> ((x - 68) % 8)))
                    if black:
                        logical[y * 21 + x // 8] |= 128 >> (x % 8)
            expected.extend(panel_pixels(logical))
        self.assertEqual(expected, actual)

    def test_unsupported_interaction_is_not_silently_ignored(self):
        bundle = fixture_bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["interaction_policy"].update(mode="timeout", inactive_route="preserve_scene")
        self.run_bundle(replace(bundle, scenes=(scene,)), reject=True)

    def test_unsupported_timer_is_not_silently_ignored(self):
        bundle = fixture_bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["event_bindings"] = [{"binding_id": "later", "event_type": "time.scene_elapsed",
            "configuration": {"delay_ms": 5000, "start_policy": "scene_entry"}}]
        scene["event_handlers"] = [{"handler_id": "later_handler", "event_ref": "later", "guards": [],
            "actions": [{"kind": "object.set_position", "object_ref": "marker", "x": 64}]}]
        self.run_bundle(replace(bundle, scenes=(scene,)), reject=True)

    def test_gui_fixture_runtime_continuity_and_pixels(self):
        bundle = load_project(DEFAULT_PROJECT)
        actual = self.run_bundle(bundle, gui=True)
        expected = bytearray()
        for phase, marker_x in [(0, 32), (1, 32), (1, 120), (1, 120), (1, 32), (0, 32), (1, 32)]:
            logical = bytearray(3024)
            frame = bundle.frames[phase]
            for y in range(144):
                for x in range(168):
                    black = marker_x <= x < marker_x + 16 and 104 <= y < 120
                    if 80 <= x < 88 and 40 <= y < 56:
                        index = (y - 40) * frame.row_stride_bytes + (x - 80) // 8
                        bit = 128 >> ((x - 80) % 8)
                        black = bool(frame.pixels[index] & frame.mask[index] & bit)
                    if black:
                        logical[y * 21 + x // 8] |= 128 >> (x % 8)
            expected.extend(panel_pixels(logical))
        self.assertEqual(expected, actual)

    def test_checked_in_fixture_is_reproducible_and_not_ordinary_export(self):
        bundle = load_project(DEFAULT_PROJECT)
        self.assertEqual(render_c(build_development_egg_v2(bundle)),
            (self.firmware / "Core/Src/ps_object_development_egg_autogen.c").read_text(encoding="ascii"))
        with self.assertRaises(EggCompileError):
            build_egg(bundle)

    def test_launch_completes_ui_handoff_and_keeps_animating(self):
        self.run_rtos_harness("launch", ("ObjectService",))

    def test_bounded_display_handoff_quarantines_timeout(self):
        self.run_rtos_harness("handoff", ("ObjectAdvance", "ObjectPresent"))

    def run_rtos_harness(self, name, functions):
        source = (self.firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        (self.work / f"object_{name}_under_test.inc").write_text(
            "\n".join(firmware_function(source, "PS_HW6_RTOS_" + function)
                      for function in functions), encoding="ascii")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        exe = self.work / f"{name}.exe"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-I", str(self.firmware / "Core/Inc"), "-I", str(self.work),
            str(Path(__file__).with_name(f"native_object_{name}.c")), "-o", str(exe)],
            capture_output=True, text=True, timeout=30, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
