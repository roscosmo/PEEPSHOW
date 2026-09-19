"""Build the scene-owned calendar fixture through the normal public exporter."""
from argparse import ArgumentParser
from copy import deepcopy
from dataclasses import replace
import hashlib
from pathlib import Path

from build_calendar_runtime_fixture import calendar_runtime_bundle
from peepshow_authoring.compiler import build_egg
from peepshow_authoring.project import load_project, save_project


def calendar_export_bundle(mode="daily", seconds=0, day_offset=0):
    bundle = calendar_runtime_bundle()
    scene = deepcopy(bundle.scenes[0])
    scene["event_bindings"] = [{"binding_id": "delay", "event_type": "time.local_schedule",
        "configuration": {"mode": mode, "time_of_day_seconds": seconds, "day_offset": day_offset}}]
    project = deepcopy(bundle.project)
    project.pop("editor", None)
    project["package"]["package_id"] = "dev.peepshow.calendar_export"
    project["scene_sources"] = ["scenes/calendar.state.json"]
    project["asset_sources"] = []
    return replace(bundle, project=project, scenes=(scene,),
                   scene_sources=("scenes/calendar.state.json",),
                   asset_catalogs=(), asset_catalog_sources=())


def write_project(root):
    root.mkdir(parents=True, exist_ok=True)
    save_project(replace(calendar_export_bundle(), root=root))
    bundle = load_project(root)
    if not bundle.valid:
        raise ValueError(bundle.issues)
    return bundle


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    bundle = write_project(args.output / "calendar_export.peepproj")
    blob = build_egg(bundle)
    path = args.output / "calendar_export.egg"
    path.write_bytes(blob)
    print(f"Public calendar export: {len(blob)} bytes -> {path}")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
