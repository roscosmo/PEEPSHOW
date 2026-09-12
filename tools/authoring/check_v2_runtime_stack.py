"""Recompile the V2 runtime paths with ARM GCC and check cumulative C stack use.

Uses the configured Debug compile database, including its actual ABI/optimization
flags. Prefixes describe the installed admission callback and owner dispatch;
direct descendants come from GCC's call graph, not a hand-maintained frame sum.
The reserve covers unmodelled library/RTOS assembly and exception context. This
is a scoped regression check, not a whole-firmware worst-case stack proof.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess


FIRMWARE = Path(__file__).resolve().parents[2] / "firmware/peepshow_hw6_fw0"
SOURCES = (
    "ps_hw6_rtos_probe.c", "ps_scene_runtime.c", "ps_egg_state_loader.c",
    "ps_egg_object_decoder.c", "ps_scene_objects.c", "ps_scene_object_graph.c",
    "ps_scene_object_render.c", "ps_scene_object_waiting.c",
)
RESERVE_BYTES = 512
OWNER = ("PS_HW6_RTOS_OwnerEntry",)
COMMAND = OWNER + ("PS_HW6_RTOS_HandleOwnerCommand", "PS_HW6_RTOS_HandleRuntimeCommand")
LAUNCH = COMMAND + ("PS_HW6_RTOS_RuntimePackageReplace",
                    "PS_HW6_RTOS_RuntimePackageActivateStub")
PLAY = OWNER + ("PS_HW6_RTOS_PackageWorkflowHandleMessage",
                "PS_HW6_RTOS_PackageWorkflowPrepare",
                "PS_HW6_RTOS_HandleRuntimeCommand",
                "PS_HW6_RTOS_RuntimePackageReplace",
                "PS_HW6_RTOS_RuntimePackageActivateStub")
PATHS = {
    "install preflight": (COMMAND + ("PS_HW6_RTOS_RunPackageValidation",),
                          "PS_HW6_RTOS_InstalledObjectCheck"),
    "installed launch": (LAUNCH + ("PS_SceneRuntime_EnterStateScene",
                                   "PS_SceneRuntime_EnterObjects"),
                         "PS_HW6_RTOS_InstalledObjectCheck"),
    "shell PLAY launch": (PLAY + ("PS_SceneRuntime_EnterStateScene",
                                  "PS_SceneRuntime_EnterObjects"),
                          "PS_HW6_RTOS_InstalledObjectCheck"),
    "input transaction": (OWNER + ("PS_HW6_RTOS_HandleRuntimeInput",
                                    "PS_SceneRuntime_HandleStateSceneInput",
                                    "PS_SceneRuntime_HandleStateSceneEventId"),
                          "PS_HW6_RTOS_InstalledObjectCheck"),
    "timer transaction": (COMMAND + ("PS_HW6_RTOS_RuntimeStateTimersService",
                                       "PS_SceneRuntime_HandleStateSceneEvent",
                                       "PS_SceneRuntime_HandleStateSceneEventId"),
                          "PS_HW6_RTOS_InstalledObjectCheck"),
    "first presentation": (LAUNCH, "PS_HW6_RTOS_InstalledObjectLaunch"),
    "shell PLAY presentation": (PLAY, "PS_HW6_RTOS_InstalledObjectLaunch"),
    # Preparation APIs are not yet called by scene replacement. These roots
    # cover their own descendants only; add the real caller chain on integration.
    "private destination admission": ((), "PS_HW6_ObjectCandidate_CheckScene"),
    "private scene-set admission": ((), "PS_HW6_ObjectCandidate_CheckSceneSet"),
    "development scene-set launch": (OWNER + ("PS_HW6_RTOS_ObjectService",
        "PS_SceneRuntime_EnterDevelopmentSceneSet"), "PS_HW6_RTOS_ObjectSceneCheck"),
    "input fresh replacement": (OWNER + ("PS_HW6_RTOS_HandleRuntimeInput",
        "PS_SceneRuntime_HandleStateSceneInput", "PS_SceneRuntime_HandleStateSceneEventId",
        "PS_SceneRuntime_ReplaceObjectScene"), "PS_HW6_RTOS_ObjectSceneCheck"),
    "timer fresh replacement": (COMMAND + ("PS_HW6_RTOS_RuntimeStateTimersService",
        "PS_SceneRuntime_HandleStateSceneEvent", "PS_SceneRuntime_HandleStateSceneEventId",
        "PS_SceneRuntime_ReplaceObjectScene"), "PS_HW6_RTOS_ObjectSceneCheck"),
    "multi-scene local transaction": (OWNER + ("PS_HW6_RTOS_HandleRuntimeInput",
        "PS_SceneRuntime_HandleStateSceneInput", "PS_SceneRuntime_HandleStateSceneEventId"),
        "PS_HW6_RTOS_ObjectSceneCheck"),
}
ADMISSION_CALLBACKS = {
    ("PS_SceneRuntime_EnterObjects", "PS_HW6_RTOS_InstalledObjectCheck"),
    ("PS_SceneRuntime_HandleStateSceneEventId", "PS_HW6_RTOS_InstalledObjectCheck"),
    ("PS_SceneRuntime_EnterDevelopmentSceneSet", "PS_HW6_RTOS_ObjectSceneCheck"),
    ("PS_SceneRuntime_ReplaceObjectScene", "PS_HW6_RTOS_ObjectSceneCheck"),
    ("PS_SceneRuntime_HandleStateSceneEventId", "PS_HW6_RTOS_ObjectSceneCheck"),
}


def compiler_args(entry, output):
    windows = "\\" in entry["command"]
    args = shlex.split(entry["command"], posix=not windows)
    if windows:
        args = [arg[1:-1] if arg.startswith('"') and arg.endswith('"') else arg for arg in args]
    if "arm-none-eabi-gcc" not in Path(args[0]).name:
        raise ValueError("stack check requires the configured ARM GCC, not host GCC")
    args[args.index("-o") + 1] = str(output)
    return args + ["-fstack-usage", "-fcallgraph-info=su"]


def compile_reports(build):
    entries = json.loads((build / "compile_commands.json").read_text())
    output = build / "v2_runtime_stack"
    output.mkdir(exist_ok=True)
    reports = []
    for name in SOURCES:
        entry = next(item for item in entries if Path(item["file"]).name == name)
        obj = output / (name + ".o")
        subprocess.run(compiler_args(entry, obj), cwd=entry["directory"], check=True,
                       capture_output=True, text=True, timeout=60)
        reports.append(obj.with_suffix(".ci").read_text())
    return reports


def parse_graph(reports):
    frames, edges = {}, {}
    for report in reports:
        for line in report.splitlines():
            node = re.match(r'node: \{ title: "([^"]+)" label: "([^"]+)"', line)
            if node:
                stack = re.search(r'\\n(\d+) bytes \(([^)]+)\)', node[2])
                if stack:
                    if stack[2].split(",")[0] != "static":
                        raise ValueError("unbounded/dynamic stack: " + node[1])
                    frames[node[1]] = int(stack[1])
            edge = re.match(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', line)
            if edge:
                edges.setdefault(edge[1], set()).add(edge[2])
    return frames, edges


def measure(frames, edges, paths=PATHS):
    unresolved = set()

    def resolve(name):
        keys = [key for key in frames if key == name or key.endswith(":" + name)]
        if len(keys) != 1:
            raise ValueError("missing/ambiguous stack frame: " + name)
        return keys[0]

    def deepest(key, active):
        if key in active:
            raise ValueError("recursive call graph: " + key)
        if key not in frames:
            unresolved.add(key)
            return 0, []
        descendants = [deepest(child, active | {key}) for child in sorted(edges.get(key, ()))]
        size, chain = max(descendants, key=lambda item: item[0], default=(0, []))
        return frames[key] + size, [key.rsplit(":", 1)[-1]] + chain

    result = {}
    for name, (prefix, root) in paths.items():
        sequence = (*prefix, root)
        for caller, callee in zip(sequence, sequence[1:]):
            if (caller, callee) not in ADMISSION_CALLBACKS:
                if resolve(callee) not in edges.get(resolve(caller), ()):
                    raise ValueError(f"missing direct call: {caller} -> {callee}")
        size, chain = deepest(resolve(root), set())
        result[name] = {"c_bytes": sum(frames[resolve(item)] for item in prefix) + size,
                        "chain": list(prefix) + chain}
    return result, sorted(unresolved)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=FIRMWARE / "build/Debug")
    args = parser.parse_args()
    frames, edges = parse_graph(compile_reports(args.build_dir.resolve()))
    paths, unresolved = measure(frames, edges)
    budget = json.loads((FIRMWARE / "config/knobs.json").read_text())["rtos_runtime_stack_bytes"]
    generated = (FIRMWARE / "Core/Inc/knobs_autogen.h").read_text()
    if not re.search(rf"#define KNOB_RTOS_RUNTIME_STACK_BYTES\s+\({budget}\)", generated):
        raise SystemExit("FAIL: regenerate knobs_autogen.h before checking stack usage")
    for name, result in paths.items():
        print(f"{name}: C={result['c_bytes']} + reserve={RESERVE_BYTES} / stack={budget}")
        print("  " + " -> ".join(result["chain"]))
    print("Reserve covers external/assembly leaves (not measured here): " + ", ".join(unresolved))
    required = max(result["c_bytes"] for result in paths.values()) + RESERVE_BYTES
    if required > budget:
        raise SystemExit(f"FAIL: need at least {required} bytes, configured {budget}")
    print(f"PASS: worst checked path plus reserve={required}; remaining headroom={budget - required}")


if __name__ == "__main__":
    main()
