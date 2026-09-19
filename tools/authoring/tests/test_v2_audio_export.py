"""Public resident V2 SFX build, capability and rejection contract."""
import base64
from copy import deepcopy
from dataclasses import replace
import hashlib
import io
import struct
import unittest
import wave

from test_v2_export import FIXTURE
from test_object_egg import repair
from build_installed_sfx_fixture import installed_sfx_bundle
from peepshow_authoring.compiler import build_egg, build_development_egg_v2, build_readiness_issues, EggCompileError
from peepshow_authoring.egg_format import parse_egg, EggFormatError, HEADER, CHUNK_ENTRY
from peepshow_authoring.protocol import ServiceRequest, ProtocolError
from peepshow_authoring.service import AuthoringService
from peepshow_authoring.target_profile import TARGET_SAMPLED_SFX
from peepshow_authoring.v2_export import LIMITS, public_v2_audio_profile


class V2AudioExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bundle = installed_sfx_bundle()
        cls.blob = build_egg(cls.bundle)

    def service(self, bundle=None):
        service = AuthoringService()
        loaded = service._activate_bundle(bundle or self.bundle)
        return service, {"project_revision": loaded["project_revision"]}, loaded

    def call(self, service, operation, params):
        return service.handle(ServiceRequest(operation, operation, params))

    def test_capabilities_and_service_build_match_tested_firmware_bytes(self):
        service, params, loaded = self.service()
        hello = self.call(service, "service.hello", {})
        profile = hello["package_export"]["v2_profile"]
        self.assertEqual(52, hello["service_api_version"])
        self.assertEqual(6, profile["profile_revision"])
        self.assertTrue(profile["audio"])
        self.assertEqual(65536, profile["limits"]["package_bytes"])
        audio = profile["audio_profile"]
        self.assertEqual(public_v2_audio_profile(), audio)
        self.assertEqual(audio, hello["scene_object_authoring"]["audio_export"])
        self.assertEqual(["play_sfx"], audio["action_kinds"])
        self.assertEqual(["local_transition", "local_timer_handler", "scene_exit", "timer_scene_exit"], audio["action_contexts"])
        self.assertEqual("after_destination_admission_and_commit", audio["scene_exit_commit"])
        self.assertEqual("no_cues", audio["scene_exit_rejection"])
        self.assertFalse(audio["sequential_playback"])
        self.assertEqual("whole_package", audio["residency"])
        self.assertEqual("stop_and_discard", audio["package_suspend"])
        self.assertEqual("new_requests_only", audio["package_resume"])
        self.assertEqual(["play_sfx"], profile["scene_exit_action_kinds"])
        for key in ("maximum_assets", "maximum_cues", "voice_limit", "sample_rate_hz", "channels", "block_samples"):
            self.assertEqual(TARGET_SAMPLED_SFX[key], audio[key])
        for caps in loaded["scene_capabilities"].values():
            self.assertTrue(caps["export_ready"])
            self.assertEqual(audio, caps["audio_export"])
            self.assertEqual(["play_sfx"], caps["scene_exit_action_kinds"])
        before = service._bundle.canonical_bytes()
        result = self.call(service, "project.build_package", params)
        blob = base64.b64decode(result["package"]["blob_base64"])
        self.assertEqual(self.blob, blob)
        self.assertEqual(54696, len(blob))
        self.assertEqual("af1fe2d09a3b527b47a640a355be318f29a37380315c9ee5e7ad53cea5feff9c",
                         hashlib.sha256(blob).hexdigest())
        self.assertEqual(2, result["package"]["audio_asset_count"])
        self.assertEqual(2, result["package"]["audio_cue_count"])
        report = result["compatibility_report"]["budgets"]
        self.assertEqual((50920, 65536), (report["audio"]["used_bytes"], report["audio"]["limit_bytes"]))
        self.assertEqual("package_size", report["audio"]["shared_with"])
        self.assertFalse(report["audio"]["independent_budget"])
        self.assertEqual(before, service._bundle.canonical_bytes())
        self.assertEqual(result, self.call(service, "project.build_package", params))
        with self.assertRaises(ProtocolError):
            self.call(service, "project.build_package", {"project_revision": params["project_revision"] - 1})

    def test_timer_only_sfx_is_reported_and_audition_uses_packaged_bytes(self):
        scenes = deepcopy(self.bundle.scenes)
        for scene in scenes:
            for route in scene["routes"]:
                route["actions"] = [a for a in route["actions"] if a["kind"] != "play_sfx"]
        bundle = replace(self.bundle, scenes=scenes)
        service, params, _ = self.service(bundle)
        report = self.call(service, "project.build_package", params)["compatibility_report"]
        sfx = next(c for c in report["capabilities"] if c["capability"] == "audio.sampled_sfx")
        self.assertEqual(["home"], sfx["requested_by"])
        scenes_report = {scene["scene_id"]: scene for scene in report["scenes"]}
        self.assertIn("audio.sampled_sfx", scenes_report["home"]["required_capabilities"])
        self.assertEqual([], scenes_report["visit"]["required_capabilities"])
        audition = self.call(service, "project.audio_audition", {**params, "cue_id": "long.cue"})
        with wave.open(io.BytesIO(base64.b64decode(audition["audio"]["wav_base64"])), "rb") as wav:
            self.assertEqual((1, 16000, 96000), (wav.getnchannels(), wav.getframerate(), wav.getnframes()))

    def test_oversized_audio_blocks_whole_project_but_not_audition(self):
        long = next(a for a in self.bundle.audio_assets if a.duration_ms == 6000)
        bundle = replace(self.bundle, audio_assets=(*self.bundle.audio_assets, replace(long, asset_id="extra")))
        self.assertIn("V2_CAPACITY", {i["code"] for i in build_readiness_issues(bundle)})
        service, params, loaded = self.service(bundle)
        self.assertTrue(all(c["audio_export"]["supported"] for c in loaded["scene_capabilities"].values()))
        self.assertTrue(all(not c["export_ready"] for c in loaded["scene_capabilities"].values()))
        with self.assertRaises(ProtocolError):
            self.call(service, "project.build_package", params)
        self.assertEqual(6000, self.call(service, "project.audio_audition", {**params, "cue_id": "long.cue"})["audio"]["duration_ms"])
        with self.assertRaises(EggFormatError):
            parse_egg(build_development_egg_v2(bundle))

    def test_audio_catalog_count_bounds_in_source_and_public_parser(self):
        short = next(a for a in self.bundle.audio_assets if a.duration_ms == 80)
        for assets, cues, allowed in ((32, 64, True), (33, 64, False), (32, 65, False), (1, 0, False)):
            with self.subTest(assets=assets, cues=cues):
                bank = tuple(replace(short, asset_id=f"sample_{i}") for i in range(assets))
                catalog = tuple({"cue_id": name, "asset_ref": "sample_0", "priority": i % 256, "volume": 255}
                    for i, name in enumerate(["long.cue", "short.cue", *(f"cue_{i}" for i in range(max(0, cues - 2)))])) if cues else ()
                scenes = deepcopy(self.bundle.scenes)
                if not cues:
                    for scene in scenes:
                        for route in (*scene["routes"], *scene["event_handlers"]):
                            route["actions"] = [a for a in route["actions"] if a["kind"] != "play_sfx"]
                bundle = replace(self.bundle, scenes=scenes, audio_assets=bank, audio_cues=catalog)
                if allowed:
                    package = parse_egg(build_egg(bundle))
                    self.assertEqual((32, 64), (len(package.audio_assets), len(package.audio_cues)))
                else:
                    self.assertIn("V2_AUDIO_CATALOG", {i["code"] for i in build_readiness_issues(bundle)})
                    with self.assertRaises(EggCompileError):
                        build_egg(bundle)
                    with self.assertRaises(EggFormatError):
                        parse_egg(build_development_egg_v2(bundle))

    def test_public_scene_exit_sfx_for_input_and_timer(self):
        for timer in (False, True):
            scenes = deepcopy(self.bundle.scenes)
            if timer:
                scenes[0]["event_handlers"][0]["target_scene"] = "visit"
                scenes[0]["event_handlers"][0]["actions"] = [{"kind": "play_sfx", "cue_ref": "long.cue"}]
            else:
                scenes[1]["routes"][-1]["actions"] = [{"kind": "play_sfx", "cue_ref": "short.cue"}]
            bundle = replace(self.bundle, scenes=scenes)
            self.assertEqual([], build_readiness_issues(bundle))
            blob = build_egg(bundle)
            self.assertEqual(blob, build_development_egg_v2(bundle))
            self.assertEqual(2, len(parse_egg(blob).audio_cues))
            service, params, loaded = self.service(bundle)
            self.assertTrue(all(c["export_ready"] for c in loaded["scene_capabilities"].values()))
            result = self.call(service, "project.build_package", params)
            self.assertEqual(blob, base64.b64decode(result["package"]["blob_base64"]))

    def test_public_exit_fixture_matches_hardware_tested_bytes(self):
        from build_scene_exit_sfx_fixture import scene_exit_sfx_bundle
        bundle = scene_exit_sfx_bundle()
        service, params, _ = self.service(bundle)
        blob = base64.b64decode(self.call(service, "project.build_package", params)["package"]["blob_base64"])
        self.assertEqual(build_development_egg_v2(bundle), blob)
        self.assertEqual(54660, len(blob))
        self.assertEqual("610119f8944e1965cb4b29e081085eb957469b33db6cc7c0c8fe71849cac0f5c",
                         hashlib.sha256(blob).hexdigest())

    def test_public_parser_rejects_invalid_cue_values_and_bank(self):
        package = parse_egg(self.blob)
        cue = next(c for c in package.chunks if c.chunk_type == 12)
        bank = next(c for c in package.chunks if c.chunk_type == 11)
        for offset, value in ((2, 65535), (4, 256), (6, 256), (8, 1)):
            blob = bytearray(self.blob)
            struct.pack_into("<H", blob, cue.offset + 16 + offset, value)
            with self.assertRaises(EggFormatError):
                parse_egg(repair(blob))
        blob = bytearray(self.blob)
        blob[bank.offset + 18] = 89
        with self.assertRaises(EggFormatError):
            parse_egg(repair(blob))

    def test_public_audio_resident_limit_includes_footer(self):
        header = HEADER.unpack_from(self.blob)
        first = min(c.offset for c in parse_egg(self.blob).chunks)
        for size in (65536, 65540):
            padding = size - len(self.blob)
            blob = bytearray(self.blob[:first] + bytes(padding) + self.blob[first:])
            struct.pack_into("<I", blob, 8, size)
            struct.pack_into("<I", blob, 16, size - 40)
            for index in range(header[6]):
                offset = header[4] + index * CHUNK_ENTRY.size + 16
                struct.pack_into("<I", blob, offset, struct.unpack_from("<I", blob, offset)[0] + padding)
            if size == LIMITS["package_bytes"]:
                self.assertEqual(2, len(parse_egg(repair(blob)).audio_cues))
            else:
                with self.assertRaises(EggFormatError):
                    parse_egg(repair(blob))

    def test_non_audio_v2_and_legacy_audio_advertisements_are_unchanged(self):
        service = AuthoringService()
        loaded = self.call(service, "project.load", {"path": str(FIXTURE)})
        self.assertTrue(all(c["export_ready"] for c in loaded["scene_capabilities"].values()))
        hello = self.call(service, "service.hello", {})
        self.assertEqual(4194304, hello["state_scene_audio"]["maximum_bank_bytes"])
        self.assertEqual(5, hello["state_scene_audio"]["voice_limit"])


if __name__ == "__main__":
    unittest.main()
