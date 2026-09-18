"""Real OBJ2 strings -> production decoder/runtime/raster -> host pixel parity."""
import base64
from copy import deepcopy
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

import test_firmware_object_awake as awake
from test_firmware_shape_primitives import panel_pixels
from build_runtime_text_fixture import runtime_text_bundle, text_object
from peepshow_authoring.compiler import build_development_egg_v2, build_preview_package, build_egg, EggCompileError
from peepshow_authoring.object_egg import parse_development_egg_v2
from peepshow_authoring.preview import StateScenePreview
from peepshow_authoring.project import apply_project_commands
from peepshow_authoring.system_fonts import runtime_text_layout, SystemFontError, SYSTEM_FONT_8X8_BASIC_ID


class RuntimeTextTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        awake.ObjectAwakeTests.setUpClass.__func__(cls)
        cls.text_exe = cls.work / "text.exe"
        result = subprocess.run([os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe"),
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-I", str(cls.firmware / "Core/Inc"), "-I", str(cls.firmware / "Core/Src"),
            "-I", str(cls.work), str(Path(__file__).with_name("native_runtime_text.c")),
            "-o", str(cls.text_exe)], capture_output=True, text=True, timeout=30, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def compare(self, bundle):
        blob = build_development_egg_v2(bundle)
        path = self.work / "text.egg"
        path.write_bytes(blob)
        path.with_suffix(".egg.sha256").write_bytes(hashlib.sha256(blob[:-40]).digest())
        output = self.work / "text.bin"
        result = subprocess.run([str(self.text_exe), str(path), str(output)],
            capture_output=True, text=True, timeout=10, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        preview = StateScenePreview(build_preview_package(bundle), bundle.scenes[0]["scene_id"])
        expected = bytearray()
        for _ in range(3):
            expected.extend(panel_pixels(base64.b64decode(preview.snapshot()["framebuffer"]["data_base64"])))
            preview.advance(375)
            preview.apply_input("BUTTON_A")
        self.assertEqual(expected, output.read_bytes())

    def test_fixture_moves_hides_and_preserves_animation(self):
        bundle = runtime_text_bundle()
        self.compare(bundle)
        package = parse_development_egg_v2(build_development_egg_v2(bundle))
        self.assertEqual(4, len(package.assets))  # Only the four orbit frames.
        self.assertEqual(build_development_egg_v2(bundle), build_egg(bundle))

    def test_scales_alignment_and_explicit_lines(self):
        bundle = runtime_text_bundle()
        for scale in range(1, 9):
            for alignment in ("left", "center", "right"):
                scene = deepcopy(bundle.scenes[0])
                scene["objects"] = scene["objects"][:2] + [
                    text_object("hidden", "a\nZ", 0, 0, 167, 144, scale, alignment)]
                self.compare(replace(bundle, scenes=(scene,)))

    def test_text_only_package_needs_no_sprite_catalog(self):
        bundle = runtime_text_bundle()
        scene = deepcopy(bundle.scenes[0])
        scene["objects"] = scene["objects"][1:]
        bundle = replace(bundle, scenes=(scene,), frames=(), assets=(), animations=())
        self.compare(bundle)
        self.assertEqual(0, len(parse_development_egg_v2(build_development_egg_v2(bundle)).assets))

    def test_rejects_overflow_unsupported_characters_and_styles(self):
        for text, scale, align, width, height in [
            ("abc", 2, "left", 47, 16), ("a\n", 1, "left", 8, 8),
            ("a\t", 1, "left", 80, 8), ("\u00e9", 1, "left", 8, 8),
            ("a", 9, "left", 168, 144), ("a", 1, "bad", 8, 8)]:
            with self.assertRaises(SystemFontError):
                runtime_text_layout(text, SYSTEM_FONT_8X8_BASIC_ID, scale, align, width, height)

    def test_text_edit_command(self):
        bundle = runtime_text_bundle()
        changed, _ = apply_project_commands(bundle, [{"kind": "object.set_text",
            "scene_id": bundle.scenes[0]["scene_id"], "object_id": "title",
            "text": "New\nText", "font_id": SYSTEM_FONT_8X8_BASIC_ID,
            "scale": 1, "alignment": "right", "width": 120, "height": 16}])
        self.assertEqual("New\nText", changed.scenes[0]["objects"][2]["text"])
        self.assertEqual("Text 2x", bundle.scenes[0]["objects"][2]["text"])
        self.compare(changed)
