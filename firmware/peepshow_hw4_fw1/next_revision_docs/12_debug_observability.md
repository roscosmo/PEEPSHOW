# Debug and Observability Contract

This document defines what must be observable in bring-up and runtime without destabilizing low-power behavior.

---

## Allowed Debug Channels

- SWD (required)
- SWO structured events (preferred)
- USB CDC optional, rate-limited

No UART printf console policy by default.

---

## Required Event Visibility

Must be observable with low overhead:
- mode transitions
- subsystem state transitions
- runtime lifecycle transitions
- storage ownership transitions
- fault and recovery transitions

---

## HardFault Record Contract

On HardFault capture:
- PC, LR
- stacked registers
- CFSR, HFSR
- MMFAR, BFAR when valid
- current mode/runtime state identifiers

HardFault data must be captured before reset.

---

## Breakpoint Policy

- avoid high-frequency ISRs and DMA callbacks
- cap active breakpoints during runtime debugging
- prefer event markers over stop-heavy breakpoint sessions for STOP validation

---

## STOP2 Debug Policy

- enable debug-in-low-power when investigating STOP behavior
- classify every wake source with evidence
- avoid instrumentation that materially changes sleep behavior

---

## Production Build Guardrails

Release builds must:
- compile out verbose debug paths
- keep structured fault capture
- preserve deterministic timing behavior

