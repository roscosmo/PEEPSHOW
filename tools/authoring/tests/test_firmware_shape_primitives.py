"""Compare production retained-primitive pixels and validation with host preview."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import TOOL_ROOT, firmware_function
from peepshow_authoring.compiler import build_egg
from peepshow_authoring.egg_format import parse_egg
from peepshow_authoring.preview import StateScenePreview
from peepshow_authoring.project import load_project


def make_shape_project(parent: Path) -> Path:
    project = parent / "shapes.peepproj"
    shutil.copytree(TOOL_ROOT / "peepshow_authoring/test_project.peepproj", project)
    path = project / "scenes/state_demo.state.json"
    scene = json.loads(path.read_text(encoding="utf-8"))
    shapes = [
        ("line", "down_right", 90, 60), ("line", "up_right", 90, 60),
        ("outline_rect", None, 13, 9), ("filled_rect", None, 13, 9),
        ("circle", None, 13, 13), ("ellipse", None, 13, 9),
        ("filled_circle", None, 13, 13), ("filled_ellipse", None, 13, 9),
    ]
    for index, (kind, direction, width, height) in enumerate(shapes):
        element = dict(element_id=f"shape_{index}", kind=kind, x=28, y=32,
                       width=width, height=height, z_order=index)
        if direction is not None:
            element["line_direction"] = direction
        scene["render_models"][0]["elements"].append(element)
    path.write_text(json.dumps(scene), encoding="utf-8")
    return project


def preview_pixels(kind: int, x: int, y: int, width: int, height: int,
                   direction: str = "down_right") -> bytearray:
    pixels = bytearray(21 * 144)
    StateScenePreview._draw_primitive(pixels, dict(
        kind=kind, element_id="test", x=x, y=y, width=width, height=height,
        line_direction=direction))
    return pixels


def panel_pixels(logical: bytes) -> bytes:
    panel = bytearray(b"\xff" * (18 * 168))
    for index, value in enumerate(logical):
        if value:
            y, column = divmod(index, 21)
            for bit in range(8):
                if value & (0x80 >> bit):
                    x = column * 8 + bit
                    panel[(167 - x) * 18 + y // 8] &= ~(1 << (y % 8))
    return bytes(panel)


class ShapePrimitivesTests(unittest.TestCase):
    def test_project_round_trip_and_invalid_geometry(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project = make_shape_project(Path(temporary))
            bundle = load_project(project)
            self.assertEqual((), bundle.issues)
            blob = build_egg(bundle)
            self.assertEqual(blob, build_egg(bundle))
            scene = next(scene for scene in parse_egg(blob).scenes if scene["scene_id"] == "state_demo")
            elements = {element["element_id"]: element for element in scene["render_models"][0]["elements"]}
            self.assertEqual("down_right", elements["shape_0"]["line_direction"])
            self.assertEqual("up_right", elements["shape_1"]["line_direction"])
            self.assertEqual(7, elements["shape_6"]["kind"])
            self.assertEqual(8, elements["shape_7"]["kind"])
            path = project / "scenes/state_demo.state.json"
            original = json.loads(path.read_text(encoding="utf-8"))
            for change, code in [
                ({"kind": "filled_circle", "width": 12}, "RENDER_GEOMETRY_INVALID"),
                ({"kind": "filled_circle", "width": 11}, "RENDER_GEOMETRY_INVALID"),
                ({"kind": "filled_ellipse", "height": 1}, "RENDER_GEOMETRY_INVALID"),
                ({"kind": "filled_ellipse", "x": 167}, "RENDER_BOUNDS_INVALID"),
                ({"kind": "filled_ellipse", "line_direction": "up_right"}, "RENDER_LINE_DIRECTION_INVALID"),
                ({"kind": "line", "line_direction": "bad"}, "RENDER_LINE_DIRECTION_INVALID"),
            ]:
                with self.subTest(change=change):
                    scene = json.loads(json.dumps(original))
                    scene["render_models"][0]["elements"][-1].update(change)
                    path.write_text(json.dumps(scene), encoding="utf-8")
                    self.assertIn(code, {issue.code for issue in load_project(project).issues})

    def test_native_pixels_and_validation(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC not installed; set HOST_CC")
        firmware = TOOL_ROOT.parents[1] / "firmware/peepshow_hw6_fw0"
        source = (firmware / "Core/Src/display_renderer.c").read_text(encoding="utf-8")
        names = ["SetBlack", "HorizontalLine", "VerticalLine", "Line", "FilledRect",
                 "EllipseRow", "RasterEllipse", "ValidateSceneModel", "DrawSceneElement"]
        definitions = "\n".join(firmware_function(source, "DisplayRenderer_" + name) for name in names)
        runtime = (firmware / "Core/Src/ps_scene_runtime.c").read_text(encoding="utf-8")
        definitions += firmware_function(runtime, "PS_SceneRuntime_RenderElementValid")
        cases = []
        expected = bytearray()

        def case(internal, wire, x, y, width, height, visible=1, valid=1, direction=None):
            direction = direction or ("up_right" if internal == 10 else "down_right")
            cases.append(f"{internal} {x} {y} {width} {height} {visible} {valid}\n")
            logical = preview_pixels(wire, x, y, width, height, direction) if visible and valid else bytes(3024)
            expected.extend(panel_pixels(logical))
            return logical

        # Sweep shallow/steep slopes, both diagonals, and horizontal/vertical/point lines.
        for width, height in [(168, h) for h in range(1, 145)] + [(w, 144) for w in range(1, 169)] + [(1, 1)]:
            for internal, direction in [(6, "down_right"), (10, "up_right")]:
                logical = case(internal, 2, 168 - width, 144 - height, width, height, direction=direction)
                self.assertEqual(max(width, height), sum(value.bit_count() for value in logical))
        for width, height in [(3, 3), (3, 143), (167, 3), (167, 143), (13, 9), (9, 13), (31, 31), (143, 143)]:
            for x, y in [(0, 0), (168 - width, 144 - height)]:
                outline = case(9, 6, x, y, width, height)
                filled = case(12, 8, x, y, width, height)
                if width == height:
                    self.assertEqual(outline, case(8, 5, x, y, width, height))
                    self.assertEqual(filled, case(11, 7, x, y, width, height))
                # Filled silhouette is exactly each outline row's inclusive span.
                for row in range(144):
                    edge = [col for col in range(168) if outline[row * 21 + col // 8] & (0x80 >> (col % 8))]
                    ink = [col for col in range(168) if filled[row * 21 + col // 8] & (0x80 >> (col % 8))]
                    self.assertEqual(list(range(min(edge), max(edge) + 1)) if edge else [], ink)
                    if ink:
                        self.assertTrue(y <= row < y + height and x <= ink[0] <= ink[-1] < x + width)
        for internal, wire in [(1, 3), (7, 4), (6, 2), (10, 2), (8, 5), (9, 6), (11, 7), (12, 8)]:
            case(internal, wire, 28, 32, 13, 13)
            case(internal, wire, 28, 32, 13, 13, visible=0)
            case(internal, wire, 167, 0, 13, 13, valid=0)
            case(internal, wire, 0, 0, 0, 13, valid=0)
        for internal, wire in [(8, 5), (9, 6), (11, 7), (12, 8)]:
            case(internal, wire, 0, 0, 12, 13, valid=0)
            case(internal, wire, 0, 0, 1, 13, valid=0)
        for internal, wire in [(8, 5), (11, 7)]:
            case(internal, wire, 0, 0, 11, 13, valid=0)
        case(13, 8, 0, 0, 13, 13, valid=0)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "shape_under_test.inc").write_text(definitions, encoding="ascii")
            executable = work / "pixels.exe"
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-I", str(work),
                "-I", str(firmware / "Core/Inc"), str(Path(__file__).with_name("native_shape_pixels.c")),
                "-o", str(executable)], capture_output=True, text=True, timeout=60, env=environment)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], input="".join(cases).encode("ascii"),
                                    capture_output=True, timeout=20, env=environment)
            self.assertEqual(0, result.returncode, result.stderr.decode())
            self.assertEqual(len(expected), len(result.stdout))
            for index, spec in enumerate(cases):
                self.assertEqual(expected[index * 3024:(index + 1) * 3024],
                                 result.stdout[index * 3024:(index + 1) * 3024], spec)


if __name__ == "__main__":
    unittest.main()
