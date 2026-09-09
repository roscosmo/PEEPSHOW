"""Decode real encoder bytes with the C object/control decoder, not generated C data."""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_object_egg import object_bundle
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.egg_format import EggFormatError
from peepshow_authoring.object_egg import (
    parse_development_egg_v2, _objects, _controls,
    OBJECT_HEADER, OBJECT_RECORD, CONTROL_HEADER, STATE_RANGE,
    OVERRIDE_RECORD, OBJECT_OPERATION,
)


class FirmwareObjectDecoderTests(unittest.TestCase):
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
        cls.exe = cls.work / "object_decoder.exe"
        cls.env = dict(os.environ)
        cls.env["PATH"] = str(Path(compiler).parent) + os.pathsep + cls.env.get("PATH", "")
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        result = subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-I", str(firmware / "Core/Inc"),
            str(firmware / "Core/Src/ps_egg_object_decoder.c"),
            str(Path(__file__).with_name("native_object_decoder.c")), "-o", str(cls.exe),
        ], capture_output=True, text=True, timeout=60, env=cls.env)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.package = parse_development_egg_v2(build_development_egg_v2(object_bundle()))
        cls.spans = tuple(next(chunk.payload for chunk in cls.package.chunks if chunk.chunk_type == kind)
                         for kind in (13, 14, 2, 7, 9))

    def run_cases(self, cases):
        path = self.work / "cases.bin"
        data = bytearray(struct.pack("<I", len(cases)))
        for spans in cases:
            for payload in spans:
                data.extend(struct.pack("<I", len(payload)))
                data.extend(payload)
        path.write_bytes(data)
        result = subprocess.run([str(self.exe), str(path)], capture_output=True, text=True,
                                timeout=20, env=self.env)
        self.assertEqual(0, result.returncode, result.stdout[-1000:] + result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(len(cases), len(lines))
        return lines

    def expected(self, spans):
        # The unchanged catalog is validated by the complete Python package reader.
        try:
            objects = _objects(spans[0], self.package.strings, self.package.assets, self.package.animations)
            states, operations = _controls(spans[1], objects, self.package.assets)
        except EggFormatError:
            return "ERR"
        values = [len(objects), len(states), sum(map(len, states)), len(operations)]
        for obj in objects:
            values.append(self.package.strings.index(obj["object_id"]))
            values.extend(obj[key] for key in ("kind", "layer", "z_order", "flags", "width", "height",
                                              "x", "y", "frame_index", "clip_index"))
        for state in states:
            for item in state:
                values.extend(item[key] for key in ("object_index", "mask", "x", "y", "frame_index", "visible"))
        for item in operations:
            values.extend(item[key] for key in ("opcode", "axis_mask", "object_index", "x", "y", "frame_index", "visible"))
        return "OK " + " ".join(map(str, values))

    def changed(self, slot, offset, fmt, *values):
        spans = list(self.spans)
        payload = bytearray(spans[slot])
        struct.pack_into(fmt, payload, offset, *values)
        spans[slot] = bytes(payload)
        return tuple(spans)

    def test_encoder_bytes_and_signed_operands_match(self):
        self.assertEqual([self.expected(self.spans)], self.run_cases([self.spans]))
        self.assertIn("-2147483648 2147483647", self.expected(self.spans))

    def test_every_object_control_byte_mutation_matches_python(self):
        cases = []
        for slot in (0, 1):
            for offset in range(len(self.spans[slot])):
                for mask in (1, 0x80, 0xFF):
                    cases.append(self.changed(slot, offset, "B", self.spans[slot][offset] ^ mask))
        self.assertEqual([self.expected(case) for case in cases], self.run_cases(cases))

    def test_all_object_control_truncations_and_tail_reject(self):
        cases = []
        for slot in (0, 1):
            for size in range(len(self.spans[slot])):
                spans = list(self.spans)
                spans[slot] = spans[slot][:size]
                cases.append(tuple(spans))
            spans = list(self.spans)
            spans[slot] += b"\0"
            cases.append(tuple(spans))
        self.assertEqual(["ERR"] * len(cases), self.run_cases(cases))

    def test_catalog_reference_spans_are_bounds_checked(self):
        cases = []
        for slot in (2, 3, 4):
            for size in (0, 1, 4, 8, 11, len(self.spans[slot]) - 1):
                spans = list(self.spans)
                spans[slot] = spans[slot][:size]
                cases.append(tuple(spans))
        first_object = OBJECT_RECORD.unpack_from(self.spans[0], OBJECT_HEADER.size)
        cases.extend((self.changed(2, 12 + first_object[0] * 4, "I", 0xFFFFFFFF),
                      self.changed(3, 8, "H", 65535),
                      self.changed(4, 10, "H", 65535),
                      self.changed(4, 16 + first_object[10] * 16 + 8, "H", 2)))
        self.assertEqual(["ERR"] * len(cases), self.run_cases(cases))

    def test_wire_ceiling_is_not_runtime_admission(self):
        names = [i for i, name in enumerate(self.package.strings) if re.fullmatch(r"[a-z][a-z0-9_.-]{0,63}", name)][:32]
        self.assertEqual(32, len(names))
        objects = OBJECT_HEADER.pack(b"OBJ2", 1, 16, 32, 20, 0, 0)
        objects += b"".join(OBJECT_RECORD.pack(name, 4, 1, 0, 1, 1, 1, 0, 0, 65535, 65535, 0) for name in names)
        controls = CONTROL_HEADER.pack(b"OCT2", 1, 20, 64, 2048, 1152, 4, 16, 16)
        controls += b"".join(STATE_RANGE.pack(state * 32, 32) for state in range(64))
        controls += b"".join(OVERRIDE_RECORD.pack(obj, 8, 0, 0, 65535, 1) for _ in range(64) for obj in range(32))
        controls += OBJECT_OPERATION.pack(3, 0, 31, 0, 0, 65535, 0) * 1152
        full = (objects, controls, *self.spans[2:])
        cases = [full]
        for slot, offset, count in ((0, 8, 33), (1, 8, 65), (1, 10, 2049), (1, 12, 1153)):
            spans = list(full)
            payload = bytearray(spans[slot])
            struct.pack_into("<H", payload, offset, count)
            spans[slot] = bytes(payload)
            cases.append(tuple(spans))
        self.assertEqual([self.expected(full), "ERR", "ERR", "ERR", "ERR"], self.run_cases(cases))

    def test_primitives_and_each_sparse_override_property(self):
        name = OBJECT_RECORD.unpack_from(self.spans[0], OBJECT_HEADER.size)[0]
        cases = []
        for kind in range(2, 9):
            for width, height in ((13, 13), (13, 11), (12, 12), (1, 1)):
                objects = OBJECT_HEADER.pack(b"OBJ2", 1, 16, 1, 20, 0, 0)
                objects += OBJECT_RECORD.pack(name, kind, 1, 0, 3 if kind == 2 else 1,
                                               width, height, 0, 0, 65535, 65535, 0)
                controls = CONTROL_HEADER.pack(b"OCT2", 1, 20, 1, 0, 0, 4, 16, 16)
                controls += STATE_RANGE.pack(0, 0)
                cases.append((objects, controls, *self.spans[2:]))
        for mask in range(1, 16):
            controls = CONTROL_HEADER.pack(b"OCT2", 1, 20, 1, 1, 0, 4, 16, 16)
            controls += STATE_RANGE.pack(0, 1)
            frame = OBJECT_RECORD.unpack_from(self.spans[0], OBJECT_HEADER.size)[9]
            controls += OVERRIDE_RECORD.pack(0, mask, 12 if mask & 1 else 0,
                                             34 if mask & 2 else 0,
                                             frame if mask & 4 else 65535,
                                             1 if mask & 8 else 0)
            cases.append((self.spans[0], controls, *self.spans[2:]))
        self.assertEqual([self.expected(case) for case in cases], self.run_cases(cases))


if __name__ == "__main__":
    unittest.main()
