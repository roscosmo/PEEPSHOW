"""Prepare a separate, deliberately invalid V2 egg for the USB recovery test."""
import argparse
import hashlib
import json
from pathlib import Path

from peepshow_authoring.egg_format import EggFormatError, FOOTER, parse_egg


def digest_rejection(blob):
    """Keep every payload byte intact; change one bit of the stored digest."""
    package = parse_egg(blob)
    if len(package.scenes) != 1 or package.scenes[0].get("execution_model") != 2:
        raise ValueError("Input must be a valid restricted V2 export, not a V1 egg.")
    footer_offset = len(blob) - FOOTER.size
    magic, version, size, digest = FOOTER.unpack_from(blob, footer_offset)
    wrong_digest = digest[:-1] + bytes([digest[-1] ^ 1])
    rejected = blob[:footer_offset] + FOOTER.pack(magic, version, size, wrong_digest)
    try:
        parse_egg(rejected)
    except EggFormatError as exc:
        if str(exc) != "package SHA-256 mismatch":
            raise ValueError(f"Unexpected rejection: {exc}") from exc
    else:
        raise ValueError("Damaged egg unexpectedly passed public validation.")
    return rejected, {
        "test": "v2_usb_digest_rejection",
        "package_id": package.manifest["package_id"],
        "bytes": len(blob),
        "source_sha256": hashlib.sha256(blob).hexdigest(),
        "rejected_sha256": hashlib.sha256(rejected).hexdigest(),
        "mutation": {"offset": len(blob) - 1, "before": blob[-1], "after": rejected[-1]},
        "payload_unchanged": True,
        "expected_host_error": "package SHA-256 mismatch",
        "expected_preflight_reason": 4,
        "hardware_result": "not_run",
    }


def prepare(egg_path, output_dir):
    source = Path(egg_path).resolve()
    rejected, report = digest_rejection(source.read_bytes())
    destination = Path(output_dir)
    # A fresh directory prevents replacing either the input or an earlier run.
    destination.mkdir(parents=True, exist_ok=False)
    output = destination / "DO_NOT_INSTALL_bad_digest.egg"
    output.write_bytes(rejected)
    report["source_path"] = str(source)
    report["rejected_file"] = output.name
    (destination / "evidence.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return output, report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--egg", required=True, type=Path, help="Good V2 egg exported by Studio; read only.")
    parser.add_argument("--output-dir", required=True, type=Path, help="New local directory; existing directories are refused.")
    args = parser.parse_args(argv)
    try:
        output, report = prepare(args.egg, args.output_dir)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"NOT PREPARED: {exc}\n")
    print(f"Prepared intentional DIGEST rejection: {output}")
    print(f"Source unchanged: {report['source_path']}")
    print(f"Bytes: {report['bytes']}; good SHA-256: {report['source_sha256']}")
    print("Copy only the bad egg for the negative scan test. Expect rejection, never VALID/INSTALLED.")
    print("No device access, installation or hardware test was performed. Follow the V2 rejection runbook.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
