#!/usr/bin/env python3
"""Persistent PPK2 source-power, live telemetry, and bounded capture service."""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import socket
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime, timezone
from pathlib import Path

from ppk2_api.ppk2_api import PPK2_API

SAMPLE_RATE_HZ = 100_000
SERVICE_PORT = 49365
SESSION_FILE = Path("PPK2_logs/ppk2_service_session.json")
SERVICE_PROTOCOL = 7
PLOT_POINTS_PER_SECOND = 100
DIGITAL_CHANNEL_COUNT = 8
PlotPoint = tuple[float, float, int, int]


def open_ppk(port: str, voltage_mv: int) -> PPK2_API:
    if not 800 <= voltage_mv <= 5000:
        raise ValueError("voltage must be 800..5000 mV")
    ppk = PPK2_API(port, timeout=0.05)
    ppk.get_modifiers()
    ppk.use_source_meter()
    ppk.set_source_voltage(voltage_mv)
    return ppk


def close_ppk(ppk: PPK2_API) -> None:
    try:
        ppk.stop_measuring()
    except Exception:
        pass
    if ppk.ser:
        ppk.ser.close()
    ppk.ser = None


def write_response(handle, value: dict) -> None:
    handle.write(json.dumps(value, separators=(",", ":")).encode("ascii") + b"\n")
    handle.flush()


def valid_label(label: str) -> bool:
    return bool(label) and label.replace("-", "").replace("_", "").isalnum()


def _window_start(points: list[PlotPoint], window_s: float | None) -> int:
    if not points or window_s is None:
        return 0
    cutoff = points[-1][0] - window_s
    low = 0
    high = len(points)
    while low < high:
        middle = (low + high) // 2
        if points[middle][0] < cutoff:
            low = middle + 1
        else:
            high = middle
    return low


def plot_view(points: list[PlotPoint], window_s: float | None, max_points: int) -> list[PlotPoint]:
    """Return a bounded display view while preserving extrema, final state, and edge masks."""
    selected = points[_window_start(points, window_s):]
    if max_points <= 0 or len(selected) <= max_points:
        return list(selected)

    bucket_count = max(1, max_points // 4)
    bucket_size = math.ceil(len(selected) / bucket_count)
    result: list[PlotPoint] = []
    for offset in range(0, len(selected), bucket_size):
        bucket = selected[offset:offset + bucket_size]
        if not bucket:
            continue
        minimum = min(range(len(bucket)), key=lambda index: bucket[index][1])
        maximum = max(range(len(bucket)), key=lambda index: bucket[index][1])
        indices = sorted({0, minimum, maximum, len(bucket) - 1})
        transition_mask = 0
        for point in bucket:
            transition_mask |= point[3]
        for index in indices:
            time_s, current_ua, digital_bits, _ = bucket[index]
            result.append((
                time_s,
                current_ua,
                digital_bits,
                transition_mask if index == indices[-1] else 0,
            ))
    return result


def recent_plot_statistics(points: list[PlotPoint], window_s: float = 1.0) -> dict:
    selected = points[_window_start(points, window_s):]
    if not selected:
        return {}
    currents = [point[1] for point in selected]
    return {
        "window_s": window_s,
        "mean_ua": sum(currents) / len(currents),
        "min_ua": min(currents),
        "max_ua": max(currents),
        "point_count": len(currents),
    }


class CaptureState:
    def __init__(self, ppk: PPK2_API, port: str, voltage_mv: int) -> None:
        self.ppk = ppk
        self.port = port
        self.voltage_mv = voltage_mv
        self.lock = threading.Lock()
        self.live_stop = threading.Event()
        self.worker: threading.Thread | None = None
        self.meter_active = False
        self.live_started = 0.0
        self.plot_started = 0.0
        self.active = False
        self.record_started = 0.0
        self.record_deadline = 0.0
        self.requested_seconds = 0.0
        self.label = ""
        self.output_dir = "PPK2_logs"
        self.sample_stride = 1
        self.sample_count = 0
        self.current_sum = 0.0
        self.current_square_sum = 0.0
        self.charge_mah = 0.0
        self.energy_mwh = 0.0
        self.min_ua: float | None = None
        self.max_ua: float | None = None
        self.plot: list[PlotPoint] = []
        self.plot_sum = 0.0
        self.plot_count = 0
        self.plot_transition_mask = 0
        self.latest_digital_bits: int | None = None
        self.digital_edge_counts = [0] * DIGITAL_CHANNEL_COUNT
        self.record_plot: list[PlotPoint] = []
        self.history = deque(maxlen=8)
        self.trace_id = ""
        self.last_result: dict | None = None
        self.last_error = ""
        self.record_file = None
        self.record_writer = None
        self.record_csv_path: Path | None = None
        self.record_metadata_path: Path | None = None
        self.record_sample_count = 0
        self.record_current_sum = 0.0
        self.record_current_square_sum = 0.0
        self.record_charge_mah = 0.0
        self.record_energy_mwh = 0.0
        self.record_min_ua: float | None = None
        self.record_max_ua: float | None = None
        self.record_latest_digital_bits: int | None = None
        self.record_plot_transition_mask = 0
        self.record_digital_edge_counts = [0] * DIGITAL_CHANNEL_COUNT

    def start_live(self) -> None:
        with self.lock:
            if self.meter_active:
                return
            self.live_stop.clear()
            self.meter_active = True
            self.live_started = time.monotonic()
            self.plot_started = self.live_started
            self.sample_count = 0
            self.current_sum = 0.0
            self.current_square_sum = 0.0
            self.charge_mah = 0.0
            self.energy_mwh = 0.0
            self.min_ua = None
            self.max_ua = None
            self.plot.clear()
            self.plot_sum = 0.0
            self.plot_count = 0
            self.plot_transition_mask = 0
            self.latest_digital_bits = None
            self.digital_edge_counts = [0] * DIGITAL_CHANNEL_COUNT
            self.last_error = ""
            self.worker = threading.Thread(target=self._meter_worker, daemon=True, name="ppk2-meter")
            self.worker.start()

    def stop_live(self) -> None:
        self.live_stop.set()
        worker = self.worker
        if worker is not None:
            worker.join(3.0)

    def reset_plot(self) -> dict:
        with self.lock:
            self.plot_started = time.monotonic()
            self.sample_count = 0
            self.current_sum = 0.0
            self.current_square_sum = 0.0
            self.charge_mah = 0.0
            self.energy_mwh = 0.0
            self.min_ua = None
            self.max_ua = None
            self.plot.clear()
            self.plot_sum = 0.0
            self.plot_count = 0
            self.plot_transition_mask = 0
            self.latest_digital_bits = None
            self.digital_edge_counts = [0] * DIGITAL_CHANNEL_COUNT
        return {"ok": True, "plot_reset": True}

    def snapshot(self, include_plot: bool = False, trace_id: str = "live",
                 plot_window_s: float | None = None, plot_max_points: int = 0) -> dict:
        with self.lock:
            stats = {"sample_count": self.sample_count}
            if self.sample_count:
                stats.update(
                    mean_ua=self.current_sum / self.sample_count,
                    min_ua=self.min_ua,
                    max_ua=self.max_ua,
                    rms_ua=math.sqrt(self.current_square_sum / self.sample_count),
                    charge_mah=self.charge_mah,
                    energy_mwh=self.energy_mwh,
                )
            selected = self.plot
            selected_id = "live"
            selected_latest_bits = self.latest_digital_bits
            selected_edge_counts = list(self.digital_edge_counts)
            if trace_id != "live":
                for item in self.history:
                    if item["id"] == trace_id:
                        selected = item["plot"]
                        selected_id = trace_id
                        selected_latest_bits = item["digital_latest_bits"]
                        selected_edge_counts = item["digital_edge_counts"]
                        break
            value = {
                "meter_active": self.meter_active,
                "capture_active": self.active,
                "capture_label": self.label,
                "capture_elapsed_s": max(0.0, time.monotonic() - self.record_started) if self.active else 0.0,
                "capture_requested_s": self.requested_seconds,
                "statistics": stats,
                "last_result": self.last_result,
                "last_error": self.last_error,
                "selected_trace_id": selected_id,
                "history": [{"id": item["id"], "label": item["label"]} for item in self.history],
                "digital_latest_bits": selected_latest_bits,
                "digital_edge_counts": selected_edge_counts,
                "digital_channels": [f"D{index}" for index in range(DIGITAL_CHANNEL_COUNT)],
                "recent_plot_statistics": recent_plot_statistics(selected),
            }
            if include_plot:
                value["plot"] = plot_view(selected, plot_window_s, plot_max_points)
                value["plot_points_available"] = len(selected)
                value["plot_points_returned"] = len(value["plot"])
            return value

    def start(self, request: dict) -> dict:
        seconds = float(request.get("seconds", 0))
        label = str(request.get("label", "capture"))
        stride = int(request.get("sample_stride", 1))
        if seconds <= 0:
            raise ValueError("seconds must be positive")
        if not 1 <= stride <= 1000:
            raise ValueError("sample stride must be 1..1000")
        if not valid_label(label):
            raise ValueError("label must contain only letters, digits, '-' and '_'")
        output_dir = Path(str(request.get("output_dir", "PPK2_logs")))
        output_dir.mkdir(parents=True, exist_ok=True)
        with self.lock:
            if not self.meter_active:
                raise RuntimeError("Target power is off; enable source power before recording")
            if self.active:
                raise RuntimeError("recording already active")
            stem = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + label
            self.record_csv_path = output_dir / f"{stem}.csv"
            self.record_metadata_path = output_dir / f"{stem}.json"
            self.record_file = self.record_csv_path.open("w", newline="", encoding="ascii")
            self.record_writer = csv.writer(self.record_file)
            self.record_writer.writerow(("sample_index", "time_us", "current_ua", "digital_bits"))
            self.active = True
            self.record_started = time.monotonic()
            self.record_deadline = self.record_started + seconds
            self.requested_seconds = seconds
            self.label = label
            self.sample_stride = stride
            self.record_sample_count = 0
            self.record_current_sum = 0.0
            self.record_current_square_sum = 0.0
            self.record_charge_mah = 0.0
            self.record_energy_mwh = 0.0
            self.record_min_ua = None
            self.record_max_ua = None
            self.record_plot = []
            self.last_error = ""
            self.record_latest_digital_bits = self.latest_digital_bits
            self.record_plot_transition_mask = 0
            self.record_digital_edge_counts = [0] * DIGITAL_CHANNEL_COUNT
        return {"ok": True, "capture_active": True}

    def stop(self) -> dict:
        with self.lock:
            self._finalize_recording_locked()
        return {"ok": True, "capture_stop_requested": True}

    def _finalize_recording_locked(self) -> None:
        if not self.active:
            return
        elapsed = time.monotonic() - self.record_started
        if self.record_file is not None:
            self.record_file.close()
        stats = {"sample_count": self.record_sample_count}
        if self.record_sample_count:
            stats.update(
                mean_ua=self.record_current_sum / self.record_sample_count,
                min_ua=self.record_min_ua,
                max_ua=self.record_max_ua,
                rms_ua=math.sqrt(self.record_current_square_sum / self.record_sample_count),
                charge_mah=self.record_charge_mah,
                energy_mwh=self.record_energy_mwh,
            )
        self.trace_id = self.record_csv_path.stem if self.record_csv_path else "recording"
        result = {
            "ok": not self.last_error,
            "statistics": stats,
            "csv": str(self.record_csv_path) if self.record_csv_path else "",
            "metadata": str(self.record_metadata_path) if self.record_metadata_path else "",
            "duration_elapsed_s": elapsed,
            "csv_sample_rate_hz": SAMPLE_RATE_HZ / self.sample_stride,
            "trace_id": self.trace_id,
        }
        if self.last_error:
            result["error"] = self.last_error
        self.last_result = result
        self.history.append({
            "id": self.trace_id,
            "label": self.label,
            "plot": list(self.record_plot),
            "digital_latest_bits": self.record_latest_digital_bits,
            "digital_edge_counts": list(self.record_digital_edge_counts),
        })
        if self.record_metadata_path is not None:
            self.record_metadata_path.write_text(json.dumps({
                "tool": "tools/ppk2_service.py",
                "instrument": "Nordic Power Profiler Kit II",
                "captured_at_utc": datetime.now(timezone.utc).isoformat(),
                "port": self.port,
                "source_voltage_mv": self.voltage_mv,
                "duration_requested_s": self.requested_seconds,
                "duration_elapsed_s": elapsed,
                "sample_rate_hz_nominal": SAMPLE_RATE_HZ,
                "csv_sample_rate_hz": SAMPLE_RATE_HZ / self.sample_stride,
                "csv_sample_stride": self.sample_stride,
                "csv": self.record_csv_path.name if self.record_csv_path else "",
                "digital_bit_mapping": {f"D{index}": index for index in range(DIGITAL_CHANNEL_COUNT)},
                "statistics": stats,
                "error": self.last_error or None,
            }, indent=2) + "\n", encoding="ascii")
        self.active = False
        self.record_file = None
        self.record_writer = None

    def _meter_worker(self) -> None:
        try:
            self.ppk.start_measuring()
            while not self.live_stop.is_set():
                raw = self.ppk.get_data()
                if not raw:
                    time.sleep(0.001)
                    continue
                values, bits = self.ppk.get_samples(raw)
                for current_ua, digital_bits in zip(values, bits):
                    current = float(current_ua)
                    digital = int(digital_bits) & 0xff
                    now = time.monotonic()
                    with self.lock:
                        if not self.meter_active:
                            break
                        time_s = now - self.plot_started
                        self.sample_count += 1
                        self.current_sum += current
                        self.current_square_sum += current * current
                        sample_mah = current / (3_600_000.0 * SAMPLE_RATE_HZ)
                        self.charge_mah += sample_mah
                        self.energy_mwh += sample_mah * (self.voltage_mv / 1000.0)
                        self.min_ua = current if self.min_ua is None else min(self.min_ua, current)
                        self.max_ua = current if self.max_ua is None else max(self.max_ua, current)
                        if self.latest_digital_bits is None:
                            self.latest_digital_bits = digital
                        else:
                            changed = self.latest_digital_bits ^ digital
                            if changed:
                                self.plot_transition_mask |= changed
                                for channel in range(DIGITAL_CHANNEL_COUNT):
                                    if changed & (1 << channel):
                                        self.digital_edge_counts[channel] += 1
                            self.latest_digital_bits = digital

                        if self.active:
                            if self.record_latest_digital_bits is None:
                                self.record_latest_digital_bits = digital
                            else:
                                record_changed = self.record_latest_digital_bits ^ digital
                                if record_changed:
                                    self.record_plot_transition_mask |= record_changed
                                    for channel in range(DIGITAL_CHANNEL_COUNT):
                                        if record_changed & (1 << channel):
                                            self.record_digital_edge_counts[channel] += 1
                                self.record_latest_digital_bits = digital

                        self.plot_sum += current
                        self.plot_count += 1
                        if self.plot_count >= SAMPLE_RATE_HZ // PLOT_POINTS_PER_SECOND:
                            point = (
                                time_s,
                                self.plot_sum / self.plot_count,
                                digital,
                                self.plot_transition_mask,
                            )
                            self.plot.append(point)
                            if self.active:
                                self.record_plot.append((
                                    now - self.record_started,
                                    point[1],
                                    digital,
                                    self.record_plot_transition_mask,
                                ))
                            self.plot_sum = 0.0
                            self.plot_count = 0
                            self.plot_transition_mask = 0
                            self.record_plot_transition_mask = 0
                        if self.active:
                            record_time_s = now - self.record_started
                            if self.record_sample_count % self.sample_stride == 0:
                                self.record_writer.writerow((
                                    self.record_sample_count,
                                    record_time_s * 1_000_000,
                                    current,
                                    digital,
                                ))
                            self.record_sample_count += 1
                            self.record_current_sum += current
                            self.record_current_square_sum += current * current
                            sample_mah = current / (3_600_000.0 * SAMPLE_RATE_HZ)
                            self.record_charge_mah += sample_mah
                            self.record_energy_mwh += sample_mah * (self.voltage_mv / 1000.0)
                            self.record_min_ua = current if self.record_min_ua is None else min(self.record_min_ua, current)
                            self.record_max_ua = current if self.record_max_ua is None else max(self.record_max_ua, current)
                            if now >= self.record_deadline:
                                self._finalize_recording_locked()
        except Exception as exc:
            with self.lock:
                self.last_error = str(exc)
                self._finalize_recording_locked()
        finally:
            try:
                self.ppk.stop_measuring()
            except Exception:
                pass
            with self.lock:
                self.meter_active = False


def service(args: argparse.Namespace) -> int:
    SESSION_FILE.parent.mkdir(parents=True, exist_ok=True)
    if os.name == "nt" and not args.quiet:
        os.system("title PeepShow PPK2 Source and Capture Service")
    if not args.quiet:
        print("PeepShow PPK2 source and capture service starting.", flush=True)
    ppk = open_ppk(args.port, args.voltage_mv)
    capture_state = CaptureState(ppk, args.port, args.voltage_mv)
    target_power = False
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", args.service_port))
    listener.listen(4)
    listener.settimeout(0.5)
    running = True
    try:
        SESSION_FILE.write_text(json.dumps({
            "pid": os.getpid(), "port": args.port, "voltage_mv": args.voltage_mv,
            "service_port": args.service_port, "protocol": SERVICE_PROTOCOL,
        }) + "\n", encoding="ascii")
        if not args.quiet:
            print(f"PPK2 acquired: {args.port} at {args.voltage_mv} mV source setting; Target power is OFF.", flush=True)
            print(f"Control listens only on 127.0.0.1:{args.service_port}.", flush=True)
            print("Keep this window open. Closing it releases the PPK2 and cuts Target power.", flush=True)
        while running:
            try:
                connection, _ = listener.accept()
            except socket.timeout:
                continue
            with connection:
                handle = connection.makefile("rwb")
                try:
                    request = json.loads(handle.readline().decode("ascii"))
                    command = request.get("command")
                    if command in ("status", "snapshot"):
                        plot_window_s = None
                        plot_max_points = 0
                        if command == "snapshot":
                            if request.get("plot_window_s") is not None:
                                plot_window_s = float(request["plot_window_s"])
                                if not 0.05 <= plot_window_s <= 86400.0:
                                    raise ValueError("plot_window_s must be 0.05..86400")
                            plot_max_points = int(request.get("plot_max_points", 0))
                            if plot_max_points != 0 and not 100 <= plot_max_points <= 10000:
                                raise ValueError("plot_max_points must be 100..10000")
                        response = {
                            "ok": True, "power": "on" if target_power else "off",
                            "port": args.port, "voltage_mv": args.voltage_mv,
                            "service_port": args.service_port, "protocol": SERVICE_PROTOCOL,
                            **capture_state.snapshot(
                                command == "snapshot",
                                str(request.get("trace_id", "live")),
                                plot_window_s,
                                plot_max_points,
                            ),
                        }
                    elif command == "power_on":
                        ppk.toggle_DUT_power("ON")
                        capture_state.start_live()
                        target_power = True
                        response = {"ok": True, "power": "on"}
                    elif command == "power_off":
                        capture_state.stop()
                        capture_state.stop_live()
                        if target_power:
                            ppk.toggle_DUT_power("OFF")
                        target_power = False
                        response = {"ok": True, "power": "off"}
                    elif command == "power_cycle":
                        off_seconds = float(request.get("off_seconds", 0.5))
                        if not 0.05 <= off_seconds <= 10.0:
                            raise ValueError("off_seconds must be 0.05..10.0")
                        capture_state.stop()
                        capture_state.stop_live()
                        if target_power:
                            ppk.toggle_DUT_power("OFF")
                        target_power = False
                        time.sleep(off_seconds)
                        ppk.toggle_DUT_power("ON")
                        capture_state.start_live()
                        target_power = True
                        response = {"ok": True, "power": "on", "cycled": True, "off_seconds": off_seconds}
                    elif command in ("capture", "capture_start"):
                        if not target_power:
                            raise RuntimeError("Target power is off; enable source power before capture")
                        response = capture_state.start(request)
                    elif command == "capture_stop":
                        response = capture_state.stop()
                    elif command == "plot_reset":
                        response = capture_state.reset_plot()
                    elif command == "stop":
                        capture_state.stop()
                        capture_state.stop_live()
                        response, running = {"ok": True, "stopping": True}, False
                    else:
                        response = {"ok": False, "error": "unsupported command"}
                except (KeyError, OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
                    response = {"ok": False, "error": str(exc)}
                write_response(handle, response)
    finally:
        capture_state.stop()
        capture_state.stop_live()
        try:
            if target_power:
                ppk.toggle_DUT_power("OFF")
        finally:
            listener.close()
            close_ppk(ppk)
            SESSION_FILE.unlink(missing_ok=True)
            if not args.quiet:
                print("Target power OFF. PPK2 service stopped.", flush=True)
    return 0


def service_request(command: str, seconds: float | None = None, label: str | None = None,
                    output_dir: str | None = None, extra: dict | None = None) -> dict:
    payload = {"command": command}
    if seconds is not None:
        payload["seconds"] = seconds
    if label is not None:
        payload["label"] = label
    if output_dir is not None:
        payload["output_dir"] = output_dir
    if extra:
        payload.update(extra)
    with socket.create_connection(("127.0.0.1", SERVICE_PORT), timeout=3.0) as connection:
        connection.settimeout(5.0)
        handle = connection.makefile("rwb")
        write_response(handle, payload)
        response = handle.readline()
    if not response:
        raise RuntimeError("PPK2 service closed the request without a response")
    result = json.loads(response.decode("ascii"))
    if not result.get("ok"):
        raise RuntimeError(result.get("error", "unknown PPK2 service error"))
    return result


def start_service(port: str, voltage_mv: int, background: bool) -> None:
    if SESSION_FILE.exists():
        try:
            service_request("status")
        except (OSError, RuntimeError):
            SESSION_FILE.unlink(missing_ok=True)
        else:
            raise RuntimeError(f"PPK2 service already active: {SESSION_FILE}")
    command = [sys.executable, str(Path(__file__).resolve()), "service", "--port", port,
               "--voltage-mv", str(voltage_mv)]
    flags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    if background and os.name == "nt":
        command.append("--quiet")
        flags |= getattr(subprocess, "CREATE_NO_WINDOW", 0)
    else:
        flags |= getattr(subprocess, "CREATE_NEW_CONSOLE", 0)
    subprocess.Popen(command, creationflags=flags)
    deadline = time.monotonic() + 5.0
    while not SESSION_FILE.exists() and time.monotonic() < deadline:
        time.sleep(0.05)
    if not SESSION_FILE.exists():
        raise RuntimeError("PPK2 service did not start")


def power(args: argparse.Namespace) -> int:
    if args.state == "off":
        print(json.dumps(service_request("stop"), indent=2))
        return 0
    start_service(args.port, args.voltage_mv, args.background)
    print(json.dumps(service_request("power_on"), indent=2))
    return 0


def capture_wait(args: argparse.Namespace) -> int:
    service_request("capture_start", args.seconds, args.label, args.output_dir)
    deadline = time.monotonic() + args.seconds + 15.0
    while time.monotonic() < deadline:
        state = service_request("status")
        if not state.get("capture_active"):
            print(json.dumps(state.get("last_result") or state, indent=2))
            return 0
        time.sleep(0.1)
    raise RuntimeError("capture did not complete within bounded timeout")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    item = commands.add_parser("service", help=argparse.SUPPRESS)
    item.add_argument("--port", required=True)
    item.add_argument("--voltage-mv", type=int, required=True)
    item.add_argument("--service-port", type=int, default=SERVICE_PORT)
    item.add_argument("--quiet", action="store_true")
    item = commands.add_parser("power", help="start or stop persistent source power")
    item.add_argument("--port", required=True)
    item.add_argument("--voltage-mv", type=int, required=True)
    item.add_argument("--state", choices=("on", "off"), required=True)
    item.add_argument("--background", action="store_true",
                      help="run service without a console; use only through the visible PPK console")
    commands.add_parser("status", help="read active source service status")
    item = commands.add_parser("capture", help="capture without releasing Target source power")
    item.add_argument("--seconds", type=float, required=True)
    item.add_argument("--label", required=True)
    item.add_argument("--output-dir", default="PPK2_logs")
    commands.add_parser("capture-stop", help="request stop for the active capture")
    args = parser.parse_args()
    try:
        if args.command == "service":
            return service(args)
        if args.command == "power":
            return power(args)
        if args.command == "status":
            print(json.dumps(service_request("status"), indent=2))
        elif args.command == "capture-stop":
            print(json.dumps(service_request("capture_stop"), indent=2))
        else:
            return capture_wait(args)
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"PPK2 error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
























