# Bring-Up Snapshot Archive

Historical validated updates and baseline appendix notes moved out of `docs/brought_up.md`
to keep the active tracker focused on phase-gate status and evidence.

Authoritative requirements still live in:
- `docs/authority.md`
- `docs/boot_and_bringup.md`

This file is historical context, not a replacement spec.

Last updated: 2026-03-22

---

## About This Split

- Split date: 2026-03-11
- Source tracker: `docs/brought_up.md`
- Purpose: preserve detailed history while keeping active phase tracking concise.

---

## Archive Update Policy

- This file is append-only by date.
- Do not rewrite prior historical entries except to fix obvious typos.
- When a temporary measure from `docs/brought_up.md` is phased out, append a
  closure note with:
  - temp ID
  - closure date
  - what replaced it
  - validation evidence reference

---

## Current Snapshot

### Recent validated updates (2026-03-22)

- USB MSC host path reached successful Windows mount milestone:
  - `USB Mass Storage Device` (`VID_0483&PID_5710`) and `USBSTOR` disk node both present as `OK`.
  - `Get-Disk` reports target disk online as `MBR` with expected size (`8388608` bytes).
  - `Get-Volume` shows mounted FAT volume with drive letter assignment.
- Arbitration model is preserved during mounted host access:
  - firmware status reports `filex: mounted=0` with `lx_open=1` while host performs read/write traffic.
  - representative successful run showed sustained MSC traffic with `read/write` counters increasing and `fail=0`.
- Packet-capture milestone:
  - successful enumeration/mount traces progressed through normal descriptor + SCSI sequence into host block I/O without prior babble signature in the success window.

### Recent validated updates (2026-03-07)

- Settings blob was extended to v3 (with v2 migration support) so runtime-persisted settings now include:
  - joystick calibration
  - joystick absolute deadzone
  - user audio gains (master/music/sfx/ui)
- REALTIME now has a public runtime tuning API overlay for active game mode config:
  - `GameRuntime_ApplyActiveModeConfigRuntimeTune(...)` applies mode-scoped live tuning without reflashing
  - `GameRuntime_ResetActiveModeConfigRuntimeTune()` restores base package config view
  - API boundary/ownership contract is documented in `docs/game_runtime_public_api.md`
- REALTIME debug tuning path is now hardened for GDB use:
  - tune requests are queued via debug symbols and applied in `thGame` thread context (no direct GDB function-call apply/reset in halted ISR/handler context)
  - dedicated safe scratch (`g_game_runtime_dbg_tune_patch`) is used for patch payload
  - current queue model is single pending slot (later queued tune overwrites earlier pending tune until `thGame` applies it)
- Joy Target page now supports live deadzone adjustment with L/R and persists via explicit save.
- Input actions are now source-distinct end-to-end (`BTN_*` and `JOY_*` are no longer merged into shared LEFT/RIGHT/etc. IDs at stage-2 or UI semantic mapping boundaries); router event mapping remains explicit per-context.
- UI router page contract now supports per-page input-source policy filtering; Joy Target uses it to accept deadzone L/R only from physical `BTN_L/BTN_R` and ignore joystick `JOY_LEFT/JOY_RIGHT`.
- Input debounce is now split by source class (`input_debounce_ticks` for buttons, `input_joy_debounce_ticks` for joystick digital edges) to improve fast joystick tap capture without reducing button debounce robustness.
- Joystick digital press threshold is now knob-driven (`sensor_joy_digital_press_permille`) so flick sensitivity can be tuned without code edits.
- Joystick input feel baseline was validated with slower polling after threshold tuning (`sensor_joy_poll_period_ms=20`, `sensor_joy_digital_press_permille=450`, joystick repeat tuned via `input_joy_repeat_*` knobs).
- UI router now supports stacked menu trees with action dispatch (`UiRouter_PushTree`/`UiRouter_PopTree` + action handler), and pet feed flow now runs as a submenu path (`feed -> choice -> feed action`) while returning to pet page context.
- REALTIME topdown input path now consumes joystick deadzone state from sensor snapshot and no longer uses debug/profile override or button-L/R movement fallbacks.
- REALTIME topdown analog profile now uses analog-only movement (no digital joystick fallback when stick is within deadzone), aligning drift behavior with cube demo.
- REALTIME topdown update now restricts per-frame `±1` quantization fallback to digital-8dir profile only; analog profile no longer force-steps from subpixel stick noise.
- REALTIME topdown controller/camera baseline now runs from package runtime config:
  - velocity-based player movement uses move speed + accel/decel params
  - camera profile (`LOCKED` / `FOLLOW_X` / `FOLLOW_XY` / `FOLLOW_DEADZONE`) uses configured deadzone, follow strength, max speed, and lookahead.
- Topdown camera lookahead now applies directionally from player velocity (forward-biases view in movement direction) and package defaults now ship with non-zero lookahead.
- STATIC options menu was reorganized into semantic buckets:
  - SYSTEM (device configuration/monitoring)
- Added a native `AUDIO LEVELS` page for runtime user gain adjustment and save.
- Base clock profile is now CubeMX-driven at 24 MHz (`CLK_LOW`) with dynamic performance profiles validated at higher operating points (64/80/120/160 MHz) under governor control in REALTIME.
- STOP-mode pet flow now has explicit dormant/select behavior:
  - dormant path re-enters STOP2 cadence
  - select path is user-interaction window with timeout back to dormancy
  - entry to STATIC/REALTIME is routed only by explicit selection.
- STOP select timeout timebase mismatch was corrected; timeout knobs now map to runtime behavior as authored.
- Button EXTI polarity/latch handling regression was fixed (stuck-pressed/repeat storms resolved in validation runs).
- Pet native page now uses sprite rendering for top/bottom icon rows + center pet sprite, with deterministic row/column navigation.
- Pet asset pipeline now has a manifest-driven baseline:
  - `Assets/pet/pet_assets.json` defines icon/state sources, row order, and pet-state sprite mapping
  - `tools/gen_pet_sprites.py` generates `ui_pet_assets_autogen.h`
  - missing PNGs fall back to generated placeholder glyphs so runtime remains functional.
- 2bpp present path and pet render sizing were corrected for sheet-driven animations:
  - Bayer present scaling now applies symmetrically on X/Y (no vertical squash)
  - Bayer dither grain is now fixed at minimal 2x2 cell size independent of zoom level
  - scaled 2bpp blits now allow arbitrary scale factors (`scale > 0`)
- Pet page UI rendering was updated for sprite-first layout:
  - center pet sprite render now accounts for effective present-scale size in centering math
  - center debug frame/outline was removed
  - top/bottom icon rows now auto-center by row item count (supports 5 icons per row cleanly)
  - selected icon highlighting now uses selection-background mask + icon-mask invert style (replaces outline box)
- Pet icon asset ingestion is now 16x16-sheet-native:
  - menu icon source is now 16x16 cells (sheet + per-icon cell mapping)
  - selection highlight background is a separate 16x16 PNG (`selection_bg`) with fallback glyph
  - active row layout now supports 5x2 menu icons through manifest row definitions.

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
- REALTIME game action ownership is now split correctly between RTOS plumbing and game logic:
  - `thGame` remains responsible for queue/timing/mode orchestration
  - gameplay control semantics are now handled in game module code via `RenderDemo*_HandleControl(...)`
  - RTOS core (`app_threadx.c`) no longer encodes game-specific control meaning
  - REALTIME control handoff now passes factual input (`source/event/tick/pressed_mask`) to game runtime instead of UI-style semantic action labels
- REALTIME runtime boundary is now explicit via a `game_runtime` adapter:
  - `thGame` now calls `GameRuntime_Init/Shutdown/HandleControl/DrawFrame`
  - `game_runtime` currently wraps `RenderDemo_*` (behavior-preserving adapter stage)
  - this prepares scenario/runtime evolution without further `thGame` loop rewrites
- REALTIME runtime adapter now includes internal scenario ops dispatch:
  - `game_runtime` routes through an active ops table (`init/shutdown/handle_control/draw_frame`)
  - current active scenario remains RenderDemo (no behavior change)
  - future scenario switching can be added in `game_runtime` without touching `thGame`
- REALTIME now has two wired scenario slots in `game_runtime`:
  - slot `0`: RenderDemo
  - slot `1`: RenderDemo3dWalk
  - compile-time selector knob `game_runtime_default_scenario` controls default active slot.
- REALTIME per-scenario control mapping is now knob-driven by physical source role:
  - source roles (`Ignore/Primary/Secondary/Back`) are configurable independently for RenderDemo and 3DWalk
  - `RenderDemo*_HandleControl(...)` now dispatches by role instead of hardcoded button semantics
  - defaults preserve existing behavior (`A=Primary`, `L/R=Secondary`, `B=Back`, joystick digital presses ignored)
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
  - logical audio manager request path now exists (`App_AudioReq_PlayEvent` / `APP_AUDIO_CMD_PLAY_EVENT`)
  - compile-time knob mapping now routes logical events to clip IDs (UI nav/confirm/cancel/denied + game action)
  - REALTIME routing now has dedicated event mappings (move/confirm/cancel/menu) so UI sound policy does not bleed into gameplay
  - `thUI` and `thGame` now post logical audio events; `thAudio` remains the sole playback owner
  - playback backend now uses decoded PCM clip assets from `Assets/audio/*.wav` (generated into `audio_assets.c`) with DMA half/full refill in `thAudio`
  - runtime audio catalog adapter stage is now in place (`AppAudioCatalogResolve`), so playback resolves through a backend-agnostic path (embedded active; external resolve now catalog-backed)
- storage-thread audio transport plumbing is now in place for external backend bring-up:
    - `APP_STORAGE_REQ_AUDIO_CATALOG_LOAD` validates/stages on-flash catalog header + entry table metadata (magic/version/entry sizing/range + optional table CRC)
    - `APP_STORAGE_REQ_AUDIO_CHUNK_READ` performs bounded raw chunk reads (`addr/len/token`) with range checks and CRC telemetry
    - `APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_EMBEDDED` now seeds a valid on-flash catalog + payload region from current embedded assets for bring-up verification (temporary source before FAT installer handoff)
    - catalog test base address is now knob-defined (`storage_audio_catalog_addr`) and exported for debugger scripts (`g_storage_audio_catalog_addr_dbg`)
  - raw flash partition map is now explicit and statically enforced for:
    - settings (`storage_settings_addr`)
    - smoke sector (`storage_smoke_addr`)
    - game package manifest slot (`storage_game_pkg_manifest_addr` / `storage_game_pkg_manifest_max_bytes`)
    - audio catalog region (`storage_audio_catalog_addr` / `storage_audio_catalog_max_bytes`)
    - installed blob region (`storage_installed_base_addr` / `storage_installed_size_bytes`)
    - FAT transport region (`storage_fat_base_addr` / `storage_fat_size_bytes`)
  - `thStorage` maintenance requests now include bounded raw erase operations for:
    - manifest slot erase
    - full raw app-managed region erase (settings/smoke/manifest/audio/installed; FAT excluded)
  - `thStorage` now includes a debug-only test manifest writer request that emits a valid minimal `MGPK` blob into the fixed manifest slot, enabling bring-up verification of `...LOAD_DEFAULT` without external installer tooling
  - `debug.gdb` storage status output now uses runtime-exported storage layout symbols (instead of compile-time `KNOB_*` symbols) so `ps_storage_status` remains usable across optimized/debug contexts
  - pre-USBX manifest ingest policy is now explicit: manifest bytes are host-programmed directly to fixed raw slot (`0x00181000` via knob), then verified by `...LOAD_DEFAULT`; no FAT install path is used in this phase
  - host-side manifest packer is now available (`tools/gen_game_package_manifest.py`) with sample input (`Assets/game_package/manifest.example.json`) for slot-program/verify bring-up
  - system-level map baseline is now in place for Tiled scene ingestion:
    - compact bounded parser module (`Core/Src/game_map.c`, `Core/Inc/game_map.h`)
    - host converter (`tools/gen_tiled_map_blob.py`) from Tiled JSON -> `TMAP` binary blob
    - runtime seam in `game_runtime` for loading parsed scene metadata (`GameRuntime_LoadSceneMapBlob`)
  - REALTIME entry now supports deterministic scene-map autoload via storage request:
    - `thGame` posts `App_StorageReq_SceneMapLoad(...)` once on REALTIME entry
    - slot is knob-defined (`game_rt_scene_map_addr`, `game_rt_scene_map_size_bytes`)
    - flash I/O still remains storage-owner only (`thStorage`).
  - REALTIME scene-tileset autoload baseline is now in place:
    - new compact tileset blob parser/runtime view (`TSET`) added (`game_tileset`)
    - `thGame` now posts `App_StorageReq_SceneTilesetLoad(...)` on REALTIME entry (knob-defined slot/size)
    - `thStorage` validates/loads tileset blob via raw flash read (no FileX in REALTIME)
    - `RenderDemo` map renderer now uses real tileset pixels by `gid` when available, with existing hatch fallback if tileset is missing/invalid.
  - external asset IDs now resolve against the staged storage catalog metadata (`source_kind=EXTERNAL`) and now decode/play through the chunk-backed ADPCM path
  - external chunk completion handling now uses a tokened completion cache (instead of single-slot last-result matching), preventing music/SFX completion races under overlap load
  - mapped SFX clips auto-stop on clip end; music clip loops continuously until explicit stop/quiesce
  - audio DMA servicing now uses an event-flag wake path (`egAudioDma`) with pending/missed counters for load diagnostics
  - at current SYSCLK 16 MHz baseline, music + single SFX voice (`audio_sfx_voice_count=1`) is hardware-validated at correct playback speed
  - overlapping multi-voice SFX at 16 MHz remains budget-limited and is intentionally deferred until higher clock-profile integration
  - overlap stress validation in REALTIME now confirms dynamic perf transition during active playback:
    - overlap script can enter REALTIME at NORM (`16 MHz`) and then converge to TURBO (`160 MHz`) shortly after mode change
    - once at TURBO, sustained overlap windows show no DMA underrun and no missed half/full service counts in validated runs
    - after external chunk-cache fix, overlap runs now confirm music cursor progression during concurrent SFX (`music d_cursor > 0`) with no underrun/missed service regressions
  - REALTIME perf-governor mode-entry policy is now hint-driven:
    - entering REALTIME no longer forces TURBO unconditionally
    - REALTIME now starts at NORM and uses existing miss/headroom streak knobs to upshift/downshift deterministically
  - REALTIME perf-hint workload model is now dirty-aware:
    - governor combines draw/present work by dirty coverage (low dirty ~= overlap cost, full flush ~= additive cost)
    - this closes the prior under-clocking case where full-frame scenes could settle below target FPS
  - 3DWalk draw loop is now change-driven instead of unconditional full-frame redraw:
    - scene motion/toggle changes trigger full redraw
    - UI-only changes redraw bars only
    - no-visual-change frames skip redraw/present work
  - 3DWalk HUD timing now uses ThreadX tick domain (`tx_time_get` / `TX_TIMER_TICKS_PER_SECOND`) for FPS/WALL counters:
    - resolves mixed timebase drift observed during dynamic clock profile changes
    - `SIM` and `WALL` counters now track coherently in realtime runs
  - `debug.gdb` now includes `ps_audio_status`, `ps_audio_start(_wait)`, `ps_audio_stop(_wait)` helpers
  - `APP_AUDIO_ASSET_NONE` macro redefinition warning is now eliminated via guarded define in generated `audio_assets.h` template/output
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
- STATIC menu routing is now `ui_router`-native end-to-end for current OPTIONS surface:
  - legacy adapter page handoff path is removed
  - native pages are active for battery stats, LIS2 status, LIS2 steps, joystick calibration, and joystick target
  - HOME -> OPTIONS now stays in router flow (no legacy bounce)
- Legacy `UI_PAGE_MENU*`, display-stress, and legacy compatibility pages (`JOY_CAL`, `JOY_TARGET`, `LIS2`, `LIS2_STEPS`, `BATT_STATS`) are removed from active bindings and build sources.
- HOME is now router-native (`UI_PAGE_HOME_NATIVE`); STATIC UI routing is router-only at runtime.
- Shared UI state/API helper plumbing is now provided by `ui_runtime_context` (`UiRuntimeContext_UpdateState/GetState/GetApi/RenderFooterHints`); legacy `ui_router` helper path is retired.
- Unused legacy UI page files have been physically removed from the repo tree (`Core/Src/ui/pages/*`, `Core/Inc/ui/pages/*`) to prevent accidental drift against active `ui_router` flow.
- Known deferred UI issue (accepted for now): some list/progress bar scrolling animations are visually inconsistent under current renderer behavior and will be reworked with the upcoming page/render pass.

### UI Architecture Baseline (2026-03-06)

- Active STATIC UI stack is:
  - `ui_router` (navigation/mode dispatch)
  - `ui_menu_*` modules (menu hierarchy data split per submenu/root)
  - `ui_page_*` modules (native page implementations split per page/feature)
  - `ui_runtime_context` (shared live state/API + footer helper rendering)
- Legacy `ui_router` and legacy `ui/pages/*` are fully removed from build and repo.
- Boot path renders native HOME via router flow (no legacy bridge page).
- Current menu behavior is stable and accepted; remaining UI work is feature/menu redesign and renderer polish, not migration cleanup.
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
- Knobs GUI advanced-visibility workflow was refined and validated:
  - advanced/basic visibility now persists correctly for schema-authored and auto-generated entries
  - Input, Joystick, LIS2, and Power tabs were cleaned up by moving low-level tuning controls to advanced view
  - default/basic view now emphasizes operator-facing controls while preserving full advanced access
- `debug_swo_enable` is now runtime-wired in boot init (`main.c` USER CODE):
  - when enabled, firmware applies debug-friendly low-power policy via
    `HAL_DBGMCU_EnableDBGStopMode()` and `HAL_DBGMCU_EnableDBGStandbyMode()`
  - when disabled, firmware applies normal low-power policy via
    `HAL_DBGMCU_DisableDBGStopMode()` and `HAL_DBGMCU_DisableDBGStandbyMode()`
  - this closes the previous gap where the knob existed in config but had no firmware effect
- STOP2 initial execution path is now integrated in `thPower` as a bounded smoke cycle:
  - `MODE_SET(STOP)` -> quiesce ACK closure -> `HAL_PWREx_EnterSTOP2Mode()` -> wake -> norm clock restore -> resume owners
  - runtime telemetry is exposed via `ps_stop2_status` (`armed/entry/wake/abort/last_err`, `last_wusr/last_sr`)
  - this is intentionally a one-shot validation path, not final production STOP-loop policy
- STOP2 re-entry cadence in STOP mode is now bounded by `KNOB_RTOS_POWER_WAIT_TICKS`:
  - `thPower` enforces a minimum awake interval between STOP2 entries using wrap-safe ThreadX tick comparison
  - STOP2 cadence state is reset on resume/abort paths to avoid stale timing gates
- STOP2 wake-decision telemetry is now explicit in `thPower`:
  - `ps_stop2_status` now reports decision reason (`ARMED/DEFER_CADENCE/REENTER_STOP/RESUME_MODE_EXIT/RESUME_REQ/ABORT`)
  - per-decision counters (`reenter/resume/defer/abort_decisions`) now expose policy behavior without single-step debugging
- STOP-exit resume grace is now knob-controlled:
  - `rtos_power_stop2_resume_grace_ticks` adds an optional bounded ThreadX-tick delay before owner resume when leaving STOP mode
  - default is `0` (no behavioral change), and `ps_stop2_status` exposes grace telemetry (`applied/last_ticks`)

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

## Package-Driven Pet Slots (Current Baseline)

- Pet page icon visibility/selection/action routing now resolves by package slot map, not fixed UI slot behavior.
- Slot resolution path:
  - `GamePackage_GetPetMenuItemBySlot(slot_index)` controls whether a slot is enabled and which action ID it maps to.
  - Disabled slots are skipped for render and navigation.
- Manifest v3 adds explicit `pet_menu_items` records:
  - slot icon id (`icon_action_id`)
  - select behavior (`select_kind`: none/feed/play/start/options)
  - status metadata (`status_kind`, `status_source_id`) for indicator-style slots
- Status-only slots (`select_kind=none`) are selectable UI entries with no launch action.
- Battery indicator rendering is now wired for pet slots:
  - `status_source_id=BATTERY` + `status_kind=LEVEL4` uses `arg0` as icon base and selects `arg0..arg0+3` by SOC quartile.
- Fallback/default package map (used when manifest omits `pet_menu_items`):
  - slot `0` -> `FEED`
  - slot `1` -> `PLAY`
  - slot `8` -> `START_GAME`
  - slot `9` -> `OPTIONS` select behavior + battery level indicator icon
- If loaded manifest has no `pet_menu_items`, runtime keeps using default fallback slot map.
