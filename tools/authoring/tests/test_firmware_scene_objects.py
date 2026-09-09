"""Exercise the real C object bank using bytes from the development encoder."""
from copy import deepcopy
from dataclasses import replace
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_object_egg import object_bundle
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.object_egg import parse_development_egg_v2


def runtime_bundle():
    bundle = object_bundle()
    source = deepcopy(bundle.scenes[0])
    first = deepcopy(source["objects"][0])
    first["defaults"] = {"x": 10, "y": 20, "visible": True, "visual_ref": "cursor.phase_b"}
    first["animation_ref"] = "runtime.four_frames"
    second = deepcopy(first)
    second["object_id"] = "marker"
    second["defaults"]["visible"] = False
    second.pop("focus_role", None)
    source["objects"] = [first, second]
    source["states"][0]["object_overrides"] = [{"object_ref": "marker", "visible": True}]
    source["states"][1]["object_overrides"] = [{"object_ref": "cursor", "x": 100}]
    source["states"][2]["object_overrides"] = [
        {"object_ref": "cursor", "y": 60, "visible": False, "visual_ref": "cursor.phase_c"},
        {"object_ref": "marker", "visible": True},
    ]
    actions = [
        {"kind": "object.move_by", "dx": 5},
        {"kind": "object.move_by", "dx": 7},
        {"kind": "object.set_position", "y": 40},
        {"kind": "object.set_visibility", "visible": False},
        {"kind": "object.set_visibility", "visible": True},
        {"kind": "object.set_frame", "frame_ref": "cursor.phase_a"},
        {"kind": "object.clear_frame"},
        {"kind": "object.move_by", "dx": -2147483648, "dy": 2147483647},
        {"kind": "object.set_position", "x": 2147483647},
        {"kind": "object.move_by", "dx": -10},
        {"kind": "object.set_position", "y": -2147483648},
        {"kind": "object.move_by", "dy": -2147483648},
        {"kind": "object.set_position", "x": 30, "y": 40},
        {"kind": "object.move_by", "dy": 5},
    ]
    for index, route in enumerate(source["routes"]):
        route["actions"] = [{**action, "object_ref": "cursor"}
                            for action in actions[index * 8:(index + 1) * 8]]
    frame = next(frame for frame in bundle.frames if frame.frame_id == "cursor.phase_a")
    extra = tuple(replace(frame, frame_id=f"cursor.phase_{suffix}") for suffix in ("c", "d", "e"))
    assets = deepcopy(bundle.assets)
    asset = next(asset for asset in assets if asset["asset_id"] == frame.asset_id)
    asset["frames"].extend({**deepcopy(asset["frames"][0]), "frame_id": item.frame_id} for item in extra)
    clip = {"animation_id": "runtime.four_frames", "loop_policy": "loop",
            "frame_refs": [f"cursor.phase_{suffix}" for suffix in ("a", "b", "c", "d")],
            "frame_duration_ms": [250] * 4}
    return replace(bundle, assets=assets, frames=(*bundle.frames, *extra), animations=(*bundle.animations, clip),
                   scenes=(source, *bundle.scenes[1:]))


class FirmwareSceneObjectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc")
        if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").is_file():
            compiler = "C:/msys64/ucrt64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("native GCC not installed; set HOST_CC")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.exe = cls.work / "scene_objects.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        result = subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-fstack-usage",
            "-I", str(firmware / "Core/Inc"),
            str(firmware / "Core/Src/ps_egg_object_decoder.c"),
            str(firmware / "Core/Src/ps_scene_objects.c"),
            str(Path(__file__).with_name("native_scene_objects.c")), "-o", str(cls.exe),
        ], capture_output=True, text=True, timeout=60, env=cls.env, cwd=cls.work)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_bundle(self, bundle, capacity=False):
        package = parse_development_egg_v2(build_development_egg_v2(bundle))
        path = self.work / "scene_objects.bin"
        data = bytearray()
        for kind in (13, 14, 2, 7, 9):
            payload = next(chunk.payload for chunk in package.chunks if chunk.chunk_type == kind)
            data.extend(struct.pack("<I", len(payload)))
            data.extend(payload)
        path.write_bytes(data)
        result = subprocess.run([str(self.exe), str(path), str(int(capacity))],
                                capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("object runtime checks passed", result.stdout)

    def test_native_ownership_masks_timing_and_atomic_actions(self):
        self.run_bundle(runtime_bundle())

    def test_object_capacity_rejects_without_replacing_bank(self):
        bundle = runtime_bundle()
        scene = deepcopy(bundle.scenes[0])
        for index in range(11):
            obj = deepcopy(scene["objects"][1])
            obj["object_id"] = f"extra_{index}"
            obj.pop("animation_ref")
            scene["objects"].append(obj)
        self.run_bundle(replace(bundle, scenes=(scene, *bundle.scenes[1:])), capacity=True)

    def test_hidden_animated_instances_count_toward_capacity(self):
        bundle = runtime_bundle()
        scene = deepcopy(bundle.scenes[0])
        for index in range(7):
            obj = deepcopy(scene["objects"][1])
            obj["object_id"] = f"extra_{index}"
            scene["objects"].append(obj)
        self.run_bundle(replace(bundle, scenes=(scene, *bundle.scenes[1:])), capacity=True)

    def test_step_capacity_rejects_without_replacing_bank(self):
        bundle = runtime_bundle()
        clips = deepcopy(bundle.animations)
        clips[-1]["frame_refs"] = ["cursor.phase_a"] * 13
        clips[-1]["frame_duration_ms"] = [250] * 13
        self.run_bundle(replace(bundle, animations=clips), capacity=True)

    def test_state_capacity_rejects_without_replacing_bank(self):
        bundle = runtime_bundle()
        scene = deepcopy(bundle.scenes[0])
        for index in range(6):
            state = deepcopy(scene["states"][0])
            state["state_id"] = f"extra_{index}"
            scene["states"].append(state)
        self.run_bundle(replace(bundle, scenes=(scene, *bundle.scenes[1:])), capacity=True)

    def test_phase_capacity_rejects_without_replacing_bank(self):
        bundle = runtime_bundle()
        clips = deepcopy(bundle.animations)
        clips[-1]["frame_refs"].append("cursor.phase_e")
        clips[-1]["frame_duration_ms"].append(250)
        self.run_bundle(replace(bundle, animations=clips), capacity=True)


if __name__ == "__main__":
    unittest.main()
