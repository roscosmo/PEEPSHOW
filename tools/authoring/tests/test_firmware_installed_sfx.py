"""Resident installed V2 audio admission using the public compiler output."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import struct
import subprocess
import unittest

import test_firmware_object_scene_replacement as replacement
from test_object_egg import repair
from build_installed_sfx_fixture import installed_sfx_bundle
from peepshow_authoring.compiler import build_development_egg_v2, build_readiness_issues, build_egg
from peepshow_authoring.egg_format import parse_egg, HEADER, CHUNK_ENTRY
from peepshow_authoring.v2_export import public_v2_export_profile


class InstalledSfxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        replacement.ObjectSceneReplacementTests.setUpClass.__func__(cls)
        cls.bundle = installed_sfx_bundle()
        cls.blob = build_egg(cls.bundle)

    def run_blob(self, blob, *args):
        path = self.work / "installed_sfx.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        result = subprocess.run([str(self.exe), str(path), *args],
            capture_output=True, text=True, timeout=20, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout

    def test_installed_effects_and_real_private_raster_admission(self):
        self.assertLessEqual(len(self.blob), 65536)
        self.assertIn("atomic effects and catalog lifetime passed", self.run_blob(self.blob, "audio"))

    def test_trusted_scene_decode_skips_package_rescans_and_revokes_on_reload(self):
        self.assertIn("lifetime invalidation passed", self.run_blob(self.blob, "trusted"))

    def test_public_audio_matches_hardware_tested_bytes(self):
        self.assertTrue(public_v2_export_profile()["audio"])
        self.assertEqual([], build_readiness_issues(self.bundle))
        self.assertEqual(build_development_egg_v2(self.bundle), self.blob)
        self.assertEqual("af1fe2d09a3b527b47a640a355be318f29a37380315c9ee5e7ad53cea5feff9c",
                         hashlib.sha256(self.blob).hexdigest())
        self.assertEqual(2, len(parse_egg(self.blob).audio_assets))

    def test_invalid_audio_bytes_and_cue_are_rejected_before_activation(self):
        package = parse_egg(self.blob, _development_v2=True)
        bank = next(chunk for chunk in package.chunks if chunk.chunk_type == 11)
        cues = next(chunk for chunk in package.chunks if chunk.chunk_type == 12)
        bad_block = bytearray(self.blob)
        bad_block[bank.offset + 16 + 2] = 89  # Invalid ADPCM initial step index.
        bad_cue = bytearray(self.blob)
        struct.pack_into("<H", bad_cue, cues.offset + 16 + 2, 0xFFFF)
        for blob in (repair(bad_block), repair(bad_cue)):
            self.run_blob(blob, "selection", "0", "0")

    def test_audio_does_not_admit_exit_actions_or_shell_actions(self):
        for kind in ("exit", "shell"):
            scenes = deepcopy(self.bundle.scenes)
            route = scenes[1]["routes"][-1 if kind == "exit" else 0]
            route["actions"] = ([{"kind": "play_sfx", "cue_ref": "short.cue"}]
                                if kind == "exit" else [{"kind": "exit_to_shell"}])
            blob = build_development_egg_v2(replace(self.bundle, scenes=scenes))
            self.run_blob(blob, "selection", "0", "0")

    def test_resident_audio_capacity_includes_footer(self):
        header = HEADER.unpack_from(self.blob)
        package = parse_egg(self.blob, _development_v2=True)
        first = min(chunk.offset for chunk in package.chunks)
        for size, expected in ((65536, "1"), (65540, "0")):
            padding = size - len(self.blob)
            blob = bytearray(self.blob[:first] + bytes(padding) + self.blob[first:])
            struct.pack_into("<I", blob, 8, size)
            struct.pack_into("<I", blob, 16, size - 40)
            for index in range(header[6]):
                offset = header[4] + index * CHUNK_ENTRY.size + 16
                struct.pack_into("<I", blob, offset, struct.unpack_from("<I", blob, offset)[0] + padding)
            self.run_blob(repair(blob), "selection", expected, "2" if expected == "1" else "0")


if __name__ == "__main__":
    unittest.main()
