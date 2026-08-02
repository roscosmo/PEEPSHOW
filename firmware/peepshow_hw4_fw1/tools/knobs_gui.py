#!/usr/bin/env python3
"""
Modern Knobs GUI editor (CustomTkinter) for:
  - config/knobs.json
  - config/knobs.schema.json

Features:
  - Tabs grouped by schema metadata `gui_tab` when present, else key prefix fallback
  - Optional in-tab section headers from schema metadata `gui_section`
  - Human-readable labels via schema `gui_label` (with automatic fallback labelization)
  - Numeric knobs with schema minimum+maximum get a slider + entry
  - Automatic timestamped backup + backup history log on changed saves
  - Scrollable per-tab editor
  - Save & Exit: writes config/knobs.json then runs tools/gen_knobs.py
  - Window close (X) or Exit (No Save): quits without saving

Run:
  pip install customtkinter
  python tools/knobs_gui.py
"""

from __future__ import annotations

import json
import hashlib
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

import customtkinter as ctk
import tkinter as tk
from tkinter import messagebox


# -----------------------------
# Repo/path discovery
# -----------------------------

def find_repo_root(start: Path) -> Path:
    """Walk up from 'start' until config/knobs.json is found."""
    cur = start.resolve()
    for _ in range(60):
        if (cur / "config" / "knobs.json").is_file():
            return cur
        if cur.parent == cur:
            break
        cur = cur.parent
    raise FileNotFoundError("Could not find repo root (expected config/knobs.json upward).")


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


# -----------------------------
# Schema helpers
# -----------------------------

@dataclass(frozen=True)
class KnobMeta:
    key: str
    type: str  # "integer", "number", "boolean", "string"
    minimum: Optional[float] = None
    maximum: Optional[float] = None
    multiple_of: Optional[float] = None
    description: str = ""
    unit: str = ""
    order: int = 10_000
    gui_tab: str = ""
    gui_section: str = ""
    gui_label: str = ""
    gui_hint: str = ""
    tick_basis: str = ""
    has_default: bool = False
    default: Any = None
    enum_values: tuple[Any, ...] = ()
    one_of: tuple[tuple[Any, str], ...] = ()
    display_base: str = ""
    display_prefix: str = ""
    display_width: Optional[int] = None
    display_widget: str = ""
    display_bits: tuple[tuple[int, str], ...] = ()


def extract_meta(schema: dict[str, Any], key: str) -> KnobMeta:
    props = schema.get("properties", {}) or {}
    p = props.get(key, {}) or {}
    t = p.get("type")
    one_of_raw = p.get("oneOf")
    if not isinstance(t, str):
        if isinstance(one_of_raw, list) and len(one_of_raw) > 0:
            first = one_of_raw[0]
            if isinstance(first, dict) and ("const" in first):
                const_val = first.get("const")
                if isinstance(const_val, bool):
                    t = "boolean"
                elif isinstance(const_val, int):
                    t = "integer"
                elif isinstance(const_val, float):
                    t = "number"
                elif isinstance(const_val, str):
                    t = "string"
        if not isinstance(t, str):
            t = "integer"

    enum_values: list[Any] = []
    enum_raw = p.get("enum")
    if isinstance(enum_raw, list):
        enum_values = list(enum_raw)

    one_of: list[tuple[Any, str]] = []
    if isinstance(one_of_raw, list):
        for item in one_of_raw:
            if not isinstance(item, dict):
                continue
            if "const" not in item:
                continue
            const_val = item.get("const")
            title = str(item.get("title", const_val))
            one_of.append((const_val, title))

    display_base = ""
    display_prefix = ""
    display_width: Optional[int] = None
    display_widget = ""
    display_bits: list[tuple[int, str]] = []
    display = p.get("display")
    if isinstance(display, dict):
        base = display.get("base")
        if isinstance(base, str):
            display_base = base.strip().lower()
        prefix = display.get("prefix")
        if isinstance(prefix, str):
            display_prefix = prefix
        width = display.get("width")
        if isinstance(width, int) and width > 0:
            display_width = width
        widget = display.get("widget")
        if isinstance(widget, str):
            display_widget = widget.strip().lower()
        bits = display.get("bits")
        if isinstance(bits, list):
            for bit_def in bits:
                if not isinstance(bit_def, dict):
                    continue
                bit_ix = bit_def.get("bit")
                if not isinstance(bit_ix, int):
                    continue
                label = str(bit_def.get("label", f"bit {bit_ix}"))
                display_bits.append((bit_ix, label))
            display_bits.sort(key=lambda x: x[0])

    return KnobMeta(
        key=key,
        type=t,
        minimum=p.get("minimum"),
        maximum=p.get("maximum"),
        multiple_of=p.get("multipleOf"),
        description=(p.get("description", "") or "").strip(),
        unit=(p.get("unit", "") or "").strip(),
        order=int(p.get("order", 10_000)),
        gui_tab=(p.get("gui_tab", "") or "").strip(),
        gui_section=(p.get("gui_section", "") or "").strip(),
        gui_label=(p.get("gui_label", "") or p.get("title", "") or "").strip(),
        gui_hint=(p.get("gui_hint", "") or "").strip(),
        tick_basis=(p.get("tick_basis", "") or "").strip().lower(),
        has_default=("default" in p),
        default=p.get("default"),
        enum_values=tuple(enum_values),
        one_of=tuple(one_of),
        display_base=display_base,
        display_prefix=display_prefix,
        display_width=display_width,
        display_widget=display_widget,
        display_bits=tuple(display_bits),
    )


def validate_value(meta: KnobMeta, value: Any) -> Optional[str]:
    """Return error string if invalid, else None."""
    t = meta.type

    if t == "boolean":
        if not isinstance(value, bool):
            return f"{meta.key}: expected boolean"
        if meta.enum_values and (value not in meta.enum_values):
            return f"{meta.key}: {value!r} not in enum"
        if meta.one_of:
            allowed = [c for (c, _lbl) in meta.one_of]
            if value not in allowed:
                return f"{meta.key}: {value!r} not in oneOf const values"
        return None

    if t == "integer":
        if not isinstance(value, int) or isinstance(value, bool):
            return f"{meta.key}: expected integer"
        if meta.enum_values and (value not in meta.enum_values):
            return f"{meta.key}: {value} not in enum"
        if meta.one_of:
            allowed = [c for (c, _lbl) in meta.one_of]
            if value not in allowed:
                return f"{meta.key}: {value} not in oneOf const values"
        if meta.minimum is not None and value < int(meta.minimum):
            return f"{meta.key}: {value} < minimum {int(meta.minimum)}"
        if meta.maximum is not None and value > int(meta.maximum):
            return f"{meta.key}: {value} > maximum {int(meta.maximum)}"
        if meta.multiple_of is not None:
            m = int(meta.multiple_of) if float(meta.multiple_of).is_integer() else meta.multiple_of
            if int(m) != 0 and (value % int(m) != 0):
                return f"{meta.key}: {value} is not multipleOf {int(m)}"
        return None

    if t == "number":
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            return f"{meta.key}: expected number"
        fv = float(value)
        if meta.enum_values and (value not in meta.enum_values):
            return f"{meta.key}: {value} not in enum"
        if meta.one_of:
            allowed = [c for (c, _lbl) in meta.one_of]
            if value not in allowed:
                return f"{meta.key}: {value} not in oneOf const values"
        if meta.minimum is not None and fv < float(meta.minimum):
            return f"{meta.key}: {fv} < minimum {meta.minimum}"
        if meta.maximum is not None and fv > float(meta.maximum):
            return f"{meta.key}: {fv} > maximum {meta.maximum}"
        if meta.multiple_of is not None:
            m = float(meta.multiple_of)
            if m != 0 and abs((fv / m) - round(fv / m)) > 1e-9:
                return f"{meta.key}: {fv} is not multipleOf {meta.multiple_of}"
        return None

    if t == "string":
        if not isinstance(value, str):
            return f"{meta.key}: expected string"
        if meta.enum_values and (value not in meta.enum_values):
            return f"{meta.key}: {value!r} not in enum"
        if meta.one_of:
            allowed = [c for (c, _lbl) in meta.one_of]
            if value not in allowed:
                return f"{meta.key}: {value!r} not in oneOf const values"
        return None

    return f"{meta.key}: unsupported schema type '{t}'"


# -----------------------------
# App
# -----------------------------

class KnobsApp(ctk.CTk):
    def __init__(self, repo_root: Path):
        super().__init__()

        self.repo_root = repo_root
        self.config_dir = repo_root / "config"
        self.knobs_path = self.config_dir / "knobs.json"
        self.schema_path = self.config_dir / "knobs.schema.json"
        self.gen_script = repo_root / "tools" / "gen_knobs.py"
        self.autogen_header_path = repo_root / "Core" / "Inc" / "knobs_autogen.h"
        self.backup_dir = self.config_dir / "knob_backups"
        self.backup_log_path = self.backup_dir / "backup_history.log"

        self.knobs_raw: dict[str, Any] = load_json(self.knobs_path)
        self.schema_raw: dict[str, Any] = load_json(self.schema_path)

        if not isinstance(self.knobs_raw, dict):
            raise TypeError("knobs.json top-level must be an object")
        if not isinstance(self.schema_raw, dict):
            raise TypeError("knobs.schema.json top-level must be an object")

        self.required = list(self.schema_raw.get("required", []) or [])
        self.additional_properties = bool(self.schema_raw.get("additionalProperties", True))

        # Build meta map from schema + existing file keys.
        schema_props = self.schema_raw.get("properties", {}) or {}
        all_keys: list[str] = []
        for k in schema_props.keys():
            if k not in all_keys:
                all_keys.append(k)
        for k in self.knobs_raw.keys():
            if k not in all_keys:
                all_keys.append(k)

        self.meta: dict[str, KnobMeta] = {k: extract_meta(self.schema_raw, k) for k in all_keys}

        # Working knob set:
        # - preserve file values
        # - inject schema defaults for missing keys
        self.knobs_data: dict[str, Any] = {}
        self.implicit_default_keys: set[str] = set()
        for k in all_keys:
            if k in self.knobs_raw:
                self.knobs_data[k] = self.knobs_raw[k]
            else:
                m = self.meta[k]
                if m.has_default:
                    self.knobs_data[k] = m.default
                    self.implicit_default_keys.add(k)

        self.all_keys = list(self.knobs_data.keys())

        # UI state
        # For each key, store:
        #   - widget type
        #   - associated variable / getter function
        self._var_map: dict[str, Any] = {}  # key -> (meta, kind, widget_state)

        # Window
        self.title("Knobs Editor")
        self.geometry("980x720")
        self.minsize(860, 600)
        self.protocol("WM_DELETE_WINDOW", self.on_close_no_save)

        # Layout
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        # Header
        header = ctk.CTkFrame(self, corner_radius=12)
        header.grid(row=0, column=0, sticky="ew", padx=12, pady=(12, 8))
        header.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            header,
            text=f"Repo: {self.repo_root}\nEditing: {self.knobs_path.relative_to(self.repo_root)}",
            justify="left",
            font=ctk.CTkFont(size=13, weight="bold"),
        ).grid(row=0, column=0, sticky="w", padx=12, pady=12)

        # Tabs
        self.tabview = ctk.CTkTabview(self, corner_radius=12)
        self.tabview.grid(row=1, column=0, sticky="nsew", padx=12, pady=(0, 8))

        self._build_tabs()

        # Footer buttons
        footer = ctk.CTkFrame(self, corner_radius=12)
        footer.grid(row=2, column=0, sticky="ew", padx=12, pady=(0, 12))
        footer.grid_columnconfigure(0, weight=1)

        left = ctk.CTkFrame(footer, fg_color="transparent")
        left.grid(row=0, column=0, sticky="w", padx=12, pady=10)
        left.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(left, text="Backup note:", anchor="w").grid(row=0, column=0, sticky="w", padx=(0, 8))
        self.backup_note_entry = ctk.CTkEntry(left, width=360, placeholder_text="Optional note saved to backup log")
        self.backup_note_entry.grid(row=0, column=1, sticky="w")

        right = ctk.CTkFrame(footer, fg_color="transparent")
        right.grid(row=0, column=1, sticky="e", padx=12, pady=10)

        ctk.CTkButton(right, text="Exit (No Save)", command=self.on_close_no_save).pack(side="right", padx=(0, 10))
        ctk.CTkButton(right, text="Save & Exit", command=self.on_save_and_exit).pack(side="right")

    def _tab_name(self, key: str, meta: Optional[KnobMeta] = None) -> str:
        m = meta or self.meta.get(key) or extract_meta(self.schema_raw, key)
        if m.gui_tab:
            return m.gui_tab.strip().lower()
        if "_" in key:
            return key.split("_", 1)[0].strip().lower() or "misc"
        return "misc"

    def _tab_sort_key(self, tab_name: str) -> tuple[int, str]:
        preferred = [
            "audio",
            "debug",
            "input",
            "joystick",
            "lis2",
            "power",
            "rtos",
            "sensor",
            "storage",
            "ui",
        ]
        try:
            idx = preferred.index(tab_name)
        except ValueError:
            idx = len(preferred)
        return (idx, tab_name)

    def _sorted_keys_for_tab(self, keys: list[str]) -> list[str]:
        def semantic_rank(k: str) -> int:
            key = k.lower()

            # Human-first repeat ordering:
            # delay -> base interval -> stage1 trigger -> stage1 interval -> stage2 trigger -> stage2 interval
            if key.endswith("_repeat_delay_ticks"):
                return 0
            if key.endswith("_repeat_period_ticks"):
                return 1
            if key.endswith("_repeat_accel_stage1_after_ticks"):
                return 2
            if key.endswith("_repeat_period_stage1_ticks"):
                return 3
            if key.endswith("_repeat_accel_stage2_after_ticks"):
                return 4
            if key.endswith("_repeat_period_stage2_ticks"):
                return 5

            return 100

        def sort_key(k: str) -> tuple[int, str]:
            m = self.meta.get(k) or extract_meta(self.schema_raw, k)
            return (semantic_rank(k), m.order, k)
        return sorted(keys, key=sort_key)

    def _build_tabs(self) -> None:
        # Group keys by explicit schema gui_tab first, then prefix fallback.
        tabs: dict[str, list[str]] = {}
        for k in self.all_keys:
            m = self.meta.get(k) or extract_meta(self.schema_raw, k)
            tabs.setdefault(self._tab_name(k, m), []).append(k)

        for tab_name in sorted(tabs.keys(), key=self._tab_sort_key):
            tab = self.tabview.add(tab_name)

            # Scrollable area
            scroll = ctk.CTkScrollableFrame(tab, corner_radius=12)
            scroll.pack(fill="both", expand=True, padx=12, pady=12)
            scroll.grid_columnconfigure(1, weight=1)

            keys = self._sorted_keys_for_tab(tabs[tab_name])
            sections: dict[str, list[str]] = {}
            for k in keys:
                m = self.meta.get(k) or extract_meta(self.schema_raw, k)
                section = m.gui_section.strip()
                sections.setdefault(section, []).append(k)

            section_names = sorted([s for s in sections.keys() if s != ""])
            if "" in sections:
                section_names = [""] + section_names

            row = 0
            for section_name in section_names:
                if section_name:
                    ctk.CTkLabel(
                        scroll,
                        text=section_name,
                        anchor="w",
                        font=ctk.CTkFont(size=14, weight="bold"),
                    ).grid(row=row, column=0, columnspan=2, sticky="w", padx=(6, 6), pady=(8, 6))
                    row += 1
                for key in self._sorted_keys_for_tab(sections[section_name]):
                    self._add_knob_row(scroll, row, key)
                    row += 1

    def _tick_rate_hz_for_meta(self, meta: KnobMeta) -> tuple[Optional[float], str]:
        if meta.tick_basis == "hal_ms":
            return 1000.0, "HAL_GetTick"
        if meta.tick_basis == "threadx":
            hz = self.knobs_data.get("rtos_tick_hz")
            if isinstance(hz, int) and hz > 0:
                return float(hz), f"ThreadX nominal ({hz} Hz)"
            return None, "ThreadX"
        return None, ""

    def _ticks_ms_context(self, meta: KnobMeta) -> str:
        hz, basis = self._tick_rate_hz_for_meta(meta)
        if hz is None:
            return ""
        ms_per_tick = 1000.0 / hz
        return f"1 tick = {ms_per_tick:.6g} ms ({basis})"

    @staticmethod
    def _format_duration_ms(ms: float) -> str:
        if ms >= 1000.0:
            return f"{ms / 1000.0:.3g} s"
        return f"{ms:.6g} ms"

    @staticmethod
    def _trim_float_text(v: float, decimals: int) -> str:
        s = f"{v:.{decimals}f}"
        if "." in s:
            s = s.rstrip("0").rstrip(".")
        return s

    def _ui_scale_for_meta(self, meta: KnobMeta) -> float:
        key_l = meta.key.lower()
        unit_l = (meta.unit or "").strip().lower()
        if unit_l == "permille":
            return 1000.0
        if (unit_l == "0.1 mt") or key_l.endswith("_x10"):
            return 10.0
        return 1.0

    def _is_ui_scaled(self, meta: KnobMeta) -> bool:
        return abs(self._ui_scale_for_meta(meta) - 1.0) > 1e-9

    def _raw_to_ui(self, meta: KnobMeta, raw_value: float) -> float:
        return float(raw_value) / self._ui_scale_for_meta(meta)

    def _ui_to_raw(self, meta: KnobMeta, ui_value: float) -> float:
        return float(ui_value) * self._ui_scale_for_meta(meta)

    def _tick_hint_text(self, meta: KnobMeta, value_ticks: float) -> str:
        hz, basis = self._tick_rate_hz_for_meta(meta)
        if hz is None:
            return f"Tick value: {value_ticks:.6g}"

        ms = value_ticks * (1000.0 / hz)
        disp_ms = self._format_duration_ms(ms)
        disp_ticks = str(int(round(value_ticks))) if meta.type == "integer" else f"{value_ticks:.6g}"
        key = meta.key.lower()

        if ("repeat_period" in key) and ("ticks" in key):
            if value_ticks <= 0.0:
                return "Repeat disabled"
            reps_per_s = hz / value_ticks
            return f"{disp_ms} per step (~{reps_per_s:.3g} repeats/s). Lower is faster."

        if ("poll_period" in key) and ("ticks" in key):
            if value_ticks <= 0.0:
                return "Polling disabled"
            polls_per_s = hz / value_ticks
            return f"{disp_ms} between polls (~{polls_per_s:.3g} polls/s)."

        if ("repeat_delay" in key) and ("ticks" in key):
            if value_ticks <= 0.0:
                return "No initial repeat delay"
            return f"First repeat after {disp_ms} hold."

        if ("accel" in key) and ("after_ticks" in key):
            if value_ticks <= 0.0:
                return "Speedup applies immediately"
            return f"Speedup stage starts after {disp_ms} hold."

        if value_ticks > 0.0:
            events_per_s = hz / value_ticks
            return f"{disp_ms} interval (~{events_per_s:.3g}/s)."
        return "Disabled (0)"

    def _numeric_hint_text(self, meta: KnobMeta, value: float) -> str:
        unit = (meta.unit or "").strip().lower()
        key = meta.key.lower()

        if unit == "ticks":
            return self._tick_hint_text(meta, value)

        if unit == "permille":
            factor = float(value) / 1000.0
            return f"{float(value):.6g} permille = {factor:.6g}x"

        if (unit == "0.1 mt") or key.endswith("_mt_x10"):
            mt = float(value) / 10.0
            return f"{float(value):.6g} (x10) = {mt:.6g} mT"

        if unit == "ms":
            ms = float(value)
            disp_ms = self._format_duration_ms(ms)
            if ms <= 0.0:
                return "Disabled (0 ms)"
            if ("poll_period" in key) or ("repeat_period" in key):
                return f"{disp_ms} interval (~{1000.0 / ms:.3g}/s)"
            if ("delay" in key) or ("after" in key):
                return f"Delay: {disp_ms}"
            return f"{disp_ms}"

        if unit == "hz":
            hz = float(value)
            if hz <= 0.0:
                return "Disabled (0 Hz)"
            return f"{hz:.6g} Hz (~{1000.0 / hz:.3g} ms period)"

        return ""

    def _numeric_hint_context_text(self, meta: KnobMeta) -> str:
        unit = (meta.unit or "").strip().lower()
        key = meta.key.lower()
        if unit == "ticks":
            ticks_ctx = self._ticks_ms_context(meta)
            if ticks_ctx:
                return ticks_ctx
            return "Ticks"
        if unit == "permille":
            return "x = value / 1000"
        if (unit == "0.1 mt") or key.endswith("_mt_x10"):
            return "mT = value / 10"
        if unit == "ms":
            return "Milliseconds"
        if unit == "hz":
            return "Frequency"
        return ""

    def _display_label(self, meta: KnobMeta) -> str:
        if meta.gui_label:
            return meta.gui_label

        key = meta.key
        key_l = key.lower()

        # High-impact RTOS label overrides.
        if key_l.startswith("rtos_"):
            thread_name_map = {
                "power": "Power",
                "display": "Display",
                "storage": "Storage",
                "input": "Input",
                "game": "Game",
                "audio": "Audio",
                "sensor": "Sensor",
            }
            for tkey, tname in thread_name_map.items():
                if key_l == f"rtos_{tkey}_thread_priority":
                    return f"{tname} Thread Priority"
                if key_l == f"rtos_{tkey}_thread_preemption_threshold":
                    return f"{tname} Preemption Threshold"
                if key_l == f"rtos_{tkey}_thread_time_slice":
                    return f"{tname} Time Slice"
                if key_l == f"rtos_{tkey}_thread_stack_bytes":
                    return f"{tname} Stack Size"
                if key_l == f"rtos_{tkey}_wait_ticks":
                    return f"{tname} Idle Wait"

            queue_name_map = {
                "qdisplay_cmd": "Display Command Queue Depth",
                "qstorage_req": "Storage Request Queue Depth",
                "qinput_cmd": "Input Command Queue Depth",
                "qinput_raw": "Input Edge Queue Depth",
                "qui_events": "UI Event Queue Depth",
                "qgame_events": "Game Event Queue Depth",
                "qaudio_cmd": "Audio Command Queue Depth",
                "qsensor_req": "Sensor Request Queue Depth",
                "qsys_events": "System Event Queue Depth",
            }
            for qk, qlabel in queue_name_map.items():
                if key_l == f"rtos_{qk}_depth":
                    return qlabel

        if (meta.gui_tab or "").strip().lower() == "joystick":
            if key.startswith("sensor_joy_"):
                key = key[len("sensor_joy_"):]
            elif key.startswith("input_joy_"):
                key = key[len("input_joy_"):]
        elif "_" in key:
            key = key.split("_", 1)[1]

        token_map = {
            "rtos": "RTOS",
            "ui": "UI",
            "fps": "FPS",
            "hz": "Hz",
            "ms": "ms",
            "mt": "mT",
            "x10": "",
            "lpbam": "LPBAM",
            "joy": "Joystick",
            "cal": "Calibration",
            "accel": "Acceleration",
            "ticks": "",
            "permille": "",
            "stage1": "Stage 1",
            "stage2": "Stage 2",
        }

        words: list[str] = []
        for tok in key.split("_"):
            if not tok:
                continue
            mapped = token_map.get(tok.lower(), tok.capitalize())
            if mapped:
                words.append(mapped)
        return " ".join(words) if words else meta.key

    def _help_text(self, meta: KnobMeta) -> str:
        def clean_technical(s: str) -> str:
            out = s
            replacements = {
                "thPower": "power thread",
                "thDisplay": "display thread",
                "thStorage": "storage thread",
                "thInput": "input thread",
                "thGame": "game thread",
                "thAudio": "audio thread",
                "thSensor": "sensor thread",
                "qSysEvents": "system event queue",
                "qDisplayCmd": "display command queue",
                "qStorageReq": "storage request queue",
                "qInputCmd": "input command queue",
                "qInputRaw": "input raw-event queue",
                "qUIEvents": "UI event queue",
                "qGameEvents": "game event queue",
                "qAudioCmd": "audio command queue",
                "qSensorReq": "sensor request queue",
            }
            for old, new in replacements.items():
                out = out.replace(old, new)
            return out

        def human_hint_for_key(m: KnobMeta) -> str:
            if m.gui_hint:
                return m.gui_hint

            k = m.key.lower()
            unit = (m.unit or "").strip().lower()
            label = self._display_label(m).strip()
            label_l = label.lower()

            # UI / top-level behavior
            if k == "ui_fps":
                return "UI refresh rate outside REALTIME mode."
            if k == "ui_static_entry_point":
                return "Which page opens when entering STATIC mode."

            # Audio
            if k == "audio_dma_frames":
                return "Audio DMA buffer length. Larger = more latency, smaller = lower latency."
            if k == "audio_sample_rate":
                return "Audio sample rate."
            if k == "audio_test_tone_hz":
                return "Frequency of the built-in audio test tone."
            if k == "audio_test_tone_amplitude":
                return "Volume level of the built-in audio test tone."

            # General power toggles
            if k == "use_lpbam":
                return "Enable or disable LPBAM low-power sequencing."
            if k == "turbo_clock_mhz":
                return "Top performance clock target."

            # Performance governor
            if k == "power_perf_hint_stride":
                return "How often performance updates are sent. 1 = every frame, 2 = every 2nd frame."
            if k == "power_perf_frame_budget_ticks":
                return "Frame time budget target used for power scaling."
            if k == "power_perf_miss_margin_ticks":
                return "Extra time above budget before a frame is counted as a miss."
            if k == "power_perf_headroom_margin_ticks":
                return "Time below budget required to count as healthy headroom."
            if k == "power_perf_up_streak_frames":
                return "How many bad frames in a row before raising performance."
            if k == "power_perf_down_streak_frames":
                return "How many good frames in a row before lowering performance."
            if k == "power_perf_min_dwell_ticks":
                return "Minimum wait time before allowing another performance-profile switch."

            # RTOS thread knobs
            if k.endswith("_thread_priority"):
                return "Thread scheduling priority. Lower number means higher priority."
            if k.endswith("_thread_preemption_threshold"):
                return "Preemption threshold for this thread."
            if k.endswith("_thread_time_slice"):
                return "Time slice for round-robin scheduling among equal-priority threads."
            if k.endswith("_wait_ticks"):
                return "How long this thread waits when idle before it wakes to check work again."
            if k.endswith("_thread_stack_bytes"):
                return "Thread stack size."
            if k.startswith("rtos_q") and k.endswith("_depth"):
                return "Queue capacity (how many messages can wait)."
            if k == "rtos_tick_hz":
                return "ThreadX scheduler tick rate."
            if k == "rtos_power_quiesce_timeout_ticks":
                return "Maximum wait time for subsystem quiesce acknowledgements."

            # Input / joystick behavior
            if k == "input_debounce_ticks":
                return "Ignore rapid edge changes within this debounce window."
            if ("poll_period" in k) and (m.unit.lower() in ("ms", "ticks")):
                return "How often this value is sampled. Lower period = more frequent updates."
            if ("repeat_period" in k) and (m.unit.lower() in ("ms", "ticks")):
                return "Repeat interval. Lower interval = faster repeats."
            if ("repeat_delay" in k) and (m.unit.lower() in ("ms", "ticks")):
                return "Delay before repeat starts after holding input."
            if ("accel" in k) and ("after" in k):
                return "Hold time before this speedup stage begins."
            if k.endswith("_enable_mask"):
                return "Select which input sources are included."
            if k == "input_long_press_ticks":
                return "Hold duration required before long-press triggers."
            if k == "input_boot_long_only":
                return "BOOT button only emits long-press actions when enabled."
            if k == "input_realtime_activity_min_ticks":
                return "Minimum spacing between activity pulses sent to power logic in REALTIME."

            # Sensor framework
            if k.startswith("sensor_recovery_"):
                return "Sensor recovery behavior after communication/bring-up failures."
            if k.startswith("sensor_fault_retry_"):
                return "Delay before retrying a sensor that entered FAULT state."
            if k == "sensor_bus_recovery_scl_pulses":
                return "SCL pulses used during I2C bus recovery."

            # PMIC
            if k.startswith("sensor_pmic_guard_"):
                return "Low-voltage guard behavior."
            if k.startswith("sensor_pmic_cutoff_"):
                return "Battery cutoff threshold behavior."
            if k.startswith("sensor_pmic_warn_"):
                return "Battery warning threshold behavior."
            if k.startswith("sensor_pmic_crit_"):
                return "Battery critical threshold behavior."

            # LIS
            if k.startswith("sensor_lis_"):
                if ("odr" in k) or ("bw" in k) or (k.endswith("_fs")):
                    return "LIS sampling profile option."
                if "stream_poll" in k:
                    return "How often live LIS samples are read while stream mode is active."
                if "step_status_poll" in k:
                    return "How often LIS step/embedded status is refreshed."
                if "step_" in k:
                    return "LIS step-counter feature option."

            # Joystick sensor
            if k.startswith("sensor_joy_cal_"):
                return "Joystick calibration timing."
            if k == "sensor_joy_neutral_deadzone_scale_permille":
                return "How much idle stick wobble to ignore after calibration. 1000 = 1.0x, 2500 = 2.5x. Raise this if the stick drifts at rest."
            if k == "sensor_joy_neutral_deadzone_min_mt_x10":
                return "Lowest center ignore zone allowed. If calibration computes smaller, this floor is used."
            if k == "sensor_joy_neutral_deadzone_max_mt_x10":
                return "Largest center ignore zone allowed. Raise this if drift remains. If set too high, tiny intentional movement near center will be ignored."
            if k.startswith("sensor_joy_neutral_deadzone_"):
                return "Joystick neutral deadzone tuning."
            if k.startswith("sensor_joy_"):
                return "Joystick runtime filtering/input mapping."

            # Storage
            if k.startswith("storage_"):
                if "addr" in k:
                    return "Flash start address for this region (advanced)."
                if "size" in k or "len" in k:
                    return "Size for this storage region or test operation."
                if "cache" in k:
                    return "FileX cache size."
                if "cluster" in k:
                    return "FAT cluster size."
                if "dir_entries" in k:
                    return "FAT root directory entry capacity."

            # Fallbacks: always provide a human hint.
            if len(m.one_of) > 0 or len(m.enum_values) > 0:
                return f"Choose {label_l} from the available options."
            if m.display_widget == "bitmask":
                return f"Select which bits are enabled for {label_l}."
            if m.type == "boolean":
                return f"Turn {label_l} on or off."
            if unit == "ms":
                return f"Set {label_l} in milliseconds."
            if unit == "ticks":
                return f"Set {label_l} in scheduler ticks."
            if unit == "hz":
                return f"Set {label_l} as a frequency."
            if unit == "bytes":
                return f"Set {label_l} in bytes."
            if unit == "%":
                return f"Set {label_l} as a percentage."
            if unit == "mhz":
                return f"Set {label_l} in MHz."
            if m.type in ("integer", "number"):
                return f"Set numeric value for {label_l}."
            return f"Configure {label_l}."

        lines: list[str] = []
        key_l = meta.key.lower()

        human = human_hint_for_key(meta).strip()
        if human:
            lines.append(human)

        if key_l in {
            "sensor_joy_neutral_deadzone_scale_permille",
            "sensor_joy_neutral_deadzone_min_mt_x10",
            "sensor_joy_neutral_deadzone_max_mt_x10",
        }:
            return "  ".join(lines)

        return "  ".join(lines)

    def _can_use_slider(self, meta: KnobMeta) -> bool:
        if meta.type not in ("integer", "number"):
            return False
        if meta.minimum is None or meta.maximum is None:
            return False
        return float(meta.maximum) > float(meta.minimum)

    def _slider_steps(self, meta: KnobMeta) -> int:
        raw_lo = float(meta.minimum) if meta.minimum is not None else 0.0
        raw_hi = float(meta.maximum) if meta.maximum is not None else raw_lo
        lo = self._raw_to_ui(meta, raw_lo)
        hi = self._raw_to_ui(meta, raw_hi)
        span = hi - lo
        if span <= 0.0:
            return 1

        if meta.multiple_of is not None and float(meta.multiple_of) > 0.0:
            step_ui = float(meta.multiple_of) / self._ui_scale_for_meta(meta)
            if step_ui <= 0.0:
                step_ui = float(meta.multiple_of)
            est = int(round(span / step_ui))
            return max(1, min(2000, est))

        if meta.type == "integer":
            if self._is_ui_scaled(meta):
                est = int(round(raw_hi - raw_lo))
                return max(1, min(2000, est))
            est = int(round(span))
            return max(1, min(2000, est))

        return 1000

    def _snap_numeric_value(self, meta: KnobMeta, value: float) -> float:
        raw_lo = float(meta.minimum) if meta.minimum is not None else self._ui_to_raw(meta, value)
        raw_hi = float(meta.maximum) if meta.maximum is not None else self._ui_to_raw(meta, value)
        lo = self._raw_to_ui(meta, raw_lo)
        hi = self._raw_to_ui(meta, raw_hi)
        v_ui = max(lo, min(hi, value))
        v_raw = self._ui_to_raw(meta, v_ui)

        if meta.multiple_of is not None and float(meta.multiple_of) > 0.0:
            step = float(meta.multiple_of)
            base = raw_lo
            v_raw = base + round((v_raw - base) / step) * step
            v_raw = max(raw_lo, min(raw_hi, v_raw))

        if meta.type == "integer":
            v_raw = float(int(round(v_raw)))
            v_raw = max(raw_lo, min(raw_hi, v_raw))

        return self._raw_to_ui(meta, v_raw)

    def _format_numeric_for_entry(self, meta: KnobMeta, value: float) -> str:
        if self._is_ui_scaled(meta):
            v_ui = self._raw_to_ui(meta, value)
            scale = self._ui_scale_for_meta(meta)
            if abs(scale - 1000.0) < 1e-9:
                return self._trim_float_text(v_ui, 3)
            if abs(scale - 10.0) < 1e-9:
                return self._trim_float_text(v_ui, 1)
            return f"{v_ui:.6g}"
        if meta.type == "integer":
            ival = int(round(value))
            if meta.display_base == "hex":
                prefix = meta.display_prefix if meta.display_prefix else "0x"
                width = meta.display_width if meta.display_width is not None else 0
                if width > 0:
                    return f"{prefix}{ival:0{width}X}"
                return f"{prefix}{ival:X}"
            return str(ival)
        return f"{float(value):.6g}"

    def _parse_integer_text(self, meta: KnobMeta, raw: str) -> int:
        s = raw.strip()
        if not s:
            raise ValueError("empty integer")
        if self._is_ui_scaled(meta):
            s_l = s.lower()
            if s_l.endswith("x"):
                s_l = s_l[:-1].strip()
            ui_v = float(s_l)
            return int(round(self._ui_to_raw(meta, ui_v)))
        if s.lower().startswith(("+0x", "-0x", "0x")):
            return int(s, 16)
        try:
            return int(s, 10)
        except Exception:
            if meta.display_base == "hex":
                return int(s, 16)
            raise

    def _add_knob_row(self, parent: ctk.CTkScrollableFrame, row: int, key: str) -> None:
        meta = self.meta.get(key) or extract_meta(self.schema_raw, key)
        self.meta[key] = meta

        # key label block: friendly label + exact JSON key
        label_block = ctk.CTkFrame(parent, fg_color="transparent")
        label_block.grid(row=row, column=0, sticky="nw", padx=(6, 14), pady=(10, 2))
        label_block.grid_columnconfigure(0, weight=1)

        key_lbl = ctk.CTkLabel(
            label_block,
            text=self._display_label(meta),
            anchor="w",
            font=ctk.CTkFont(size=13, weight="bold"),
        )
        key_lbl.grid(row=0, column=0, sticky="w")

        json_key_lbl = ctk.CTkLabel(
            label_block,
            text=meta.key,
            anchor="w",
            text_color="#8a94a6",
            font=ctk.CTkFont(size=11, family="Consolas"),
        )
        json_key_lbl.grid(row=1, column=0, sticky="w", pady=(2, 0))
        if key in self.implicit_default_keys:
            ctk.CTkLabel(
                label_block,
                text="schema default",
                anchor="w",
                text_color="#d2a85e",
                font=ctk.CTkFont(size=10),
            ).grid(row=2, column=0, sticky="w", pady=(1, 0))

        # editor container
        editor = ctk.CTkFrame(parent, corner_radius=12)
        editor.grid(row=row, column=1, sticky="ew", padx=(0, 6), pady=(8, 6))
        editor.grid_columnconfigure(0, weight=1)

        val = self.knobs_data.get(key)

        default_target = meta.default if meta.has_default else val
        default_button_text = "Default" if meta.has_default else "Reset"

        # Render priority:
        # 1) bitmask widget
        # 2) oneOf dropdown
        # 3) enum dropdown
        # 4) numeric entry (+ slider)
        # 5) boolean checkbox
        # 6) string entry
        if (meta.display_widget == "bitmask") and (meta.type == "integer") and (len(meta.display_bits) > 0):
            try:
                init_int = int(val)
            except Exception:
                init_int = 0
            bits_frame = ctk.CTkFrame(editor, fg_color="transparent")
            bits_frame.grid(row=0, column=0, sticky="w", padx=12, pady=(8, 4))

            value_lbl = ctk.CTkLabel(editor, text="", anchor="w", text_color="#8fb7ff", font=ctk.CTkFont(size=12))
            value_lbl.grid(row=1, column=0, sticky="w", padx=12, pady=(0, 6))

            bit_vars: list[tuple[int, tk.BooleanVar]] = []

            def refresh_bitmask_label() -> None:
                v = 0
                for bit_ix, bvar in bit_vars:
                    if bvar.get():
                        v |= (1 << bit_ix)
                if meta.display_base == "hex":
                    prefix = meta.display_prefix if meta.display_prefix else "0x"
                    width = meta.display_width if meta.display_width is not None else 0
                    if width > 0:
                        value_lbl.configure(text=f"value: {prefix}{v:0{width}X} ({v})")
                    else:
                        value_lbl.configure(text=f"value: {prefix}{v:X} ({v})")
                else:
                    value_lbl.configure(text=f"value: {v}")

            for ix, (bit_ix, bit_label) in enumerate(meta.display_bits):
                bvar = tk.BooleanVar(value=((init_int & (1 << bit_ix)) != 0))
                bit_vars.append((bit_ix, bvar))
                cb = ctk.CTkCheckBox(bits_frame, text=f"{bit_label} (bit {bit_ix})", variable=bvar, command=refresh_bitmask_label)
                cb.grid(row=ix // 2, column=ix % 2, sticky="w", padx=(0, 14), pady=(2, 2))

            def reset_bitmask() -> None:
                try:
                    target = int(default_target)
                except Exception:
                    target = 0
                for bit_ix, bvar in bit_vars:
                    bvar.set((target & (1 << bit_ix)) != 0)
                refresh_bitmask_label()

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_bitmask).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )

            refresh_bitmask_label()
            self._var_map[key] = (meta, "bitmask", {"bit_vars": bit_vars})
            help_row = 2

        elif len(meta.one_of) > 0:
            label_to_const: dict[str, Any] = {}
            labels: list[str] = []
            for const_val, title in meta.one_of:
                label_to_const[title] = const_val
                labels.append(title)

            initial_label = labels[0]
            for label, const_val in label_to_const.items():
                if const_val == val:
                    initial_label = label
                    break

            var = tk.StringVar(value=initial_label)
            opt = ctk.CTkOptionMenu(editor, values=labels, variable=var)
            opt.grid(row=0, column=0, sticky="ew", padx=12, pady=(10, 6))

            def reset_oneof() -> None:
                target_label = labels[0]
                for label, const_val in label_to_const.items():
                    if const_val == default_target:
                        target_label = label
                        break
                var.set(target_label)

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_oneof).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )

            self._var_map[key] = (meta, "oneof", {"var": var, "label_to_const": label_to_const})
            help_row = 1

        elif len(meta.enum_values) > 0:
            value_by_label: dict[str, Any] = {}
            labels: list[str] = []
            for enum_val in meta.enum_values:
                label = str(enum_val)
                labels.append(label)
                value_by_label[label] = enum_val

            initial_label = labels[0]
            for label, enum_val in value_by_label.items():
                if enum_val == val:
                    initial_label = label
                    break

            var = tk.StringVar(value=initial_label)
            opt = ctk.CTkOptionMenu(editor, values=labels, variable=var)
            opt.grid(row=0, column=0, sticky="ew", padx=12, pady=(10, 6))

            def reset_enum() -> None:
                target_label = labels[0]
                for label, enum_val in value_by_label.items():
                    if enum_val == default_target:
                        target_label = label
                        break
                var.set(target_label)

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_enum).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )

            self._var_map[key] = (meta, "enum", {"var": var, "value_by_label": value_by_label})
            help_row = 1

        elif meta.type in ("integer", "number"):
            default_str = str(val)
            if meta.type == "integer":
                try:
                    default_str = self._format_numeric_for_entry(meta, float(int(val)))
                except Exception:
                    default_str = self._format_numeric_for_entry(meta, 0.0)

            entry = ctk.CTkEntry(editor)
            entry.insert(0, default_str)
            entry.grid(row=0, column=0, sticky="ew", padx=12, pady=(10, 6))
            self._var_map[key] = (meta, "entry", entry)

            next_row = 1
            slider: Optional[ctk.CTkSlider] = None
            slider_var: Optional[tk.DoubleVar] = None
            tick_hint_lbl: Optional[ctk.CTkLabel] = None

            def update_tick_hint_from_entry() -> None:
                if tick_hint_lbl is None:
                    return
                raw = entry.get().strip()
                try:
                    if meta.type == "integer":
                        v = float(self._parse_integer_text(meta, raw))
                    else:
                        v = float(raw)
                except Exception:
                    tick_hint_lbl.configure(text=self._numeric_hint_context_text(meta))
                    return
                if meta.type == "integer":
                    v = float(int(round(v)))
                tick_hint_lbl.configure(text=self._numeric_hint_text(meta, v))

            if meta.unit.lower() in ("ticks", "ms", "hz"):
                tick_hint_lbl = ctk.CTkLabel(
                    editor,
                    text="",
                    anchor="w",
                    text_color="#8fb7ff",
                    font=ctk.CTkFont(size=12),
                )
                tick_hint_lbl.grid(row=next_row, column=0, columnspan=2, sticky="w", padx=12, pady=(0, 6))
                next_row += 1

            if self._can_use_slider(meta):
                try:
                    if meta.type == "integer":
                        init_slider_val = self._raw_to_ui(meta, float(int(val)))
                    else:
                        init_slider_val = self._raw_to_ui(meta, float(val))
                except Exception:
                    init_slider_val = self._raw_to_ui(meta, float(meta.minimum)) if (meta.minimum is not None) else 0.0
                slider_var = tk.DoubleVar(value=init_slider_val)
                slider = ctk.CTkSlider(
                    editor,
                    from_=self._raw_to_ui(meta, float(meta.minimum)),
                    to=self._raw_to_ui(meta, float(meta.maximum)),
                    number_of_steps=self._slider_steps(meta),
                    variable=slider_var,
                )

                def slider_to_entry(v: float) -> None:
                    snapped_ui = self._snap_numeric_value(meta, float(v))
                    snapped_raw = self._ui_to_raw(meta, snapped_ui)
                    text = self._format_numeric_for_entry(meta, snapped_raw)
                    entry.delete(0, "end")
                    entry.insert(0, text)
                    update_tick_hint_from_entry()

                def entry_to_slider(_event: Optional[tk.Event] = None) -> None:
                    if slider is None:
                        update_tick_hint_from_entry()
                        return
                    raw = entry.get().strip()
                    try:
                        if meta.type == "integer":
                            f_raw = float(self._parse_integer_text(meta, raw))
                        else:
                            f_raw = self._ui_to_raw(meta, float(raw))
                    except Exception:
                        update_tick_hint_from_entry()
                        return
                    snapped_ui = self._snap_numeric_value(meta, self._raw_to_ui(meta, f_raw))
                    snapped_raw = self._ui_to_raw(meta, snapped_ui)
                    slider.set(snapped_ui)
                    text = self._format_numeric_for_entry(meta, snapped_raw)
                    entry.delete(0, "end")
                    entry.insert(0, text)
                    update_tick_hint_from_entry()

                slider.configure(command=slider_to_entry)
                slider.grid(row=next_row, column=0, columnspan=2, sticky="ew", padx=12, pady=(0, 6))
                next_row += 1
                entry.bind("<Return>", entry_to_slider)
                entry.bind("<FocusOut>", entry_to_slider)
                slider_to_entry(init_slider_val)
            else:
                update_tick_hint_from_entry()

            help_row = next_row

            def reset_numeric(e=entry, sld=slider) -> None:
                if meta.type == "integer":
                    try:
                        target_text = self._format_numeric_for_entry(meta, float(int(default_target)))
                    except Exception:
                        target_text = self._format_numeric_for_entry(meta, 0.0)
                else:
                    target_text = str(default_target)
                e.delete(0, "end")
                e.insert(0, target_text)
                if sld is not None:
                    try:
                        if meta.type == "integer":
                            sld.set(self._snap_numeric_value(meta, self._raw_to_ui(meta, float(int(default_target)))))
                        else:
                            sld.set(self._snap_numeric_value(meta, self._raw_to_ui(meta, float(default_target))))
                    except Exception:
                        pass
                update_tick_hint_from_entry()

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_numeric).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )

        elif meta.type == "boolean":
            var = tk.BooleanVar(value=bool(val))
            w = ctk.CTkCheckBox(editor, text="", variable=var)
            w.grid(row=0, column=0, sticky="w", padx=12, pady=(10, 6))

            def reset_bool() -> None:
                var.set(bool(default_target))

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_bool).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )
            self._var_map[key] = (meta, "bool", var)
            help_row = 1

        elif meta.type == "string":
            default_str = "" if val is None else str(val)
            entry = ctk.CTkEntry(editor)
            entry.insert(0, default_str)
            entry.grid(row=0, column=0, sticky="ew", padx=12, pady=(10, 6))

            def reset_str() -> None:
                entry.delete(0, "end")
                entry.insert(0, "" if default_target is None else str(default_target))

            ctk.CTkButton(editor, text=default_button_text, width=80, command=reset_str).grid(
                row=0, column=1, sticky="e", padx=(0, 12), pady=(10, 6)
            )
            self._var_map[key] = (meta, "entry", entry)
            help_row = 1

        else:
            # fallback
            entry = ctk.CTkEntry(editor)
            entry.insert(0, str(val))
            entry.grid(row=0, column=0, sticky="ew", padx=12, pady=(10, 6))
            self._var_map[key] = (meta, "entry", entry)
            help_row = 1

        # help text
        help_txt = self._help_text(meta)
        if help_txt:
            ctk.CTkLabel(
                editor,
                text=help_txt,
                anchor="w",
                text_color="#a0a0a0",
                font=ctk.CTkFont(size=12),
            ).grid(row=help_row, column=0, columnspan=2, sticky="w", padx=12, pady=(0, 10))

    def _parse_value(self, meta: KnobMeta, kind: str, obj: Any) -> Any:
        if kind == "bool":
            return bool(obj.get())

        if kind == "enum":
            label = obj["var"].get()
            return obj["value_by_label"][label]

        if kind == "oneof":
            label = obj["var"].get()
            return obj["label_to_const"][label]

        if kind == "bitmask":
            value = 0
            for bit_ix, bvar in obj["bit_vars"]:
                if bvar.get():
                    value |= (1 << bit_ix)
            return value

        raw = obj.get().strip()  # entry text

        if meta.type == "integer":
            return self._parse_integer_text(meta, raw)

        if meta.type == "number":
            return float(raw)

        if meta.type == "string":
            return raw

        # fallback
        return raw

    def _collect_and_validate(self) -> tuple[Optional[dict[str, Any]], list[str]]:
        new_knobs: dict[str, Any] = {}
        errors: list[str] = []

        for key in self.all_keys:
            meta, kind, obj = self._var_map[key]
            try:
                value = self._parse_value(meta, kind, obj)
            except Exception as e:
                errors.append(f"{key}: parse error: {e}")
                continue

            err = validate_value(meta, value)
            if err:
                errors.append(err)
            new_knobs[key] = value

        for r in self.required:
            if r not in new_knobs:
                errors.append(f"missing required knob: {r}")

        if self.additional_properties is False:
            schema_props = set((self.schema_raw.get("properties") or {}).keys())
            extra = set(new_knobs.keys()) - schema_props
            if extra:
                errors.append(f"schema disallows unknown knobs: {', '.join(sorted(extra))}")

        return (None if errors else new_knobs), errors

    def _ordered_dump(self, knobs: dict[str, Any]) -> str:
        def tab_of(k: str) -> str:
            return self._tab_name(k, self.meta.get(k))

        def order_of(k: str) -> int:
            return self.meta.get(k, extract_meta(self.schema_raw, k)).order

        ordered_keys = sorted(knobs.keys(), key=lambda k: (self._tab_sort_key(tab_of(k)), order_of(k), k))
        ordered = {k: knobs[k] for k in ordered_keys}
        return json.dumps(ordered, indent=2, sort_keys=False) + "\n"

    @staticmethod
    def _sha256_hex(text: str) -> str:
        return hashlib.sha256(text.encode("utf-8")).hexdigest()

    def _backup_note_text(self) -> str:
        try:
            return self.backup_note_entry.get().strip()
        except Exception:
            return ""

    def _write_backup(self, old_text: str, new_text: str) -> Path:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        backup_name = f"knobs_{timestamp}.json"
        backup_path = self.backup_dir / backup_name
        note = self._backup_note_text()

        self.backup_dir.mkdir(parents=True, exist_ok=True)
        backup_path.write_text(old_text, encoding="utf-8", newline="\n")

        log_parts = [
            timestamp,
            f"file={backup_name}",
            f"old_sha256={self._sha256_hex(old_text)}",
            f"new_sha256={self._sha256_hex(new_text)}",
        ]
        if note:
            safe_note = note.replace("\n", " ").replace("\r", " ")
            log_parts.append(f'note="{safe_note}"')

        with self.backup_log_path.open("a", encoding="utf-8", newline="\n") as f:
            f.write(" ".join(log_parts) + "\n")

        return backup_path

    def on_close_no_save(self) -> None:
        self.destroy()

    def on_save_and_exit(self) -> None:
        new_knobs, errors = self._collect_and_validate()
        if errors:
            messagebox.showerror("Invalid values", "Fix these before saving:\n\n" + "\n".join(errors[:80]))
            return
        assert new_knobs is not None

        new_text = self._ordered_dump(new_knobs)
        old_text = ""
        try:
            if self.knobs_path.is_file():
                old_text = self.knobs_path.read_text(encoding="utf-8")
        except Exception as e:
            messagebox.showerror("Read failed", f"Could not read current {self.knobs_path}:\n{e}")
            return

        backup_path: Optional[Path] = None
        if old_text != new_text:
            try:
                backup_path = self._write_backup(old_text, new_text)
            except Exception as e:
                messagebox.showerror("Backup failed", f"Could not create knob backup:\n{e}")
                return

        # Write knobs.json
        try:
            self.knobs_path.write_text(new_text, encoding="utf-8", newline="\n")
        except Exception as e:
            messagebox.showerror("Save failed", f"Could not write {self.knobs_path}:\n{e}")
            return

        # Run generator
        if not self.gen_script.is_file():
            messagebox.showerror("Generator not found", f"Expected:\n{self.gen_script}")
            return

        autogen_before = ""
        try:
            if self.autogen_header_path.is_file():
                autogen_before = self.autogen_header_path.read_text(encoding="utf-8")
        except Exception:
            autogen_before = ""

        try:
            proc = subprocess.run(
                [sys.executable, str(self.gen_script)],
                cwd=str(self.repo_root),
                capture_output=True,
                text=True,
            )
        except Exception as e:
            messagebox.showerror("Generator failed", f"Could not run generator:\n{e}")
            return

        if proc.returncode != 0:
            out = (proc.stdout or "") + ("\n" if proc.stdout and proc.stderr else "") + (proc.stderr or "")
            messagebox.showerror("Generator failed", f"tools/gen_knobs.py exited {proc.returncode}:\n\n{out.strip()}")
            return

        autogen_after = ""
        autogen_changed = False
        try:
            if self.autogen_header_path.is_file():
                autogen_after = self.autogen_header_path.read_text(encoding="utf-8")
        except Exception:
            autogen_after = ""
        autogen_changed = (autogen_before != autogen_after)

        msg_lines = [
            f"Saved: {self.knobs_path.relative_to(self.repo_root)}",
            f"Generated: {self.autogen_header_path.relative_to(self.repo_root)}",
            f"Autogen changed: {'yes' if autogen_changed else 'no'}",
        ]
        if backup_path is not None:
            msg_lines.append(f"Backup: {backup_path.relative_to(self.repo_root)}")
        if proc.stdout and proc.stdout.strip():
            msg_lines.append("")
            msg_lines.append("gen_knobs.py output:")
            msg_lines.append(proc.stdout.strip())

        messagebox.showinfo("Knobs Saved", "\n".join(msg_lines))

        self.destroy()


def main() -> int:
    # Nice defaults
    ctk.set_appearance_mode("dark")        # "light" | "dark" | "system"
    ctk.set_default_color_theme("blue")    # "blue" | "green" | "dark-blue"

    here = Path(__file__).resolve()
    repo_root = find_repo_root(here.parent)

    app = KnobsApp(repo_root)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
