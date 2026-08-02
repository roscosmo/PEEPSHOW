# Bring-Up Progress Tracker

Working record of what has actually been brought up so far.

Authoritative requirements still live in:
- `docs/authority.md`
- `docs/boot_and_bringup.md`

This file is a status tracker, not a replacement spec.

Last updated: 2026-02-25

---

## Current Snapshot

- ThreadX object creation at init is in place (`MX_ThreadX_Init` path).
- Core ownership skeleton threads exist:
  - `thPower`
  - `thDisplay`
  - `thStorage`
  - `thInput`
  - `thSensor`
- Inter-thread transport is in place:
  - `qSysEvents`
  - `qDisplayCmd`
  - `qStorageReq`
  - `qInputCmd`
  - `qInputRaw`
  - `qSensorReq`
  - `egMode`
  - `egPower`
  - `egSensorHealth`
- Quiesce/Resume state machine is implemented in `thPower` with ACK mask tracking and timeout handling.
- `debug.gdb` helpers (`ps_smoke`, `ps_timeout`, `ps_resume`) are wired and being used for validation.
- Input repeat timing uses the `HAL_GetTick()` millisecond domain, with joystick-specific repeat knobs (`input_joy_repeat_*`) separated from generic button repeat knobs (`input_repeat_*`).
- STATIC UI entry-page policy is now knob-controlled (`ui_static_entry_point`):
  - `0`: auto (enter JoyCal only when joystick gate is invalid at STATIC entry)
  - `1`: force Home
  - `2`: force JoyCal
  - JoyCal routing is now evaluated on STATIC entry transition (one-shot), not forced every UI tick.
- REALTIME game-thread sensor integration now explicitly manages LIS stream lifecycle:
  - on REALTIME entry, `thGame` requests `App_SensorReq_LisStreamStart()`
  - on REALTIME exit, `thGame` requests `App_SensorReq_LisStreamStop()`
  - frame logic consumes `App_SensorSnapshot_Get()` per-frame for render/gameplay use
- PMIC bring-up (`ADP5360_init`) now runs in `thSensor` startup/resume path instead of `main`, preserving I2C owner-thread discipline.
- PMIC telemetry path is now explicit and hardware-validated:
  - charging control is PMIC-owned (configured in PMIC init), not firmware mode-forced
  - firmware reports charger state, charging-active boolean, battery SOC, and VBAT
  - firmware also publishes battery health FSM (`UNKNOWN/OK/WARN/CRIT`) with reason mask (low-VBAT / low-SOC contributors)
  - periodic PMIC polling remains suppressed in `FLASHING`
  - PMIC live counters are expected to reset on sensor resume path (`AppSensorRunResumeSequence` -> `AppSensorPmicRuntimeReset`).
- `thSensor` now owns per-device FSM state (`OFF/INITING/READY/FAULT/RECOVERING/SUSPENDED`) for PMIC/TMAG/LIS and publishes readiness/fault bits through `egSensorHealth`.
- LIS service-thread poll path now validates live data flow (STATUS + OUT_X/Y/Z raw read), not WHOAMI-only checks, and records a lightweight live snapshot for debugger diagnostics.
- LIS lifecycle mode-policy baseline is now explicit and validated:
  - `STOP`: low-power profile baseline, stream disabled
  - `STATIC`: low-power profile baseline, stream optional by explicit request
  - `REALTIME`: low-power profile baseline, stream optional by explicit request
  - `FLASHING`: stream disabled, no live polling
- LIS stream control and profile control are now separated:
  - `App_SensorReq_LisStreamStart/Stop()` controls periodic polling enable
  - `App_SensorReq_LisSetLowPower/Live()` controls profile explicitly
  - stream start does not implicitly force LIVE profile
- `thSensor` now performs queue-idle bounded auto-recovery polling for degraded devices (`state != READY && state != SUSPENDED`), gated by power flags so recovery does not run while quiescing/quiesced.
- Sensor FSM now tracks per-device recovery bookkeeping (`recovery_attempts`, `next_retry_tick`) with bounded retry policy controlled by knobs:
  - `sensor_recovery_max_attempts`
  - `sensor_recovery_backoff_ticks`
  - `sensor_fault_retry_ticks`
- `thSensor` now includes bounded I2C bus recovery capability (GPIO-based SCL pulse + STOP generation on I2C3 `PC0/PC1`, followed by I2C re-init and filter restore) and retries a failed probe once after bus recovery.
- Bus recovery pulse count is knob-controlled via `sensor_bus_recovery_scl_pulses` (range 9-16).
- `qSensorReq` now supports typed request payloads for:
  - `QUIESCE` / `RESUME`
  - targeted `POLL`
  - targeted `CONFIG_DEFAULTS`
  - `HEALTH_SNAPSHOT` publish refresh
- Sensor mode policy is now enforced in `thSensor`:
  - queue-idle auto-recovery runs in `STOP` / `STATIC` only
  - queue-idle auto-recovery is suppressed in `REALTIME` / `FLASHING`
  - sensor `RESUME` requests are suppressed while in `FLASHING`
  - sensor `POLL` / `CONFIG_DEFAULTS` requests are suppressed in `REALTIME` / `FLASHING`
- Sensor health flags now include explicit suspended-state visibility per device:
  - `PMIC_SUSPENDED`
  - `TMAG_SUSPENDED`
  - `LIS_SUSPENDED`
- Mode-change wiring now posts `APP_SENSOR_REQ_MODE_CHANGED` from `thPower` to `thSensor`:
  - entering `FLASHING` suspends sensors immediately in `thSensor`
  - leaving `FLASHING` to `STOP` / `STATIC` / `REALTIME` runs the sensor resume sequence
  - debugger evidence confirms suspend/resume transitions and mode-token updates
- Sensor FSM now tracks per-device `last_success_tick` and updates it on successful probe/poll paths for runtime health observability.
- `thAudio` ownership scaffold is now present:
  - `qAudioCmd` + `thAudio` created during `App_ThreadX_Init`
  - power quiesce/resume now includes audio ack source
  - deterministic SAI1 DMA test-tone start/stop path exists for phase-3 bring-up
  - `debug.gdb` now includes `ps_audio_status`, `ps_audio_start(_wait)`, `ps_audio_stop(_wait)` helpers
- `thInput` now includes stage-1 + stage-2 input routing path:
  - EXTI ISR edge callbacks post bounded raw events into `qInputRaw` (no ISR-side routing logic)
  - `thInput` consumes raw events and tracks debug counters (`post/recv/drop/suppressed`)
  - quiesce/resume gating for raw input processing is validated (`suppressed` increments while quiesced)
  - raw button edges now map to logical actions in `thInput` (`CONFIRM/CANCEL/LEFT/RIGHT/MENU`)
  - mode-aware logical routing policy is validated in counters:
    - STOP/STATIC -> UI route counter
    - REALTIME -> Game route counter
    - MENU override -> System route counter
  - stage-2 queue plumbing is now active:
    - STOP/STATIC actions are enqueued to `qUIEvents`
    - REALTIME actions are enqueued to `qGameEvents`
    - bounded `TX_NO_WAIT` post/drop counters are exposed in `ps_input_status`
  - consumer stubs are now active:
    - `thUI` drains `qUIEvents` and tracks consume counters
    - `thGame` drains `qGameEvents` and tracks consume counters
  - bounded input filtering + system posting is now active:
    - debounce/repeat policy in `thInput` is knob-controlled
    - `APP_SYS_EVT_INPUT_ACTIVITY` and `APP_SYS_EVT_INPUT_MENU` are posted via `qSysEvents`
    - `thPower` now consumes these input system events and tracks counters
- `debug.gdb` now includes `ps_input_status` helper
- `debug.gdb` now includes `ps_lis_diag` helper (LIS FSM + live snapshot: addr/whoami/status/raw xyz/sample/fail counters).
- `debug.gdb` now includes robust LIS stream smoke helpers:
  - `ps_lis_stream_smoke_static`
  - `ps_lis_stream_smoke_realtime`
  - `ps_lis_stream_smoke_end`
  These now wait for `thSensor` mode-change consumption (`g_sensor_mode_token`) before issuing stream requests.
- `debug.gdb` now includes frame/perf helpers for regression triage:
  - `ps_perf_mark`
  - `ps_perf_delta`
  with mode, thread-run, render/present, and LIS sampling deltas for quick budget checks.
- Sensor/input scheduling knobs were re-tuned after debugger-confirmed joystick release starvation under repeat-heavy UI load:
  - `rtos_sensor_thread_priority = 5`
  - `rtos_sensor_thread_preemption_threshold = 5`
  - `rtos_sensor_wait_ticks = 1`
- Knobs toolchain/UI workflow was expanded for safer tuning sessions:
  - GUI now supports schema-driven dropdowns (`enum`/`oneOf`), defaults, hex display, and bitmask widgets
  - save path keeps backup history and note text for traceability
  - `gen_knobs.py` now applies schema defaults for missing keys before autogen emit

Important clarification:
- `thStorage` currently exists as an ownership/queue scaffold for power handshakes.
- `thStorage` now includes phase-2 raw-flash validation handlers:
  - `APP_STORAGE_REQ_FLASH_PROBE` (AT25 boot init + JEDEC probe)
  - `APP_STORAGE_REQ_RAW_SMOKE` (bounded erase/program/readback/erase cycle at knob-defined smoke address)
- `thStorage` now includes LevelX/FileX request handlers and state telemetry:
  - `APP_STORAGE_REQ_FILEX_MOUNT`
  - `APP_STORAGE_REQ_FILEX_FORMAT` (format + remount)
  - `APP_STORAGE_REQ_FILEX_UNMOUNT`
- LevelX custom NOR driver now routes through AT25 raw operations for a knob-defined FAT transport region.
- Storage debug telemetry is now exposed for hardware bring-up through `debug.gdb` (`ps_storage_*` helpers).
- Hardware evidence now confirms FileX bring-up behavior:
  - mount may fail on an unformatted region (`fx_status=33`) until format is applied
  - format succeeds and remounts (`fmt>=1`, `m_ok` increments, `mounted=1`, `fx_status=0`)
  - explicit unmount is verified (`um_ok` increments, `mounted=0`, `last_err=0`, `fx_status=0`)

---

## Bring-Up Phase Status

Reference phases: `docs/boot_and_bringup.md`

| Phase | Status | Notes |
|------|--------|-------|
| 0 - Power + Clock Stability | In progress | System boots and runs ThreadX; no dedicated checklist record yet for full phase-0 criteria in this tracker. |
| 1 - Display Validation | In progress | `thDisplay` now uses command-driven invalidate/present and renderer dirty-row policy, flushing through `LS013B7DH05` (`LCD_FlushRows`/`LCD_FlushAll`) with a deterministic renderer-backed bootstrap frame. `thPower` now also produces mode-indicator updates via `qDisplayCmd` on mode-change events. |
| 2 - Storage Validation | In progress | `thStorage` now has command-driven JEDEC probe/raw smoke plus LevelX/FileX mount/format/unmount scaffolding. Hardware evidence confirms probe, mount/format/remount, and unmount paths on target hardware. |
| 3 - Audio Validation | In progress | `thAudio` owner-thread scaffold and SAI1 DMA test-tone path are implemented and baseline hardware behavior is validated (`start/stop`, quiesce/resume interaction). Remaining closeout is to record bounded-runtime evidence for sustained run stability and transition stress per checklist below. |
| 4 - Input + Sensors | In progress | `thInput` stage-1 raw EXTI capture (`qInputRaw`) and stage-2 logical action mapping/routing are validated on hardware, including quiesce suppression and resume behavior. Queue delivery is wired and validated to `qUIEvents`/`qGameEvents`, and consumer stubs (`thUI`/`thGame`) now drain both queues with verified consume counters. Knob-controlled debounce/repeat and input system-event posting (`APP_SYS_EVT_INPUT_ACTIVITY`/`APP_SYS_EVT_INPUT_MENU`) are also validated. `thSensor` runs bounded PMIC/TMAG/LIS probe/config verification on resume, tracks per-device FSM states, publishes health flags, and accepts typed targeted sensor requests via `qSensorReq`. LIS runtime poll now reads live status/raw XYZ for liveness validation. Additional scheduling tuning for `thSensor` is now recorded to prevent joystick-release starvation during high-rate UI repeat traffic. |
| 5 - RTOS Integration | In progress | Queue/event topology, thread ownership skeleton, and power handshake logic are implemented and tested. |
| 6 - STOP2 Introduction | Not started | STOP2 entry/exit integration and evidence not recorded yet. |
| 7 - FLASHING Mode | Not started | USB MSC/FileX flashing path not yet validated in this tracker. |

---

## Verified So Far (Debugger Evidence)

Using `>source debug.gdb` and scripted commands:

- `>ps_smoke`
  - QUIESCE path reaches expected `egPower=0x00000003` (QUIESCE_REQ + QUIESCED).
  - RESUME path reaches expected `egPower=0x0000000c` (RESUME_REQ + RUNNING).
- `>ps_timeout`
  - Forced no-ACK path reaches expected `egPower=0x00000011` (QUIESCE_REQ + QUIESCE_TIMEOUT).
- `>ps_mode_static`, `>ps_mode_realtime`, `>ps_mode_flashing`, `>ps_mode_stop`
  - Mode-set requests all queue successfully (`rc=0`) through `qSysEvents` / `thPower`.
- `>ps_resume_sensor` + forced LIS FAULT injection (`set g_sensor_lis.state=3`)
  - Sensor thread auto-recovered LIS back to `state=2` (`READY`) before the next halted inspection.
  - Health flags remained stable (`egSensor=0x00000007`, `bus_fault=0`).
- `>ps_storage_probe_wait`
  - JEDEC probe succeeds (`flash_ready=1`, `jedec=0x001f4218`, `last_err=0`).
- `>ps_storage_filex_mount_wait` (before format)
  - expected mount failure on blank FAT region (`fx_status=33`, `last_err=-110`, `m_fail=1`).
- `>ps_storage_filex_format_wait`
  - format path succeeds and remounts (`fmt=1`, `m_ok=1`, `mounted=1`, `fx_status=0`, `last_err=0`).
- `>ps_storage_filex_unmount_wait`
  - unmount path succeeds (`um_ok` increments, `mounted=0`, `fx_status=0`, `last_err=0`).
- `>ps_mode_verify_static` + `>ps_sensor_policy`
  - mode transitions to STATIC (`egMode=0x00000002`) and sensor policy allows auto-recovery (`autorecover_allowed=1`).
- `>ps_mode_verify_realtime` + `>ps_sensor_policy`
  - mode transitions to REALTIME (`egMode=0x00000004`) and auto-recovery is suppressed (`autorecover_allowed=0`).
- `>ps_mode_verify_flashing` + `>ps_sensor_policy`
  - mode transitions to FLASHING (`egMode=0x00000008`) and auto-recovery is suppressed (`autorecover_allowed=0`).
- `>ps_mode_verify_flashing` + run/pause + PMIC deltas
  - PMIC periodic polling suppression in FLASHING is confirmed (`sample_count delta=0`, `last_sample_tick delta=0` over paused run window).
- `>ps_mode_verify_static` + run/pause + PMIC deltas
  - PMIC sample counter wrap/underflow was observed when comparing against a pre-resume baseline; this is expected because PMIC live counters are reset during resume sequence.
- `>ps_mode_verify_realtime` + run/pause + PMIC deltas
  - PMIC periodic polling in REALTIME is confirmed (positive sample/tick deltas matching poll cadence).
- `>ps_pmic_diag` (post telemetry-only update)
  - PMIC diagnostics report `enabled_cfg`, `active`, SOC (`percent/raw`), and VBAT (`mV/raw`) without mode-driven charge forcing.
- `>ps_pmic_diag` (battery-health update)
  - PMIC diagnostics additionally report `health` and `reason` for battery-first runtime gating/UX decisions.
- `>ps_lis_stream_smoke_static` + run + `>ps_lis_stream_smoke_end`
  - STATIC stream policy validated: `sensor_mode_token=1`, `stream=1`, `profile requested/applied=0`, and positive sample/tick deltas (`sample` advanced significantly while held in STATIC).
- `>ps_lis_stream_smoke_realtime` + run + `>ps_lis_stream_smoke_end`
  - REALTIME stream policy validated: `sensor_mode_token=2`, `stream=1`, `profile requested/applied=0`, and positive sample/tick deltas (`sample` advanced while held in REALTIME).
- `>ps_sensor_poll_all_wait` / `>ps_sensor_cfg_all_wait` while in FLASHING
  - command paths are intentionally suppressed, do not block, and health remains stable (`egSensor=0x00000007`, `bus_fault=0`).
- `>tbreak AppSensorHandleModeChange` + `>ps_mode_flashing` + `>finish`
  - `thSensor` handles mode change to FLASHING and parks all sensors in `SUSPENDED` (`state=5`), with `sensor_mode_token=3`.
- `>tbreak AppSensorHandleModeChange` + `>ps_mode_static` + `>finish`
  - `thSensor` handles mode change back to STATIC, runs resume path, and returns sensors to READY (`state=2`) with `sensor_mode_token=1`.
- `>ps_mode_verify_realtime` + `>ps_sensor_poll_all_wait` / `>ps_sensor_cfg_all_wait`
  - in REALTIME, explicit POLL and CONFIG_DEFAULTS request paths are intentionally suppressed and do not block.
- `>ps_mode_verify_static` + `>ps_sensor_poll_all_wait` / `>ps_sensor_cfg_all_wait`
  - in STATIC, explicit POLL and CONFIG_DEFAULTS execute normally (`AppSensorRunPollSequence` / `AppSensorApplyDefaults` hit).
- `>tbreak AppSensorHandleModeChange` + `>ps_mode_flashing` + `>finish` + `>ps_sensor_health`
  - suspended-state visibility confirmed: `egSensor=0x00000380`, `suspended_flags=0x00000380`, `sensor_mode_token=3`, device states `SUSPENDED` (`state=5`).
- `>tbreak AppSensorHandleModeChange` + `>ps_mode_static` + `>finish` + `>ps_sensor_health`
  - resume visibility confirmed: `egSensor=0x00000007`, `suspended_flags=0x00000000`, `sensor_mode_token=1`, device states `READY` (`state=2`).
- `>ps_mode_verify_static` + `>ps_sensor_poll_all_wait` / `>ps_sensor_cfg_all_wait` + `>ps_sensor_health`
  - per-device `ok=` ticks advance in STATIC after successful poll/default paths (`last_success_tick` visibility working).
- `>ps_mode_verify_realtime` + `>ps_sensor_poll_all_wait` / `>ps_sensor_cfg_all_wait` + `>ps_sensor_health`
  - `ok=` ticks remain unchanged while request paths are suppressed in REALTIME.
- `>ps_mode_verify_flashing` + `>ps_sensor_health`
  - immediate post-queue reads can still show pre-change `sensor_mode_token` until `thSensor` consumes mode-change request; the tbreak-based mode-change checks above remain the authoritative validation.
- `>ps_audio_start` + run/pause + `>ps_audio_status`
  - continuous SAI test-tone playback confirmed on target with stable counters (`state=1`, `restarts` increasing, `underrun=0`, `last_err=0`).
- `>ps_audio_stop` + run/pause + `>ps_audio_status`
  - queued stop path confirmed (`state=0`, `stops` incremented, tone stops, `underrun=0`).
- `>ps_audio_power_smoke`
  - audio-active quiesce/resume interaction validated:
    - before quiesce: `state=1`
    - after quiesce: `state=0`, `stops` incremented
    - after resume: audio remains stopped until explicitly commanded again (`state=0`)
    - power flags still transition correctly (`egPower=0x00000003` on QUIESCE, `0x0000000c` on RESUME).
- Audio debug note:
  - breakpoint-based `*_wait` helpers can perturb live DMA timing and inflate `err` counters during halted stop/restart windows.
  - non-blocking run/pause status checks are the authoritative validation for audible behavior.
- `>ps_input_status` after run/pause + button presses
  - raw input ISR/thread path confirmed active with matched counts (`post=20`, `recv=20`, `drop=0`), and valid last-event metadata (`src/edge/level/tick` updating).
- `>ps_quiesce` then run/pause + button presses + `>ps_input_status`
  - quiesce gating confirmed (`quiesced=1`) with suppression behavior active (`suppressed=8`, `recv=0` during quiesced sample window).
- `>ps_resume` then run/pause + button presses + `>ps_input_status`
  - resume behavior confirmed (`quiesced=0`) and raw event consumption resumes (`recv` increases again while `drop=0`).
- `>ps_mode_static` + run/pause + `>ps_input_status`
  - logical stage-2 routing confirmed in STATIC: `action.total=4`, `ui=4`, `game=0`, `sys=0`, `ignored=0`, `last_mode=0x00000002`.
- `>ps_mode_realtime` + run/pause + `>ps_input_status`
  - logical stage-2 routing confirmed in REALTIME: counters advance with `game` path (`action.total=8`, `ui=4`, `game=4`, `sys=0`, `ignored=0`, `last_mode=0x00000004`).
- run/pause + BOOT press + `>ps_input_status`
  - MENU override path confirmed: `sys` increments (`action.total=9`, `ui=4`, `game=4`, `sys=1`, `ignored=0`, `last_action=5`).
- `>ps_quiesce` + run/pause + button presses + `>ps_input_status`
  - quiesce still suppresses input cleanly with stage-2 enabled (`quiesced=1`, `post=26`, `recv=18`, `suppressed=8`; action counters unchanged while quiesced).
- `>ps_mode_static` + run/pause + `>ps_input_status` (queue plumbing validation)
  - STOP/STATIC logical actions are enqueued into `qUIEvents` (`qUI=3`, `ui_ok=3`, `ui_drop=0`), while `qGame` remains unchanged.
- `>ps_mode_realtime` + run/pause + `>ps_input_status` (queue plumbing validation)
  - REALTIME logical actions are enqueued into `qGameEvents` (`qGame=5`, `game_ok=5`, `game_drop=0`), while `qUI` remains unchanged.
- `>ps_mode_static` + run/pause + `>ps_input_status` (consumer validation)
  - with `thUI` stub active, UI queue is drained (`qUI=0`) and consume telemetry matches posts (`ui_ok=4`, `consumed ui=4`).
- `>ps_mode_realtime` + run/pause + `>ps_input_status` (consumer validation)
  - with `thGame` stub active, game queue is drained (`qGame=0`) and consume telemetry matches posts (`game_ok=4`, `consumed game=4`).
- `>ps_quiesce` + run/pause + `>ps_input_status` (consumer + suppression validation)
  - quiesce suppression still holds with consumers enabled (`quiesced=1`, `suppressed=14`) while action post and consume counters remain unchanged during quiesce.
- `>ps_mode_static` + run/pause + `>ps_input_status` (debounce/repeat + system post validation)
  - repeat policy active and bounded (`repeat_emit=2` then `repeat_emit=31`) with no queue drops (`ui_drop=0`, `game_drop=0`).
  - input activity system posting is active and lossless in this slice (`activity_ok` tracks `action.total`, `activity_drop=0`).
- run/pause + BOOT press + `>ps_input_status` (menu system post validation)
  - menu override posts successfully to `qSysEvents` (`menu_ok=1`, `menu_drop=0`) while preserving route counters.
- `>ps_mode_realtime` + run/pause + `>ps_input_status` (debounce behavior visibility)
  - debounce filter is active in realtime path (`debounce_drop=2`) and routing/consumption remain stable (`game_ok=7`, `consumed game=7`).
- `>ps_mode_static` hold A + `>ps_input_status`, then `>ps_mode_realtime` hold A + `>ps_input_status` (mode-aware repeat policy validation)
  - STATIC hold emits repeats (`repeat_emit` increments from `0` to `1`) and routes to UI (`ui` increments).
  - REALTIME hold suppresses repeats (`repeat_emit` unchanged) while initial press still routes to game (`game` increments).
- `>ps_mode_static` + single A press/release + `>ps_input_status` (UI handler path validation)
  - UI route is end-to-end valid in STATIC (`ui=1`, `posts ui_ok=1`, `consumed ui=1`, `handled ui_ok=1`, queue errors `0`).
- `>ps_mode_realtime` + button presses + `>ps_input_status` (Game handler path validation)
  - Game route is end-to-end valid in REALTIME (`game=6`, `posts game_ok=6`, `consumed game=6`, `handled game_ok=6`, queue errors `0`).
- `>ps_quiesce` / `>ps_resume` + `>ps_input_status` (handler gating validation)
  - While quiesced, suppression increases and action/handler counters remain stable; after resume, processing resumes without queue drops.
- Joystick runaway-latch diagnostic (STATIC menu, repeat stress):
  - when runaway was active, `g_input_button_state[6].pressed=1` (JOY_UP) and no release edge advanced (`last_edge_tick` stayed fixed), while repeat/action counters continued to climb.
  - scheduler delta probe during runaway showed sensor starvation (`thSensor run_count delta=0`) while UI continued (`thUI run_count delta>0`) over ~2 seconds.
  - this evidence drove the sensor scheduling knob adjustments recorded in Current Snapshot.

These checks confirm current power transitions, sensor auto-recovery and mode-policy behavior (including explicit REALTIME request suppression, suspended-state visibility, and success-tick observability), and first-time FileX bring-up behavior in the active runtime.

---

## Phase 3 Closeout Checklist (Audio)

Use this as the explicit signoff gate before marking Phase 3 complete.

1. Sustained playback stability
   - Command: `ps_audio_start`, run for >= 5 minutes, then inspect `ps_audio_status`.
   - Pass criteria:
     - `state=1` during run
     - `underrun` remains `0`
     - `last_err=0`
     - audible output remains stable (no dropouts/stalls)

2. Controlled stop behavior
   - Command: `ps_audio_stop`, run briefly, inspect `ps_audio_status`.
   - Pass criteria:
     - `state=0`
     - `stops` increments exactly once per requested stop
     - no new underrun/error side effects

3. Quiesce/resume while audio active
   - Command: `ps_audio_start` -> `ps_quiesce` -> `ps_resume` with status checks.
   - Pass criteria:
     - audio stops cleanly on quiesce (`state=0`)
     - no hangs in power handshake
     - post-resume remains stopped until explicit restart command

4. Rapid transition stress (bounded)
   - Procedure: perform 20 cycles of start/stop and 20 cycles of quiesce/resume with short spacing.
   - Pass criteria:
     - no HardFault
     - no queue deadlock
     - no persistent error counters

5. Documentation signoff
   - Add final evidence snapshot lines under "Verified So Far (Debugger Evidence)".
   - Update Phase 3 status note from "In progress" to "Complete" only after steps 1-4 pass.

---

## What Is Next

Near-term target:
1. Close phase-4 scope cleanly:
   - keep LIS in low-power baseline by default
   - leave LIS gameplay usage deferred
   - optionally add a UI/system debug toggle for LIS stream start/stop if desired for field diagnostics
2. Phase-3 evidence completion:
   - capture and log final hardware validation evidence for `thAudio` DMA path under normal run and quiesce/resume transitions
3. Phase-5 expansion:
   - replace UI/Game consumer stubs with real owner-thread handlers where behavior is still placeholder
4. STOP2 preparation (phase-6 prework):
   - tighten quiesce latency checks and add explicit SWO markers around pre-stop readiness conditions
5. Input feel polish:
   - tune repeat/debounce knobs from current stable baseline after gameplay-handler integration

After that:
- continue phase progression with explicit evidence logged per phase in this file.
