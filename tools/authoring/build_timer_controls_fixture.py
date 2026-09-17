"""Build the OS-only installed V2 timer-control bench fixture."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
from pathlib import Path

from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.project import load_project


def timer_controls_bundle():
    root = Path(__file__).resolve().parents[2]
    bundle = load_project(root / "examples/authoring/native_v2_timer_four_frames.peepproj")
    scene = deepcopy(bundle.scenes[0])
    scene.update(display_name="Timer Controls", variables=[
        {"variable_id": "allow", "value_type": "int32", "initial": 1, "minimum": 0, "maximum": 1}],
        states=[{"state_id": "start", "display_name": "Guard On", "object_overrides": []},
                {"state_id": "guard_off", "display_name": "Guard Off", "object_overrides": []}])
    objects = []
    for name, y in (("guard", 84), ("done", 114)):
        objects.append({"object_id": name, "kind": "filled_rect", "width": 12, "height": 12,
            "z_order": 1, "layer": "SCENE",
            "defaults": {"x": 134, "y": y + 2, "visible": name == "guard"}})
    for name, y in (("guard", 84), ("done", 114)):
        objects.append({"object_id": name + "_outline", "kind": "outline_rect", "width": 20,
            "height": 20, "z_order": 0, "layer": "SCENE",
            "defaults": {"x": 130, "y": y - 2, "visible": True}})
    glyphs = {
        " ": (0,) * 7, "0": (14, 17, 19, 21, 25, 17, 14), "1": (4, 12, 4, 4, 4, 4, 14),
        "A": (14, 17, 17, 31, 17, 17, 17), "B": (30, 17, 17, 30, 17, 17, 30),
        "C": (14, 17, 16, 16, 16, 17, 14), "D": (30, 17, 17, 17, 17, 17, 30),
        "E": (31, 16, 16, 30, 16, 16, 31), "G": (14, 17, 16, 23, 17, 17, 15),
        "I": (14, 4, 4, 4, 4, 4, 14), "L": (16, 16, 16, 16, 16, 16, 31),
        "M": (17, 27, 21, 21, 17, 17, 17), "N": (17, 25, 21, 19, 17, 17, 17),
        "O": (14, 17, 17, 17, 17, 17, 14), "R": (30, 17, 17, 30, 20, 18, 17),
        "S": (15, 16, 16, 14, 1, 1, 30), "T": (31, 4, 4, 4, 4, 4, 4),
        "U": (17, 17, 17, 17, 17, 17, 14),
    }
    frames, assets = [], []
    for index, (text, y) in enumerate((("TIMER 10S", 6), ("A START", 30),
            ("B RESTART", 48), ("L CANCEL", 66), ("R GUARD", 84), ("DONE", 114))):
        width = (len(text) * 6 - 1) * 2
        stride = (width + 7) // 8
        pixels = bytearray(stride * 14)
        for letter, character in enumerate(text):
            for row, bits in enumerate(glyphs[character]):
                for column in range(5):
                    if bits & (16 >> column):
                        for dy in range(2):
                            for dx in range(2):
                                x = letter * 12 + column * 2 + dx
                                pixels[(row * 2 + dy) * stride + x // 8] |= 128 >> (x % 8)
        name = f"timer_label_{index}"
        frames.append(replace(bundle.frames[0], asset_id=name, frame_id=name + ".0", width=width,
            height=14, row_stride_bytes=stride, pixels=bytes(pixels),
            mask=bytes([255] * len(pixels)), opaque=True))
        assets.append({"asset_id": name, "asset_type": "masked_1bpp", "frames": [
            {"frame_id": name + ".0", "source_rect": {"x": 0, "y": 0, "width": width, "height": 14}}]})
        objects.append({"object_id": name, "kind": "sprite", "width": width, "height": 14,
            "z_order": 2, "layer": "SCENE",
            "defaults": {"x": 12, "y": y, "visible": True, "visual_ref": name + ".0"}})
    scene["objects"] = objects
    scene["input_actions"] = [{"action_id": name, "logical_source": button, "event_kind": "press"}
        for name, button in (("start", "BUTTON_A"), ("restart", "BUTTON_B"),
                             ("cancel", "BUTTON_L"), ("guard", "BUTTON_R"))]
    scene["routes"] = []
    for name in ("start", "restart", "cancel"):
        for state in ("start", "guard_off"):
            scene["routes"].append({"route_id": name + "_" + state, "action_ref": name,
                "from_states": [state], "guards": [], "target_state": state, "actions": [
                    {"kind": name + "_timer", "timer_ref": "delay"},
                    {"kind": "object.set_visibility", "object_ref": "done", "visible": False}]})
    for enabled in (0, 1):
        scene["routes"].append({"route_id": f"guard_{enabled}", "action_ref": "guard",
            "from_states": ["start" if enabled else "guard_off"],
            "target_state": "guard_off" if enabled else "start", "guards": [],
            "actions": [{"kind": "set_variable", "variable_ref": "allow", "operation": "assign", "value": 1 - enabled},
                        {"kind": "object.set_visibility", "object_ref": "guard", "visible": not enabled}]})
    scene["event_bindings"] = [{"binding_id": "delay", "event_type": "time.scene_elapsed",
        "configuration": {"delay_ms": 10000, "start_policy": "action"}}]
    scene["event_handlers"] = [{"handler_id": "expired", "event_ref": "delay",
        "guards": [{"variable_ref": "allow", "operator": "eq", "value": 1}],
        "actions": [{"kind": "object.set_visibility", "object_ref": "done", "visible": True}]}]
    inputs = [action["action_id"] for action in scene["input_actions"]]
    scene["interaction_policy"]["meaningful_activity_actions"] = inputs
    scene["reactive_wait_default"]["event_interests"] = inputs + ["delay"]
    project = deepcopy(bundle.project)
    project["package"]["package_id"] = "dev.peepshow.timer_controls"
    return replace(bundle, project=project, scenes=(scene,), frames=tuple(frames),
                   assets=assets, animations=())


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--egg-output", type=Path, required=True)
    args = parser.parse_args()
    blob = build_development_egg_v2(timer_controls_bundle())
    args.egg_output.parent.mkdir(parents=True, exist_ok=True)
    args.egg_output.write_bytes(blob)
    print(f"Development-only timer controls egg: {len(blob)} bytes -> {args.egg_output}")


if __name__ == "__main__":
    main()
