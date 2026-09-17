"""One-shot TraceX lifecycle uses the existing buffer and freezes matching work."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_firmware_package_workflow import firmware_function


class ObjectTraceTests(unittest.TestCase):
    def test_panel_transfer_markers_preserve_errors_chunks_and_commit(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        driver = (firmware / "Core/Src/LS013B7DH05.c").read_text()
        owner = (firmware / "Core/Src/ps_hw6_owner_services.c").read_text()
        functions = "\n".join(firmware_function(driver, name) for name in (
            "BuildWriteBurst", "lcd_dma_wait", "LCD_FlushRows_DMA", "LCD_PresentRows_DMA"))
        functions += firmware_function(owner, "PS_HW6_DisplayOwner_PresentRendererRows")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "panel_under_test.inc").write_text(functions, encoding="ascii")
            exe = work / "panel.exe"
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(firmware / "Core/Inc"), "-I", str(work),
                str(Path(__file__).with_name("native_object_panel_trace.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_panel_scope_and_renderer_markers_enclose_real_work(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        begin = source.index("PS_HW6_TraceObjectPanelBegin();")
        render = source.index("result = PS_HW6_DisplayOwner_RenderDevelopmentObjects", begin)
        end = source.index("PS_HW6_TraceObjectPanelEnd((uint32_t)result);", render)
        self.assertLess(begin, render)
        self.assertLess(render, end)
        self.assertIn("render_token == (uint32_t)message[2]", source[begin - 230:begin])
        renderer = (firmware / "Core/Src/display_renderer.c").read_text()
        for function, phase, work in (
            ("DisplayRenderer_ClearWhite", "CLEAR", "memset(s_display_framebuffer"),
            ("DisplayRenderer_RecordCursorBaseFrame", "BASE_COPY", "memcpy(s_display_cursor_base_framebuffer"),
            ("DisplayRenderer_ComputeDirtyRowsFromCommitted", "DIRTY_ROWS", "memcmp("),
            ("DisplayRenderer_FillStats", "STATS", "DisplayRenderer_FramebufferHash()"),
            ("DisplayRenderer_CommitPresentedFrame", "COMMIT", "memcpy(s_display_committed_framebuffer"),
            ("DisplayRenderer_PrepareUIPage", "COMPOSE", "DisplayRenderer_DrawSceneModel(scene_model)"),
        ):
            with self.subTest(phase=phase):
                body = firmware_function(renderer, function)
                begin = body.index(f"PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_{phase}, 0UL")
                call = body.index(work, begin)
                end = body.index(f"PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_{phase}, 1UL", call)
                self.assertLess(begin, call)
                self.assertLess(call, end)
        driver = (firmware / "Core/Src/LS013B7DH05.c").read_text()
        for name in ("lcd_dma_wait", "HAL_SPI_TxCpltCallback", "HAL_SPI_ErrorCallback"):
            self.assertNotIn("TraceObjectPanel", firmware_function(driver, name))

    def test_arm_markers_freeze_failures_and_rearm(self):
        root = Path(__file__).resolve().parents[3]
        firmware = root / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_trace.c").read_text(encoding="utf-8")
        functions = ("PS_HW6_ObjectTraceWrap", "PS_HW6_ObjectTraceInsert", "PS_HW6_TraceObjectArm",
                     "PS_HW6_TraceObjectBegin", "PS_HW6_TraceObjectStage", "PS_HW6_TraceObjectRaster",
                     "PS_HW6_TraceObjectOwnerBegin", "PS_HW6_TraceObjectOwnerEnd",
                     "PS_HW6_TraceObjectPanel", "PS_HW6_TraceObjectPanelBegin", "PS_HW6_TraceObjectPanelEnd",
                     "PS_HW6_TraceObjectEnd", "PS_HW6_TraceSysTickBefore", "PS_HW6_TraceSysTickAfter")
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "trace_under_test.inc").write_text("\n".join(firmware_function(source, name)
                for name in functions) + firmware_function(
                    (firmware / "Core/Src/ps_hw6_clock_policy.c").read_text(),
                    "PS_HW6_ClockPolicy_RetuneThreadXSysTick") + "\n".join(
                        firmware_function((firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text(), name)
                        for name in ("PS_HW6_SM_SuspendThreadXSystick", "PS_HW6_SM_RestoreThreadXSystick")),
                encoding="ascii")
            (work / "trace_service_under_test.inc").write_text(firmware_function(
                (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(),
                "PS_HW6_RTOS_ObjectTraceService"), encoding="ascii")
            exe = work / "trace.exe"
            result = subprocess.run([compiler,
                "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-I", str(firmware / "Core/Inc"),
                "-I", str(work), str(Path(__file__).with_name("native_object_trace.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_installed_request_is_serviced_before_runtime_receipt_without_sleep_override(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text()
        body = firmware_function(source, "PS_HW6_RTOS_HandleRuntimeInput")
        self.assertLess(body.index("PS_HW6_RTOS_ObjectTraceService();"),
                        body.index("PS_HW6_RTOS_ObjectLatencyBegin("))
        service = firmware_function(source, "PS_HW6_RTOS_ObjectTraceService")
        self.assertNotIn("g_ps_object_lpbam_probe", service)
        self.assertNotIn("RequestRuntimeClock", service)
        self.assertIn("PS_HW6_RTOS_ObjectTraceService();",
                      firmware_function(source, "PS_HW6_RTOS_ObjectService"))
        helper = (firmware / "__fw0_object_trace_enable.gdb").read_text()
        self.assertIn("g_ps_object_trace_probe.api_version != 3", helper)
        self.assertNotIn("g_ps_object_lpbam_probe.enabled", helper)
        self.assertEqual(["set pagination off", "set g_ps_object_trace_probe.request = 1"],
                         [line.strip() for line in helper.splitlines() if line.strip().startswith("set ")])
    def test_owner_markers_enclose_actual_driver_calls(self):
        root = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0/Core/Src"
        power = firmware_function((root / "ps_hw6_owner_services.c").read_text(),
                                  "PS_HW6_PowerOwner_RunSnapshot")
        joystick = firmware_function((root / "ps_hw6_owner_state_machines.c").read_text(),
                                     "PS_HW6_SM_RunJoystickCardinalProbe")
        for body, phase, driver, result in (
            (power, "PMIC_SNAPSHOT", "ps_dev_adp5360_read_power_snapshot", "status"),
            (joystick, "JOYSTICK_WAKE", "ps_dev_tmag3001_wake_continuous", "driver_status"),
            (joystick, "JOYSTICK_READ", "ps_dev_tmag3001_read_raw_sample", "driver_status"),
            (joystick, "JOYSTICK_SUSPEND", "ps_dev_tmag3001_suspend", "driver_status"),
        ):
            with self.subTest(phase=phase):
                begin = body.index(f"PS_HW6_TraceObjectOwnerBegin(PS_TRACE_OWNER_{phase})")
                call = body.index(driver + "(", begin)
                end = body.index(f"PS_HW6_TraceObjectOwnerEnd(PS_TRACE_OWNER_{phase}", call)
                self.assertLess(begin, call)
                self.assertLess(call, end)
                self.assertIn(f"trace_sequence, (uint32_t){result})", body[end:])
                self.assertEqual(body.count(driver + "("), 1)

    def test_local_timestamp_source_is_cycle_counter(self):
        root = Path(__file__).resolve().parents[3]
        port = (root / "firmware/peepshow_hw6_fw0/Middlewares/ST/threadx/ports/cortex_m33/gnu/inc/tx_port.h").read_text()
        self.assertIn("#define TX_TRACE_TIME_SOURCE                    *((volatile ULONG *) 0xE0001004)", port)
        self.assertIn("#define TX_TRACE_TIME_MASK                      0xFFFFFFFFUL", port)
