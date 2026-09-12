"""Multi-scene decode, scheduling and real raster through the owner transaction."""
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_candidate_queue as queue
from test_firmware_package_workflow import firmware_function
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


class ObjectSceneAdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        queue.ObjectCandidateQueueTests.setUpClass.__func__(cls)
        snapshot_source = Path(__file__).with_name("native_object_display_admission.c").read_text(encoding="utf-8")
        (cls.work / "candidate_payload_snapshot.inc").write_text(
            firmware_function(snapshot_source, "payload_snapshot"), encoding="ascii")
        cls.exe = cls.work / "scene_admission.exe"
        result = subprocess.run([os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe"),
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_object_scene_admission.c")),
            "-o", str(cls.exe)], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.gui = load_project(Path(__file__).resolve().parents[3] /
                               "examples/authoring/native_v2_installation.peepproj")

    def check(self, kind):
        scenes = []
        ids = ["main", *(f"scene_{i}" for i in range(2, 9 if kind == 4 else 3))]
        for index, scene_id in enumerate(ids):
            scene = deepcopy(self.gui.scenes[0])
            scene["scene_id"] = scene_id
            if index:
                scene["objects"][1]["defaults"]["x"] = 100
            route = scene["routes"][0]
            route.pop("target_state")
            route.update(target_scene=ids[(index + 1) % len(ids)], actions=[])
            scenes.append(scene)
        bundle = replace(self.gui, scenes=tuple(scenes))
        if kind == 1:
            for index, x in enumerate((0, 28, 104, 140)):
                obj = deepcopy(scenes[1]["objects"][0])
                obj["object_id"] = f"extra{index}"
                obj["defaults"].update(x=x, y=90)
                scenes[1]["objects"].append(obj)
        elif kind == 2:
            clip = deepcopy(bundle.animations[0])
            clip.update(animation_id="awkward_loop", frame_duration_ms=[700] * 4)
            obj = deepcopy(scenes[1]["objects"][0])
            obj.update(object_id="awkward_sprite", animation_ref="awkward_loop")
            obj["defaults"].update(x=20, y=90)
            scenes[1]["objects"].append(obj)
            bundle = replace(bundle, animations=(*bundle.animations, clip))
        elif kind == 3:
            scenes[1]["objects"][0]["defaults"]["visible"] = False
        # Same frame IDs with different pixels expose accidental use of live assets.
        baseline = replace(self.gui, frames=tuple(replace(frame, pixels=bytes(b ^ 255 for b in frame.pixels))
                                                 for frame in self.gui.frames))
        paths = []
        for name, source in (("live", baseline), ("scenes", bundle)):
            blob = build_development_egg_v2(source)
            path = self.work / f"{name}_{kind}.egg"
            path.write_bytes(blob)
            path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
            paths.append(str(path))
        result = subprocess.run([str(self.exe), *paths, str(kind)], capture_output=True,
                                text=True, timeout=15, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_selected_and_all_scene_checks_and_late_completion(self):
        self.check(0)

    def test_later_scene_payload_overflow_stops_batch(self):
        self.check(1)

    def test_later_scene_schedule_overflow_stops_before_display(self):
        self.check(2)

    def test_static_destination_is_valid(self):
        self.check(3)

    def test_maximum_scene_count_is_bounded(self):
        self.check(4)
