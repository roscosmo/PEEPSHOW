"""Private multi-scene preparation; installed/export capability stays unchanged."""
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
from peepshow_authoring.compiler import build_egg, build_development_egg_v2
from peepshow_authoring.egg_format import (
    HEADER, CHUNK_ENTRY, GRAPH_HEADER, VARIABLE_RECORD, EVENT_RECORD, STATE_RECORD,
)
from peepshow_authoring.object_egg import parse_development_egg_v2, OBJECT_HEADER
from peepshow_authoring.project import load_project


class V2SceneCandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        if not Path(compiler).is_file():
            raise unittest.SkipTest("native GCC required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.exe = cls.work / "scene_candidate.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
            str(Path(__file__).with_name("native_v2_scene_candidate.c")), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.gui = load_project(TOOL_ROOT.parents[1] / "examples/authoring/native_v2_installation.peepproj")
        cls.legacy = build_egg(load_project(TOOL_ROOT / "peepshow_authoring/test_project.peepproj"))

    def bundle(self, count=2, timer_exit=False):
        scenes = []
        ids = ["main", *(f"scene_{index}" for index in range(2, count + 1))]
        for index, scene_id in enumerate(ids):
            scene = deepcopy(self.gui.scenes[0])
            scene["scene_id"] = scene_id
            # Distinct defaults make selection observable, not just an ID check.
            scene["objects"][1]["defaults"]["x"] = 20 + index * 8
            route = scene["event_handlers"][0] if timer_exit else scene["routes"][0]
            route.pop("target_state", None)
            route.update(target_scene=ids[(index + 1) % count], actions=[])
            scenes.append(scene)
        return replace(self.gui, scenes=tuple(scenes))

    def run_cases(self, active_v2, cases):
        def write(name, blob):
            path = self.work / f"{name}.egg"
            path.write_bytes(blob)
            path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
            return str(path)

        baseline = write("baseline", build_egg(self.gui) if active_v2 else self.legacy)
        args = [str(self.exe), baseline, "v2" if active_v2 else "v1"]
        for name, blob, selected, reason, loader, scene_id, item, count in cases:
            args.extend([write(name, blob), *(str(value) for value in
                (selected, reason, loader, scene_id, item, count))])
        result = subprocess.run(args, capture_output=True, text=True, timeout=20, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("all-scene validation, selected decoding and live isolation passed", result.stdout)

    def accepted_cases(self):
        invalid = 0xFFFFFFFF
        cases = [("single", build_egg(self.gui), 0, 0, 0, 1, invalid, 1)]
        for timer_exit in (False, True):
            bundle = self.bundle(timer_exit=timer_exit)
            blob = build_development_egg_v2(bundle)
            cases.extend((f"connected_{timer_exit}_{selected}", blob, selected, 0, 0,
                          selected or 1, invalid, 2) for selected in (0, 1, 2))
            # Package entry is not necessarily the first sorted scene.
            bundle = replace(bundle, project={**bundle.project, "entry_scene": "scene_2"})
            cases.append((f"entry_second_{timer_exit}", build_development_egg_v2(bundle),
                          0, 0, 0, 2, invalid, 2))
        maximum = build_development_egg_v2(self.bundle(8))
        cases.extend((f"maximum_{selected}", maximum, selected, 0, 0, selected, invalid, 8)
                     for selected in range(1, 9))
        header = HEADER.unpack_from(maximum)
        package = parse_development_egg_v2(maximum)
        first = min(chunk.offset for chunk in package.chunks)
        padding = 65536 - len(maximum)
        padded = bytearray(maximum[:first] + bytes(padding) + maximum[first:])
        struct.pack_into("<I", padded, 8, len(padded))
        struct.pack_into("<I", padded, 16, len(padded) - 40)
        for index in range(header[6]):
            offset = header[4] + index * CHUNK_ENTRY.size + 16
            struct.pack_into("<I", padded, offset, struct.unpack_from("<I", padded, offset)[0] + padding)
        cases.append(("maximum_at_resident_limit", repair(padded), 8, 0, 0, 8, invalid, 8))
        return cases

    def rejected_cases(self):
        invalid = 0xFFFFFFFF
        bundle = self.bundle()
        good = build_development_egg_v2(bundle)
        cases = [("selected_out_of_range", good, 3, 3, 1, 3, invalid, 0),
                 ("selected_max_uint", good, invalid, 3, 1, invalid, invalid, 0)]
        cases.append(("too_many_scenes", build_development_egg_v2(self.bundle(9)),
                      0, 3, 8, 0, invalid, 0))
        # The later scene is rejected even when asking only for scene 1.
        scene = deepcopy(bundle.scenes[1])
        scene["routes"][1]["actions"] = [{"kind": "exit_to_shell"}]
        blocked = build_development_egg_v2(replace(bundle, scenes=(bundle.scenes[0], scene)))
        cases.append(("non_entry_action", blocked, 1, 10, 0, 2, 0, 0))
        mixed_bundle = object_bundle()
        mixed_scenes = deepcopy(mixed_bundle.scenes)
        for scene in mixed_scenes:
            scene["interaction_policy"]["mode"] = "continuous"
            scene["interaction_policy"].pop("inactive_route", None)
            for route in scene["routes"]:
                route["actions"] = []
        mixed = build_development_egg_v2(replace(mixed_bundle, scenes=mixed_scenes))
        cases.append(("mixed", mixed, 0, 5, 0, 2, invalid, 0))
        package = parse_development_egg_v2(good)
        later_objects = [chunk for chunk in package.chunks if chunk.chunk_type == 13][1]
        corrupt = bytearray(good)
        corrupt[later_objects.offset + OBJECT_HEADER.size + 2] = 9
        cases.append(("non_entry_object", repair(corrupt), 1, 3, 11, 2, invalid, 0))
        later_graph = [chunk for chunk in package.chunks if chunk.chunk_type == 4][1]
        header = GRAPH_HEADER.unpack_from(later_graph.payload)
        route_start = (later_graph.offset + GRAPH_HEADER.size + header[4] * VARIABLE_RECORD.size
                       + header[5] * EVENT_RECORD.size + header[6] * STATE_RECORD.size)
        # Repair integrity after mutation so these exercise semantic rejection.
        corrupt = bytearray(good)
        struct.pack_into("<H", corrupt, route_start + 6, package.strings.index("scene_2"))
        cases.append(("self_exit", repair(corrupt), 1, 9, 0, 2, 0, 0))
        # A target string absent from the scene catalog cannot become a local route.
        corrupt = bytearray(good)
        struct.pack_into("<H", corrupt, route_start + 6, 0xFFFE)
        cases.append(("missing_destination", repair(corrupt), 1, 3, 10, 2, invalid, 0))
        # Give the cross-scene route an object action. It must not execute on
        # either source or destination, even with correct package checksums.
        corrupt = bytearray(good)
        struct.pack_into("<H", corrupt, route_start + 18, 1)
        cases.append(("exit_with_action", repair(corrupt), 1, 3, 10, 2, invalid, 0))
        corrupt = bytearray(good)
        corrupt[-1] ^= 1
        cases.append(("digest", corrupt, 0, 3, 4, 0, invalid, 0))
        cases.append(("recovery", good, 2, 0, 0, 2, invalid, 2))
        return cases

    def test_selected_scenes_preserve_live_v1(self):
        self.run_cases(False, self.accepted_cases())

    def test_selected_scenes_preserve_live_v2(self):
        self.run_cases(True, self.accepted_cases())

    def test_rejected_scene_sets_preserve_live_v1(self):
        self.run_cases(False, self.rejected_cases())

    def test_rejected_scene_sets_preserve_live_v2(self):
        self.run_cases(True, self.rejected_cases())
