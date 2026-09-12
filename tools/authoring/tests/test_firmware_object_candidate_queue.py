"""Real candidate decoder/raster work under a deterministic owner-queue harness."""
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_display_admission as admission
from test_firmware_package_workflow import firmware_function
from build_object_development import dual_fixture_bundle
from peepshow_authoring.compiler import build_development_egg_v2, build_egg
from peepshow_authoring.project import load_project


class ObjectCandidateQueueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        admission.ObjectDisplayAdmissionTests.setUpClass.__func__(cls)
        source = (cls.firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        cls.source = source
        start = source.index("static volatile uint32_t ps_candidate_busy;")
        end = source.index("volatile ps_hw6_object_lpbam_probe_t", start)
        (cls.work / "candidate_globals.inc").write_text(source[start:end], encoding="ascii")
        (cls.work / "candidate_queue_under_test.inc").write_text("\n".join(
            firmware_function(source, "PS_HW6_RTOS_Candidate" + name) for name in
            ("Release", "Reap", "Display", "Send", "Check", "Begin", "Service")) + "\n" +
            firmware_function(source, "PS_HW6_RTOS_InstalledObjectCheck") + "\n" +
            firmware_function(source, "PS_HW6_RTOS_RunPackageValidation"), encoding="ascii")
        cls.exe = cls.work / "candidate_queue.exe"
        result = subprocess.run([os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe"),
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_candidate_queue.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_real_candidate_success_rejection_and_owner_lifetimes(self):
        blob = build_development_egg_v2(dual_fixture_bundle())
        path = self.work / "dual.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_owner_dispatch_and_both_sleep_paths_include_candidate(self):
        owner = firmware_function(self.source, "PS_HW6_RTOS_OwnerEntry")
        self.assertIn("message[0] == PS_HW6_RTOS_OBJECT_CANDIDATE_MAGIC", owner)
        self.assertIn("PS_HW6_RTOS_CandidateDisplay(message);", owner)
        self.assertIn("PS_HW6_RTOS_CandidateService();", owner)
        command = firmware_function(self.source, "PS_HW6_RTOS_HandleRuntimeCommand")
        self.assertIn("PS_HW6_RTOS_RunPackageValidation(clock_status);", command)
        for name in ("RunStop2EligibilityDryRun", "Stop2AutoRuntimeAllowsIdle"):
            function = firmware_function(self.source, "PS_HW6_RTOS_" + name)
            self.assertIn("ps_candidate_busy != 0UL", function)
            self.assertIn("g_ps_object_candidate_request != 0UL", function)

    def test_exact_gui_installed_entry_and_transaction_rollback(self):
        project = Path(__file__).resolve().parents[3] / "examples/authoring/native_v2_installation.peepproj"
        blob = build_egg(load_project(project))
        path = self.work / "gui_install.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path), "installed"],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_workflow_reports_actual_rejection_and_not_stale_candidate(self):
        project = Path(__file__).resolve().parents[3] / "examples/authoring/native_v2_installation.peepproj"
        blob = build_egg(load_project(project))
        path = self.work / "gui_workflow.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path), "workflow"],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("workflow rejection reasons passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
