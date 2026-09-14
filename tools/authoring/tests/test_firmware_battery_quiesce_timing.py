"""Battery clock handoff and observer tests with simulated queue boundaries."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class BatteryQuiesceTimingTests(unittest.TestCase):
    def test_battery_clock_handoff_and_failure_cleanup(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        probe = re.search(r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryQuiesceTimingProbe;", source).group()
        functions = "\n".join(firmware_function(source, name) for name in (
            "PS_HW6_RTOS_RequestPowerClockProfile",
            "PS_HW6_RTOS_BeginPowerDisplayBarrier", "PS_HW6_RTOS_EndPowerDisplayBarrier",
            "PS_HW6_RTOS_RunPowerQuiesceBarrier"))
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "timing_probe.inc").write_text(probe, encoding="ascii")
            (work / "timing_functions.inc").write_text(functions, encoding="ascii")
            exe = work / "timing.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-O2", "-I", str(work), str(Path(__file__).with_name("native_battery_quiesce_timing.c")),
                "-o", str(exe)], capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
