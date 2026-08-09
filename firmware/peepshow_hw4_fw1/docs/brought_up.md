# Bring-Up Progress Tracker

Working record of what has actually been brought up so far.

Authoritative requirements still live in:
- `docs/authority.md`
- `docs/boot_and_bringup.md`

This file is a status tracker, not a replacement spec.

Last updated: 2026-03-22

---

## How To Read This Tracker

Use this file in three layers:

1. `Historical Snapshot Archive` in `docs/brought_up_archive.md` holds the detailed chronological update log.
2. `Bring-Up Phase Status` is the phase-gate view against `docs/boot_and_bringup.md`.
3. `Verified So Far (Debugger Evidence)` is the active supporting evidence ledger.

Status tokens in `Bring-Up Phase Status` are strict:

- `Not started`: no hardware evidence logged for the phase goals.
- `In progress`: partial evidence exists, but at least one phase goal/precondition
  remains open.
- `Complete`: all phase goals/preconditions are met and evidence is recorded in
  this tracker.

If there is any conflict, `docs/authority.md` then `docs/boot_and_bringup.md`
remain authoritative.

---

## Historical Snapshot Archive

Large chronological update logs and baseline appendix notes were split out to:
- `docs/brought_up_archive.md`

This tracker now stays focused on phase-gate status, evidence, and closeout criteria.

---

## Bring-Up Phase Status

Reference phases: `docs/boot_and_bringup.md`

| Phase | Status | Notes |
|------|--------|-------|
| 0 - Power + Clock Stability | In progress | System boots and runs ThreadX; no dedicated checklist record yet for full phase-0 criteria in this tracker. |
| 1 - Display Validation | In progress | `thDisplay` now uses command-driven invalidate/present and renderer dirty-row policy, flushing through `LS013B7DH05` (`LCD_FlushRows`/`LCD_FlushAll`) with a deterministic renderer-backed bootstrap frame. `thPower` now also produces mode-indicator updates via `qDisplayCmd` on mode-change events. |
| 2 - Storage Validation | In progress | `thStorage` now has command-driven JEDEC probe/raw smoke plus LevelX/FileX mount/format/unmount scaffolding. Hardware evidence confirms probe, mount/format/remount, and unmount paths on target hardware. |
| 3 - Audio Validation | In progress | `thAudio` owner-thread scaffold and SAI1 DMA path are implemented and validated (`start/stop`, quiesce/resume, event-driven refill). At 16 MHz, music + single SFX voice is stable at correct speed. Phase-3 baseline closeout evidence is now captured; multi-voice overlap remains deferred until higher clock profile work. |
| 4 - Input + Sensors | In progress | `thInput` stage-1 raw EXTI capture (`qInputRaw`) and stage-2 logical action mapping/routing are validated on hardware, including quiesce suppression and resume behavior. Queue delivery is wired and validated to `qUIEvents`/`qGameEvents`, and consumer stubs (`thUI`/`thGame`) now drain both queues with verified consume counters. Knob-controlled debounce/repeat and input system-event posting (`APP_SYS_EVT_INPUT_ACTIVITY`/`APP_SYS_EVT_INPUT_MENU`) are also validated. `thSensor` runs bounded PMIC/TMAG/LIS probe/config verification on resume, tracks per-device FSM states, publishes health flags, and accepts typed targeted sensor requests via `qSensorReq`. LIS runtime poll now reads live status/raw XYZ for liveness validation. Additional scheduling tuning for `thSensor` is now recorded to prevent joystick-release starvation during high-rate UI repeat traffic. |
| 5 - RTOS Integration | In progress | Queue/event topology, thread ownership skeleton, and power handshake logic are implemented and tested. |
| 6 - STOP2 Introduction | In progress | STOP-loop policy is active with interaction-gated wake promotion; timer-only wakes now run a bounded owner-resume cadence for pet/display updates before re-entering STOP2, and HG-1 hard-gate closeout evidence is captured. |
| 7 - FLASHING Mode | In progress | USB MSC host path now enumerates and mounts on Windows (`USBSTOR` disk present, `Get-Disk` shows `MBR/Online`, `Get-Volume` shows mounted `FAT` volume). Final closeout still requires reconnect/soak stability evidence. |

---

## Plan/Tracker Alignment Notes

- Clock wording alignment:
  - `docs/boot_and_bringup.md` now treats MHz values as policy examples.
  - Current validated baseline in this tracker is `CLK_LOW` at 24 MHz with
    higher profiles validated under governor control.
- FLASHING wording alignment:
  - Phase-2 storage validation can include internal control-plane mode routing
    checks.
  - Phase-7 remains the gate for full USB MSC/FileX host-facing flashing
    workflow validation.
- Status closure alignment:
  - Keep a phase at `In progress` until all phase goals are met and evidence is
    explicitly logged, even if most functionality is already working.

---

## Temporary Measures Register (Phase-Out Tracking)

Use this table to track stop-gap or temporary behavior that must be removed or
replaced.

Lifecycle rules:
- Add an entry when temporary behavior is introduced.
- Do not delete rows; move status through `active` -> `scheduled_remove` -> `removed`.
- When status becomes `removed`, append a closure note in
  `docs/brought_up_archive.md` and keep the archive reference.

| ID | Introduced | Scope | Exit Criteria | Owner | Status | Archive Ref |
|----|------------|-------|---------------|-------|--------|-------------|
| `TMP-STOP2-ONE_SHOT` | pre-2026-03-11 | Historical one-shot STOP2 smoke behavior (now retired); row retained until archive closure note is recorded. | Final STOP policy implemented and validated: owners stay quiesced in STOP, wake-source policy defined, re-entry cadence validated, SWO checkpoints logged. | `thPower` | `scheduled_remove` | `docs/brought_up_archive.md` (`Current Snapshot`, STOP2 notes) |
| `TMP-AUDIO-SFX1-16MHZ` | pre-2026-03-11 | Audio overlap baseline held at `audio_sfx_voice_count=1` for 16 MHz profile. | Clock-uplift integration complete with overlap stress evidence and no sustained underrun/missed-service regressions at target profile. | `thAudio`, `thPower` | `active` | `docs/brought_up_archive.md` (`Current Snapshot`, audio overlap notes) |
| `TMP-UI-GAME-CONSUMER-STUBS` | pre-2026-03-11 | `thUI`/`thGame` still include stub consumer handlers for parts of phase-5 scope. | Replace remaining stubs with real owner-thread handlers and keep queue/behavior validation green. | `thUI`, `thGame` | `active` | `docs/brought_up_archive.md` (`Current Snapshot`, queue consumer notes) |
| `TMP-LIS-GAMEPLAY-DEFERRED` | pre-2026-03-11 | LIS gameplay usage intentionally deferred; low-power baseline remains default. | Gameplay LIS contract finalized and validated across mode policy and power budget constraints. | `thSensor`, `thGame` | `active` | `docs/brought_up_archive.md` (`Current Snapshot`, LIS policy notes) |
| `TMP-RET-SRAM4-NON_DURABLE` | 2026-03-15 | STOP2 fast-resume continuity currently uses SRAM4-retained RAM snapshot only (pet + topdown session). | Add flash-backed periodic checkpoint/restore path and prove clean fallback across reset/power-loss. | `thPower`, `thGame`, `thStorage` | `active` | `docs/power_management.md` (STOP2 SRAM4 retained runtime state) |

---

## Verified So Far (Debugger Evidence)

Using `>source debug.gdb` and scripted commands:

- `>ps_smoke`
  - QUIESCE path reaches expected `egPower=0x00000003` (QUIESCE_REQ + QUIESCED).
  - RESUME path reaches expected `egPower=0x0000000c` (RESUME_REQ + RUNNING).
- `2026-03-15 - retained fast-resume scaffolding` (`>ps_retained_status`, `>ps_retained_clear`)
  - SRAM4 retained blob now carries validated pet + topdown session state (`magic/version/CRC/valid_mask`).
  - `thPower` persists pet state updates; `thGame` saves on REALTIME exit and attempts restore on REALTIME entry for topdown backend.
  - debug helpers added: `>ps_retained_status` and `>ps_retained_clear`.
- `2026-03-16 - retained cross-map resume binding validation` (`>ps_retained_clear` + `>ps_topdown_m2_prepare` + STOP2 sleep/wake + `>ps_retained_status`)
  - retained game state now resumes with scene bindings (`g_game_rt_scene_map_id=1002`, `g_game_rt_scene_tileset_id=2002`) instead of always falling back to initial `pet_house` spawn.
  - retained status remains valid across run (`seq` increments, `valid_mask=0x00000003`, `crc_ok=1`, `game topdown_valid=1`).
- `2026-03-16 - STOP invalid-state recovery validation` (`>ps_freeze_dump` follow-up + runtime re-test)
  - queue-error/invalid STOP guard path now recovers through normal STATIC mode handling instead of leaving STOP-without-arming dead state.
  - post-fix run no longer reproduced the prior display/audio stall signature during the same flow.
- `>ps_stop2_prep_smoke` (debug-low-power A/B preflight)
  - with `debug_swo_enable=1`: `DBGMCU_CR=0x00000006` (`STOP/STANDBY` debug-hold bits set), while quiesce/resume remains correct (`egPower 0x00000003 -> 0x0000000c`).
  - with `debug_swo_enable=0`: `DBGMCU_CR=0x00000000` (`STOP/STANDBY` debug-hold bits clear), with identical quiesce/resume correctness and no pending-ack/timeout residue.
- `>ps_mode_verify_stop` + `>ps_stop2_status` (STOP2 execution smoke)
  - STOP2 execution path is confirmed (`entry` and `wake` increment in lockstep, `abort=0`, `last_err=0`, `last_sr=0x00000002`).
  - historical baseline: this captured the prior one-shot sleep/wake/resume phase behavior before STOP-loop policy integration.
- `2026-03-11 - HG-1 runtime regression check` (manual hardware validation in STOP)
  - no-input STOP awake interval returned to ~150-200 ms after wake-path correction (regression had expanded awake time to ~500 ms).
  - display update cadence in STOP is restored to every wake cycle (no every-second-wake miss observed).
  - pending quiesced wake edges are now physically confirmed before interaction-window promotion to reject stale/noisy input latches.
- `2026-03-11 - HG-2 detached STOP2 timebase evidence` (retained telemetry across debugger disconnect)
  - `>ps_stop2_timebase_persisted_clear` confirmed reset state (`magic=0x50535444`, `samples=0`, awake-window deltas all zero).
  - after real STOP2 run + reconnect: `samples=69`, `last_wake=69`, `awake_window_ms: hal_dt=247 tx_dt=290 abs_diff=43`, `awake_window_max_abs_diff_ms=100`.
  - result: PASS for HG-2 signal quality (bounded awake-window delta, no runaway drift signature).
- `>ps_mode_stop` + `>ps_stop2_wake_decode` (wake-cause decode evidence)
  - decode shows `sr_flags: STOPF` with `wusr_flags: none` (both latched and live samples), confirming STOP entry/exit is being observed while no wakeup-pin (`WUFx`) source is flagged in this debug path.
  - current hardware configuration does not use configured PWR wakeup pins, so `WUFx=none` is the expected baseline.
- `>ps_stop2_audio_soak` (repeated STOP2->STATIC + audio event)
  - repeated STOP2 cycles are confirmed with clean telemetry (`entry`/`wake` increment together, `abort=0`, `last_err=0`).
  - audio restart after wake is confirmed (`state=1`, `starts` increments, DMA half/full counters advance, queue remains drained).
  - debugger-induced clipping during soak is expected because `__tx_ts_wait` breakpoint halts the core; this is not a runtime audio quality regression.
- `>ps_mode_stop` then `>ps_mode_static` + `>ps_stop2_status` (decision telemetry validation)
  - while STOP is active, decision reports `REENTER_STOP` with `reenter` counter increasing.
  - after STATIC mode request is consumed, decision reports `RESUME_MODE_EXIT`, `armed=0`, and `resume` counter increments with `power=0x0000000c`.
- `>ps_mode_stop` -> `>ps_mode_static` + `>ps_stop2_status_min` (quick-loop validation)
  - repeated short cycles remain stable (`armed=0`, `abort=0`, `mode=0x00000002`, `power=0x0000000c`) with decision token `dec=4` each time.
  - observed halts in ThreadX scheduler/resume internals during debugger pauses are expected and did not indicate policy faults.
- `2026-03-13 - HG-1 owner-quiesce closeout evidence` (`>ps_mode_stop` + detached run + reconnect + `>ps_stop2_timebase_persisted`)
  - historical evidence captured the quiesced-loop variant (`armed=1`, `entry=4`, `wake=4`, `abort=0`, `mode=0x00000001`, `power=0x00000003`, `dec=2`).
  - current policy has since moved back to bounded timer-wake owner resume so deep STOP cadence can refresh pet/display state.
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
- `>ps_mode_flashing` + paused `>ps_storage_status` + blocked storage ops
  - FLASHING entry request is accepted (`queue MODE_SET FLASHING rc=0`), storage status shows `last_op=5` (`FILEX_UNMOUNT`) with `last_err=0`, and `filex: mounted=0`.
  - competing storage requests are rejected in FLASHING (`>ps_storage_probe rc=32`, `>ps_storage_smoke rc=32`; `TX_NOT_DONE` gate).
- `>ps_mode_flashing` + `>ps_sensor_health` + `>ps_audio_status`
  - FLASHING isolation behavior confirmed for sensor/audio paths: sensors are suspended (`sensor_mode_token=3`, device `state=5`) and audio remains stopped (`state=0`).
- `>ps_mode_static` + paused `>ps_storage_status` + storage probes
  - clean return from FLASHING to STATIC is confirmed (`queue MODE_SET STATIC rc=0`): sensor mode returns to STATIC/READY (`sensor_mode_token=1`, device `state=2`) and storage requests are accepted again (`>ps_storage_probe rc=0`, `>ps_storage_smoke rc=0`).
- `>ps_mode_flashing` + mode-flag check + `>ps_perf_mark` / `>ps_perf_delta`
  - mode transition consumption confirmed (`egMode` settles to `0x00000008` after queue processing).
  - FLASHING perf window confirms no active rendering/game/sensor work (`presents=0`, `frames(est)=0`, `input_game_routes=0`, `lis samples=0`, `sensor_mode_token=3`).
  - non-zero thread run counters in this window are scheduler wakeups/idle waits, not proof of render/game execution.
- `>ps_storage_pkg_manifest_txn_smoke [64]` (transactional safety smoke)
  - known-good manifest path succeeds first (`erase` -> `write_test` -> `load_default`) with active package baseline captured (`ptr/id/ver/modes`).
  - intentionally failing short load (`size=64`) returns parser failure (`last_status=5`) while `GamePackage_GetActive()` remains unchanged (`PASS: active package preserved across failed load`).
  - confirms failed manifest update does not invalidate active runtime package state in current pre-index bring-up path.
- `>ps_storage_filex_fallback_smoke` (FileX mount-failure fallback smoke)
  - mount attempt fails as expected on current FAT state (`AppStorageRunFileXMount -> 32`, `last_err=-110`, `filex m_fail+=1`, `mounted=0`).
  - active runtime package remains intact and usable across the mount failure (`active_pkg ptr/id/ver/modes unchanged`, helper reports `PASS`).
  - confirms mount-failure fallback usability for current installed/raw package path.
- `2026-03-12 - >ps_storage_install_index_smoke` (failed install isolation)
  - baseline install-index entry is established (`valid=1`, `slot/seq` populated, `w_ok` increments).
  - forced manifest parser failure (`size=64`, `last_status=5`) leaves install-index unchanged (`slot/seq/write_ok unchanged`, helper `PASS`).
  - confirms failed install path does not corrupt active installed index.
- `2026-03-12 - >ps_storage_install_index_atomic_smoke` (atomic fallback proof)
  - helper creates predecessor and newest index records, then corrupts newest CRC field in flash.
  - after probe/reload, loader selects immediate predecessor record (`slot/seq` roll back one generation), with `l_ok` increment and no load-fail increment.
  - helper reports `PASS: loader ignored corrupt newest slot and recovered immediate predecessor`, confirming atomic update fallback behavior.
- `2026-03-12 - HG-5 pre-MSC arbitration precheck` (`>ps_mode_verify_static` / `>ps_mode_verify_flashing` + storage commands)
  - in STATIC, storage ops queue/execute normally (`>ps_storage_pkg_manifest_write_test rc=0`, `>ps_storage_probe rc=0`, `>ps_storage_smoke rc=0`), and AT25 activity advances.
  - in FLASHING, MCU storage ops are rejected (`rc=32`) and no additional AT25 activity is observed during the blocked window; MCU FileX remains unmounted (`mounted=0`).
  - on return to STATIC, storage ops resume immediately (`>ps_storage_pkg_manifest_write_test rc=0`) and AT25 activity advances again.
  - host-write side remains deferred until final MSC bring-up phase.
- `2026-03-13 - P-4A install-index authority validation` (`>ps_storage_install_index_status` + repeated `>ps_storage_pkg_manifest_load_default_wait`)
  - with valid install-index state (`valid=1`, `slot=1`, `seq=10`), repeated default loads report `source=3` (`install-index` authority path).
  - install-index write counters stay flat during repeated default loads (`w_ok=0`, `w_fail=0` unchanged), confirming read-only default loads no longer churn journal writes.
- `2026-03-13 - P-4B bounded fallback + self-heal validation` (`>ps_storage_pkg_manifest_load_default_wait` with forced bad install-index manifest size)
  - baseline load uses install-index authority (`source=3`, index valid at `seq=12`).
  - after forcing an invalid index manifest size (`g_storage_install_index_manifest_size=64`), default load still succeeds via fixed-slot fallback (`source=2`), while install-index commit advances (`w_ok: 1 -> 2`, `seq: 12 -> 13`).
  - next default load returns to install-index authority (`source=3`) with stable index counters (`w_ok=2`, `seq=13`), confirming one-shot recovery and healed authority path.
- `2026-03-13 - P-4C manifest v4 content-ref contract validation` (`>ps_storage_pkg_manifest_write_test_wait` + `>ps_storage_pkg_manifest_load_default_wait` + `>ps_storage_pkg_manifest_status`)
  - write-test emits and loads a v4 manifest payload successfully (`size=268`, `last_status=0`, `source=2` on direct write-test load path).
  - default-load path remains install-index authoritative (`source=3`, `last_status=0`) after the v4 write-test payload.
  - active package status now surfaces explicit mode content refs (`mode0_refs: map_id=1001 tileset_id=2001 music=3001 sfx_interact=3002 sfx_confirm=3003 sfx_error=3004`) with blob addresses unchanged (`mode0_blob: map=0x00300000/0 tileset=0x00301000/0`).
  - install-index remains healthy on the same run (`valid=1`, `seq=16`, `manifest=0x00181000/268`, `w_ok=1`, `w_fail=0`).
- `2026-03-13 - G-1 runtime manifest-ref bind path validation` (`>p GamePackage_RequestRuntimeModeById(1)` + `>ps_mode_verify_realtime` + `>ps_rt_manifest_refs` + `>ps_audio_status`)
  - mode request is accepted before REALTIME entry (`GamePackage_RequestRuntimeModeById(1) -> 0`) and realtime runtime config becomes active (`scene refs: map_id=1001 tileset_id=2001`, `audio refs: music=3001 interact=3002 confirm=3003 error=3004`).
  - with current placeholder audio IDs (`3001..3004`) not yet catalog-resolvable on target, runtime binds remain empty (`audio bound: music=0 interact=0 confirm=0 error=0 started=0`) and fallback/no-op behavior remains bounded.
  - audio service remains healthy under realtime interaction load (`starts/stops` advance, `underrun=0`, `last_err=0`).
  - final `ps_mode_verify_static` invocation aborted in GDB function-evaluation context; no firmware error counters or runtime fault signature observed in this sample.
  - follow-up clean closure check with `>ps_mode_static` confirms expected teardown state (`ps_rt_manifest_refs`: no active runtime config; `ps_audio_status`: `state=0`, `underrun=0`, `last_err=0`).
- `2026-03-13 - G-2A manifest-audio fallback bridge validation` (`>p GamePackage_RequestRuntimeModeById(1)` + `>ps_mode_verify_realtime` + `>ps_rt_manifest_refs` + `>ps_audio_status`)
  - placeholder manifest audio refs now resolve through bounded fallback mapping when direct catalog IDs are absent (`audio bound: music=8 confirm=11 error=10`, `started=1`).
  - `interact` remains unbound (`0`) in current knob baseline because `KNOB_AUDIO_MAP_GAME_ACTION_CLIP=0`.
  - realtime audio remains healthy during this path (`state=1`, `underrun=0`, `last_err=0`) and returns cleanly to STATIC (`state=0` after mode exit).
- `2026-03-13 - G-2B manifest scene-ref bridge validation` (`>p GamePackage_RequestRuntimeModeById(1)` + `>ps_mode_verify_realtime` + `>ps_rt_manifest_refs` + `>ps_storage_status`)
  - scene ID binding is active in REALTIME (`scene bound: map=0x00300000/0 tileset=0x00301000/0`) while refs remain manifest-authoritative (`map_id=1001`, `tileset_id=2001`).
  - storage loads both map and tileset successfully from bound targets (`scene_map loaded=1 status=0 size=1128 wh=12x10`, `scene_tileset loaded=1 status=0 size=16944 tile=16x16 count=176`).
  - header-probed map sizing (`size=0` on request) eliminates prior fixed-size truncation regression (`status=5`) seen in earlier G-2B attempt.
- `2026-03-14 - G-2C manifest-audio bring-up path update` (implementation landed, target re-run pending)
  - `tools/gen_game_package_manifest.py` now supports manifest v4 mode records from JSON, including `scene_map_id` / `scene_tileset_id` and `music/sfx` asset IDs.
  - `Assets/game_package/manifest.example.json` now uses `manifest_version=4` and includes explicit mode audio reference fields.
  - storage write-test manifest has been restored to package-owned refs (`music=3001`, `interact=3002`, `confirm=3003`, `error=3004`) for external-catalog validation.
  - `ps_topdown_m1_prepare` now uses `ps_storage_audio_install_manifest_refs_wait`, which installs a bounded external audio catalog mapping those refs to known test clips before REALTIME bind checks.
- `2026-03-16 - G-2E cross-map transition validation` (`>ps_topdown_m2_prepare` + `>ps_topdown_m2_verify`)
  - dual-map install/load path validated in one run (`pet_house` at `0x00300000/0x00301000`, `town_map` at `0x00310000/0x00311000`) with successful scene map+tileset loads for both.
  - runtime transition path now requests map switch + exact spawn on map change, and manual exit trigger confirms clean transition with no stale player ghost left behind.
- `2026-03-14 - G-2D realtime audio perf-floor anti-freeze validation` (`>ps_topdown_m1_prepare` + REALTIME music+SFX overlap + `>ps_power_perf` + `>ps_audio_status`)
  - heavy overlap no longer exhibits prior soft-freeze behavior in REALTIME (user-verified under music + repeated SFX).
  - thAudio now asserts audio active/inactive intent to thPower via `qSysEvents`; thPower holds/release REALTIME perf floor accordingly.
  - debug verification now exposes `audio_boost_active` via `>ps_power_perf`.
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
- `>ps_audio_quick_play_music` + run/pause + `>ps_audio_status`
  - direct music-asset playback in REALTIME confirmed at correct speed (`state=1`, `qAudio` drains to `0`, half/full callbacks advance steadily).
- `2026-03-12 - >ps_audio_assets` + `>ps_audio_diag_realtime_start` + `>ps_audio_diag_realtime_music_end` (with `audio_sfx_voice_count=1`)
  - asset-backed realtime path is confirmed (`MUSIC_whispers_in_the_fog.wav` and mapped realtime SFX present in table).
  - music-only realtime run remained stable over the measured window (`dt_ms=7670`, `half=501`, `full=501`, `underrun=0`, `last_err=0`).
- `2026-03-12 - >ps_audio_diag_realtime_stress_start` + `>ps_audio_diag_realtime_stress_end` (with `audio_sfx_voice_count=1`)
  - bounded mixed music+SFX run remained stable (`dt_ms=2534`, `underrun=0`, `missed_half=0`, `missed_full=0`, `last_err=0`).
- `2026-03-12 - >ps_audio_phase3_stress20 20`
  - rapid transition stress completed with helper PASS (`starts=40`, `stops=81`, `underrun=0`, `err_irq=0`, `last_err=0`).
  - `missed_half/full=40` is expected in this specific test because each cycle intentionally quiesces/resumes while audio is active.
  - occasional post-helper `ps_audio_status` frame-context errors in GDB console are debugger/UI artifacts, not runtime audio faults (status already printed inside helper).
- `>ps_audio_overlap_start` + `>ps_audio_overlap_end` (long music-loop + burst overlap)
  - validated mode/perf transition behavior during overlap run:
    - start snapshot can show NORM profile (`profile_cur=0`, `sysclk=16000000`) immediately after mode request
    - end snapshot confirms TURBO active (`profile_cur=1`, `target=1`, `sysclk=160000000`)
  - validated audio service health in the same run (`underrun=0`, `missed_half=0`, `missed_full=0`) with active display concurrency (`thDisplay_runs` advancing).
  - post token-cache fix, overlap validation now confirms concurrent external playback correctness:
    - `music(start): active=1 kind=2 cursor=0`
    - `music(end): active=1 kind=2 cursor=96871 d_cursor=96871`
    - confirms music voice progression while burst SFX are active (no single-voice stall).
- `>ps_power_perf` during 3DWalk static/active transitions (dirty-render + perf-governor validation)
  - active scene snapshots show high dirty/full-flush workload and expected upshift (`dirty_rows=168`, `full_flush=1`, profile rising through mid/high tiers).
  - static scene snapshots show idle workload (`draw_ticks=0`, `dirty_rows=0`, `full_flush=0`) and bounded downshift to low profile (`profile current=0` observed).
  - confirms realtime governor now down-clocks when render work idles and re-upshifts when scene work returns.
- 3DWalk HUD timing validation after tick-domain migration
  - realtime field checks now show `SIM` and `WALL` counters tracking together while FPS remains near target (~30) under dynamic profile changes.
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
- `2026-03-12 - P-2 thGame mode-guard validation` (`>ps_mode_verify_static` / `>ps_mode_verify_realtime` + `>ps_input_status` + `>ps_audio_status`)
  - STATIC evidence: no game-routing side effects (`action game=0`, `posts game_ok=0`, `consumed game=0`, `handled game_ok=0`, `game_qerr=0`).
  - REALTIME evidence: game semantics still execute only in REALTIME (`action game=9`, `posts game_ok=9`, `consumed game=9`, `handled game_ok=7`, `game_ignored=2`).
  - audio path remains healthy during this validation (`state=0`, `underrun=0`, `last_err=0`).
- `2026-03-12 - P-2 thUI ignore-reason telemetry validation` (`>ps_input_reset` + `>ps_mode_verify_static`/`>ps_mode_verify_realtime` + `>ps_input_status`)
  - new `ui_ignored_reason` counters are live in debugger output.
  - observed STATIC sample: `map_fail=8` while `mode=0`, `stop_joy=0`, `no_action=0`, `no_event=0`.
  - observed REALTIME sample: UI reason counters remained unchanged while game routing advanced, consistent with mode-based queue ownership.
- `2026-03-12 - P-2 thUI map-fail reason split` (`>ps_input_status` after mixed press/release activity in STATIC and REALTIME)
  - STATIC sample: `ui_ignored=12` resolved as `release_rej=11` + `router_unhandled=1` (no unknown reject bucket growth).
  - REALTIME sample: UI reason counters remained flat while game counters advanced (`game_ok=4`, `game_ignored=6`), confirming `thUI` is not consuming realtime semantics.
- `2026-03-12 - P-2 thGame stale-event drain validation` (`>ps_input_reset` + STATIC activity -> `>ps_mode_verify_realtime` + `>ps_input_status`)
  - STATIC sample remained UI-only (`game=0`, `game_ok=0`, `game_ignored=0`).
  - after transition to REALTIME, game routing/handling resumed normally (`game=9`, `game_ok=7`, `game_ignored=2`) with queue healthy (`qGame=0`).
  - `game_stale_drops=0` in this run indicates no stale backlog existed to drain (guard active, no regression).
- `2026-03-13 - P-2A UI no-op cleanup validation` (`>ps_mode_verify_static`/`>ps_mode_verify_realtime` + `>ps_input_reset` + `>ps_input_status`)
  - STATIC sample now resolves benign UI ignores explicitly (`menu_noop=13`, `release_rej=19`, `map_fail=19`) with `router_unhandled=0`.
  - REALTIME sample keeps UI ignore-reason counters at zero while game routing advances (`game=10`, `game_ok=3`, `game_ignored=7`), confirming no UI-router ambiguity in gameplay mode.
- `2026-03-13 - P-2B thGame burst-drain fairness validation` (`>ps_mode_verify_realtime` + `>ps_perf_mark`/`>ps_perf_delta` + `>ps_input_status`)
  - realtime window stayed stable while routing input (`dt_ms=7174`, `frames(est)=245`, `fps~=34`, `input_game_routes=30`).
  - queue and stale-event guards remained healthy (`qGame=0`, `game_stale_drops=0`) with no starvation signature in this sample.
- `>ps_quiesce` / `>ps_resume` + `>ps_input_status` (handler gating validation)
  - While quiesced, suppression increases and action/handler counters remain stable; after resume, processing resumes without queue drops.
- Joystick runaway-latch diagnostic (STATIC menu, repeat stress):
  - when runaway was active, `g_input_button_state[6].pressed=1` (JOY_UP) and no release edge advanced (`last_edge_tick` stayed fixed), while repeat/action counters continued to climb.
  - scheduler delta probe during runaway showed sensor starvation (`thSensor run_count delta=0`) while UI continued (`thUI run_count delta>0`) over ~2 seconds.
  - this evidence drove the sensor scheduling knob adjustments recorded in Current Snapshot.
- `2026-03-22 - USB MSC host mount milestone` (Windows + debugger + packet capture)
  - Windows now enumerates both the USB interface and storage disk node (`USB\\VID_0483&PID_5710\\000000000001` and `USBSTOR\\DISK&VEN_AZURERTO&PROD_USBX_STORAGE_DEV...`) with status `OK`.
  - `Get-Disk` now reports the target as a real disk (`AzureRTO USBX storage dev`, `MBR`, `Online`, `8388608` bytes).
  - `Get-Volume` now shows a mounted FAT volume (`E:`) for the MSC device (`~8.37 MB`).
  - Runtime ownership arbitration remains intact during host access: `>ps_usb_status` shows `filex: mounted=0`, `lx_open=1`, and sustained host traffic (`msc read=145`, `write=54`, `fail=0` in capture-22 sample).
  - no `BABBLE_DETECTED` regression signature observed in the successful capture window.

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

## Phase 7 Closeout Checklist (USB MSC Host Mount + Stability)

Use this to close Phase 7 from `In progress` to `Complete`.

1. Enumerate and mount check (single plug-in)
   - Flash and run firmware.
   - Plug USB data cable to Windows host.
   - Run:
     - `Get-PnpDevice -PresentOnly | ? { $_.InstanceId -match "VID_0483&PID_5710|USBSTOR\\DISK&VEN_AZURERTO" } | ft -Auto Status,Class,FriendlyName,InstanceId`
     - `Get-Disk | ? { $_.BusType -eq "USB" -or $_.FriendlyName -match "AzureRTO|USBX storage dev" } | ft -Auto Number,FriendlyName,PartitionStyle,OperationalStatus,Size`
     - `Get-Volume | ft -Auto DriveLetter,FileSystemLabel,FileSystem,OperationalStatus,Size`
   - Pass criteria:
     - both USB + DiskDrive PnP nodes appear as `OK`
     - disk appears as `MBR`, `Online`, `8388608` bytes
     - FAT volume appears with a drive letter

2. Reconnect soak (8 cycles)
   - With target running, unplug/replug USB data cable 8 times (2-3 s between cycles).
   - After each cycle run Step-1 commands.
   - Pass criteria:
     - all 8 cycles enumerate disk + volume without manual reflash/reset

3. Host read/write smoke
   - On mounted drive (example `E:`):
     - `ni E:\\msc_write_smoke.txt -Force`
     - `'peepshow msc smoke' | sc E:\\msc_write_smoke.txt`
     - `gc E:\\msc_write_smoke.txt`
     - `Remove-Item E:\\msc_write_smoke.txt -Force`
   - Pass criteria:
     - create/write/read/delete all succeed with no host error dialog

4. Firmware-side arbitration check
   - In GDB:
     - `>ps_usb_status`
     - `>ps_usb_scsi_trace`
   - Pass criteria:
     - `usb: active=1`
     - `filex: mounted=0` and `lx_open=1` while host is mounted
     - `msc: fail=0`
     - trace shows normal SCSI flow (`0x12/0x23/0x25/0x28` etc.)

5. Documentation signoff
   - Append final evidence lines in "Verified So Far".
   - Change `Phase 7 - FLASHING Mode` to `Complete`.
   - Move `HG-6` from `IN_PROGRESS` to `PASS` once soak criteria above are met.

---

## What Is Next (By Phase Gate)

Near-term target:
1. Close phase-4 scope cleanly:
   - keep LIS in low-power baseline by default
   - leave LIS gameplay usage deferred
   - optionally add a UI/system debug toggle for LIS stream start/stop if desired for field diagnostics
2. Phase-3 uplift follow-up:
   - hold `audio_sfx_voice_count=1` as the 16 MHz baseline profile until clock uplift phase is integrated
   - keep long-run overlap verification queued for the higher-clock profile phase
3. Phase-5 expansion:
   - replace UI/Game consumer stubs with real owner-thread handlers where behavior is still placeholder
4. STOP2 execution (phase-6):
   - keep STOP evidence cadence intact after HG-1/HG-2 pass:
     - maintain timer-only wake re-entry behavior with bounded owner-resume cadence for display/pet updates
     - retain bounded-failure behavior and explicit wake evidence capture
     - add SWO markers around stop-entry-ready and post-wake decision checkpoints when practical
5. Storage authority follow-through (pre-MSC):
   - continue P-4 manifest-path retirement work:
     - keep default manifest load install-index-authoritative
     - avoid install-index write churn on read-only default loads
     - keep fixed-slot fallback only for bootstrap/recovery paths
6. Input feel polish:
   - tune repeat/debounce knobs from current stable baseline after gameplay-handler integration

After that:
- continue phase progression with explicit evidence logged per phase in this file.

---

## Pre-MSC Hard-Gate Checklist (Execution Order)

Use this as the authoritative execution order before starting USB MSC payload
bring-up work.

Status tokens:
- `OPEN`: not started
- `IN_PROGRESS`: work underway
- `BLOCKED`: waiting on dependency/evidence
- `PASS`: gate complete and evidenced

### Hard Gates (Blocking Order)

1. `HG-1` STOP2 final policy closeout  
   Depends on: none  
   Owners: `thPower` (+ ack owners)
   - Status: `PASS`
   - Checklist:
     - [x] Replace one-shot STOP behavior with final STOP-loop policy.
     - [x] Keep owners quiesced while mode remains `STOP`.
     - [x] Define and validate explicit wake-source policy/cadence.
     - [x] Validate bounded failure paths (no hangs / no infinite waits).
     - [x] Add evidence lines in "Verified So Far" for STOP entry/wake decision checkpoints.

2. `HG-2` Timebase correctness across mode/clock transitions  
   Depends on: `HG-1`  
   Owners: `thPower`
   - Status: `PASS`
   - Checklist:
     - [x] SysTick/HAL tick correctness validated after relevant transitions.
     - [x] ThreadX timer/sleep behavior remains correct post-transition.
     - [x] No observed timebase drift/regression in mode transition evidence.
     - [x] Evidence lines added in "Verified So Far" for transition timing checks.
   - Runtime evidence note (real STOP2, debugger-detached path):
     - use `>ps_stop2_timebase_persisted` after reconnect to read retained HAL-vs-ThreadX wake telemetry.

3. `HG-3` FLASHING isolation preflight (control-plane only)  
   Depends on: `HG-2`  
   Owners: `thPower`, `thStorage`
   - Status: `PASS`
   - Implementation note (2026-03-11):
     - `thPower` now treats FLASHING as explicit isolation entry: FileX unmount is queued before owner quiesce, and normal display present is skipped on FLASHING mode-set.
     - `thStorage` now filters non-control requests in FLASHING and posting-side storage APIs reject non-allowed requests while FLASHING is active.
     - Runtime evidence confirms FileX unmount path invocation, storage arbitration, render/game suppression, and clean exit-resume path.
   - Checklist:
     - [x] FileX cleanly unmounts on `FLASHING` entry.
     - [x] Rendering/audio/gameplay/sensor polling are disabled in `FLASHING`.
     - [x] No competing storage access while `FLASHING` is active.
     - [x] Clean ownership return and subsystem resume on `FLASHING` exit.
     - [x] Evidence lines added in "Verified So Far" for entry/exit isolation.

4. `HG-4` Storage install/update safety gates  
   Depends on: `HG-3`  
   Owners: `thStorage`
   - Status: `PASS`
   - Note:
     - transactional manifest-load safety is evidenced (failed manifest load preserves active runtime package).
     - installed-index failure isolation and atomic-fallback behavior are now evidenced via dedicated install-index smoke helpers.
   - Checklist:
     - [x] Failed install cannot corrupt active installed index.
     - [x] Index update order/atomicity is validated.
     - [x] Invalid blob handling remains recoverable (device stays usable).
     - [x] FileX mount failure fallback remains usable with installed blobs.
     - [x] Evidence lines added in "Verified So Far" for safety/failure paths.

5. `HG-5` FAT single-writer arbitration proof  
   Depends on: `HG-4`  
   Owners: `thPower`, `thStorage`
   - Status: `PASS`
   - Note:
     - MCU-side arbitration remains evidenced (writes allowed in STATIC, blocked in FLASHING, no blocked-window AT25 activity).
     - host-write arbitration is now evidenced in final MSC path: Windows mounts FAT volume while firmware keeps MCU FileX unmounted (`filex mounted=0`, `lx_open=1`).
   - Checklist:
     - [x] Host writes occur only while MSC path is active.
     - [x] MCU writes occur only while MSC path is inactive.
     - [x] No overlap windows with competing writers.
     - [x] Evidence lines added in "Verified So Far" for ownership arbitration.

6. `HG-6` MSC readiness signoff gate  
   Depends on: `HG-5`  
   Owners: system-level signoff
   - Status: `IN_PROGRESS`
   - Checklist:
     - [x] `HG-1` to `HG-5` are all `PASS`.
     - [x] `Bring-Up Phase Status` updated with current MSC readiness state.
     - [ ] Temporary measure register statuses updated (`active/scheduled_remove/removed`).
     - [ ] Any retired temp measures have closure notes appended to `docs/brought_up_archive.md`.

### Parallel Tracks (Non-Blocking, Run Alongside Hard Gates)

1. `P-1` Phase-3 audio closeout (`thAudio`)  
   Can start after: `HG-1`  
   Status: `PASS`

2. `P-2` Phase-5 UI/Game handler cleanup (`thUI`, `thGame`)  
   Can start after: `HG-1`  
   Status: `PASS`
   - Note:
     - UI no-op/ignore telemetry is now explicit (`menu_noop`, `release_rej`, `policy_rej`) and `router_unhandled` ambiguity is cleared in current samples.
     - thGame burst-drain fairness guard is validated with stable realtime perf and no stale-queue growth.

3. `P-3` Temporary-measure lifecycle maintenance  
   Can start: immediately  
   Status: `IN_PROGRESS`

4. `P-4` Pre-USBX manifest-path retirement planning (`thStorage`)  
   Can start after: `HG-3`  
   Status: `IN_PROGRESS`
   - Note:
     - P-4A landed: default manifest load now uses install-index authority (`source=3`) when index is valid, without repeated install-index rewrite churn.
     - P-4B landed: bounded one-shot fallback to fixed slot is validated when install-index authority is stale/invalid, and successful fallback self-heals index metadata for subsequent `source=3` loads.
     - P-4C landed: manifest v4 mode records now carry explicit content refs (map/tileset IDs + music/SFX IDs) and are validated through storage load/status telemetry.

---

## HG-1 Evidence Runbook (STOP2 Final Policy)

Use this runbook for HG-1 regression re-validation (gate already `PASS`).

Execution notes:
- Start each session with `>source debug.gdb`.
- Keep breakpoint count <= 5 and follow `docs/debugging.md` restrictions.
- After each step, append a concise evidence line under "Verified So Far".

Step 1. Baseline quiesce/resume handshake
- Commands:
  - `>ps_smoke`
- Pass criteria:
  - quiesce/resume flags transition correctly (`egPower 0x00000003 -> 0x0000000c`)
  - no stuck timeout/pending-ack residue

Step 2. Debug-low-power A/B preflight
- Commands:
  - `set debug_swo_enable=1`
  - `>ps_stop2_prep_smoke`
  - `set debug_swo_enable=0`
  - `>ps_stop2_prep_smoke`
- Pass criteria:
  - expected DBGMCU behavior in both modes
  - quiesce/resume path remains correct in both runs

Step 3. STOP execution + wake-cause evidence
- Commands:
  - `>ps_mode_stop` (allow detached STOP run; debugger disconnect is expected on some setups)
  - reconnect and run `>source debug.gdb`
  - `>ps_stop2_timebase_persisted`
  - `>ps_stop2_status`
  - `>ps_stop2_wake_decode`
- Pass criteria:
  - persisted sample shows active STOP loop with quiesced owners (`mode=STOP`, `power=QUIESCED`)
  - `entry`/`wake` counters increment in lockstep, `abort=0`, `last_err=0`
  - wake cause is captured and recorded explicitly

Step 4. STOP-loop decision behavior
- Commands:
  - `>ps_mode_stop` then run/pause, then `>ps_stop2_status`
  - `>ps_mode_static` then `>ps_stop2_status_min`
- Pass criteria:
  - while in STOP: decision/evidence shows re-enter behavior
  - on STATIC request: STOP exits cleanly and resume state is correct

Step 5. Bounded failure-path proof
- Commands:
  - `>ps_timeout`
  - `>ps_resume` (or `>ps_smoke` to confirm clean recovery)
- Pass criteria:
  - forced timeout path latches expected timeout indicators
  - system recovers to normal running state without deadlock

Step 6. Repetition soak
- Commands:
  - `>ps_stop2_audio_soak` (or equivalent repeated STOP<->STATIC cycles)
- Pass criteria:
  - no HardFault
  - no persistent timeout residue
  - stable STOP entry/wake counter progression

HG-1 evidence log template (append to "Verified So Far"):
- `DATE - HG-1 Step N - cmd(s): ... - result: PASS/FAIL - key counters/flags: ... - notes: ...`

---

## UI Router Regression Checklist

Run this after each menu tree/router change.

1. Navigation flow integrity
   - Boot to HOME, enter OPTIONS, traverse every submenu, and return to HOME with BACK.
   - Pass: no spontaneous jumps, no immediate auto-open of unrelated pages, no bounce to legacy menu.

2. Page lifecycle behavior
   - For each native page, verify enter/render/back once.
   - Pass: page opens once, back exits once, no duplicate enter/exit side effects.

3. Input repeat behavior in STATIC
   - Hold JOY_UP and JOY_DOWN in a multi-row menu.
   - Pass: cursor advances predictably with repeat cadence and does not freeze while repeats continue.

4. Audio + UI coexistence
   - Repeat navigation while UI beep audio is active.
   - Pass: audible feedback occurs per repeat and cursor updates remain visible/ordered.

5. Queue/counter sanity spot-check (`debug.gdb`)
   - `ps_input_status`, `ps_audio_status`.
   - Pass: no queue growth (`qUI/qAudio` not stuck above 0), no rising drop/error counters during normal navigation.

6. Mode safety spot-check
   - Enter REALTIME then return to STATIC and reopen OPTIONS.
   - Pass: menu routing remains correct across mode transitions.

---
