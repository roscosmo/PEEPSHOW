"""Check the stack measurement itself and the configured ARM V2 launch budget."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_v2_runtime_stack as stack


class RuntimeStackTests(unittest.TestCase):
    def test_direct_chain_and_unmeasured_leaf(self):
        report = r'''
node: { title: "unit.c:owner" label: "owner\nunit.c:1:1\n64 bytes (static)" }
node: { title: "decode" label: "decode\nunit.c:2:1\n96 bytes (static)" }
edge: { sourcename: "unit.c:owner" targetname: "decode" }
edge: { sourcename: "decode" targetname: "memcpy" }
'''
        frames, edges = stack.parse_graph([report])
        paths, unresolved = stack.measure(frames, edges, {"launch": (("owner",), "decode")})
        self.assertEqual(160, paths["launch"]["c_bytes"])
        self.assertEqual(["owner", "decode"], paths["launch"]["chain"])
        self.assertEqual(["memcpy"], unresolved)

    def test_missing_prefix_edge_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "missing direct call"):
            stack.measure({"owner": 64, "decode": 96}, {},
                          {"launch": (("owner",), "decode")})

    def test_dynamic_stack_and_recursion_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "dynamic stack"):
            stack.parse_graph([r'node: { title: "fn" label: "fn\nfile:1:1\n32 bytes (dynamic)" }'])
        with self.assertRaisesRegex(ValueError, "recursive"):
            stack.measure({"fn": 32}, {"fn": {"fn"}}, {"cycle": ((), "fn")})

    def test_arm_installed_paths_fit_with_reserve(self):
        build = stack.FIRMWARE / "build/Debug"
        database = build / "compile_commands.json"
        if not database.exists():
            self.skipTest("configure the HW6 Debug build to run the ARM stack check")
        entries = json.loads(database.read_text())
        compiler = stack.compiler_args(entries[0], build / "unused.o")[0]
        if not Path(compiler).is_file():
            self.skipTest("configured ARM compiler is unavailable")
        frames, edges = stack.parse_graph(stack.compile_reports(build))
        paths, _ = stack.measure(frames, edges)
        required = max(path["c_bytes"] for path in paths.values()) + stack.RESERVE_BYTES
        budget = json.loads((stack.FIRMWARE / "config/knobs.json").read_text())["rtos_runtime_stack_bytes"]
        self.assertLessEqual(required, budget, paths)
        self.assertEqual(set(stack.PATHS), set(paths))


if __name__ == "__main__":
    unittest.main()
