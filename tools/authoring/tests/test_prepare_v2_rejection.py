import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from peepshow_authoring.compiler import build_egg
from peepshow_authoring.egg_format import EggFormatError, FOOTER, parse_egg
from peepshow_authoring.project import load_project
from prepare_v2_rejection import digest_rejection, prepare

ROOT = Path(__file__).resolve().parents[3]


class RejectionPreparationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.good = build_egg(load_project(ROOT / "examples/authoring/native_v2_installation.peepproj"))

    def test_only_one_digest_bit_changes_and_both_hashes_are_recorded(self):
        bad, report = digest_rejection(self.good)
        self.assertEqual(len(self.good), len(bad))
        self.assertEqual([len(bad) - 1], [i for i, (a, b) in enumerate(zip(self.good, bad)) if a != b])
        self.assertEqual(1, bad[-1] ^ self.good[-1])
        self.assertEqual(self.good[:-FOOTER.size], bad[:-FOOTER.size])
        self.assertEqual(hashlib.sha256(self.good).hexdigest(), report["source_sha256"])
        self.assertEqual(hashlib.sha256(bad).hexdigest(), report["rejected_sha256"])
        self.assertEqual("not_run", report["hardware_result"])
        with self.assertRaisesRegex(EggFormatError, "^package SHA-256 mismatch$"):
            parse_egg(bad)
        self.assertEqual((bad, report), digest_rejection(self.good))

    def test_writes_separate_artifact_without_changing_source(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "studio.egg"
            source.write_bytes(self.good)
            output, report = prepare(source, Path(temp) / "negative")
            self.assertEqual(self.good, source.read_bytes())
            self.assertEqual("DO_NOT_INSTALL_bad_digest.egg", output.name)
            self.assertEqual(report, json.loads((output.parent / "evidence.json").read_text()))
            self.assertEqual(report["rejected_sha256"], hashlib.sha256(output.read_bytes()).hexdigest())

    def test_existing_output_directory_and_source_directory_are_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "DO_NOT_INSTALL_bad_digest.egg"
            source.write_bytes(self.good)
            with self.assertRaises(FileExistsError):
                prepare(source, temp)
            self.assertEqual(self.good, source.read_bytes())
            output, _ = prepare(source, Path(temp) / "negative")
            before = output.read_bytes()
            with self.assertRaises(FileExistsError):
                prepare(source, output.parent)
            self.assertEqual(before, output.read_bytes())

    def test_invalid_source_creates_no_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "invalid.egg"
            source.write_bytes(b"not an egg")
            output = Path(temp) / "negative"
            with self.assertRaises(EggFormatError):
                prepare(source, output)
            self.assertFalse(output.exists())

    def test_legacy_source_is_not_a_v2_rejection_test(self):
        legacy = build_egg(load_project(ROOT / "tools/authoring/peepshow_authoring/test_project.peepproj"))
        with self.assertRaisesRegex(ValueError, "valid restricted V2"):
            digest_rejection(legacy)


if __name__ == "__main__":
    unittest.main()
