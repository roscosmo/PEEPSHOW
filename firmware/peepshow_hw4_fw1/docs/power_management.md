# Power Management

Authoritative specification for STOP2 policy, wake sequencing, clock profile
transitions, and mode-governed power behavior in PeepShow V5.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- mode-level power policy (STOP/STATIC/REALTIME/FLASHING)
- STOP2 entry and exit contracts
- clock profile ownership and transition sequencing
- wake source handling requirements
- timebase correctness requirements after clock transitions

Does NOT define:
- electrical topology (see `docs/hardware.md`)
- thread ownership model (see `docs/rtos_architecture.md`)
- renderer or storage internals (see domain docs)

---

## Design Principles

- STOP2-first policy for low-power runtime.
- `thPower` is the single owner of mode state and clock transitions.
- No mode transition is complete until HAL + ThreadX timebases are verified.
- Quiesce-before-sleep and resume-before-validate are mandatory.
- Determinism over convenience (no unbounded retries or hidden polling loops).

---

## Mode Power Contract

Canonical modes:
- `STOP`: RTC-driven low-power pet runtime (STOP2-first)
- `STATIC`: awake UI/menu mode
- `REALTIME`: frame-scheduled 30 FPS runtime
- `FLASHING`: USB MSC transport mode

High-level policy:
- `STOP`: minimize clocks/peripherals; cadence from RTC, not SysTick.
- `STATIC`: interactive but power-aware; no degradation of STOP2 readiness.
- `REALTIME`: performance profile allowed, but still bounded and deterministic.
- `FLASHING`: USB-only operation; non-USB subsystems disabled.

---

## Clock Profile Ownership

Clock profile naming is canonical:
- `CLK_LOW`
- `CLK_NORM`
- `CLK_TURBO`

Rules:
- Only `thPower` may request/apply clock profile changes.
- No clock/prescaler changes during active DMA or bus transactions.
- Profile changes must be bracketed by subsystem quiesce/resume coordination.

---

## Transition Sequence (Required)

For any mode change that can affect clocks or STOP2:

1. `thPower` issues quiesce request to affected owner threads.
2. Owners stop or park active transfers with bounded completion.
3. `thPower` applies clock/mode transition.
4. SysTick/HAL tick is revalidated or reprogrammed as required.
5. ThreadX timebase behavior is validated.
6. Owners resume and revalidate peripheral liveness.
7. Transition is committed only after all required acknowledgements.

No infinite waits are allowed in this sequence.

---

## STOP2 Entry Contract

Before STOP2 entry:
- stop/pause active DMAs that are not explicitly retained
- ensure no owner thread is enqueueing work that cannot run in STOP2
- verify storage/datapath activity is quiesced (especially OCTOSPI operations)
- collect explicit quiesce acknowledgements with bounded timeout

STOP2 entry must fail cleanly if required acknowledgements are missing.

---

## STOP2 Exit Contract

After wake:
- restore clocks/peripheral state required by current mode
- reinitialize peripherals not guaranteed retained
- run liveness checks in owner threads (probe/health checks)
- publish fault state when a dependency fails (no silent failure)

Resume success requires both timebase correctness and dependency readiness.

---

## Wake Source Discipline

Wake handling must be explicit and auditable.

Rules:
- wake cause must be captured and published for mode logic
- RTC-driven wake remains authoritative for STOP cadence
- external wake sources (buttons/interrupt lines/USB attach) must be bounded and debounced in owning threads

Do not infer wake causes without hardware evidence.

---

## Timebase Correctness (Mandatory)

After any SYSCLK/HCLK change:
- SysTick configuration must be correct for HAL timing
- `HAL_GetTick()` must remain valid (or be replaced deliberately and documented)
- ThreadX timer/sleep behavior must remain correct

A transition that breaks timebase correctness is invalid.

---

## FLASHING Isolation

In `FLASHING` mode:
- USB MSC owns the transport volume path
- rendering, audio, gameplay, and sensor polling are disabled
- no competing storage access is allowed

This isolation is mandatory to preserve host timing and storage integrity.

---

## Invariants (Do Not Violate)

- `thPower` is sole owner of mode/clock transitions.
- STOP cadence is RTC-driven.
- REALTIME cadence remains frame-scheduled.
- STOP2 uses quiesce-then-sleep and resume-then-validate.
- No unbounded waits/retries in power transition paths.
- Timebase correctness is validated after clock changes.

---

## Cross-References

- `docs/authority.md`
- `docs/hardware.md`
- `docs/rtos_architecture.md`
- `docs/storage_and_updates.md`
- `docs/peripheral_robustness.md`

---

Last updated: 2026-02-20
