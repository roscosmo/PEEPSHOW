# Display and Rendering Contract

This document defines display ownership, buffer semantics, and present behavior.

---

## Scope

Defines:
- display owner and command model
- panel data format contract
- flush/update behavior
- dirty-region behavior
- low-power display behavior

---

## Ownership

- `thDisplay` is the sole owner of display peripheral handles and transfer operations.
- Other threads submit display requests only through `qDisplayCmd`.
- No direct display HAL calls outside `thDisplay`.

---

## Required Display FSM

Use states from `05_subsystem_state_machines.md`:
- `DISP_OFF`
- `DISP_STATIC_HOLD`
- `DISP_LP_UPDATE`
- `DISP_RT_UPDATE`
- `DISP_SUSPENDED`
- `DISP_ERROR`

All update requests must validate current state before execution.

---

## Buffer Contract

Define and freeze:
- panel resolution
- pixel polarity convention
- row/column addressing convention
- byte ordering convention
- required alignment and memory region for DMA

These values must be documented in this file once hardware is final.

---

## Present Contract

Required command types:
- invalidate region
- present frame or rows
- suspend display path
- resume display path

Rules:
- no overlapping flush transactions
- no partial transaction state leakage across mode transitions
- blocked/rejected present while suspended

---

## Dirty-Region Policy

Dirty update policy must define:
- dirty granularity (row/tile/region)
- threshold for full refresh fallback
- max update budget per cycle in low-power modes

Policy must be deterministic and bounded.

---

## Low-Power Behavior

When entering deep low-power states:
- quiesce display transfers
- preserve panel rules required for stable visual behavior
- resume with explicit validation path

Display must not block STOP entry indefinitely.

---

## Validation Cases

1. first-frame bring-up correctness
2. partial update correctness
3. suspend and resume correctness
4. rapid mode-switch robustness
5. fault and recovery path

