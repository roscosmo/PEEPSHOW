"""Run both production owners against deterministic bounded queue/time stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class CalendarRuntimeTests(unittest.TestCase):
    def test_runtime_delivery(self):
        fw = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        cc = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(cc).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            for source, target in (("ps_hw6_calendar.c", "calendar_owner.inc"),
                                   ("ps_hw6_calendar_runtime.c", "calendar_runtime.inc")):
                text = (fw / "Core/Src" / source).read_text().replace('#include "tx_api.h"', '')
                (work / target).write_text(text, encoding="ascii")
            exe = work / "runtime.exe"
            sources = [fw / "Core/Src" / name for name in (
                "ps_system_time.c", "ps_calendar_timer.c", "ps_calendar_delivery.c",
                "ps_calendar_transport.c")]
            result = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(fw / "Core/Inc"), *map(str, sources),
                str(Path(__file__).with_name("native_calendar_runtime.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
