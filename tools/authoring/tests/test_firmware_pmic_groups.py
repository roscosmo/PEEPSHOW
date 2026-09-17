"""Exercise the complete PMIC driver against a deterministic fake I2C transport."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class PmicGroupTests(unittest.TestCase):
    def test_read_selection_failures_and_last_good_records(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "stm32u5xx_hal.h").write_text(
                "#pragma once\n#include <stdint.h>\n"
                "typedef struct {uint32_t unused;} I2C_HandleTypeDef;\n", encoding="ascii")
            (work / "tx_api.h").write_text(
                "#pragma once\n#include <stdint.h>\n"
                "typedef uint32_t ULONG;\ntypedef uint32_t UINT;\n", encoding="ascii")
            exe = work / "pmic.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(firmware / "Core/Src/ps_dev_adp5360.c"),
                str(Path(__file__).with_name("native_pmic_groups.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_owner_keeps_full_snapshot_and_invalidates_on_configuration(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_owner_services.c").read_text()
        snapshot = firmware_function(source, "PS_HW6_PowerOwner_RunSnapshot")
        self.assertIn("ps_dev_adp5360_read_power_snapshot(&ps_hw6_pmic, &snapshot)", snapshot)
        self.assertIn("ps_dev_adp5360_monitor_record", snapshot)
        self.assertIn("return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;", snapshot)
        for name in ("EnableMrShippingMode", "PrepareFuelGauge", "ConfigureThermistor",
                     "ConfigureChargerProfile", "ConfigurePmicInterrupts", "EnterSoftwareShipmentMode"):
            body = firmware_function(source, "PS_HW6_PowerOwner_" + name)
            self.assertIn("ps_dev_adp5360_monitor_invalidate", body)
        states = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        monitor = firmware_function(states, "PS_HW6_OwnerStateMachines_RunBatteryMonitor")
        self.assertIn("PS_HW6_PowerOwner_RunSnapshot()", monitor)
        self.assertNotIn("read_groups", monitor)
        self.assertNotIn("g_ps_hw6_pmic_monitor_probe", states)


if __name__ == "__main__":
    unittest.main()
