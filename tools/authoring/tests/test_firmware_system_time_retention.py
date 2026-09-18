"""Backup-register adapter and core record tests; no hardware retention claim."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class TimeRetentionTests(unittest.TestCase):
    def test_retention(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_time_retention.c").read_text()
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "retention.inc").write_text(source.replace('#include "main.h"', ''), encoding="ascii")
            exe = work / "retention.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(firmware / "Core/Src/ps_system_time.c"),
                str(Path(__file__).with_name("native_system_time_retention.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_startup_hooks_are_in_user_blocks(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/main.c").read_text()
        block = source.split("/* USER CODE BEGIN Check_RTC_BKUP */", 1)[1].split("/* USER CODE END Check_RTC_BKUP */", 1)[0]
        self.assertIn("PS_HW6_TimeRetention_BootPreserve()", block)
        self.assertIn("HAL_RTCEx_SetCalibrationOutPut", block)
        self.assertIn("return;", block)
        block = source.split("/* USER CODE BEGIN RTC_Init 2 */", 1)[1].split("/* USER CODE END RTC_Init 2 */", 1)[0]
        self.assertIn("PS_HW6_TimeRetention_BootFresh() == 0U", block)
        self.assertIn("Error_Handler();", block)
