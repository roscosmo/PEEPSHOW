"""Check the actual CMake admission rules for the isolated shipment build."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


class BatteryBuildProfileTests(unittest.TestCase):
    def test_only_approved_snapshot_is_admitted(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        spec = importlib.util.spec_from_file_location("hw6_knobs", firmware / "tools/gen_knobs.py")
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        knobs = json.loads((firmware / "config/knobs.json").read_text())
        self.assertTrue(knobs["power_critical_software_ship_enable"])
        self.assertTrue(knobs["power_boot_low_battery_ship_enable"])
        self.assertFalse(knobs["power_start_software_ship_enable"])
        base = generator._render_header(knobs)
        self.assertEqual(base, (firmware / "Core/Inc/knobs_autogen.h").read_text())
        test_knobs = dict(knobs, power_critical_software_ship_enable=True,
                          power_boot_low_battery_ship_enable=True)
        snapshot = generator._render_header(test_knobs)
        gates_off = generator._render_header(dict(knobs, power_critical_software_ship_enable=False,
                                                  power_boot_low_battery_ship_enable=False))
        cmake = (firmware / "CMakeLists.txt").read_text()
        guard = cmake[cmake.index("option(PS_HW6_BATTERY_SHUTDOWN_TEST"):
                      cmake.index("    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS")]
        cases = (
            ("approved", snapshot, base, "BatteryShutdownTest", "ON", True),
            ("disabled_gates", gates_off, base, "BatteryShutdownTest", "ON", False),
            ("disabled_critical", generator._render_header(dict(test_knobs, power_critical_software_ship_enable=False)),
             base, "BatteryShutdownTest", "ON", False),
            ("disabled_boot", generator._render_header(dict(test_knobs, power_boot_low_battery_ship_enable=False)),
             base, "BatteryShutdownTest", "ON", False),
            ("start_enabled", generator._render_header(dict(test_knobs, power_start_software_ship_enable=True)),
             base, "BatteryShutdownTest", "ON", False),
            ("changed_threshold", generator._render_header(dict(test_knobs, power_battery_warning_mv=3499)),
             base, "BatteryShutdownTest", "ON", False),
            ("missing", None, base, "BatteryShutdownTest", "ON", False),
            ("normal_directory", snapshot, base, "Debug", "ON", False),
            ("normal_disabled", snapshot, gates_off, "BatteryShutdownTest", "ON", True),
            ("normal_default", None, base, "Debug", "OFF", True),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, candidate, normal, folder, enabled, accepted in cases:
                with self.subTest(name=name):
                    source = root / name
                    binary = source / "build" / folder
                    binary.mkdir(parents=True)
                    (source / "Core/Inc").mkdir(parents=True)
                    (source / "Core/Inc/knobs_autogen.h").write_text(normal, encoding="ascii")
                    if candidate is not None:
                        (binary / "battery-shutdown-knobs.h").write_text(candidate, encoding="ascii")
                    script = source / "guard.cmake"
                    script.write_text(
                        f'set(CMAKE_SOURCE_DIR "{source.as_posix()}")\n'
                        f'set(CMAKE_BINARY_DIR "{binary.as_posix()}")\n'
                        f'set(PS_HW6_BATTERY_SHUTDOWN_TEST {enabled} CACHE BOOL "")\n'
                        + guard + "endif()\n", encoding="ascii")
                    result = subprocess.run(["cmake", "-P", str(script)],
                        capture_output=True, text=True, timeout=10)
                    self.assertEqual(accepted, result.returncode == 0, result.stdout + result.stderr)
