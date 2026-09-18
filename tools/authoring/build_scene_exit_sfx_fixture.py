"""OS-only scene-exit SFX bench fixture; does not widen public export."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path

from build_installed_sfx_fixture import installed_sfx_bundle
from peepshow_authoring.compiler import build_development_egg_v2


def scene_exit_sfx_bundle():
    bundle = installed_sfx_bundle()
    scenes = deepcopy(bundle.scenes)
    for scene in scenes:
        for route in scene["routes"]:
            route["actions"] = [action for action in route["actions"]
                                if action["kind"] != "play_sfx"]
            if route.get("target_scene"):
                route["actions"] = [{"kind": "play_sfx", "cue_ref":
                    "long.cue" if scene["scene_id"] == "home" else "short.cue"}]
        for handler in scene["event_handlers"]:
            handler["actions"] = [action for action in handler["actions"]
                                  if action["kind"] != "play_sfx"]
    project = deepcopy(bundle.project)
    project["package"]["package_id"] = "dev.peepshow.scene_exit_sfx"
    return replace(bundle, project=project, scenes=scenes)


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--egg-output", type=Path, required=True)
    args = parser.parse_args()
    blob = build_development_egg_v2(scene_exit_sfx_bundle())
    if len(blob) > 65536:
        raise ValueError("Scene-exit SFX fixture exceeds resident capacity")
    args.egg_output.parent.mkdir(parents=True, exist_ok=True)
    args.egg_output.write_bytes(blob)
    print(f"{len(blob)} bytes -> {args.egg_output}")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
