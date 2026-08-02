# Peripheral Robustness Contract

Authoritative specification for hardening peripheral bring-up and STOP2
resume behavior in PeepShow V5.

This document defines how peripherals must be initialized, resumed,
validated, and recovered to prevent intermittent “device didn’t come up”
failures at boot or after STOP2.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- Single-owner bus model (I2C and shared resources)
- Per-device state machines (FSM) and health publication
- Boot sequencing and resume sequencing
- STOP2 quiesce participation requirements
- I2C bus health checks and bus recovery
- Retry policy and failure policy

Does NOT define:
- Exact device register configs (driver-level details)
- Power governor policy (see power_management.md)
- RTOS topology (see rtos_architecture.md)

---

## Design Principles

- One owner per bus/peripheral handle.
- Never assume a peripheral survives STOP2 without re-validation.
- Recovery is built-in, not ad-hoc.
- All transactions are bounded (timeouts everywhere).
- Failures become visible (health flags), not silent.
- Drivers are dumb; policy lives in the owning service thread.

---

## Ownership Model

### I2C Ownership (Non-Negotiable)

- Only `thSensor` may access the I2C HAL handle.
- No other thread may call sensor drivers directly.
- All sensor requests go via `qSensorReq`.

This prevents concurrency races during boot, mode transitions, and wake.

### Coordination vs Execution

- `thPower` may coordinate sequencing by sending commands (RESUME/QUIESCE).
- Only `thSensor` executes I2C operations and sensor transactions.

---

## Layering: Driver vs Service Thread

### Driver Layer (Device Driver)

Drivers must be “dumb” and deterministic:

Allowed in driver:
- `whoami()`
- `soft_reset()` (if supported)
- `apply_config()` (write required registers)
- `read_sample()`
- `clear_int()` / `read_status()`

Forbidden in driver:
- ThreadX usage (queues, flags, semaphores)
- STOP2 knowledge
- retries/backoff policy
- bus recovery
- random delays (except unavoidable spec-mandated micro-delays)

### Service Layer (thSensor)

`thSensor` owns:
- per-device FSM (state, transitions, timers)
- retry policy + backoff
- bus recovery policy
- boot init sequencing
- STOP2 resume sequencing
- STOP2 quiesce participation
- publishing health flags to the rest of the system

---

## Per-Device State Machine

Each device managed by thSensor must maintain explicit state:

- OFF / UNKNOWN
- INITING
- READY
- FAULT
- RECOVERING
- SUSPENDED (quiesced for STOP2)

Transitions are controlled only by thSensor.

### READY Definition

A device is READY only after:

1. Bus sanity check passed
2. WHOAMI / identity probe passed
3. Configuration applied
4. Read-back verification passed (critical registers)

If any step fails, device must not be marked READY.

---

## Boot Bring-Up Contract (Sensors)

On boot, thSensor must:

1. Wait for I2C rail / pull-ups to be valid (bounded delay if required)
2. Perform I2C bus sanity check
3. For each device:
   - probe identity (WHOAMI)
   - apply config
   - verify config
   - mark READY
4. Publish health flags

If a device fails:
- enter RECOVERING and attempt bounded recovery
- if still failing, mark FAULT and publish FAULT flag
- system must remain usable (degrade gracefully)

---

## STOP2 Resume Contract (Sensors)

After STOP2 wake, thSensor must not assume devices remain configured.

Resume sequence is explicit:

1. Receive RESUME command from thPower (or mode manager)
2. Run I2C bus sanity check
3. For each managed device:
   - probe identity (WHOAMI)
   - reapply config
   - verify
   - clear latched interrupts if required
4. Publish READY/FAULT flags

Devices that fail resume:
- go to RECOVERING
- perform bus recovery if needed
- retry bounded times
- fall back to FAULT

---

## STOP2 Quiesce Contract (Sensors)

Before STOP2 entry, thSensor must participate in the quiesce barrier:

On QUIESCE request:

1. Stop starting new transactions immediately
2. Wait for in-flight transaction to complete (bounded timeout)
3. If stuck:
   - abort transaction
   - perform bus recovery
4. Place devices into SUSPENDED state if required
5. Acknowledge quiesce

STOP2 must not be entered while I2C is mid-transaction.

---

## I2C Bus Robustness (Mandatory)

All I2C transactions must:

- have timeouts
- return explicit error codes
- never block indefinitely

If an I2C transaction fails:

1. Record failure counters
2. Attempt a small bounded retry (policy below)
3. If still failing:
   - deinit I2C peripheral
   - perform GPIO-based bus recovery
   - reinit I2C
   - retry once
4. If still failing:
   - mark device RECOVERING or FAULT

### Bus Recovery (Required Capability)

Bus recovery must be implemented in thSensor bus wrapper:

- Reconfigure SCL/SDA as GPIO open-drain with pull-ups
- If SDA is held low:
  - pulse SCL 9–16 cycles to release slave state machine
- Generate STOP:
  - SDA low → high while SCL high
- Restore pins to I2C alternate function
- Reinitialize I2C peripheral

This is the canonical fix for “sometimes dead after wake”.

---

## Retry and Backoff Policy (Bounded)

Default policy (recommended):

- Per transaction: 0–1 immediate retry
- On device init/resume: up to 3 attempts total
- Between attempts: small bounded delay (e.g. 2–10 ms)
- After exhausting attempts: mark FAULT and publish

No unbounded retries.
No infinite recovery loops.
No silent failures.

---

## Health Publication (Required)

thSensor must publish device health to the rest of the system.

Recommended mechanisms:
- Event flags:
  - SENSOR_TMAG_READY / SENSOR_TMAG_FAULT
  - SENSOR_LIS_READY / SENSOR_LIS_FAULT
  - SENSOR_BUS_FAULT
- Optional health struct for diagnostics:
  - last_error
  - fail counters
  - last successful timestamp

Callers must tolerate NOT READY.

Gameplay/UI must degrade gracefully.

---

## Logging / Instrumentation (SWO Preferred)

On failures, emit concise SWO markers (rate-limited):

Include:
- device ID
- operation (probe/config/read)
- HAL error code
- whether bus recovery ran
- outcome

Do not spam strings.
Do not log in tight loops.

---

## Forbidden Patterns

- Any thread other than thSensor touching I2C.
- Entering STOP2 while I2C transfer is active.
- Drivers performing retries or backoff on their own.
- Random `HAL_Delay()` scattered in unrelated code paths.
- Treating intermittent comms failures as “random” without state/health visibility.

---

## Invariants (Do Not Violate)

- Single-owner bus model is absolute.
- Resume always re-probes + re-configures + verifies.
- All transactions are bounded with timeouts.
- Bus recovery exists and is used on stuck bus.
- Quiesce prevents STOP2 entry mid-transaction.
- Failure is visible via health flags.

---

Last updated: 2026-02-20
