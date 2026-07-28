#!/usr/bin/env python3
"""Bounded PPK2 source-power and current-capture utility for HW5 bring-up."""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    from ppk2_api.ppk2_api import PPK2_API
except ModuleNotFoundError as exc:
    raise SystemExit("Missing PPK2 dependency. Run: tools/.venv/Scripts/python.exe -m pip install -r tools/requirements-ppk2.txt") from exc

SAMPLE_RATE_HZ = 100_000


def timestamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def open_source_meter(port: str, voltage_mv: int) -> PPK2_API:
    if not 800 <= voltage_mv <= 5000:
        raise ValueError("voltage must be 800..5000 mV")
    ppk = PPK2_API(port, timeout=0.05)
    ppk.get_modifiers()
    ppk.use_source_meter()
    ppk.set_source_voltage(voltage_mv)
    return ppk


def close(ppk: PPK2_API) -> None:
    """Release the host handle without issuing the API destructor reset command."""
    try:
        ppk.stop_measuring()
    except Exception:
        pass
    try:
        if ppk.ser:
            ppk.ser.close()
        ppk.ser = None
    except Exception:
        pass


def list_devices(_: argparse.Namespace) -> int:
    devices = PPK2_API.list_devices()
    if not devices:
        print("No PPK2 source-control serial interface found.")
        return 1
    for port, serial_number in devices:
        print(f"{port}\t{serial_number}")
    return 0


def session_paths(session_file: str) -> tuple[Path, Path]:
    state = Path(session_file)
    return state, state.with_suffix(state.suffix + ".stop")


def source_hold(args: argparse.Namespace) -> int:
    state_path, stop_path = session_paths(args.session_file)
    state_path.parent.mkdir(parents=True, exist_ok=True)
    stop_path.unlink(missing_ok=True)
    if os.name == "nt":
        os.system("title PeepShow PPK2 Source Session")
    print("PeepShow PPK2 source session starting.", flush=True)
    ppk = open_source_meter(args.port, args.voltage_mv)
    try:
        ppk.toggle_DUT_power("ON")
        print(f"DUT power ACTIVE: {args.port} at {args.voltage_mv} mV.", flush=True)
        print("Keep this window open while the target needs PPK2 power.", flush=True)
        print("Closing this window or sending power off will cut DUT power.", flush=True)
        state_path.write_text(json.dumps({"pid": os.getpid(), "port": args.port,
                                          "voltage_mv": args.voltage_mv}) + "\n", encoding="ascii")
        while not stop_path.exists():
            time.sleep(0.1)
    finally:
        try:
            ppk.toggle_DUT_power("OFF")
        finally:
            close(ppk)
            state_path.unlink(missing_ok=True)
            stop_path.unlink(missing_ok=True)
    return 0


def power(args: argparse.Namespace) -> int:
    state_path, stop_path = session_paths(args.session_file)
    if args.state == "off":
        if not state_path.exists():
            print("PPK2 source session is already off.")
            return 0
        stop_path.write_text("off\n", encoding="ascii")
        deadline = time.monotonic() + 5.0
        while state_path.exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        if state_path.exists():
            raise RuntimeError("PPK2 source session did not acknowledge power-off")
        print("PPK2 source power off.")
        return 0

    if state_path.exists():
        raise RuntimeError(f"PPK2 source session is already active: {state_path}")
    stop_path.unlink(missing_ok=True)
    command = [sys.executable, str(Path(__file__).resolve()), "_source-hold",
               "--port", args.port, "--voltage-mv", str(args.voltage_mv),
               "--session-file", args.session_file]
    flags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) | getattr(subprocess, "CREATE_NEW_CONSOLE", 0)
    subprocess.Popen(command, creationflags=flags)
    deadline = time.monotonic() + 5.0
    while not state_path.exists() and time.monotonic() < deadline:
        time.sleep(0.05)
    if not state_path.exists():
        raise RuntimeError("PPK2 source session did not start")
    print(f"PPK2 source power on: port={args.port}, voltage={args.voltage_mv}mV")
    return 0

def capture(args: argparse.Namespace) -> int:
    if args.seconds <= 0:
        raise ValueError("seconds must be positive")
    if not args.label.replace("-", "").replace("_", "").isalnum():
        raise ValueError("label must contain only letters, digits, '-' and '_'")

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{timestamp()}_{args.label}"
    csv_path = out_dir / f"{stem}.csv"
    metadata_path = out_dir / f"{stem}.json"
    rows: list[tuple[int, float, float, int]] = []
    currents: list[float] = []

    print("PPK2 capture: " +
          f"port={args.port}, voltage={args.voltage_mv}mV, duration={args.seconds}s, " +
          f"power_action={'on' if args.power_on else 'unchanged'}")
    ppk = open_source_meter(args.port, args.voltage_mv)
    started = time.monotonic()
    index = 0
    try:
        if args.power_on:
            ppk.toggle_DUT_power("ON")
            time.sleep(args.settle_seconds)
        ppk.start_measuring()
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            raw = ppk.get_data()
            if not raw:
                time.sleep(0.001)
                continue
            values, digital_bits = ppk.get_samples(raw)
            for current_ua, bits in zip(values, digital_bits):
                currents.append(current_ua)
                rows.append((index, index * 1_000_000 / SAMPLE_RATE_HZ, current_ua, bits))
                index += 1
    finally:
        close(ppk)

    with csv_path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.writer(handle)
        writer.writerow(("sample_index", "time_us_nominal", "current_ua", "digital_bits"))
        writer.writerows(rows)

    statistics = {"sample_count": len(currents)}
    if currents:
        statistics.update({
            "mean_ua": sum(currents) / len(currents),
            "min_ua": min(currents),
            "max_ua": max(currents),
            "rms_ua": math.sqrt(sum(value * value for value in currents) / len(currents)),
        })
    metadata = {
        "tool": "tools/ppk2_capture.py",
        "instrument": "Nordic Power Profiler Kit II",
        "api": "IRNAS ppk2-api-python 0.9.2",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "source_voltage_mv": args.voltage_mv,
        "source_power_enabled_by_capture": args.power_on,
        "duration_requested_s": args.seconds,
        "duration_elapsed_s": time.monotonic() - started,
        "sample_rate_hz_nominal": SAMPLE_RATE_HZ,
        "csv": csv_path.name,
        "statistics": statistics,
    }
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="ascii")
    print(json.dumps(statistics, indent=2))
    print(f"CSV: {csv_path}")
    print(f"Metadata: {metadata_path}")
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)

    item = commands.add_parser("list", help="list PPK2 source-control serial ports")
    item.set_defaults(func=list_devices)

    item = commands.add_parser("power", help="explicitly control PPK2 DUT source power")
    item.add_argument("--port", required=True)
    item.add_argument("--state", choices=("on", "off"), required=True)
    item.add_argument("--voltage-mv", type=int, required=True)
    item.add_argument("--session-file", default="PPK2_logs/ppk2_source_session.json")
    item.set_defaults(func=power)

    item = commands.add_parser("_source-hold")
    item.add_argument("--port", required=True)
    item.add_argument("--voltage-mv", type=int, required=True)
    item.add_argument("--session-file", required=True)
    item.set_defaults(func=source_hold)

    item = commands.add_parser("capture", help="capture a bounded source-meter current trace")
    item.add_argument("--port", required=True)
    item.add_argument("--voltage-mv", type=int, required=True)
    item.add_argument("--seconds", type=float, required=True)
    item.add_argument("--label", required=True)
    item.add_argument("--output-dir", default="PPK2_logs")
    item.add_argument("--power-on", action="store_true", help="explicitly enable DUT source power before capture")
    item.add_argument("--settle-seconds", type=float, default=0.25)
    item.set_defaults(func=capture)
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        return args.func(args)
    except (OSError, ValueError) as exc:
        print(f"PPK2 error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())





