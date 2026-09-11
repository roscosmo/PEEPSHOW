"""Opt-in object playback: production compositor, payloads and sleep clock."""
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_lpbam_prepare as prepare
from test_firmware_package_workflow import firmware_function


class ObjectStop2Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        prepare.ObjectLpbamPrepareTests.setUpClass.__func__(cls)
        sources = {
            "display_renderer.c": ["DisplayRenderer_ValidateWaitingAnimation",
                "DisplayRenderer_PublishFullSceneWaiting", "DisplayRenderer_ResolveFullSceneWaiting",
                "DisplayRenderer_GetGuaranteedWaitingAnimation", "DisplayRenderer_SelectWaitingAnimation",
                "DisplayRenderer_GetSelectedWaitingAnimation", "DisplayRenderer_CopyWaitingAnimationFrame"],
            "ps_hw6_owner_services.c": ["PS_HW6_DisplayOwner_ComposeObjectStep",
                "PS_HW6_DisplayOwner_ObjectWaitingPosition", "PS_HW6_DisplayOwner_PublishDevelopmentWaiting",
                "PS_HW6_DisplayOwner_CompileWaitingAnimationPayload",
                "PS_HW6_DisplayOwner_SetObjectFirstInterval"],
            "ps_hw6_rtos_probe.c": ["PS_HW6_RTOS_ObjectRtcMilliseconds",
                "PS_HW6_RTOS_ObjectSleepClockBegin", "PS_HW6_RTOS_ObjectSleepClockFinish",
                "PS_HW6_RTOS_ObjectAdvance"],
        }
        functions = []
        for filename, names in sources.items():
            text = (cls.firmware / "Core/Src" / filename).read_text(encoding="utf-8")
            functions.extend(firmware_function(text, name) for name in names)
        (cls.work / "object_stop2_under_test.inc").write_text("\n".join(functions), encoding="ascii")
        cls.exe = cls.work / "stop2.exe"
        result = subprocess.run([os.environ["HOST_CC"], "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_stop2.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_numbered_frames_partial_commit_and_sleep_reconciliation(self):
        prepare.ObjectLpbamPrepareTests.check(self, prepare.ObjectLpbamPrepareTests.bundle(self))

    def test_runtime_waits_for_display_recovery_before_processing(self):
        text = (self.firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        entry = firmware_function(text, "PS_HW6_RTOS_RunStop2ControlledEntry")
        self.assertLess(entry.index("ps_object_runtime_busy != 0UL"),
                        entry.index("g_ps_object_lpbam_probe.barrier = 1UL"))
        self.assertLess(entry.index("g_ps_object_lpbam_probe.barrier = 1UL"),
                        entry.index("PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold()"))
        self.assertLess(entry.index("PS_HW6_RTOS_RequestDisplayLpbamAbort(1UL)"),
                        entry.index("g_ps_object_lpbam_probe.barrier = 0UL"))
        owner = firmware_function(text, "PS_HW6_RTOS_OwnerEntry")
        self.assertLess(owner.index("tx_queue_receive("), owner.index("PS_HW6_RTOS_OBJECT_WAKE_READY"))
        self.assertLess(owner.index("PS_HW6_RTOS_OBJECT_WAKE_READY"),
                        owner.index("ps_object_runtime_busy = 1UL"))
        text = (self.firmware / "Core/Src/ps_hw6_owner_state_machines.c").read_text(encoding="utf-8")
        sleep = firmware_function(text, "PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold")
        finish = sleep.index("PS_HW6_RTOS_ObjectSleepClockFinish()")
        self.assertLess(sleep.index("HAL_PWREx_EnterSTOP2Mode("), finish)
        self.assertLess(finish, sleep.index("PS_HW6_SM_RestoreThreadXSystick(", finish))


if __name__ == "__main__":
    unittest.main()
