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
    def test_actual_policy_retries(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text()
        header = (firmware / "Core/Inc/ps_hw6_owner_state_machines.h").read_text()
        functions = "\n".join(firmware_function(source, name) for name in (
            "PS_HW6_BatteryPolicyPrepareForShipment",
            "PS_HW6_BatteryPolicyRequestSoftwareShipment",
            "PS_HW6_BatteryShutdownReset", "PS_HW6_BatteryShutdownTryPrepare",
            "PS_HW6_SM_EvaluateBatteryPolicy"))
        typedef = re.search(r"typedef struct\s*\{[^}]*\}\s*PS_HW6_BatteryShutdownProbe;", header).group()
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
            for enabled in (0, 1):
                exe = work / f"battery{enabled}.exe"
                result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-O2", f"-DTEST_SHIP_ENABLED={enabled}", "-I", str(work),
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
        for name in ("critical_software", "boot_low_battery", "start_software"):
            self.assertFalse(knobs[f"power_{name}_ship_enable"])
