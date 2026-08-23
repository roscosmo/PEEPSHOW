"""Command line interface for the first PeepShow authoring model."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from .compiler import EggCompileError, write_egg, write_embedded_egg_c
from .egg_format import EggFormatError, parse_egg
from .project import format_issues, load_project


def _validate(project_path: str) -> int:
    bundle = load_project(project_path)
    if not bundle.valid:
        print(format_issues(bundle.issues))
        return 1
    digest = hashlib.sha256(bundle.canonical_bytes()).hexdigest()
    print(f"valid project={bundle.project['project_id']} scenes={len(bundle.scenes)} sha256={digest}")
    return 0


def _normalize(project_path: str, output_path: str) -> int:
    bundle = load_project(project_path)
    if not bundle.valid:
        print(format_issues(bundle.issues))
        return 1
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(bundle.canonical_bytes())
    print(f"normalized {bundle.project['project_id']} -> {output}")
    return 0


def _build(project_path: str, output_path: str) -> int:
    bundle = load_project(project_path)
    if not bundle.valid:
        print(format_issues(bundle.issues))
        return 1
    try:
        output = write_egg(bundle, output_path)
        package = parse_egg(output.read_bytes())
    except (EggCompileError, EggFormatError, OSError) as exc:
        print(f"BUILD_PACKAGE_INVALID {exc}")
        return 1
    print(
        f"built package={package.manifest['package_id']} scenes={len(package.scenes)} "
        f"chunks={len(package.chunks)} bytes={output.stat().st_size} sha256={package.sha256} -> {output}"
    )
    return 0


def _inspect(package_path: str) -> int:
    path = Path(package_path)
    if path.suffix.lower() != ".egg":
        print("PACKAGE_SUFFIX_INVALID installable package must end in .egg")
        return 1
    try:
        package = parse_egg(path.read_bytes())
    except (EggFormatError, OSError) as exc:
        print(f"PACKAGE_INVALID {exc}")
        return 1
    manifest = package.manifest
    print(
        f"valid egg package={manifest['package_id']} version="
        f"{manifest['version_major']}.{manifest['version_minor']}.{manifest['version_patch']} "
        f"target={manifest['target_profile']} entry={manifest['entry_scene']} "
        f"scenes={len(package.scenes)} chunks={len(package.chunks)} sha256={package.sha256}"
    )
    for scene in package.scenes:
        print(
            f"scene id={scene['scene_id']} type=STATE states={scene['state_count']} "
            f"routes={scene['route_count']}"
        )
    return 0


def _embed(project_path: str, output_path: str, symbol: str) -> int:
    bundle = load_project(project_path)
    if not bundle.valid:
        print(format_issues(bundle.issues))
        return 1
    try:
        output = write_embedded_egg_c(bundle, output_path, symbol)
    except (EggCompileError, OSError) as exc:
        print(f"EMBED_PACKAGE_INVALID {exc}")
        return 1
    print(f"embedded package={bundle.project['package']['package_id']} symbol={symbol} -> {output}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(prog="egg-tool", description="PeepShow authoring project tools")
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate", help="validate a .peepproj directory")
    validate.add_argument("project")

    normalize = commands.add_parser("normalize", help="write deterministic normalized authoring JSON")
    normalize.add_argument("project")
    normalize.add_argument("--output", required=True)

    build = commands.add_parser("build", help="compile a .peepproj directory into a deterministic .egg")
    build.add_argument("project")
    build.add_argument("--output", required=True)

    inspect = commands.add_parser("inspect", help="validate and summarize a compiled .egg")
    inspect.add_argument("package")

    embed = commands.add_parser("embed", help="compile a .peepproj directory into a firmware C array")
    embed.add_argument("project")
    embed.add_argument("--output", required=True)
    embed.add_argument("--symbol", default="g_ps_embedded_egg")

    commands.add_parser("service", help="run the versioned authoring service over stdin/stdout")

    args = parser.parse_args()
    if args.command == "validate":
        return _validate(args.project)
    if args.command == "normalize":
        return _normalize(args.project, args.output)
    if args.command == "build":
        return _build(args.project, args.output)
    if args.command == "inspect":
        return _inspect(args.package)
    if args.command == "service":
        from .service import main as service_main

        return service_main()
    return _embed(args.project, args.output, args.symbol)
