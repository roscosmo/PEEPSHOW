"""Compile the peripheral-free production time core; no hardware claims."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class SystemTimeTests(unittest.TestCase):
    def test_native_time_core(self):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc")
        if not compiler:
            candidate = Path("C:/msys64/ucrt64/bin/gcc.exe")
            if candidate.is_file():
                compiler = str(candidate)
        if not compiler:
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / "system_time.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(firmware / "Core/Inc"), str(firmware / "Core/Src/ps_system_time.c"),
                str(Path(__file__).with_name("native_system_time.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("system time checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
