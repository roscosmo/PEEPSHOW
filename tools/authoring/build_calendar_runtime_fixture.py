"""OS-only calendar dispatch fixture. No advertised calendar package encoding."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path

from build_timer_controls_fixture import timer_controls_bundle
from build_runtime_text_fixture import text_object
from peepshow_authoring.compiler import build_development_egg_v2


def calendar_runtime_bundle():
    bundle = timer_controls_bundle()
    scene = deepcopy(bundle.scenes[0])
    scene["display_name"] = "Calendar Dispatch"
    scene["variables"] = []
    scene["states"] = [{"state_id": "start", "display_name": "Calendar", "object_overrides": []}]
    scene["objects"] = [
        text_object("title", "Calendar", 8, 8, 128, 16, 2),
        text_object("instructions", "Scheduled event\nA clears square", 8, 36, 152, 16),
        {"object_id": "outline", "kind": "outline_rect", "width": 32, "height": 32,
         "layer": "SCENE", "z_order": 0, "defaults": {"x": 64, "y": 74, "visible": True}},
        {"object_id": "done", "kind": "filled_rect", "width": 24, "height": 24,
         "layer": "SCENE", "z_order": 1, "defaults": {"x": 68, "y": 78, "visible": False}}]
    # An action-started timer provides a real existing independent V2 handler.
    # No action starts it. Only the explicit firmware bench registration fires it.
    scene["input_actions"] = [{"action_id": "clear", "logical_source": "BUTTON_A", "event_kind": "press"}]
    scene["routes"] = [{"route_id": "clear", "action_ref": "clear", "from_states": ["start"],
        "target_state": "start", "guards": [], "actions": [
            {"kind": "object.set_visibility", "object_ref": "done", "visible": False}]}]
    scene["event_handlers"][0]["guards"] = []
    scene["interaction_policy"]["meaningful_activity_actions"] = ["clear"]
    scene["reactive_wait_default"]["event_interests"] = ["clear", "delay"]
    project = deepcopy(bundle.project)
    project["package"]["package_id"] = "dev.peepshow.calendar_dispatch"
    return replace(bundle, project=project, scenes=(scene,), frames=(), assets=[], animations=())


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--egg-output", type=Path, required=True)
    args = parser.parse_args()
    blob = build_development_egg_v2(calendar_runtime_bundle())
    args.egg_output.parent.mkdir(parents=True, exist_ok=True)
    args.egg_output.write_bytes(blob)
    print(f"Calendar dispatch bench: {len(blob)} bytes -> {args.egg_output}")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
