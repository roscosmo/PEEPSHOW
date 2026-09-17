"""Battery deadlines and actual shared RTC selection/finish, with fake hardware."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class BatteryWakeTests(unittest.TestCase):
    def run_native(self, name, rtc=False):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            if rtc:
                source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
                functions = "\n".join(firmware_function(source, fn) for fn in (
                    "RTC_IRQHandler", "PS_HW6_RTOS_InteractionStop2TimeoutPrepare",
                    "PS_HW6_RTOS_InteractionStop2TimeoutFinish"))
                owner = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
                functions += "\n" + firmware_function(owner,
                    "PS_HW6_OwnerStateMachines_RunBatteryMonitor")
                (work / "rtc_under_test.inc").write_text(functions, encoding="ascii")
                fields = sorted(set(re.findall(r"g_ps_hw6_rtos_probe\.(\w+)", functions)))
                (work / "rtc_probe.inc").write_text(
                    "static struct {\n" + "\n".join(f"uint32_t {f};" for f in fields)
                    + "\n} g_ps_hw6_rtos_probe;\n", encoding="ascii")
            exe = work / "battery.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(firmware / "Core/Src/ps_battery_wake.c"),
                str(Path(__file__).with_name(name)), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_deadline_accounting_and_failures(self):
        self.run_native("native_battery_wake.c")

    def test_shared_rtc_hardware_boundary(self):
        self.run_native("native_battery_rtc.c", rtc=True)

    def test_owner_integration_and_automatic_battery_shipping_enabled(self):
        import json
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        policy = firmware_function(source, "PS_HW6_SM_EvaluateBatteryPolicy")
        self.assertIn("PS_BatteryWake_Record", policy)
        self.assertIn("(snapshot_status == HAL_OK) && (fuel_ok != 0UL)", policy)
        self.assertLess(policy.index("vbat_mv == 0UL"), policy.index("PS_BatteryWake_Record"))
        monitor = firmware_function(source, "PS_HW6_OwnerStateMachines_RunBatteryMonitor")
        self.assertIn("PS_BatteryWake_Remaining", monitor)
        self.assertIn("PS_HW6_PowerOwner_RunSnapshot()", monitor)
        self.assertIn("PS_HW6_SM_EvaluateBatteryPolicy", monitor)
        self.assertEqual(2, source.count("PS_HW6_RTOS_InteractionStop2TimeoutFinish();"))
        knobs = json.loads((firmware / "config/knobs.json").read_text())
        self.assertEqual(1800000, knobs["power_battery_sleep_check_ms"])
        self.assertEqual(60000, knobs["power_battery_sleep_warning_ms"])
        self.assertEqual(60000, knobs["power_battery_sleep_retry_ms"])
        self.assertTrue(knobs["power_critical_software_ship_enable"])
        self.assertTrue(knobs["power_boot_low_battery_ship_enable"])
        self.assertFalse(knobs["power_start_software_ship_enable"])
