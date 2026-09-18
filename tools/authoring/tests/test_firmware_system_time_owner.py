"""Exercise the production owner transaction with deterministic queue/RTC stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class SystemTimeOwnerTests(unittest.TestCase):
    def test_owner_transaction(self):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc")
        if not compiler:
            candidate = Path("C:/msys64/ucrt64/bin/gcc.exe")
            if candidate.is_file():
                compiler = str(candidate)
        if not compiler:
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        begin = source.index("/* System-time transaction:")
        end = source.index("static HAL_StatusTypeDef PS_HW6_RTOS_ObjectRtcMilliseconds", begin)
        block = source[begin:end]
        self.assertNotIn("HAL_RTC_Set", block)
        self.assertIn("PS_SystemTime_Init(&ps_system_clock);", source[end:])
        self.assertIn("PS_HW6_SystemTime_Owner(message);", source[end:])
        self.assertIn("PS_HW6_SystemTime_DebugUi();", source[end:])
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as temp:
            (Path(temp) / "system_time_owner.inc").write_text(block)
            begin = source.index("static void PS_HW6_SystemTime_EditorUi(void)")
            end = source.index("uint32_t PS_HW6_RTOS_ObjectSleepClockBegin(void)", begin)
            (Path(temp) / "system_time_editor_owner.inc").write_text(source[begin:end])
            exe = Path(temp) / "system_time_owner.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", temp, "-I", str(firmware / "Core/Inc"),
                str(firmware / "Core/Src/ps_system_time.c"),
                str(firmware / "Core/Src/ps_ui_router.c"),
                str(Path(__file__).with_name("native_system_time_owner.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("system time owner checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
