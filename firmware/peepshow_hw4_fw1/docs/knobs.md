# Compile-Time Knobs System

PeepShow uses a centralized compile-time configuration system called **knobs**.

The purpose of this system is to:

- Eliminate scattered `#define` values
- Centralize all build-time tunables
- Ensure deterministic configuration
- Support future GUI-driven tuning workflows
- Maintain a single authoritative configuration source

This system is compile-time only.

Any knob change requires regeneration + rebuild.

Firmware never parses `knobs.json` at runtime.

---

## Architecture Overview

The knobs pipeline is:

    config/knobs.json        (human editable source of truth)
            ↓
    tools/gen_knobs.py       (generator)
            ↓
    Core/Inc/knobs_autogen.h (auto-generated firmware header)

Firmware includes only `knobs_autogen.h`.

`knobs.json` is never included or parsed by firmware.

---

## Timebase Contract

Every knob that represents time must declare its timebase and runtime compare domain.

Canonical domains:

- `threadx`: value is in ThreadX ticks and is compared directly against `tx_time_get()`/ThreadX waits.
- `hal_ms`: value is in HAL tick milliseconds and is compared against `HAL_GetTick()`.
- `knob_rtos_tick_hz`: value is authored in `KNOB_RTOS_TICK_HZ` units and converted once to runtime ticks before compare.

Rule for implementation:

- If conversion is needed, do it at one explicit boundary helper near the compare site.
- Do not mix domains in-place inside business logic.
- Schema descriptions must say both the authored domain and the runtime compare domain.

---

## Files

### 1) config/knobs.json

This is the single source of truth.

Example:

    {
      "ui_fps": 30,
      "rtos_tick_hz": 1000,
      "audio_sample_rate": 16000,
      "use_lpbam": false,
      "turbo_clock_mhz": 160
    }

All compile-time tunables must live here.

Do not scatter configuration values across headers or source files.

---

### 2) config/knobs.schema.json

Defines validation and editor metadata.

Used by VS Code to provide:

- Validation
- Range enforcement
- Enum dropdowns
- Descriptions / tooltips

This ensures structured and safe editing of `knobs.json`.

Schema metadata is also consumed by `tools/knobs_gui.py` for richer widgets.
Supported GUI-facing schema annotations include:

- `default`
- `enum`
- `oneOf` (`const` + `title`)
- `display` metadata:
  - `base` (`hex` supported)
  - `prefix`
  - `width`
  - `widget` (`bitmask`)
  - `bits` (bit index + label)

---

### 3) tools/gen_knobs.py

This script:

- Reads `config/knobs.json`
- Loads `config/knobs.schema.json`
- Injects schema `default` values for missing keys
- Verifies required keys after default injection
- Validates input
- Generates `Core/Inc/knobs_autogen.h`

Type conversion rules:

- integer  → (123)
- boolean  → (1) or (0)
- float    → (1.25)
- string   → "text"

All numeric values are emitted as parenthesized literals.

Important:
- Injected defaults are compile-time values exactly like explicit values in `knobs.json`.
- Generated output remains scalar macro literals only; no runtime schema parsing is introduced.

---

### 4) Core/Inc/knobs_autogen.h

This file is auto-generated.

Example:

    /* AUTO-GENERATED FILE. DO NOT EDIT. */

    #define KNOB_UI_FPS            (30)
    #define KNOB_RTOS_TICK_HZ      (1000)
    #define KNOB_AUDIO_SAMPLE_RATE (16000)
    #define KNOB_USE_LPBAM         (0)
    #define KNOB_TURBO_CLOCK_MHZ   (160)

Rules:

- Do not manually edit this file.
- Do not commit manual modifications.
- All changes must originate from `knobs.json`.

---

## Using Knobs in Firmware

Include:

    #include "knobs_autogen.h"

Use directly:

    uint32_t fps = KNOB_UI_FPS;

Compile-time toggles:

    #if KNOB_USE_LPBAM
        // enabled code path
    #endif

Generated macros always expand to safe, parenthesized literals.

---

## Updating Knobs

### Step 1 — Edit

Modify:

    config/knobs.json

Schema validation will enforce correctness.

---

### Step 2 — Regenerate Header

Run:

    python tools/gen_knobs.py

Or use the VS Code task.

This updates:

    Core/Inc/knobs_autogen.h

---

### Step 3 — Rebuild Firmware

Because knobs are compile-time constants:

Any change requires recompilation.

---

## Adding a New Knob

To add a knob properly:

1) Add to `config/knobs.json`

    "new_feature_enable": true

2) Add schema entry in `config/knobs.schema.json`
   - description
   - type
   - constraints
   - optional UI metadata

3) Regenerate header:

    python tools/gen_knobs.py

4) Use in firmware:

    #if KNOB_NEW_FEATURE_ENABLE
        ...
    #endif

---

## Rules (Non-Negotiable)

- `config/knobs.json` is the only source of truth.
- Never manually edit `knobs_autogen.h`.
- All compile-time tuning must flow through this system.
- Firmware must never parse JSON at runtime.
- Knobs must not introduce non-deterministic behavior.
- Knobs must not be used to silently change system architecture.

---

## Determinism Guarantee

Knobs are compile-time constants.

They must not:

- Introduce runtime randomness
- Add hidden retry loops
- Change execution timing unpredictably
- Alter peripheral ownership

All knob effects must remain deterministic.

---

## Future: GUI-Driven Knob Editing (Edit → Build → Flash)

The system is designed to support GUI-based tuning.

The GUI must treat the pipeline as authoritative:

- `config/knobs.json` is the only editable source
- `config/knobs.schema.json` defines validation and metadata
- `tools/gen_knobs.py` generates firmware header
- Firmware consumes only the generated header

Expected workflow:

1. Edit `config/knobs.json`
2. Validate against schema
3. Run generator
4. Build firmware
5. Flash firmware
6. (Optional) Record knob-set version for traceability

No tool may modify `knobs_autogen.h` directly.

Current GUI behavior highlights:

- Friendly label and exact JSON key are both shown per knob row.
- Schema default is used as reset target when present.
- Missing key + schema default can be shown as implicit-default in the GUI.
- Rendering priority is deterministic:
  1. `display.widget == "bitmask"`
  2. `oneOf`
  3. `enum`
  4. numeric editor (entry + slider where range is defined)
  5. boolean
  6. string
- Integer fields with `display.base = "hex"` render as hex and accept both hex/decimal input, while saving numeric JSON values (not strings).

---

## Schema Metadata for UI Grouping (Recommended)

To keep GUI clean and maintainable, schema entries may include:

- `category`:
  - UI
  - Gameplay
  - Audio
  - Power
  - Storage
  - Debug
  - RTOS
- `order`: integer for stable ordering
- `advanced`: boolean (hidden behind advanced toggle)
- `unit`: display unit (ms, Hz, px, etc.)
- `restart_required`: informational (true for compile-time knobs)

This metadata affects only tooling, not firmware.

---

## Version Stamping (Recommended)

For reproducibility:

In `knobs.json`:

    "knobs_set_version": "1.2.0"

Generator may emit:

    #define KNOB_SET_VERSION  "1.2.0"
    #define KNOB_BUILD_ID     "2026-02-18-abcdef"

This allows traceability of flashed builds.

---

## Compile-Time vs Runtime Configuration

Compile-time knobs are appropriate for:

- RTOS configuration
- Buffer sizes
- Feature gates
- Clock/power tradeoffs
- Debug toggles

If frequent tuning becomes burdensome, introduce a second tier:

- Runtime settings stored in raw settings region
- Adjustable without reflashing
- Clearly separated from compile-time knobs

Do not mix runtime configuration into compile-time knobs.

---

## Cleanliness Requirements

Because knobs are a configuration interface:

- Group related knobs logically.
- Use clear, descriptive names.
- Avoid redundant or overlapping knobs.
- Remove deprecated knobs.
- Keep schema descriptions accurate.
- Maintain consistent formatting.

`knobs.json` must remain clean and intentional.
It is not a dumping ground.
