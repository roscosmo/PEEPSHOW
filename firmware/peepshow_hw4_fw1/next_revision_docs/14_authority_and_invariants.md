# Authority and Cross-Cutting Invariants

This document is the single source of truth for rules that apply across all subsystems.

If any other document conflicts with this one, this document wins.

---

## Scope

Defines:
- ownership and concurrency invariants
- timing and cadence invariants
- storage and runtime isolation invariants
- determinism and debug invariants

Does not define:
- board electrical details (see `15_hardware_revision_contract.md`)
- per-subsystem FSM details (see `05_subsystem_state_machines.md`)

---

## Canonical Runtime Classes

Use these exact runtime class tokens:
- `SHELL`
- `LP_GRAPH`
- `LP_TEMPLATE`
- `RT_SCENE`
- `INSTALLER`

---

## Ownership Model (Non-Negotiable)

- Every peripheral and shared subsystem has exactly one owner thread.
- Other threads must send typed requests only.
- No cross-thread direct HAL/LL register access.
- ISR code must signal and return immediately.

Request payload rules:
- fixed-size POD structs
- no transient pointer ownership
- no function pointers
- bounded work only

---

## Timing Model

- Low-power cadence must be RTC/event driven.
- `RT_SCENE` cadence must be frame-scheduled and deterministic.
- No mode transition is complete until HAL and RTOS timebases are valid.

Time-domain labels are mandatory for all timing knobs:
- `threadx`
- `hal_ms`
- `knob_rtos_tick_hz`

---

## Power and Clock Invariants

- Power owner thread is sole owner of sleep class and clock transitions.
- Quiesce-before-sleep and resume-before-validate are mandatory.
- No clock changes during active DMA or active bus transaction.
- No STOP entry while critical owners are unquiesced.

---

## Storage and Installer Invariants

- Storage owner thread is sole owner of flash and filesystem operations.
- Runtime hosts must not use FAT for active runtime execution.
- `INSTALLER` is single-writer mode for host-visible transport.
- Non-installer subsystems are isolated while installer path is active.

---

## Determinism Invariants

- No unbounded loops in runtime-critical paths.
- No hidden retries or random backoff logic.
- No runtime dynamic allocation unless explicitly documented and approved.
- No filesystem streaming in active runtime loops.

---

## Debug Invariants

- HardFault capture is mandatory.
- Breakpoint use must be strategic and bounded.
- Structured trace events are preferred over heavy halt-based debugging.
- STOP behavior must be validated with evidence, not inference.

---

## Knobs Invariants

- All tunable firmware constants flow through knobs pipeline.
- Generated outputs are never edited manually.
- Knob changes require regeneration and rebuild.
- Knobs must not silently alter architecture boundaries.
- Knobs are visibility-gated as `public` or `private`.
- Packages and runtime hosts may request `public` knobs only.
- `private` knobs are firmware-internal and never exposed in package SDK.
- Public knob changes must be validated and routed to owner threads.
- Safety-critical knobs remain `private` by rule.

---

## Document Priority

When implementing:
1. `14_authority_and_invariants.md`
2. relevant subsystem contract doc
3. architecture/runtime/package contracts
4. other docs and comments

Any rule conflict must be fixed in docs immediately.
