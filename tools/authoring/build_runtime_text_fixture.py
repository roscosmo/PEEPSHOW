"""Development-only genuine runtime text fixture; no baked text assets."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path

from build_object_development import fixture_bundle
from peepshow_authoring.compiler import build_development_egg_v2
from peepshow_authoring.system_fonts import SYSTEM_FONT_8X8_BASIC_ID


def text_object(name, text, x, y, width, height, scale=1, alignment="left"):
    return dict(object_id=name, kind="text", layer="UI", z_order=2,
                width=width, height=height, text=text,
                font_id=SYSTEM_FONT_8X8_BASIC_ID, scale=scale, alignment=alignment,
                defaults=dict(x=x, y=y, visible=True))


def runtime_text_bundle():
    bundle = fixture_bundle()
    scene = deepcopy(bundle.scenes[0])
    scene["interaction_policy"].pop("bounded_deferrals", None)
    scene["routes"][2]["actions"] = []
    scene["objects"][0]["defaults"].update(x=128, y=0)
    scene["objects"][1] = text_object("marker", "Aa", 8, 64, 32, 16, 2)
    scene["states"][1]["object_overrides"] = [
        {"object_ref": "marker", "x": 120},
        {"object_ref": "hidden", "visible": False}]
    scene["objects"].extend([
        text_object("title", "Text 2x", 0, 0, 120, 16, 2),
        text_object("lines", "abc XYZ\n012 !?", 0, 22, 120, 32, 2, "center"),
        text_object("hidden", "Visible", 0, 86, 168, 16, 2, "right"),
        text_object("footer", "A: move/hide\nB: restore", 0, 106, 168, 16),
        text_object("large", "3x", 60, 64, 48, 24, 3)])
    project = deepcopy(bundle.project)
    project["package"]["package_id"] = "dev.peepshow.runtime_text"
    return replace(bundle, project=project, scenes=(scene,))


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--egg-output", type=Path, required=True)
    args = parser.parse_args()
    blob = build_development_egg_v2(runtime_text_bundle())
    args.egg_output.parent.mkdir(parents=True, exist_ok=True)
    args.egg_output.write_bytes(blob)
    print(f"Runtime text fixture: {len(blob)} bytes -> {args.egg_output}")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
