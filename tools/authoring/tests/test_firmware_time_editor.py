"""Compile the actual shell router and time core, without peripherals."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class TimeEditorTests(unittest.TestCase):
    def test_native_editor(self):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as temp:
            renderer = (firmware / "Core/Src/display_renderer.c").read_text()
            glyphs = renderer[renderer.index("static uint32_t DisplayRenderer_GlyphRows("):
                              renderer.index("static const char *DisplayRenderer_ShutdownCountdownLine(")]
            editor = renderer[renderer.index("static void DisplayRenderer_TimeDigits("):
                              renderer.index("void DisplayRenderer_PrepareUIPage(")]
            (Path(temp) / "time_renderer.inc").write_text(glyphs + editor)
            exe = Path(temp) / "time_editor.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", temp, "-I", str(firmware / "Core/Inc"), str(firmware / "Core/Src/ps_system_time.c"),
                str(firmware / "Core/Src/ps_ui_router.c"),
                str(Path(__file__).with_name("native_time_editor.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("time editor checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
