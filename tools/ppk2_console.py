#!/usr/bin/env python3
"""Visible development console for the PeepShow PPK2 service."""
from __future__ import annotations

import math
import queue
import threading
import tkinter as tk
from tkinter import ttk

import ppk2_service

POLL_MS = 250
LOGIC_CHANNELS = tuple(f"D{index}" for index in range(1, 8))


class PpkConsole:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("PeepShow PPK2 Console")
        self.root.minsize(900, 650)
        self.port = tk.StringVar(value="COM10")
        self.voltage = tk.StringVar(value="3300")
        self.seconds = tk.StringVar(value="20")
        self.label = tk.StringVar(value="capture")
        self.plot_range = tk.StringVar(value="Since power on/reset")
        self.csv_rate = tk.StringVar(value="100 kS/s")
        self.trace_choice = tk.StringVar(value="Live")
        self.logic_channel = tk.StringVar(value="D7")
        self.logic_status = tk.StringVar(value="D7 logic: --    Edges: --")
        self.trace_lookup = {"Live": "live"}
        self.trace_id = "live"
        self.owner = tk.StringVar(value="Checking PPK2 ownership...")
        self.capture = tk.StringVar(value="No capture active.")
        self.stats = tk.StringVar(value="Session mean: --    Min: --    Max: --    RMS: --")
        self.recent_stats = tk.StringVar(value="Recent current: --")
        self.energy_stats = tk.StringVar(value="Charge/Energy: --")
        self.artifact = tk.StringVar(value="Last artifact: --")
        self.results: queue.Queue[tuple[str, object]] = queue.Queue()
        self.poll_busy = False
        self.state: dict = {}
        self._build()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(20, self._tick)

    def close(self) -> None:
        self.owner.set("Releasing PPK2 and turning Target power off...")
        self.root.update_idletasks()
        try:
            self._request("stop")
        except Exception:
            pass
        self.root.destroy()
    def _build(self) -> None:
        shell = ttk.Frame(self.root, padding=14)
        shell.grid(sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        shell.columnconfigure(0, weight=1)
        shell.rowconfigure(4, weight=1)

        ttk.Label(shell, text="PPK2 Ownership", font=("", 14, "bold")).grid(row=0, column=0, sticky="w")
        self.owner_label = ttk.Label(shell, textvariable=self.owner, foreground="#555555")
        self.owner_label.grid(row=1, column=0, sticky="w", pady=(3, 12))

        controls = ttk.LabelFrame(shell, text="Instrument And Source Power", padding=10)
        controls.grid(row=2, column=0, sticky="ew")
        ttk.Label(controls, text="PPK port").grid(row=0, column=0, sticky="w")
        ttk.Entry(controls, textvariable=self.port, width=10).grid(row=0, column=1, padx=(6, 14))
        ttk.Label(controls, text="Source mV").grid(row=0, column=2, sticky="w")
        ttk.Entry(controls, textvariable=self.voltage, width=8).grid(row=0, column=3, padx=(6, 14))
        ttk.Button(controls, text="Acquire PPK2", command=self.acquire).grid(row=0, column=4, padx=(0, 8))
        ttk.Button(controls, text="Release PPK2", command=self.release).grid(row=0, column=5)
        self.power_button = tk.Button(
            controls, command=self.toggle_power, width=29, height=2, relief="flat",
            font=("", 10, "bold"), text="Target POWER UNKNOWN", background="#777777", foreground="white",
        )
        self.power_button.grid(row=1, column=0, columnspan=5, sticky="ew", pady=(10, 0), padx=(0, 8))
        self.reset_power_button = tk.Button(
            controls, command=self.reset_target_power, width=16, height=2, relief="flat",
            font=("", 10, "bold"), text="RESET TARGET", background="#d97706", foreground="white",
        )
        self.reset_power_button.grid(row=1, column=5, sticky="ew", pady=(10, 0))

        capture = ttk.LabelFrame(shell, text="Capture", padding=10)
        capture.grid(row=3, column=0, sticky="ew", pady=(12, 0))
        ttk.Label(capture, text="Duration s").grid(row=0, column=0, sticky="w")
        ttk.Entry(capture, textvariable=self.seconds, width=8).grid(row=0, column=1, padx=(6, 14))
        ttk.Label(capture, text="Label").grid(row=0, column=2, sticky="w")
        ttk.Entry(capture, textvariable=self.label, width=22).grid(row=0, column=3, padx=(6, 14))
        ttk.Button(capture, text="Start Capture", command=self.start_capture).grid(row=0, column=4, padx=(0, 6))
        ttk.Button(capture, text="Stop Capture", command=self.stop_capture).grid(row=0, column=5)
        self.record_indicator = tk.Label(capture, text="NOT RECORDING", width=18, relief="flat", background="#b52a2a", foreground="white", font=("", 9, "bold"))
        self.record_indicator.grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Label(capture, textvariable=self.capture).grid(row=1, column=1, columnspan=5, sticky="w", padx=(6, 0), pady=(8, 0))
        ttk.Label(capture, text="CSV rate").grid(row=2, column=0, sticky="w", pady=(8, 0))
        ttk.Combobox(capture, textvariable=self.csv_rate, width=10, state="readonly",
                     values=("100 kS/s", "10 kS/s", "1 kS/s", "100 S/s")).grid(row=2, column=1, sticky="w", padx=(6, 14), pady=(8, 0))
        ttk.Label(capture, text="Saved trace").grid(row=2, column=2, sticky="w", pady=(8, 0))
        self.trace_box = ttk.Combobox(capture, textvariable=self.trace_choice, width=28, state="readonly", values=("Live",))
        self.trace_box.grid(row=2, column=3, columnspan=3, sticky="w", padx=(6, 0), pady=(8, 0))
        self.trace_box.bind("<<ComboboxSelected>>", self.select_trace)

        chart = ttk.LabelFrame(shell, text="Live Current And Logic", padding=8)
        chart.grid(row=4, column=0, sticky="nsew", pady=(12, 0))
        chart.columnconfigure(1, weight=1)
        chart.rowconfigure(1, weight=1)
        ttk.Label(chart, text="Plot range").grid(row=0, column=0, sticky="w", pady=(0, 8))
        window = ttk.Combobox(chart, textvariable=self.plot_range, width=20, state="readonly",
                              values=("Since power on/reset", "Latest 0.5 s", "Latest 1 s", "Latest 2 s", "Latest 5 s", "Latest 10 s", "Latest 20 s"))
        window.grid(row=0, column=1, sticky="w", padx=(6, 14), pady=(0, 8))
        window.bind("<<ComboboxSelected>>", lambda _: self._draw(self.state.get("plot", [])))
        ttk.Label(chart, text="Logic input").grid(row=0, column=2, sticky="e", pady=(0, 8))
        self.logic_box = ttk.Combobox(
            chart, textvariable=self.logic_channel, width=5, state="readonly", values=LOGIC_CHANNELS,
        )
        self.logic_box.grid(row=0, column=3, sticky="w", padx=(6, 8), pady=(0, 8))
        self.logic_box.bind("<<ComboboxSelected>>", self.select_logic)
        self.logic_indicator = tk.Label(
            chart, text="D7 --", width=10, relief="flat", background="#777777",
            foreground="white", font=("", 9, "bold"),
        )
        self.logic_indicator.grid(row=0, column=4, sticky="w", padx=(0, 8), pady=(0, 8))
        tk.Button(
            chart, text="RESET PLOT DATA", command=self.reset_plot, relief="flat",
            background="#6b7280", foreground="white", font=("", 9, "bold"),
        ).grid(row=0, column=5, sticky="e", pady=(0, 8))
        self.canvas = tk.Canvas(chart, background="#121820", height=310, highlightthickness=0)
        self.canvas.grid(row=1, column=0, columnspan=6, sticky="nsew")
        ttk.Label(chart, textvariable=self.logic_status).grid(row=2, column=0, columnspan=6, sticky="w", pady=(8, 0))
        ttk.Label(chart, textvariable=self.recent_stats).grid(row=3, column=0, columnspan=6, sticky="w")
        ttk.Label(chart, textvariable=self.energy_stats).grid(row=4, column=0, columnspan=6, sticky="w")
        ttk.Label(chart, textvariable=self.stats).grid(row=5, column=0, columnspan=6, sticky="w")
        ttk.Label(chart, textvariable=self.artifact).grid(row=6, column=0, columnspan=6, sticky="w")

    def _request(self, command: str, seconds: float | None = None, label: str | None = None, extra: dict | None = None) -> dict:
        return ppk2_service.service_request(command, seconds, label, "PPK2_logs", extra)

    def _run(self, name: str, action) -> None:
        def worker() -> None:
            try:
                self.results.put((name, action()))
            except Exception as exc:
                self.results.put((name + "_error", str(exc)))
        threading.Thread(target=worker, daemon=True, name="ppk2-console-" + name).start()

    def acquire(self) -> None:
        try:
            voltage = int(self.voltage.get())
        except ValueError:
            self.owner.set("Source voltage must be an integer in mV.")
            return
        self.owner.set("Acquiring PPK2. Target source power remains off until enabled.")
        self._run("acquire", lambda: ppk2_service.start_service(self.port.get().strip(), voltage, background=True))

    def toggle_power(self) -> None:
        if self.state.get("power") == "on":
            self.owner.set("Turning Target source power off...")
            self._run("power_off", lambda: self._request("power_off"))
        else:
            self.owner.set("Turning Target source power on...")
            self._run("power_on", lambda: self._request("power_on"))

    def release(self) -> None:
        self.owner.set("Releasing PPK2 and cutting Target source power...")
        self._run("release", lambda: self._request("stop"))

    def reset_target_power(self) -> None:
        self.owner.set("Power-cycling Target source power...")
        self._run("power_cycle", lambda: self._request("power_cycle", extra={"off_seconds": 0.5}))

    def start_capture(self) -> None:
        try:
            seconds = float(self.seconds.get())
        except ValueError:
            self.capture.set("Capture duration must be numeric.")
            return
        stride = {"100 kS/s": 1, "10 kS/s": 10, "1 kS/s": 100, "100 S/s": 1000}[self.csv_rate.get()]
        self._run("capture_start", lambda: self._request("capture_start", seconds, self.label.get().strip(), {"sample_stride": stride}))

    def stop_capture(self) -> None:
        self._run("capture_stop", lambda: self._request("capture_stop"))

    def reset_plot(self) -> None:
        self.trace_id = "live"
        self.trace_choice.set("Live")
        self._run("plot_reset", lambda: self._request("plot_reset"))

    def _plot_window_seconds(self) -> float | None:
        mode = self.plot_range.get()
        if not mode.startswith("Latest "):
            return None
        try:
            return float(mode.replace("Latest ", "").replace(" s", ""))
        except ValueError:
            return 5.0

    def _poll(self) -> None:
        if self.poll_busy:
            return
        self.poll_busy = True
        request = {
            "trace_id": self.trace_id,
            "plot_max_points": max(400, min(2400, self.canvas.winfo_width() * 2)),
        }
        window_s = self._plot_window_seconds()
        if window_s is not None:
            request["plot_window_s"] = window_s

        def worker() -> None:
            try:
                value = self._request("snapshot", extra=request)
                self.results.put(("poll", value))
            except Exception as exc:
                self.results.put(("poll_error", str(exc)))
        threading.Thread(target=worker, daemon=True, name="ppk2-console-poll").start()

    def select_trace(self, _event=None) -> None:
        self.trace_id = self.trace_lookup.get(self.trace_choice.get(), "live")

    def select_logic(self, _event=None) -> None:
        self._draw(self.state.get("plot", []))
        self._update_logic_status()

    def _tick(self) -> None:
        while True:
            try:
                name, value = self.results.get_nowait()
            except queue.Empty:
                break
            if name == "poll":
                self.poll_busy = False
                self._apply_state(value)
            elif name == "poll_error":
                self.poll_busy = False
                self._apply_released()
            elif name.endswith("_error"):
                self.owner.set("PPK2 operation failed: " + str(value))
            else:
                self.owner.set("PPK2 operation complete. Refreshing ownership state...")
        self._poll()
        self.root.after(POLL_MS, self._tick)

    def _apply_released(self) -> None:
        self.owner.set("PPK2 released: Nordic Power Profiler may use the instrument.")
        self.owner_label.configure(foreground="#555555")
        self.capture.set("No PeepShow PPK2 service is reachable.")
        self._set_record_indicator(False)
        self._set_power_button(None)
        self._set_logic_indicator(None)
        self.logic_status.set(f"{self.logic_channel.get()} logic: --    Edges: --")
        self._draw([])

    def _apply_state(self, value: object) -> None:
        if not isinstance(value, dict):
            self._apply_released()
            return
        self.state = value
        port = self.state.get("port", self.port.get())
        voltage = self.state.get("voltage_mv", self.voltage.get())
        powered = self.state.get("power") == "on"
        if self.state.get("capture_active"):
            self.owner.set(f"PeepShow owns {port}; capture active at {voltage} mV. Nordic Power Profiler cannot use the PPK.")
            self.owner_label.configure(foreground="#a14a00")
        elif powered:
            self.owner.set(f"PeepShow owns {port}; Target source power is ON at {voltage} mV.")
            self.owner_label.configure(foreground="#006d45")
        else:
            self.owner.set(f"PeepShow owns {port}; Target source power is OFF. Release PPK2 before opening Nordic Power Profiler.")
            self.owner_label.configure(foreground="#006d45")
        history = self.state.get("history", [])
        self.trace_lookup = {"Live": "live"}
        names = ["Live"]
        for item in history:
            name = f"{item['label']} ({item['id']})"
            self.trace_lookup[name] = item["id"]
            names.append(name)
        self.trace_box.configure(values=names)
        if self.state.get("selected_trace_id") != self.trace_id:
            self.trace_id = self.state.get("selected_trace_id", "live")
            for name, trace_id in self.trace_lookup.items():
                if trace_id == self.trace_id:
                    self.trace_choice.set(name)
                    break
        self._set_power_button(powered)
        active = self.state.get("capture_active", False)
        self._set_record_indicator(active)
        self.capture.set(
            f"Capture: {self.state.get('capture_label', '--')}  "
            f"{self.state.get('capture_elapsed_s', 0):.1f} / {self.state.get('capture_requested_s', 0):.1f} s"
            if active else "No capture active."
        )
        stats = self.state.get("statistics", {})
        if stats.get("sample_count"):
            self.stats.set(
                f"Session mean: {stats['mean_ua'] / 1000:.3f} mA    "
                f"Min: {stats['min_ua'] / 1000:.3f} mA    "
                f"Max: {stats['max_ua'] / 1000:.3f} mA    "
                f"RMS: {stats['rms_ua'] / 1000:.3f} mA    "
                f"Samples: {stats['sample_count']}"
            )
            self.energy_stats.set(
                f"Charge/Energy since power on or plot reset: "
                f"{stats.get('charge_mah', 0.0):.6f} mAh    "
                f"{stats.get('energy_mwh', 0.0):.6f} mWh at {voltage} mV"
            )
        else:
            self.stats.set("Session mean: --    Min: --    Max: --    RMS: --    Samples: 0")
            self.energy_stats.set("Charge/Energy: --")
        result = self.state.get("last_result") or {}
        if result.get("csv"):
            self.artifact.set(f"Last artifact: {result['csv']}")
        self._draw(self.state.get("plot", []))
        self._update_recent_stats(self.state.get("plot", []), self.state.get("recent_plot_statistics"))
        self._update_logic_status()

    def _format_time_label(self, seconds: float) -> str:
        seconds = max(0.0, seconds)
        if seconds < 10:
            return f"{seconds:.1f}s"
        if seconds < 60:
            return f"{seconds:.0f}s"
        minutes = seconds / 60
        if minutes < 60:
            return f"{minutes:.1f}m"
        return f"{minutes / 60:.1f}h"

    def _update_recent_stats(self, samples: list, precomputed: dict | None = None) -> None:
        if precomputed:
            self.recent_stats.set(
                f"Recent current, last {precomputed.get('window_s', 1.0):g} s: "
                f"{precomputed['mean_ua'] / 1000:.3f} mA    "
                f"Min: {precomputed['min_ua'] / 1000:.3f} mA    "
                f"Max: {precomputed['max_ua'] / 1000:.3f} mA"
            )
            return
        points = [(float(item[0]), float(item[1])) for item in samples if isinstance(item, (list, tuple)) and len(item) >= 2]
        if len(points) < 1:
            self.recent_stats.set("Recent current: --")
            return
        end = points[-1][0]
        recent = [value for point_time, value in points if point_time >= end - 1.0]
        if not recent:
            self.recent_stats.set("Recent current: --")
            return
        mean = sum(recent) / len(recent)
        self.recent_stats.set(
            f"Recent current, last 1 s: {mean / 1000:.3f} mA    "
            f"Min: {min(recent) / 1000:.3f} mA    "
            f"Max: {max(recent) / 1000:.3f} mA"
        )

    def _selected_logic_index(self) -> int:
        try:
            return int(self.logic_channel.get().removeprefix("D"))
        except ValueError:
            return 7

    def _update_logic_status(self) -> None:
        channel = self.logic_channel.get()
        index = self._selected_logic_index()
        latest = self.state.get("digital_latest_bits")
        counts = self.state.get("digital_edge_counts", [])
        if latest is None:
            self._set_logic_indicator(None)
            self.logic_status.set(f"{channel} logic: --    Edges: --")
            return
        high = bool(int(latest) & (1 << index))
        edge_count = int(counts[index]) if isinstance(counts, list) and len(counts) > index else 0
        self._set_logic_indicator(high)
        self.logic_status.set(
            f"{channel} logic: {'HIGH' if high else 'LOW'}    "
            f"Edges since power on or plot reset: {edge_count}    "
            "Orange marks indicate one or more edges inside a 10 ms plot bucket."
        )

    def _set_logic_indicator(self, high: bool | None) -> None:
        channel = self.logic_channel.get()
        if high is None:
            self.logic_indicator.configure(text=f"{channel} --", background="#777777")
        elif high:
            self.logic_indicator.configure(text=f"{channel} HIGH", background="#087f4a")
        else:
            self.logic_indicator.configure(text=f"{channel} LOW", background="#374151")

    def _set_record_indicator(self, recording: bool) -> None:
        if recording:
            self.record_indicator.configure(text="RECORDING", background="#087f4a")
        else:
            self.record_indicator.configure(text="NOT RECORDING", background="#b52a2a")
    def _set_power_button(self, powered: bool | None) -> None:
        if powered is None:
            self.power_button.configure(text="Target POWER UNKNOWN", background="#777777", state="disabled")
            self.reset_power_button.configure(state="disabled")
        elif powered:
            self.reset_power_button.configure(state="normal")
            self.power_button.configure(
                text="Target POWER ON  -  CLICK TO TURN OFF", background="#087f4a", state="normal",
            )
        else:
            self.reset_power_button.configure(state="normal")
            self.power_button.configure(
                text="Target POWER OFF  -  CLICK TO TURN ON", background="#b52a2a", state="normal",
            )

    def _draw(self, samples: list) -> None:
        points = []
        for item in samples:
            if not isinstance(item, (list, tuple)) or len(item) < 2:
                continue
            digital_bits = int(item[2]) if len(item) >= 3 else None
            transition_mask = int(item[3]) if len(item) >= 4 else 0
            points.append((float(item[0]), float(item[1]), digital_bits, transition_mask))

        canvas = self.canvas
        canvas.delete("all")
        width = max(1, canvas.winfo_width())
        height = max(1, canvas.winfo_height())
        mode = self.plot_range.get()
        label = "Current (mA), since power on/reset"
        if mode.startswith("Latest "):
            try:
                window_s = float(mode.replace("Latest ", "").replace(" s", ""))
            except ValueError:
                window_s = 5.0
            if points:
                end = points[-1][0]
                start = max(0.0, end - window_s)
                points = [point for point in points if point[0] >= start]
            label = f"Current (mA), {mode.lower()}"
        elif points:
            start = points[0][0]
            end = points[-1][0]
            window_s = max(end - start, 0.001)
        else:
            start = 0.0
            window_s = 1.0
        canvas.create_text(10, 10, anchor="nw", fill="#a8b4c4", text=label)
        if len(points) < 2:
            canvas.create_text(width // 2, height // 2, fill="#a8b4c4", text="No live samples")
            return
        start = points[0][0]
        end = points[-1][0]
        window_s = max(end - start, 0.001)
        low = min(value for _, value, _, _ in points)
        high = max(value for _, value, _, _ in points)
        span = max(high - low, 1.0)
        left = 58
        right = width - 14
        top = 30
        current_bottom = height - 100
        logic_top = current_bottom + 24
        logic_bottom = height - 46
        plot_width = max(1, right - left)
        plot_height = max(1, current_bottom - top)
        canvas.create_line(left, top, left, current_bottom, fill="#3a4654")
        canvas.create_line(left, current_bottom, right, current_bottom, fill="#3a4654")
        for index in range(5):
            fraction = index / 4
            y = current_bottom - fraction * plot_height
            value_ma = (low + fraction * span) / 1000
            canvas.create_line(left - 4, y, right, y, fill="#25313d")
            canvas.create_text(left - 8, y, anchor="e", fill="#a8b4c4", text=f"{value_ma:.3g}")
        for index in range(5):
            fraction = index / 4
            x = left + fraction * plot_width
            t = start + fraction * window_s
            canvas.create_line(x, current_bottom, x, logic_bottom, fill="#25313d")
            canvas.create_line(x, logic_bottom, x, logic_bottom + 4, fill="#3a4654")
            canvas.create_text(x, logic_bottom + 8, anchor="n", fill="#a8b4c4", text=self._format_time_label(t))
        canvas.create_text(8, top - 2, anchor="nw", fill="#a8b4c4", text="mA")
        canvas.create_text(right, height - 4, anchor="se", fill="#a8b4c4", text="time")

        max_draw_points = max(2, width * 2)
        current_points = points
        if len(current_points) > max_draw_points:
            stride = max(1, len(current_points) // max_draw_points)
            current_points = current_points[::stride] + [current_points[-1]]
        draw = []
        for point_time, value, _, _ in current_points:
            x = left + (point_time - start) * plot_width / window_s
            y = current_bottom - ((value - low) / span) * plot_height
            draw.extend((x, y))
        canvas.create_line(*draw, fill="#42c4a0", width=1)

        channel = self.logic_channel.get()
        bit_mask = 1 << self._selected_logic_index()
        canvas.create_rectangle(left, logic_top, right, logic_bottom, outline="#3a4654")
        canvas.create_text(left - 8, logic_top + 4, anchor="e", fill="#a8b4c4", text="H")
        canvas.create_text(left - 8, logic_bottom - 4, anchor="e", fill="#a8b4c4", text="L")
        canvas.create_text(10, logic_top + 1, anchor="nw", fill="#a8b4c4", text=channel)
        logic_points = [point for point in points if point[2] is not None]
        if not logic_points:
            canvas.create_text(
                (left + right) // 2, (logic_top + logic_bottom) // 2,
                fill="#a8b4c4", text="Restart the PPK2 service to enable logic telemetry",
            )
            return
        high_y = logic_top + 5
        low_y = logic_bottom - 5
        first_time, _, first_bits, _ = logic_points[0]
        last_x = left + (first_time - start) * plot_width / window_s
        last_y = high_y if first_bits & bit_mask else low_y
        logic_draw = [last_x, last_y]
        transition_x = []
        for point_time, _, digital_bits, transition_mask in logic_points[1:]:
            x = left + (point_time - start) * plot_width / window_s
            y = high_y if digital_bits & bit_mask else low_y
            logic_draw.extend((x, last_y, x, y))
            if transition_mask & bit_mask:
                transition_x.append(round(x))
            last_x = x
            last_y = y
        logic_draw.extend((right, last_y))
        canvas.create_line(*logic_draw, fill="#67a8ff", width=2)

        marker_x = sorted(set(transition_x))
        if len(marker_x) > 200:
            stride = math.ceil(len(marker_x) / 200)
            marker_x = marker_x[::stride]
        for x in marker_x:
            canvas.create_line(x, logic_top, x, logic_bottom, fill="#f59e0b", width=1)


def main() -> int:
    root = tk.Tk()
    PpkConsole(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())









