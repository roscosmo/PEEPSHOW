"""One-shot TraceX lifecycle uses the existing buffer and freezes matching work."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_firmware_package_workflow import firmware_function


class ObjectTraceTests(unittest.TestCase):
    def test_arm_markers_freeze_failures_and_rearm(self):
        root = Path(__file__).resolve().parents[3]
        firmware = root / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_trace.c").read_text(encoding="utf-8")
        functions = ("PS_HW6_ObjectTraceWrap", "PS_HW6_ObjectTraceInsert", "PS_HW6_TraceObjectArm",
                     "PS_HW6_TraceObjectBegin", "PS_HW6_TraceObjectStage", "PS_HW6_TraceObjectRaster",
                     "PS_HW6_TraceObjectEnd")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "trace_under_test.inc").write_text("\n".join(firmware_function(source, name)
                                                               for name in functions), encoding="ascii")
            exe = work / "trace.exe"
            result = subprocess.run([compiler,
                "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-I", str(firmware / "Core/Inc"),
                "-I", str(work), str(Path(__file__).with_name("native_object_trace.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_local_timestamp_source_is_cycle_counter(self):
        root = Path(__file__).resolve().parents[3]
        port = (root / "firmware/peepshow_hw6_fw0/Middlewares/ST/threadx/ports/cortex_m33/gnu/inc/tx_port.h").read_text()
        self.assertIn("#define TX_TRACE_TIME_SOURCE                    *((volatile ULONG *) 0xE0001004)", port)
        self.assertIn("#define TX_TRACE_TIME_MASK                      0xFFFFFFFFUL", port)
