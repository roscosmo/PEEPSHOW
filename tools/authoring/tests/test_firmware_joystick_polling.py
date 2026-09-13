"""Run the actual bounded owner poll and preparation against a fake TMAG."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class JoystickPollingTests(unittest.TestCase):
    def test_awake_lifetime_and_failure_cleanup(self):
        root = Path(__file__).resolve().parents[3]
        firmware = root / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        table = re.search(r"static const PS_HW6_StateTransition ps_joystick_transitions\[\] =\s*\{.*?\n\};",
                          source, re.S).group(0)
        functions = ("PS_HW6_SM_PrepareJoystickInput", "PS_HW6_SM_RunJoystickCardinalProbe")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "joystick_table.inc").write_text(table, encoding="ascii")
            (work / "joystick_under_test.inc").write_text(
                "\n".join(firmware_function(source, name) for name in functions), encoding="ascii")
            (work / "stm32u5xx_hal.h").write_text(
                "#pragma once\n#include <stdint.h>\n"
                "typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;\n"
                "typedef struct {uint32_t unused;} I2C_HandleTypeDef;\n", encoding="ascii")
            (work / "tx_api.h").write_text(
                "#pragma once\n#include <stdint.h>\ntypedef uint32_t ULONG;\ntypedef uint32_t UINT;\n",
                encoding="ascii")
            exe = work / "joystick.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_joystick_polling.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_stop_and_poll_admission_still_use_existing_paths(self):
        root = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0/Core/Src"
        source = (root / "ps_hw6_owner_state_machines.c").read_text()
        quiesce = firmware_function(source, "PS_HW6_SM_QuiesceJoystick")
        self.assertIn("PS_HW6_SM_JoystickTerminalSleepProofValid()", quiesce)
        self.assertIn("ps_dev_tmag3001_prepare_wake_sleep_omnipolar_xy(", quiesce)
        self.assertIn("ps_dev_tmag3001_prepare_sleep(", quiesce)
        self.assertIn("JOY_EV_QUIESCE", quiesce)
        rtos = (root / "ps_hw6_rtos_probe.c").read_text()
        allowed = firmware_function(rtos, "PS_HW6_RTOS_JoystickAwakePollingAllowed")
        for guard in ("PWR_ACTIVE_LP", "PWR_ACTIVE_RT", "INTERACTION_STATE_INACTIVE",
                      "joystick_calibration_persistent_load_available",
                      "JoystickWakeCharacterizationActive", "JoystickCalibrationCaptureActive"):
            self.assertIn(guard, allowed)
        poll = firmware_function(rtos, "PS_HW6_RTOS_RunJoystickAwakeInput")
        self.assertIn("KNOB_INPUT_JOYSTICK_AWAKE_POLL_PERIOD_MS", poll)
        self.assertIn("PS_HW6_RTOS_TimeReached", poll)
