"""Compile the actual battery policy and preparation retry path with fake owners."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import firmware_function


class BatteryShutdownTests(unittest.TestCase):
    def test_fault_wait_ignores_disabled_service_deadlines(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        function = firmware_function(source, "PS_HW6_RTOS_OwnerReceiveWaitTicks")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "owner_wait.inc").write_text(function, encoding="ascii")
            exe = work / "owner_wait.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-O2", "-I", str(work),
                str(Path(__file__).with_name("native_battery_owner_wait.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_fault_wait_uses_checked_stop_without_runtime_resume(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        owner = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        rtos = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        stop = firmware_function(owner, "PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold")
        # Structural checks supplement fake-owner policy/RTC tests; not physical proof.
        for check in ("PS_HW6_RequestPowerQuiesce", "PS_HW6_ClockPolicy_PrepareStop2",
                      "PS_HW6_RTOS_Stop2FinalInputReady"):
            self.assertLess(stop.index(check), stop.index("HAL_PWREx_EnterSTOP2Mode"))
        self.assertIn("(fault_wait != 0UL) ? HAL_OK : PS_HW6_RequestPostStopResume()", stop)
        self.assertIn("clock_restore_status != TX_SUCCESS", stop)
        self.assertIn("g_ps_hw6_battery_fault_wait_probe.force_read = 1UL", stop)
        park = firmware_function(owner, "PS_HW6_OwnerStateMachines_QuiesceForPowerBarrier")
        self.assertIn("PS_HW6_DisplayOwner_AbortLpbamStop2()", park)
        self.assertLess(park.index("PS_HW6_BatteryFaultTestOwnerResult(owner_id, status)"),
                        park.index("power_quiesce_owner_status[owner_id] ="))
        self.assertLess(park.index("power_quiesce_owner_status[owner_id] ="),
                        park.index("power_quiesce_success_mask |= owner_bit"))
        for function in ("PS_HW6_RTOS_RunStop2AutoIdlePeriodic",
                         "PS_HW6_RTOS_RunDisplayCursorBlinkPeriodic",
                         "PS_HW6_RTOS_DeliverInputLogicalEvent",
                         "PS_HW6_RTOS_DeliverJoystickLogicalEvent",
                         "PS_HW6_RTOS_SendPowerStartEvent",
                         "PS_HW6_RTOS_HandleRuntimeCommand",
                         "PS_HW6_RTOS_ObjectSleepClockBegin"):
            self.assertIn("g_ps_hw6_battery_fault_wait_probe.active",
                          firmware_function(rtos, function), function)
        ready = firmware_function(rtos, "PS_HW6_RTOS_Stop2FinalInputReady")
        for guard in ("ps_object_runtime_busy", "ps_candidate_busy",
                      "ps_package_validation_busy", "g_ps_package_workflow_probe.active",
                      "g_ps_object_candidate_probe.leased"):
            self.assertIn(guard, ready)
        command = firmware_function(rtos, "PS_HW6_RTOS_HandleRuntimeCommand")
        refusal = command[:command.index("return;")]
        self.assertIn("ps_package_validation_busy = 0UL", refusal)
        self.assertIn("PS_HW6_RTOS_PACKAGE_VALIDATE_ACK", refusal)

    def test_actual_policy_retries(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        header = (firmware / "Core/Inc/ps_hw6_owner_state_machines.h").read_text()
        functions = "\n".join(firmware_function(source, name) for name in (
            "PS_HW6_BatteryPolicyPrepareForShipment",
            "PS_HW6_BatteryPolicyRequestSoftwareShipment",
            "PS_HW6_BatteryShutdownReset", "PS_HW6_BatteryFaultWaitLatch",
            "PS_HW6_BatteryFaultTestRequest",
            "PS_HW6_BatteryFaultTestOwnerResult",
            "PS_HW6_OwnerStateMachines_EndPowerQuiesce",
            "PS_HW6_OwnerStateMachines_BatteryFaultTestWake",
            "PS_HW6_OwnerStateMachines_ProcessSoftwareShipment",
            "PS_HW6_BatteryShutdownTryPrepare",
            "PS_HW6_SM_EvaluateBatteryPolicy",
            "PS_HW6_OwnerStateMachines_RunBatteryFaultWait"))
        typedef = re.search(r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryShutdownProbe;", header).group()
        typedef += "\n" + re.search(
            r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryFaultWaitProbe;", header).group()
        typedef += "\n" + re.search(
            r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryFaultTestProbe;", header).group()
        tables = "\n".join(re.search(
            r"static const PS_HW6_StateTransition " + name + r"\[\]\s*=\s*\{.*?\n\};",
            source, re.S).group() for name in ("ps_power_transitions", "ps_pmic_transitions"))
        constants = sorted(set(re.findall(r"\b(?:PS_HW6|PMIC|PWR)_[A-Z][A-Z0-9_]+\b", functions + tables)))
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            declarations = [typedef]
            for index, name in enumerate(constants, 1):
                # Preserve real contract values rather than synthetic enum numbering.
                if name == "PS_HW6_RTOS_OWNER_SENSOR":
                    declarations.append(f"#define {name} 4U")
                    continue
                match = re.search(r"#define\s+" + name + r"\s+([^\n]+)", header)
                if match:
                    declarations.append(f"#define {name} {match[1]}")
                else:
                    declarations.append(f"#define {name} {index}U")
            for probe in ("g_ps_hw6_owner_sm_probe", "g_ps_hw6_owner_probe"):
                fields = sorted(set(re.findall(probe + r"\.(\w+)", functions)))
                declarations.append("static struct {" + "\n".join(
                    f"uint32_t {f}" + ("[128];" if f == "current_state" else ";")
                    for f in fields) + "} " + probe + ";")
            (work / "battery_declarations.inc").write_text("\n".join(declarations), encoding="ascii")
            (work / "battery_transitions.inc").write_text(tables, encoding="ascii")
            (work / "battery_policy.inc").write_text(functions, encoding="ascii")
            for enabled, start_enabled in ((0, 0), (1, 0), (0, 1)):
                exe = work / f"battery{enabled}_{start_enabled}.exe"
                result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-O2", f"-DTEST_SHIP_ENABLED={enabled}",
                    f"-DTEST_START_SHIP_ENABLED={start_enabled}", "-I", str(work),
                    "-I", str(firmware / "Core/Inc"),
                    str(firmware / "Core/Src/ps_battery_wake.c"),
                    str(Path(__file__).with_name("native_battery_shutdown.c")), "-o", str(exe)],
                    capture_output=True, text=True, timeout=30, env=env)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        knobs = json.loads((firmware / "config/knobs.json").read_text())
        self.assertEqual(3, knobs["power_battery_shutdown_prep_attempts"])
        self.assertEqual(1000, knobs["power_battery_shutdown_prep_retry_ms"])
        for name in ("critical_software", "boot_low_battery"):
            self.assertTrue(knobs[f"power_{name}_ship_enable"])
        self.assertFalse(knobs["power_start_software_ship_enable"])
        schema = json.loads((firmware / "config/knobs.schema.json").read_text())
        generated = (firmware / "Core/Inc/knobs_autogen.h").read_text()
        for name in ("critical_software", "boot_low_battery", "start_software"):
            key = f"power_{name}_ship_enable"
            self.assertEqual(knobs[key], schema["properties"][key]["default"])
            self.assertRegex(generated, rf"#define KNOB_{key.upper()}\s+\({int(knobs[key])}\)")
