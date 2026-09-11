"""Full binary profile preflight, with live V1 and V2 runtime isolation."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import TOOL_ROOT
from test_object_egg import object_bundle, repair
from build_object_development import (
    DEFAULT_PROJECT, fixture_bundle, timer_fixture_bundle, sfx_fixture_bundle,
    structured_fixture_bundle, dual_fixture_bundle,
)
from peepshow_authoring.compiler import build_egg, build_development_egg_v2
from peepshow_authoring.project import load_project
from peepshow_authoring.object_egg import parse_development_egg_v2, OBJECT_HEADER
from peepshow_authoring.egg_format import HEADER, CHUNK_ENTRY, RENDER_HEADER


class V2ProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        if not Path(compiler).is_file():
            raise unittest.SkipTest("native GCC required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.exe = cls.work / "profile.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
            str(Path(__file__).with_name("native_v2_profile.c")), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def check_candidates(self, active_v2):
        args = []

        def fixture(name, blob, reason=0, loader=0, item=0xFFFFFFFF):
            path = self.work / f"{name}.egg"
            path.write_bytes(blob)
            path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
            args.extend([str(path), str(reason), str(loader), str(item)])
            return path

        legacy = build_egg(load_project(TOOL_ROOT / "peepshow_authoring/test_project.peepproj"))
        gui = load_project(DEFAULT_PROJECT.parent / "native_v2_timer_four_frames.peepproj")
        good = build_development_egg_v2(gui)
        baseline = self.work / "baseline.egg"
        baseline.write_bytes(good if active_v2 else legacy)
        baseline.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(baseline.read_bytes()[:-40]).digest())
        fixture("legacy_not_v2", legacy, 3, 2)
        fixture("gui", good)
        installed_gui = load_project(TOOL_ROOT.parents[1] / "examples/authoring/native_v2_installation.peepproj")
        fixture("public_export_gui", build_egg(installed_gui))
        fixture("structured", build_development_egg_v2(structured_fixture_bundle()))
        fixture("dual", build_development_egg_v2(dual_fixture_bundle()))
        fixture("timer_controls", build_development_egg_v2(timer_fixture_bundle()))
        mixed = build_development_egg_v2(object_bundle())
        fixture("mixed_multiscene", mixed, 4)
        other_render = next(chunk for chunk in parse_development_egg_v2(mixed).chunks
                            if chunk.chunk_type == 5)
        corrupt = bytearray(mixed)
        struct.pack_into("<H", corrupt, other_render.offset + RENDER_HEADER.size + 6, 0)
        fixture("bad_non_entry_scene", repair(corrupt), 3, 11)
        scene = deepcopy(gui.scenes[0])
        scene["interaction_policy"].update(mode="timeout", inactive_route="preserve_scene")
        fixture("timeout", build_development_egg_v2(replace(gui, scenes=(scene,))), 6)
        fixture("audio", build_development_egg_v2(sfx_fixture_bundle()), 7)
        fixture("shell_action", build_development_egg_v2(fixture_bundle()), 10, item=0)
        corrupt = bytearray(good)
        corrupt[-1] ^= 1
        fixture("digest", corrupt, 3, 4)
        corrupt = bytearray(good)
        corrupt[44] ^= 1
        fixture("header_crc", corrupt, 3, 3)
        package = parse_development_egg_v2(good)
        objects = next(chunk for chunk in package.chunks if chunk.chunk_type == 13)
        corrupt = bytearray(good)
        corrupt[objects.offset + OBJECT_HEADER.size + 2] = 9
        corrupt[-32:] = hashlib.sha256(corrupt[:-40]).digest()
        fixture("chunk_crc", corrupt, 3, 6)
        fixture("unsupported_object", repair(corrupt), 3, 11)
        # Intact checksums cannot make an invalid control span safe.
        controls = next(chunk for chunk in package.chunks if chunk.chunk_type == 14)
        corrupt = bytearray(good)
        struct.pack_into("<H", corrupt, controls.offset + 10, 65535)
        fixture("control_bounds", repair(corrupt), 3, 11)
        # The profile ceiling includes the footer, and is inclusive at 64 KiB.
        header = HEADER.unpack_from(good)
        first = min(chunk.offset for chunk in package.chunks)
        padding = 65536 - len(good)
        padded = bytearray(good[:first] + bytes(padding) + good[first:])
        struct.pack_into("<I", padded, 8, len(padded))
        struct.pack_into("<I", padded, 16, len(padded) - 40)
        for index in range(header[6]):
            offset = header[4] + index * CHUNK_ENTRY.size + 16
            struct.pack_into("<I", padded, offset, struct.unpack_from("<I", padded, offset)[0] + padding)
        fixture("exact_capacity", repair(padded))
        fixture("recovery_after_failures", good)
        result = subprocess.run([str(self.exe), str(baseline), "v2" if active_v2 else "v1", *args],
            capture_output=True, text=True, timeout=20, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("isolation and production rejection passed", result.stdout)

    def test_preflight_does_not_disturb_live_legacy_package(self):
        self.check_candidates(False)

    def test_preflight_does_not_disturb_live_object_bank_or_animation(self):
        self.check_candidates(True)


if __name__ == "__main__":
    unittest.main()
