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

### REALTIME audio perf floor

During REALTIME clip playback, audio may require temporary guaranteed headroom.

Rules:
- `thAudio` publishes active/inactive state via `qSysEvents` only.
- `thPower` applies the floor while active and may release it when inactive.
- Floor target is the REALTIME entry profile knob (`KNOB_POWER_PERF_REALTIME_ENTRY_PROFILE`).
- This mechanism must never bypass `thPower` ownership of clocks/profiles.

### REALTIME input-reactive boost

To avoid visible hitching when gameplay transitions from idle to movement, `thPower`
may apply an immediate upward profile request on `APP_SYS_EVT_INPUT_ACTIVITY` while
in REALTIME.

Rules:
- Trigger is input activity routed through `qSysEvents` (not ISR direct clock changes).
- Immediate path is upward-only and one-shot (used to bypass dwell for the first rise).
- Target remains bounded by the existing profile cap/floor logic and knobs.
- Downshift behavior remains governor-controlled (streak + dwell), preserving power savings.

## CubeMX Regeneration Clock Contract

`CLK_NORM` is the CubeMX-owned base clock.

Required behavior:
- `SystemClock_Config()` remains authoritative for base SYSCLK.
- Entering `CLK_NORM` must restore clocks via `SystemClock_Config()` (not a duplicated hand-written tree).
- Dynamic boost logic must not downshift below the detected base SYSCLK.
- Boost profiles may use fixed sources (for example HSI-fed PLL paths) so performance levels remain stable even if CubeMX base changes.
- Peripheral clock restore paths must keep SAI/OCTOSPI timing deterministic across profile transitions.

Operational guidance:
- If CubeMX base SYSCLK is changed (for example MSIS 16 -> 24 MHz), dynamic profiles must adapt without requiring hardcoded profile rewrites.
- Debug tooling should expose both active profile and runtime base SYSCLK (`ps_power_perf: base_sysclk_mhz`) to validate this contract.

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
- when STOP re-entry or active-window deadlines are armed, `thPower` wait cadence
  must be bounded by the nearest deadline (no coarse fixed-time oversleep)

Resume success requires both timebase correctness and dependency readiness.

---

### STOP2 SRAM4 Retained Runtime State (Fast Resume)

STOP2 resume may use retained SRAM4 state to restore runtime continuity quickly.

Rules:
- Retained state must be validated with `magic + version + CRC` before use.
- Invalid retained data must be discarded and runtime must fall back to safe defaults.
- Retained SRAM4 state is **not durable storage**; it is RAM continuity only.
- Durable continuity across reset/power-loss remains a separate flash-backed path.
- Retained writes must stay bounded and thread-owned (no ISR writes, no HAL ownership changes).
- For REALTIME topdown restore, retained state must carry both gameplay snapshot data and
  scene binding IDs (`scene_map_id`, `scene_tileset_id`), so resume can restore the
  correct map/tileset before applying player/camera snapshot state.
- If retained scene binding IDs are absent or no longer resolvable, resume must fall back
  to package runtime-config scene IDs (safe default binding path).

Current scope:
- Pet state continuity across STOP2 wake cycles.
- REALTIME topdown session continuity across mode sleep/wake transitions.

---

## STOP Invalid-State Recovery Guard

`thPower` must not remain in a logically invalid STOP state.

Required behavior:
- If STOP mode is observed while STOP2 is not armed, recover through normal mode handling
  (`AppPowerHandleModeSet(APP_MODE_STATIC)` path) instead of forcing raw mode/power flags.
- If a `qSysEvents` queue error path is hit, use the same normal mode recovery path and
  clear pending quiesce/ack bookkeeping first.
- Recovery must keep wait loops bounded (no dead wait while display/audio/game are parked).

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

Last updated: 2026-03-16
