"""Run the actual power-owned bench adapter with deterministic time samples."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class CalendarOwnerTests(unittest.TestCase):
    def test_adapter(self):
        fw = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = (fw / "Core/Src/ps_hw6_calendar.c").read_text()
            (work / "calendar_owner.inc").write_text(source.replace('#include "tx_api.h"', ''), encoding="ascii")
            exe = work / "calendar.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(fw / "Core/Inc"),
                str(fw / "Core/Src/ps_system_time.c"), str(fw / "Core/Src/ps_calendar_timer.c"),
                str(Path(__file__).with_name("native_calendar_owner.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
