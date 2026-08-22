"""Command line interface for the first PeepShow authoring model."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

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


def main() -> int:
    parser = argparse.ArgumentParser(prog="egg-tool", description="PeepShow authoring project tools")
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate", help="validate a .peepproj directory")
    validate.add_argument("project")

    normalize = commands.add_parser("normalize", help="write deterministic normalized authoring JSON")
    normalize.add_argument("project")
    normalize.add_argument("--output", required=True)

    args = parser.parse_args()
    if args.command == "validate":
        return _validate(args.project)
    return _normalize(args.project, args.output)
