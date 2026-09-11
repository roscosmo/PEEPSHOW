"""Production SFX shutdown, queue admission and runtime lifetime paths."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import TOOL_ROOT, firmware_function


class AudioPackageTests(unittest.TestCase):
    def test_fifo_stop_lifetime_and_fault_quarantine(self):
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        if not Path(compiler).is_file():
            self.skipTest("native GCC required")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        definitions = "\n".join(firmware_function(source, "PS_HW6_RTOS_" + name) for name in (
            "OpenPackageSfx", "StopPackageSfx", "StopPackageSfxOwner",
            "RuntimeSuspend", "RuntimeResume", "RuntimePackageReturn"))
        # Compile the actual PLAY branch, including the post-clock admission check.
        begin = source.index("    HAL_StatusTypeDef sfx_status = HAL_ERROR;",
                             source.index("static void PS_HW6_RTOS_HandleOwnerCommand("))
        end = source.index("\n  }\n  else if", begin)
        definitions += "\nstatic void play(uint32_t cycle_index)\n{\n" + source[begin:end] + "\n}\n"
        constants = []
        for suffix in ("COMMAND_AUDIO_STOP_PACKAGE", "AUDIO_STOP_ACK", "EVENT_DEBUG_INDEX",
                       "OWNER_ACK_WAIT_TICKS", "STATUS_NOT_RUN", "AUDIO_CLOCK_REASON_REACTIVE_SFX",
                       "RUNTIME_CLOCK_REASON_RELEASE", "RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION",
                       "RUNTIME_CLOCK_REASON_REALTIME_DEADLINE"):
            constants.append(re.search(r"^#define PS_HW6_RTOS_" + suffix + r"\s+[^\n]+",
                                       source, re.MULTILINE).group(0))
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "audio_package_under_test.inc").write_text(definitions, encoding="ascii")
            (work / "audio_package_constants.inc").write_text("\n".join(constants), encoding="ascii")
            (work / "tx_api.h").write_text("#include <stdint.h>\ntypedef uint32_t UINT;\n"
                "typedef uint32_t ULONG;\ntypedef struct { uint32_t unused; } TX_BYTE_POOL;\n", encoding="ascii")
            exe = work / "audio_package.exe"
            env = dict(os.environ)
            env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_audio_package.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=60, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            for mode in range(10):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(exe), str(mode)], capture_output=True,
                                            text=True, timeout=10, env=env)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_source_reuse_follows_stop_barrier(self):
        source = (TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0/Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        for name, mutation in (("RuntimePackageActivateStub", "PS_HW6_RTOS_RequestStoragePackageLoadAndWait"),
                               ("RuntimeEnterInstaller", "PS_SceneRuntime_ExitStateScene"),
                               ("RuntimePackageReturn", "PS_SceneRuntime_ExitStateScene"),
                               ("RuntimePackageReplace", "PS_SceneRuntime_ExitStateScene")):
            body = firmware_function(source, "PS_HW6_RTOS_" + name)
            self.assertLess(body.index("PS_HW6_RTOS_StopPackageSfx()"), body.index(mutation), name)
        admission = firmware_function(source, "PS_HW6_RTOS_AdmitSystemAction")
        self.assertIn("(power_action != 0UL) ? PS_HW6_RTOS_COMMAND_RUNTIME_POWER_SUSPEND", admission)
        workflow = firmware_function(source, "PS_HW6_RTOS_PackageWorkflowPrepare")
        self.assertIn("runtime_last_status == PS_STATUS_OK", workflow)
