"""Native regression tests for candidate preflight and owner workflow ordering."""
from __future__ import annotations

import hashlib
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
import zlib

TOOL_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_ROOT))
from peepshow_authoring.compiler import build_egg
from peepshow_authoring.project import load_project
from peepshow_authoring.egg_format import CHUNK_ENTRY, HEADER, RENDER_HEADER, RENDER_MODEL_RECORD


def firmware_function(source: str, name: str) -> str:
    """Extract a file-scope definition (not its forward declaration) for host C."""
    match = re.search(r"^static \w+ " + re.escape(name) + r"\([^;{}]*\)\s*\{", source, re.MULTILINE)
    if match is None:
        raise AssertionError(f"Missing production function: {name}")
    end = source.index("\n}", match.end()) + 2
    return source[match.start():end] + "\n"


class FirmwarePackageWorkflowTests(unittest.TestCase):
    def test_shell_recovery_and_input(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        functions = [
            "LogicalSourceIsButton", "LogicalSourceIsJoystickDirection", "RouterEventForLogicalSource",
            "ShutdownStateIsBlocking", "SystemOverlayActive", "InputPolicySystemOverlayActive", "InputPolicyShellPageActive",
            "InputPolicyRuntimeClassOwnsInput", "SendRuntimeInputEvent", "DeliverInputLogicalEvent",
            "LogicalSourceForJoystickDirection", "DeliverJoystickLogicalEvent", "JoystickLogicalPolicy",
            "RuntimeRecord", "RuntimeReturnPage", "RuntimeFallbackClass", "RuntimeSetState",
            "RuntimeErrorInstaller", "RuntimeRecordAdmission", "RequestPackageInstallErrorUi",
            "RuntimePackageReplacementFail", "RuntimePackageActivateStub",
        ]
        definitions = "\n".join(firmware_function(source, "PS_HW6_RTOS_" + name) for name in functions)
        constants = re.findall(r"^#define PS_HW6_RTOS_INPUT_POLICY_.*$", source, re.MULTILINE)
        for suffix in ("COMMAND_RUNTIME_INTERACTION_CUE", "RUNTIME_CLOCK_REASON_RELEASE", "STATUS_NOT_RUN"):
            constants.append(re.search(r"^#define PS_HW6_RTOS_" + suffix + r"\s+[^\n]+", source, re.MULTILINE).group(0))
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "shell_under_test.inc").write_text(definitions, encoding="ascii")
            (work / "shell_constants.inc").write_text("\n".join(constants), encoding="ascii")
            (work / "stm32u5xx_hal.h").write_text(
                "typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;\n", encoding="ascii")
            (work / "tx_api.h").write_text(
                "#include <stdint.h>\ntypedef uint32_t UINT;\ntypedef uint32_t ULONG;\n"
                "typedef struct { uint32_t unused; } TX_BYTE_POOL;\n", encoding="ascii")
            executable = work / "shell.exe"
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_shell_recovery.c")),
                str(firmware / "Core/Src/ps_ui_router.c"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                                    timeout=10, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("shell navigation and recovery checks passed", result.stdout)

    def test_owner_workflow_ordering(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/ps_hw6_rtos_probe.c").read_text(encoding="utf-8")
        start = source.index("static uint32_t PS_HW6_RTOS_PackageWorkflowPowerOverlay(")
        end = source.index("static void PS_HW6_RTOS_HandleUiRouterAction(", start)
        definitions = source[start:end]
        start = source.index("static void PS_HW6_RTOS_PackageWorkflowPrepare(void)\n{")
        end = source.index("static void PS_HW6_RTOS_RunStorageFlashInitRequest(void)\n{", start)
        definitions += source[start:end]
        wanted = {
            "PACKAGE_WORKFLOW_MAGIC", "PACKAGE_VALIDATE_ACK", "EVENT_DEBUG_INDEX",
            "COMMAND_RUNTIME_PACKAGE_VALIDATE", "COMMAND_RUNTIME_PACKAGE_REPLACE",
            "OWNER_ACK_WAIT_TICKS", "STATUS_NOT_RUN", "ADMISSION_ACTION_NONE",
            "DISPLAY_CLOCK_REASON_TRANSFER", "DISPLAY_CLOCK_REASON_RELEASE",
            "UI_CLOCK_REASON_REACTIVE_TRANSACTION", "UI_CLOCK_REASON_RELEASE",
        }
        constants = []
        for suffix in sorted(wanted):
            match = re.search(r"^#define PS_HW6_RTOS_" + suffix + r"\s+[^\n]+", source, re.MULTILINE)
            self.assertIsNotNone(match, suffix)
            constants.append(match.group(0))
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "workflow_under_test.inc").write_text(definitions, encoding="ascii")
            (work / "workflow_constants.inc").write_text("\n".join(constants), encoding="ascii")
            (work / "tx_api.h").write_text(
                "#include <stdint.h>\ntypedef uint32_t UINT;\ntypedef uint32_t ULONG;\n"
                "typedef struct { uint32_t unused; } TX_BYTE_POOL;\n", encoding="ascii")
            executable = work / "workflow.exe"
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"),
                str(Path(__file__).with_name("native_package_workflow.c")),
                str(firmware / "Core/Src/ps_ui_router.c"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                                    timeout=10, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("reservation checks passed", result.stdout)

    def test_candidate_validation_isolation(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc")
        if not compiler:
            compiler = "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC not installed; set HOST_CC")
        root = TOOL_ROOT.parents[1]
        firmware = root / "firmware/peepshow_hw6_fw0"
        baseline = build_egg(load_project(TOOL_ROOT / "peepshow_authoring/test_project.peepproj"))
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            args = []

            def fixture(name, data, reason=None):
                path = work / (name + ".egg")
                path.write_bytes(data)
                path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(data[:-40]).digest())
                args.append(str(path))
                if reason is not None:
                    args.append(str(reason))

            fixture("baseline", baseline)
            fixture("valid", baseline, 0)
            # Preserve valid container CRCs/digest while emptying just one model.
            render_entries = []
            header = HEADER.unpack_from(baseline)
            for index in range(header[6]):
                entry_offset = HEADER.size + index * CHUNK_ENTRY.size
                entry = CHUNK_ENTRY.unpack_from(baseline, entry_offset)
                if entry[0] == 5:
                    render_entries.append((entry_offset, entry))
            self.assertGreaterEqual(len(render_entries), 2)
            for scene_index, (entry_offset, entry) in enumerate(render_entries):
                for state_index in range(RENDER_HEADER.unpack_from(baseline, entry[4])[3]):
                    data = bytearray(baseline)
                    model = entry[4] + RENDER_HEADER.size + state_index * RENDER_MODEL_RECORD.size
                    struct.pack_into("<H", data, model + 6, 0)
                    struct.pack_into("<I", data, entry_offset + 24,
                                     zlib.crc32(data[entry[4]:entry[4] + entry[5]]))
                    data[-32:] = hashlib.sha256(data[:-40]).digest()
                    fixture(f"empty_{scene_index}_{state_index}", data, 11)
            corrupt = bytearray(baseline)
            corrupt[-1] ^= 1
            fixture("bad_digest", corrupt, 4)
            corrupt = bytearray(baseline)
            corrupt[44] ^= 1
            fixture("bad_header_crc", corrupt, 3)
            # A structurally valid file can still put required metadata outside
            # the target's resident prefix. It must fail before installation.
            first_chunk = min(CHUNK_ENTRY.unpack_from(baseline, HEADER.size +
                              index * CHUNK_ENTRY.size)[4] for index in range(header[6]))
            padded = bytearray(baseline[:first_chunk] + bytes(65536) + baseline[first_chunk:])
            struct.pack_into("<I", padded, 8, len(padded))
            struct.pack_into("<I", padded, 16, len(padded) - 40)
            for index in range(header[6]):
                offset = HEADER.size + index * CHUNK_ENTRY.size + 16
                struct.pack_into("<I", padded, offset, struct.unpack_from("<I", padded, offset)[0] + 65536)
            struct.pack_into("<I", padded, 44, 0)
            struct.pack_into("<I", padded, 44, zlib.crc32(padded[:64]))
            padded[-32:] = hashlib.sha256(padded[:-40]).digest()
            fixture("metadata_outside_resident_prefix", padded, 6)
            from test_authoring_model import make_audio_project
            audio_project = make_audio_project(work)
            with wave.open(str(audio_project / "assets/select.wav"), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(16000)
                wav.writeframes(struct.pack("<h", 1000) * 192000)
            large_audio = build_egg(load_project(audio_project))
            self.assertGreater(len(large_audio), 65536)
            fixture("large_streamed_audio", large_audio, 0)
            fixture("valid_after_rejections", baseline, 0)
            # Optional reproduction, never a required external-worktree fixture.
            reported = Path("G:/PEEPSHOW-PeepStudio/workbench/peep-studio/Authoring_pass.peepproj/authoring_pass_test.egg")
            if reported.is_file():
                data = reported.read_bytes()
                if hashlib.sha256(data).hexdigest() == "e8c5445a4e9c3aba20b239ddb087dc984394f0a1dc167dca58379ff492387c2e":
                    fixture("reported_empty_scene", data, 11)
            executable = work / "package_validation.exe"
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                       "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                       "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
                       str(Path(__file__).with_name("native_package_validation.c")), "-o", str(executable)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable), *args], capture_output=True, text=True,
                                    timeout=20, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("live-context isolation checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
