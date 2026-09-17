"""OS-only resident V2 SFX fixture; public audio export remains disabled."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path

from build_object_development import scene_exit_fixture_bundle, sfx_fixture_bundle
from peepshow_authoring.compiler import build_development_egg_v2


def installed_sfx_bundle():
    bundle = scene_exit_fixture_bundle()
    audio = sfx_fixture_bundle()
    scenes = deepcopy(bundle.scenes)
    for scene in scenes:
        for route in scene["routes"]:
            if route.get("target_scene") is None:
                route["actions"].append({"kind": "play_sfx", "cue_ref": "short.cue"})
        if scene["scene_id"] == "home":
            scene["event_handlers"][0]["actions"].append(
                {"kind": "play_sfx", "cue_ref": "long.cue"})
        else:
            scene["event_bindings"] = []
            scene["event_handlers"] = []
            scene["reactive_wait_default"]["event_interests"].remove("scene_delay")
    project = deepcopy(bundle.project)
    project["package"]["package_id"] = "dev.peepshow.installed_v2_sfx"
    return replace(bundle, project=project, scenes=scenes,
                   audio_assets=audio.audio_assets, audio_cues=audio.audio_cues)


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--egg-output", type=Path, required=True)
    args = parser.parse_args()
    blob = build_development_egg_v2(installed_sfx_bundle())
    if len(blob) > 65536:
        raise ValueError("SFX bench fixture exceeds resident package capacity")
    args.egg_output.parent.mkdir(parents=True, exist_ok=True)
    args.egg_output.write_bytes(blob)
    print(f"OS-only installed SFX egg: {len(blob)} bytes -> {args.egg_output}")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
