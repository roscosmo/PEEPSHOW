"""Exercise the actual display sequence service with a suspended package."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class ShellWaitingTests(unittest.TestCase):
    def test_timeline_ownership_and_failed_deadline(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / "firmware/peepshow_hw6_fw0/Core/Src/ps_hw6_rtos_probe.c").read_text()
        function = firmware_function(source, "PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "waiting.inc").write_text(function, encoding="ascii")
            exe = work / "waiting.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(work), str(Path(__file__).with_name("native_shell_waiting.c")),
                "-o", str(exe)], capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
