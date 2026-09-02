#!/usr/bin/env python3
"""Generate the HW6 firmware target-profile header from canonical JSON."""

from __future__ import annotations

import argparse

from peepshow_authoring.target_profile import (
    TARGET_PROFILE_HEADER_PATH,
    render_firmware_header,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if the committed firmware header is stale",
    )
    args = parser.parse_args()
    expected = render_firmware_header()
    if args.check:
        try:
            current = TARGET_PROFILE_HEADER_PATH.read_text(encoding="ascii")
        except OSError:
            current = ""
        if current != expected:
            parser.error(
                f"{TARGET_PROFILE_HEADER_PATH} is stale; run gen_target_profile.py"
            )
        return 0
    TARGET_PROFILE_HEADER_PATH.write_text(expected, encoding="ascii")
    print(f"generated {TARGET_PROFILE_HEADER_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
