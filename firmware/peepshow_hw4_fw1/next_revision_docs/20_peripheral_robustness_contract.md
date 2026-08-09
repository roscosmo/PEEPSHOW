# Peripheral Robustness Contract

This document defines required robustness behavior for peripheral bring-up and wake/resume.

---

## Core Rules

- every peripheral path has one owner thread
- all transactions are bounded by timeout
- failures are visible through health publication
- recovery behavior is explicit, not ad hoc

---

## Per-Device FSM Requirement

Each managed device requires explicit states:
- `OFFLINE`
- `INIT`
- `ONLINE`
- `ERROR`
- `RECOVERING`
- `SUSPENDED` (if quiesced)

State transitions must be logged and testable.

---

## Boot Bring-Up Contract

For each peripheral:
1. probe identity/liveness
2. apply configuration
3. verify critical config
4. publish health state

Failure must transition to recovery/fault path with bounded retries.

---

## Quiesce and Resume Contract

Before deep sleep:
- stop new transactions
- drain or abort active transactions with timeout
- acknowledge quiesce

After wake:
- revalidate bus and device liveness
- reapply required configuration
- clear stale fault states only after successful verification

---

## Shared Bus Recovery (Required)

For shared buses (for example I2C), implement recovery flow:
- detect bus stuck condition
- apply bounded recovery pulses/stop condition
- reinitialize peripheral
- retry bounded times

No infinite recovery loops.

---

## Health Publication Contract

Each owner must publish:
- ready/fault flags
- last error code
- failure counters
- last success timestamp

Callers must handle dependencies not ready.

---

## Validation Cases

1. cold boot init success/failure handling
2. repeated sleep/wake recovery behavior
3. forced bus fault recovery behavior
4. dependency gating on mode entry
5. non-blocking behavior under repeated faults

