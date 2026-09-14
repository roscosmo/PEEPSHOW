"""Execute the actual shipment helper only against host memory, never hardware."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class BatteryShipHelperTests(unittest.TestCase):
    def test_guarded_request(self):
        root = Path(__file__).resolve().parents[3]
        firmware = root / "firmware/peepshow_hw6_fw0"
        compiler = Path(os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe"))
        debugger = compiler.with_name("gdb.exe")
        if not debugger.is_file():
            self.skipTest("Host GDB is required; never substitute a connected target")
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        probe = re.search(r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryQuiesceTimingProbe;", source).group()
        helper = firmware / "__fw0_battery_ship_once_enable.gdb"
        writes = re.findall(r"^\s*set var (.+)$", helper.read_text(), re.M)
        self.assertEqual(["g_ps_hw6_pmic_software_ship_request = 1"], writes)
        env = dict(os.environ)
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
        cases = {
            "boot_prepared": ([], 1),
            "critical_prepared": ([
                "g_ps_hw6_battery_shutdown_probe.reason = 2",
                "g_ps_hw6_owner_sm_probe.power_quiesce_reason = 2",
                "g_ps_hw6_battery_quiesce_timing_probe.reason = 2",
                "g_ps_hw6_owner_sm_probe.battery_policy_state = 4",
                "g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv = 3175"], 1),
            "wrong_api": (["g_ps_hw6_owner_sm_probe.version = 84"], 0),
            "healthy": (["g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv = 3782"], 0),
            "invalid_read": (["g_ps_hw6_owner_sm_probe.battery_policy_last_snapshot_status = 1"], 0),
            "usb": (["g_ps_hw6_owner_probe.power_mcu_vbus_present = 1"], 0),
            "unprepared": (["g_ps_hw6_battery_shutdown_probe.prepared = 0"], 0),
            "failed_owner": (["g_ps_hw6_owner_sm_probe.power_quiesce_owner_status[2] = 1"], 0),
            "missing_ack": (["g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask = 0x76"], 0),
            "clock_failure": (["g_ps_hw6_battery_quiesce_timing_probe.storage_clock_release_status = 1"], 0),
            "pending_barrier": (["g_ps_hw6_battery_quiesce_timing_probe.active = 1"], 0),
            "already_attempted": (["g_ps_hw6_owner_probe.power_software_ship_request_count = 1"], 0),
            "automatic_enabled": (["g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled = 1"], 0),
        }
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "timing_probe.inc").write_text(probe, encoding="ascii")
            (work / "stm32u5xx_hal.h").write_text(
                "#pragma once\n#include <stdint.h>\n"
                "typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;\n"
                "typedef struct {uint32_t unused;} I2C_HandleTypeDef;\n",
                encoding="ascii")
            (work / "tx_api.h").write_text(
                "#pragma once\n#include <stdint.h>\ntypedef uint32_t ULONG;\ntypedef uint32_t UINT;\n",
                encoding="ascii")
            exe = work / "ship_helper.exe"
            result = subprocess.run([str(compiler), "-g", "-O0", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_battery_ship_helper.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            commands = ["set pagination off", "set confirm off"]
            for name, (changes, expected) in cases.items():
                commands += ["start"] + [f"set var {change}" for change in changes]
                commands += [f"source {helper.as_posix()}",
                    f'printf "CASE {name}=%u\\n", g_ps_hw6_pmic_software_ship_request']
                if expected:
                    commands += [f"source {helper.as_posix()}"]
                commands += ["kill"]
            script = work / "checks.gdb"
            script.write_text("\n".join(commands) + "\n", encoding="ascii")
            result = subprocess.run([str(debugger), "-q", "-batch", str(exe), "-x", str(script)],
                capture_output=True, text=True, timeout=60, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            for name, (_, expected) in cases.items():
                self.assertIn(f"CASE {name}={expected}", result.stdout)
            self.assertEqual(2, result.stdout.count("Queued ONE physical shipment request"))
            self.assertIn("shipment is already queued", result.stdout)
