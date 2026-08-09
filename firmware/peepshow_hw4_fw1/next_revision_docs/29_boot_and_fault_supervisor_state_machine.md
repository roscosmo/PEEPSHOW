# Boot and Fault Supervisor State Machine

This document defines explicit FSMs for deterministic boot sequencing and system fault supervision.

---

## Scope

Defines:
- boot phase progression FSM
- fault supervision and recovery FSM

Does not define:
- per-peripheral recovery internals (see `20_peripheral_robustness_contract.md`)

---

## 1) Boot Phase FSM

States:
- `BOOT_RESET`
- `BOOT_CLOCK_INIT`
- `BOOT_PERIPHERAL_INIT`
- `BOOT_RTOS_INIT`
- `BOOT_SERVICE_INIT`
- `BOOT_SELF_TEST`
- `BOOT_READY`
- `BOOT_SAFE_MODE`
- `BOOT_FAULT_LATCHED`

Key events:
- `EV_RESET_VECTOR`
- `EV_CLOCK_OK`
- `EV_CLOCK_FAIL`
- `EV_PERIPH_OK`
- `EV_PERIPH_FAIL`
- `EV_RTOS_OK`
- `EV_RTOS_FAIL`
- `EV_SERVICES_OK`
- `EV_SERVICES_FAIL`
- `EV_SELF_TEST_OK`
- `EV_SELF_TEST_FAIL`
- `EV_SAFE_MODE_REQUEST`
- `EV_FATAL_FAULT`

Rules:
- Phase skipping is forbidden.
- `BOOT_READY` is reachable only through successful completion of all required prior phases.
- Any fatal boot failure transitions to `BOOT_FAULT_LATCHED` with fault record preserved.

---

## 2) Fault Supervisor FSM

States:
- `FAULT_HEALTHY`
- `FAULT_DEGRADED`
- `FAULT_RECOVERY_PENDING`
- `FAULT_RECOVERING`
- `FAULT_QUARANTINED`
- `FAULT_PANIC`

Key events:
- `EV_SUBSYSTEM_FAULT`
- `EV_FAULT_THRESHOLD_REACHED`
- `EV_RECOVERY_START`
- `EV_RECOVERY_OK`
- `EV_RECOVERY_FAIL`
- `EV_RECOVERY_EXHAUSTED`
- `EV_FAULT_CLEARED`
- `EV_PANIC_REQUEST`

Rules:
- Recovery attempts must be bounded.
- Fault escalation policy must be explicit and deterministic.
- `FAULT_PANIC` triggers safe-state behavior and preserves fault diagnostics.

---

## Supervisor Responsibilities

- Own global boot progress state publication.
- Aggregate subsystem fault signals and apply escalation policy.
- Trigger safe-mode routing when normal operation is unsafe.
- Expose state to diagnostics and brought-up tracker evidence.

---

## Validation Cases

1. successful full boot phase progression
2. fault-injected boot phase failure handling
3. safe-mode entry and shell availability
4. bounded recovery retries and escalation correctness
5. persistent fault latching with complete diagnostics

