# Debugging

Authoritative specification for debugging methodology, toolchain usage,
HardFault handling, SWO instrumentation, and breakpoint discipline
in PeepShow V5.

This document defines how the system must be debugged and what is allowed.
If debugging practice deviates from this document, results are unreliable.

---

## Scope

Defines:
- Supported debug interfaces
- HardFault capture requirements
- Breakpoint discipline
- SWO logging rules
- STOP2 debug constraints
- USB and FLASHING debug isolation

Does NOT define:
- Boot sequencing (see boot_and_bringup.md)
- Power policy (see power_management.md)
- RTOS ownership rules (see rtos_architecture.md)

---

## Supported Debug Interfaces

Primary:
- SWD via ST-Link V3 MINI-E

Secondary:
- SWO (preferred runtime instrumentation channel)

Optional:
- USB CDC (developer-only, timing sensitive)

UART console logging is not supported.

---

## Debug Philosophy

- Hardware-first debugging.
- HardFault capture is mandatory.
- SWO is preferred over printf.
- Breakpoints must be rare and strategic.
- STOP2 behavior must be validated explicitly.

---

## HardFault Policy (Non-Negotiable)

On HardFault:

Firmware must capture:

- R0-R3
- R12
- LR
- PC
- xPSR
- CFSR
- HFSR
- MMFAR (if valid)
- BFAR (if valid)

Optional:
- Current ThreadX thread pointer or ID

HardFault handler must:

1. Store fault record in static structure.
2. Emit concise SWO marker.
3. Halt or reset safely.

HardFault must never be ignored or auto-cleared without inspection.

---

## Breakpoint Discipline

Breakpoints must be limited and high-value.

Maximum:
- 5 total active breakpoints in debug.gdb

Allowed breakpoint targets:

1. HardFault handler
2. ThreadX stack overflow hook
3. ThreadX malloc failure hook (if used)
4. STOP2 entry or wake boundary
5. USB connect/disconnect handler (when debugging MSC)

Breakpoints must not be placed in:

- High-frequency ISRs
- DMA callbacks
- Display flush loops
- Audio buffer refill loops
- Frame loop hot paths

Timing distortion must be avoided.

---

## debug.gdb Contract

The file debug.gdb in project root is authoritative.

It must:

- Contain only essential breakpoints
- Avoid experimental clutter
- Be edited deliberately

The debugger must not guess random breakpoints.

Scratch-memory safety for runtime tuning:
- Do not use buffers that back active runtime views as temporary debugger write
  targets.
- In particular, if a map is currently loaded, avoid writing temporary structs
  into `g_storage_scene_map_blob_buf` (runtime map view keeps pointers into this
  memory).
- Prefer `g_storage_game_package_manifest_buf` or a dedicated scratch region for
  temporary tune patches.

Runtime tune call-context safety:
- Do not invoke runtime tune apply/reset functions directly from arbitrary halted
  debugger context (can be in ISR/handler context and corrupt execution state).
- Use queued debug-tune workflow (`ps_rt_tune_*` helpers) so apply/reset happens
  inside `thGame` REALTIME thread context.
- Current queued model is single-slot pending; issuing multiple `ps_rt_tune_*`
  commands before letting `thGame` run will overwrite earlier pending values.

---

## SWO Logging Rules

SWO is the primary runtime visibility channel.

Logging must be:

- Structured (event codes preferred)
- Rate-limited
- Non-blocking
- Short

Recommended event categories:

- BOOT_x milestones
- PWR_ENTER_STOP2
- PWR_WAKE
- AUDIO_START / AUDIO_STOP
- STORAGE_INSTALL
- USB_ENUM_OK
- HARDFAULT summary

Continuous streaming logs are forbidden.

---

## STOP2 Debug Discipline

When debugging STOP2:

- Enable debug-in-low-power.
- Prefer SWO markers around STOP entry/exit.
- Avoid breakpoints near STOP entry.

If debugger disconnects unexpectedly:

- Lower SWD frequency.
- Use connect-under-reset.
- Confirm NRST wiring.

STOP2 bugs must not be diagnosed without trace evidence.

---

## USB / FLASHING Debug Discipline

In FLASHING mode:

- All non-USB functions must be disabled.
- No rendering.
- No audio.
- No STOP tick runtime.

When debugging USB:

- Minimize SWO noise.
- Avoid high-frequency logs.
- Use single strategic breakpoint if required.

USB timing must not be distorted by excessive breakpoints.

---

## SWD Frequency Guidelines

Known good:
- 8000 kHz

If unstable:
- 4000 kHz
- 2000 kHz
- 1000 kHz

Lower frequency during:
- Early bring-up
- STOP2 validation
- Clock tree experimentation

---

## Common Debug Playbooks

### Does Not Boot

- Connect under reset.
- Break at early init milestone.
- Inspect clock config.
- Inspect HardFault record if triggered.

---

### Random Crash

- Inspect HardFault record.
- Check stack overflow hook.
- Check invalid pointer use.
- Validate DMA buffer placement.

---

### STOP2 Not Waking

- Add SWO markers before STOP entry.
- Add SWO marker on wake.
- Verify wake source flags.
- Confirm debug-in-low-power enabled.

---

### USB Corruption

- Confirm single-writer rule.
- Confirm FileX unmounted before MSC.
- Reduce logging.
- Avoid breakpoints inside USB ISR.

---

## Forbidden Debug Practices

- Ignoring HardFault context.
- Leaving experimental breakpoints active.
- Printing continuously via USB.
- Debugging STOP2 without low-power debug enabled.
- Modifying clock tree blindly during runtime.

---

## Invariants

- HardFault capture is always enabled.
- debug.gdb contains <= 5 breakpoints.
- SWO is structured and rate-limited.
- STOP2 is debugged with instrumentation, not guesswork.
- FLASHING mode disables all non-USB activity.

---

Last updated: 2026-02-20
