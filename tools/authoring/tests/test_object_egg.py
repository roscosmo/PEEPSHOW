from copy import deepcopy
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.compiler import build_egg, build_development_egg_v2, EggCompileError
from peepshow_authoring.egg_format import (
    EggFormatError, parse_egg, HEADER, HEADER_CRC_OFFSET, CHUNK_ENTRY, FOOTER,
    GRAPH_HEADER, VARIABLE_RECORD, EVENT_RECORD, STATE_RECORD, ROUTE_RECORD_V2,
    GUARD_RECORD, OPERATION_RECORD, SCENE_HEADER, SCENE_RECORD,
    RENDER_HEADER, RENDER_MODEL_RECORD,
)
from peepshow_authoring.object_egg import (
    parse_development_egg_v2, OBJECT_HEADER, OBJECT_RECORD, CONTROL_HEADER,
    STATE_RANGE, OVERRIDE_RECORD, OBJECT_OPERATION,
)
from peepshow_authoring.project import load_project, apply_project_commands
from peepshow_authoring.scene_objects import plan_object_migration, materialize_object_migration


FIXTURE = Path(__file__).resolve().parents[1] / "peepshow_authoring/test_project.peepproj"


def object_bundle():
    bundle = load_project(FIXTURE)
    plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
    scene, clips = materialize_object_migration(bundle, plan, accept_continuous_animation=True)
    bundle, _ = apply_project_commands(bundle, [{"kind": "animation.upsert", "animation": clip} for clip in clips])
    bundle = replace(bundle, scenes=tuple(scene if item["scene_id"] == scene["scene_id"] else item for item in bundle.scenes))
    clip = next(clip for clip in bundle.animations if clip["animation_id"] == scene["objects"][0]["animation_ref"])
    actions = [
        {"kind": "object.set_position", "object_ref": "cursor", "x": 20},
        {"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": 2},
        {"kind": "object.move_by", "object_ref": "cursor", "dx": -2147483648, "dy": 2147483647},
        {"kind": "object.set_visibility", "object_ref": "cursor", "visible": False},
        {"kind": "object.set_frame", "object_ref": "cursor", "frame_ref": clip["frame_refs"][0]},
        {"kind": "object.clear_frame", "object_ref": "cursor"},
    ]
    bundle, _ = apply_project_commands(bundle, [{"kind": "object_actions.set", "scene_id": "state_demo",
        "owner_kind": "route", "owner_id": "center_to_right", "actions": actions}])
    return bundle


def repair(blob):
    result = bytearray(blob)
    header = HEADER.unpack_from(result)
    for index in range(header[6]):
        start = header[4] + index * CHUNK_ENTRY.size
        record = CHUNK_ENTRY.unpack_from(result, start)
        checksum = zlib.crc32(result[record[4]:record[4] + record[5]]) & 0xFFFFFFFF
        struct.pack_into("<I", result, start + 24, checksum)
    struct.pack_into("<I", result, HEADER_CRC_OFFSET, 0)
    struct.pack_into("<I", result, HEADER_CRC_OFFSET, zlib.crc32(result[:HEADER.size]) & 0xFFFFFFFF)
    result[header[5]:] = FOOTER.pack(b"END1", 1, FOOTER.size, hashlib.sha256(result[:header[5]]).digest())
    return bytes(result)


class ObjectEggTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bundle = object_bundle()
        cls.blob = build_development_egg_v2(cls.bundle)
        cls.package = parse_development_egg_v2(cls.blob)

    def chunk(self, kind):
        return next(chunk for chunk in self.package.chunks if chunk.chunk_type == kind)

    def corrupt(self, kind, relative, fmt, *values):
        blob = bytearray(self.blob)
        struct.pack_into(fmt, blob, self.chunk(kind).offset + relative, *values)
        return repair(blob)

    def rejects(self, kind, relative, fmt, *values):
        with self.assertRaises(EggFormatError):
            parse_development_egg_v2(self.corrupt(kind, relative, fmt, *values))

    def test_deterministic_mixed_scene_bytes(self):
        self.assertEqual(self.blob, build_development_egg_v2(object_bundle()))
        self.assertEqual(2, HEADER.unpack_from(self.blob)[1])
        self.assertEqual([2, 1], [scene["execution_model"] for scene in self.package.scenes])
        scene = self.package.scenes[0]
        self.assertEqual(7, scene["graph"]["format_version"])
        self.assertEqual(["cursor", "marker"], [obj["object_id"] for obj in scene["objects"]])
        self.assertTrue(scene["state_overrides"])
        self.assertTrue(self.package.assets)
        self.assertTrue(self.package.animations)
        self.assertNotIn("object_source", scene)
        self.assertNotIn("render_models", scene)

    def test_normal_export_and_reader_do_not_accept_development_format(self):
        with self.assertRaises(EggCompileError):
            build_egg(self.bundle)
        with self.assertRaisesRegex(EggFormatError, "restricted V2 export profile"):
            parse_egg(self.blob)
        legacy = load_project(FIXTURE)
        before = build_egg(legacy)
        build_development_egg_v2(self.bundle)
        self.assertEqual(before, build_egg(legacy))
        with self.assertRaises(EggFormatError):
            parse_development_egg_v2(before)
        with self.assertRaises(EggCompileError):
            build_development_egg_v2(legacy)

    def test_decoder_does_not_call_encoder_or_source_validation(self):
        with patch("peepshow_authoring.compiler._compile_graph", side_effect=AssertionError), \
             patch("peepshow_authoring.scene_objects.validate_object_model", side_effect=AssertionError):
            self.assertEqual(self.package, parse_development_egg_v2(self.blob))

    def test_all_object_actions_and_interleaving_survive(self):
        scene = self.package.scenes[0]
        self.assertEqual([1, 2, 3, 4, 5], [op["opcode"] for op in scene["object_operations"]])
        move = scene["object_operations"][1]
        self.assertEqual((-2147483648, 2147483647), (move["x"], move["y"]))
        self.assertEqual(3, move["axis_mask"])
        route = next(route for route in scene["graph"]["routes"] if route["route_id"] == "center_to_right")
        self.assertEqual([12, 1, 12, 12, 12, 12], [op["kind"] for op in route["operations"]])
        self.assertEqual(list(range(5)), [op["object_operation_index"] for op in route["operations"] if op["kind"] == 12])

    def test_objects_reject_unknown_kinds_flags_bounds_and_refs(self):
        for offset, fmt, value in ((2, "B", 9), (3, "B", 3), (5, "B", 128), (6, "H", 0),
                                   (10, "h", -1), (14, "H", 65535), (16, "H", 999), (18, "H", 1)):
            with self.subTest(offset=offset):
                self.rejects(13, OBJECT_HEADER.size + offset, fmt, value)
        self.rejects(13, 8, "H", 33)
        self.rejects(13, 10, "H", OBJECT_RECORD.size + 1)
        first = OBJECT_RECORD.unpack_from(self.chunk(13).payload, OBJECT_HEADER.size)[0]
        self.rejects(13, OBJECT_HEADER.size + OBJECT_RECORD.size, "H", first)

    def test_overrides_reject_unknown_masks_refs_and_unused_fields(self):
        header = CONTROL_HEADER.unpack_from(self.chunk(14).payload)
        start = CONTROL_HEADER.size + header[3] * STATE_RANGE.size
        for offset, fmt, value in ((0, "H", 999), (2, "H", 16), (2, "H", 0), (4, "i", -1),
                                   (12, "H", 0), (14, "H", 2)):
            with self.subTest(offset=offset, value=value):
                self.rejects(14, start + offset, fmt, value)
        self.rejects(14, CONTROL_HEADER.size, "H", 1)

    def test_operations_reject_invalid_mask_fields_and_references(self):
        header = CONTROL_HEADER.unpack_from(self.chunk(14).payload)
        start = CONTROL_HEADER.size + header[3] * STATE_RANGE.size + header[4] * OVERRIDE_RECORD.size
        for offset, fmt, value in ((0, "B", 6), (1, "B", 0), (1, "B", 8), (2, "H", 999),
                                   (8, "i", 1), (12, "H", 0), (14, "H", 1)):
            with self.subTest(offset=offset):
                self.rejects(14, start + offset, fmt, value)
        self.rejects(14, 12, "H", 1153)

    def test_scene_models_and_chunk_ownership_are_explicit(self):
        self.rejects(3, SCENE_HEADER.size + 14, "H", 3)
        self.rejects(3, SCENE_HEADER.size + 16, "I", 1)
        first = SCENE_RECORD.unpack_from(self.chunk(3).payload, SCENE_HEADER.size)
        self.rejects(3, SCENE_HEADER.size + SCENE_RECORD.size, "H", first[0])
        self.rejects(3, SCENE_HEADER.size + 10, "H", first[4])

    def test_mixed_legacy_scene_frame_references_are_also_checked(self):
        payload = self.chunk(5).payload
        header = RENDER_HEADER.unpack_from(payload)
        first_element = RENDER_HEADER.size + header[3] * RENDER_MODEL_RECORD.size
        # A valid string reference is not necessarily a valid sprite frame.
        not_a_frame = self.package.strings.index("state_demo")
        self.rejects(5, first_element + 2, "H", not_a_frame)

    def test_scene_timer_independent_handler_encodes_object_operation(self):
        scenes = deepcopy(self.bundle.scenes)
        scene = scenes[0]
        scene["event_bindings"] = [{"binding_id": "tick", "event_type": "time.scene_elapsed", "configuration": {"delay_ms": 5000, "start_policy": "scene_entry"}}]
        scene["event_handlers"] = [{"handler_id": "tick_handler", "event_ref": "tick", "guards": [],
                                    "actions": [{"kind": "object.move_by", "object_ref": "marker", "dy": 2}]}]
        bundle = replace(self.bundle, scenes=scenes)
        package = parse_development_egg_v2(build_development_egg_v2(bundle))
        handler = next(route for route in package.scenes[0]["graph"]["routes"] if route["route_id"] == "tick_handler")
        self.assertEqual((), handler["source_state_indexes"])
        self.assertIsNone(handler["target_state_index"])
        self.assertEqual(12, handler["operations"][0]["kind"])

    def test_record_sizes_are_frozen(self):
        self.assertEqual((16, 20, 20, 4, 16, 16), (OBJECT_HEADER.size, OBJECT_RECORD.size,
                         CONTROL_HEADER.size, STATE_RANGE.size, OVERRIDE_RECORD.size, OBJECT_OPERATION.size))

    def graph_offsets(self):
        header = GRAPH_HEADER.unpack_from(self.chunk(4).payload)
        variable, bindings, states, routes, sources, guards = header[4:10]
        state = GRAPH_HEADER.size + variable * VARIABLE_RECORD.size + bindings * EVENT_RECORD.size
        route = state + states * STATE_RECORD.size
        guard = route + routes * ROUTE_RECORD_V2.size + sources * 2
        operation = guard + guards * GUARD_RECORD.size
        return state, route, guard, operation

    def test_graph_revision_sentinels_and_operation_refs_are_checked(self):
        state, route, guard, operation = self.graph_offsets()
        self.rejects(4, 4, "H", 6)
        self.rejects(4, 26, "H", 0)
        self.rejects(4, state + 4, "H", 0)
        self.rejects(4, route + 8, "H", 1)
        self.rejects(4, guard + 3, "B", 1)
        payload = self.chunk(4).payload
        object_ops = [offset for offset in range(operation, operation + GRAPH_HEADER.unpack_from(payload)[10] * OPERATION_RECORD.size, OPERATION_RECORD.size)
                      if payload[offset] == 12]
        self.rejects(4, object_ops[0] + 2, "H", 999)
        self.rejects(4, object_ops[1] + 2, "H", 0)
        self.rejects(4, object_ops[0] + 1, "B", 1)
        self.rejects(4, object_ops[0], "B", 3)

    def test_header_version_flags_and_chunk_capability_are_checked(self):
        for offset, fmt, value in ((4, "H", 3), (28, "I", 1), (48, "B", 1),
                                  (HEADER.size + 4, "I", 1), (HEADER.size + 32, "Q", 1)):
            with self.subTest(offset=offset):
                blob = bytearray(self.blob)
                struct.pack_into(fmt, blob, offset, value)
                with self.assertRaises(EggFormatError):
                    parse_development_egg_v2(repair(blob))

    def test_truncation_and_integrity_are_checked_before_publication(self):
        for cut in (0, 4, 63, 64, len(self.blob) - 1):
            with self.subTest(cut=cut), self.assertRaises(EggFormatError):
                parse_development_egg_v2(self.blob[:cut])
        blob = bytearray(self.blob)
        blob[self.chunk(13).offset + OBJECT_HEADER.size + 10] ^= 1
        with self.assertRaisesRegex(EggFormatError, "SHA-256 mismatch"):
            parse_development_egg_v2(bytes(blob))

    def test_small_corruption_sweep_returns_only_valid_package_or_format_error(self):
        for kind in (3, 4, 13, 14):
            chunk = self.chunk(kind)
            for offset in range(min(96, chunk.size)):
                with self.subTest(kind=kind, offset=offset):
                    blob = bytearray(self.blob)
                    blob[chunk.offset + offset] ^= 0x80
                    try:
                        parse_development_egg_v2(repair(blob))
                    except EggFormatError:
                        pass

    def test_audio_encoding_is_reused_and_scene_transition_keeps_sfx_order(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "audio.peepproj"
            shutil.copytree(FIXTURE, root)
            with wave.open(str(root / "assets/test.wav"), "wb") as wav:
                wav.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
                wav.writeframes(struct.pack("<160h", *([1000, -1000] * 80)))
            catalog_path = root / "assets/catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["audio_assets"] = [{"asset_id": "sound", "asset_type": "sampled_sfx", "source_path": "assets/test.wav", "source_format": "wav"}]
            catalog["audio_cues"] = [{"cue_id": "sound.cue", "asset_ref": "sound", "priority": 1, "volume": 100}]
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            bundle = load_project(root)
            self.assertTrue(bundle.valid, bundle.issues)
            legacy = parse_egg(build_egg(bundle))
            plan = plan_object_migration(bundle, "state_demo", accept_continuous_animation=True)
            scene, clips = materialize_object_migration(bundle, plan, accept_continuous_animation=True)
            bundle, _ = apply_project_commands(bundle, [{"kind": "animation.upsert", "animation": clip} for clip in clips])
            route = next(route for route in scene["routes"] if "target_scene" in route)
            route["actions"] = [{"kind": "play_sfx", "cue_ref": "sound.cue"}]
            bundle = replace(bundle, scenes=tuple(scene if item["scene_id"] == "state_demo" else item for item in bundle.scenes))
            package = parse_development_egg_v2(build_development_egg_v2(bundle))
            self.assertEqual(legacy.audio_assets, package.audio_assets)
            self.assertEqual(legacy.audio_cues, package.audio_cues)
            parsed = next(item for item in package.scenes[0]["graph"]["routes"] if item["route_id"] == route["route_id"])
            self.assertEqual("state_details", parsed["target_scene"])
            self.assertEqual([7], [op["kind"] for op in parsed["operations"]])


if __name__ == "__main__":
    unittest.main()
