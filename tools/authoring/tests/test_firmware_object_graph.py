"""Full V2 bytes through the C package loader and transactional graph executor."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_firmware_scene_objects import runtime_bundle
from test_object_egg import repair, FIXTURE
from peepshow_authoring.compiler import build_egg, build_development_egg_v2
from peepshow_authoring.project import load_project
from peepshow_authoring.object_egg import parse_development_egg_v2
from peepshow_authoring.egg_format import (GRAPH_HEADER, VARIABLE_RECORD, EVENT_RECORD,
    STATE_RECORD, ROUTE_RECORD_V2, GUARD_RECORD, SCENE_HEADER, SCENE_RECORD)


def graph_bundle():
    bundle = runtime_bundle()
    source = deepcopy(bundle.scenes[0])
    source["variables"][0].update(minimum=-2147483648, maximum=2147483647)
    source["event_bindings"] = [{"binding_id": "tick", "event_type": "time.scene_elapsed",
        "configuration": {"delay_ms": 5000, "start_policy": "action"}}]
    source["event_handlers"] = [{"handler_id": "tick_handler", "event_ref": "tick", "guards": [],
        "actions": [{"kind": "object.move_by", "object_ref": "marker", "dy": 2},
                    {"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 1}]}]
    for route in source["routes"]:
        route["actions"] = []
        if route["route_id"] == "center_to_right":
            route["actions"] = [
                {"kind": "object.move_by", "object_ref": "cursor", "dx": 5},
                {"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": 2},
                {"kind": "object.move_by", "object_ref": "cursor", "dx": 7},
                {"kind": "start_timer", "timer_ref": "tick"},
            ]
        elif route["route_id"] == "center_to_left":
            route["actions"] = [
                {"kind": "object.move_by", "object_ref": "cursor", "dx": 5},
                {"kind": "set_variable", "variable_ref": "selected_index", "operation": "assign", "value": 2147483647},
                {"kind": "set_variable", "variable_ref": "selected_index", "operation": "add", "value": 1},
                {"kind": "start_timer", "timer_ref": "tick"},
            ]
    return replace(bundle, scenes=(source, *bundle.scenes[1:]))


class FirmwareObjectGraphTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            raise unittest.SkipTest("native GCC not installed; set HOST_CC")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.exe = cls.work / "object_graph.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
            str(Path(__file__).with_name("native_object_graph.c")), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.baseline = cls.work / "baseline.egg"
        cls.write_blob(cls.baseline, build_egg(load_project(FIXTURE)))
        cls.blob = build_development_egg_v2(graph_bundle())
        cls.package = parse_development_egg_v2(cls.blob)

    @staticmethod
    def write_blob(path, blob):
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())

    def run_blob(self, blob, reason=0, execute=False):
        path = self.work / "candidate.egg"
        self.write_blob(path, blob)
        result = subprocess.run([str(self.exe), str(self.baseline), str(path), str(reason), str(int(execute))],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def chunk(self, kind):
        return next(chunk for chunk in self.package.chunks if chunk.chunk_type == kind)

    def corrupt(self, kind, offset, fmt, *values):
        blob = bytearray(self.blob)
        struct.pack_into("<" + fmt, blob, self.chunk(kind).offset + offset, *values)
        return repair(blob)

    def graph_offsets(self):
        header = GRAPH_HEADER.unpack_from(self.chunk(4).payload)
        variables, bindings, states, routes, sources, guards = header[4:10]
        state = GRAPH_HEADER.size + variables * VARIABLE_RECORD.size + bindings * EVENT_RECORD.size
        route = state + states * STATE_RECORD.size
        guard = route + routes * ROUTE_RECORD_V2.size + sources * 2
        return state, route, guard + guards * GUARD_RECORD.size

    def test_complete_mixed_package_and_atomic_graph_execution(self):
        self.run_blob(self.blob, execute=True)

    def test_container_integrity_still_required(self):
        blob = bytearray(self.blob)
        blob[-1] ^= 1
        self.run_blob(blob, 4)

    def test_model_and_graph_version_must_agree(self):
        self.run_blob(self.corrupt(4, 4, "H", 6), 10)
        self.run_blob(self.corrupt(3, SCENE_HEADER.size + 14, "H", 1), 9)

    def test_nonentry_scene_is_validated(self):
        self.run_blob(self.corrupt(5, 4, "H", 99), 11)

    def test_scene_chunks_cannot_be_shared(self):
        first = SCENE_RECORD.unpack_from(self.chunk(3).payload, SCENE_HEADER.size)
        self.run_blob(self.corrupt(3, SCENE_HEADER.size + SCENE_RECORD.size + 8, "H", first[4]), 9)

    def test_no_legacy_state_visuals_or_element_operations(self):
        state, _, operations = self.graph_offsets()
        self.run_blob(self.corrupt(4, state + 4, "H", 0), 10)
        self.run_blob(self.corrupt(4, operations, "B", 3), 10)

    def test_object_operation_references_and_partitions(self):
        _, route, operations = self.graph_offsets()
        # Find the first real object reference rather than relying on route order.
        payload = self.chunk(4).payload
        count = GRAPH_HEADER.unpack_from(payload)[10]
        first = next(operations + i * 12 for i in range(count) if payload[operations + i * 12] == 12)
        self.run_blob(self.corrupt(4, first + 2, "H", 65535), 10)
        self.run_blob(self.corrupt(4, route + 16, "H", 1), 10)

    def test_unused_animation_catalog_is_checked(self):
        self.run_blob(self.corrupt(9, 16 + 14, "H", 1), 16)

    def test_control_state_count_must_match_graph(self):
        self.run_blob(self.corrupt(14, 8, "H", 2), 11)

    def test_object_target_capacity_is_enforced(self):
        bundle = graph_bundle()
        source = deepcopy(bundle.scenes[0])
        for index in range(11):
            obj = deepcopy(source["objects"][1])
            obj["object_id"] = f"extra_{index}"
            obj.pop("animation_ref")
            source["objects"].append(obj)
        self.run_blob(build_development_egg_v2(replace(bundle, scenes=(source, *bundle.scenes[1:]))), 10)


if __name__ == "__main__":
    unittest.main()
