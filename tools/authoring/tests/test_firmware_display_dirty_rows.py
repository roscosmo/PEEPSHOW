"""Native production dirty-row insertion and committed-frame comparison."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class DisplayDirtyRowsTests(unittest.TestCase):
    def test_order_duplicates_bounds_capacity_and_frame_comparison(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/display_renderer.c").read_text()
        functions = "\n".join(firmware_function(source, "DisplayRenderer_" + name)
                              for name in ("ResetDirtyRows", "MarkPanelRowDirty",
                                           "MarkAllRowsDirty", "ComputeDirtyRowsFromCommitted"))
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "dirty_rows_under_test.inc").write_text(functions, encoding="ascii")
            exe = work / "dirty_rows.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), str(Path(__file__).with_name("native_display_dirty_rows.c")),
                "-o", str(exe)], capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
