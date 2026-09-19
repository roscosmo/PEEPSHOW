"""Power endpoint protocol tests; queue failures are deterministic simulations."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class CalendarTransportTests(unittest.TestCase):
    def test_native_transport(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / "transport.exe"
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(firmware / "Core/Inc"),
                str(firmware / "Core/Src/ps_calendar_delivery.c"),
                str(firmware / "Core/Src/ps_calendar_transport.c"),
                str(Path(__file__).with_name("native_calendar_transport.c")),
                "-o", str(exe)], capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True,
                                    timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
