"""Host-check the real scheduler functions with scene/RTOS interfaces stubbed."""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class FirmwareSceneTimerTests(unittest.TestCase):
    def test_native_scheduler(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc")
        if not compiler:
            candidate = Path("C:/msys64/ucrt64/bin/gcc.exe")
            if candidate.is_file():
                compiler = str(candidate)
        if not compiler:
            self.skipTest("native GCC not installed; set HOST_CC to run firmware scheduler checks")
        root = Path(__file__).resolve().parents[3]
        firmware = root / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        # Compile the production definitions verbatim, not a Python timer reimplementation.
        start = source.index("typedef struct\n{\n  uint32_t configured;")
        end = source.index("static uint32_t ps_runtime_rtc_wake_source;", start)
        declarations = source[start:end]
        start = source.index("static void PS_HW6_RTOS_RuntimeStateTimersClear(void)\n{")
        end = source.index("static void PS_HW6_RTOS_RuntimeInteractionBegin(uint32_t now_tick)\n{", start)
        functions = source[start:end]
        fields = sorted(set(re.findall(r"g_ps_hw6_rtos_probe\.(\w+)", functions)))
        probe = "static struct {\n" + "".join(f"  uint32_t {name};\n" for name in fields)
        probe += "} g_ps_hw6_rtos_probe;\n"
        with tempfile.TemporaryDirectory() as temp:
            work = Path(temp)
            (work / "timer_probe_under_test.inc").write_text(probe, encoding="ascii")
            (work / "scene_timers_under_test.inc").write_text(declarations + functions, encoding="ascii")
            executable = work / "scene_timers.exe"
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_scene_timers.c")), "-o", str(executable),
            ], capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                                    timeout=10, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("scheduling checks passed", result.stdout)
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
                str(Path(__file__).with_name("native_scene_timer_runtime.c")), "-o", str(executable),
            ], capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                                    timeout=10, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("runtime timer checks passed", result.stdout)
