# Validation Plan and Acceptance Matrix

This document defines what must be proven before platform freeze and before package-facing expansion.

Evidence artifacts and tracker links should follow [[Evidence_Artifact_Convention]].

Hardware-dependent constants and limits should be tracked in [[Pending_Measured_Constants_Register]] until promoted by evidence.

---

## Validation Stages

1. Hardware bring-up validation
2. Platform service validation
3. Runtime host lifecycle validation
4. Low-power and wake validation
5. Installer/storage ownership validation
6. Long-run stability validation
7. Digital twin parity validation, only after active-target Platform hardware validation is complete

The HW6 digital twin stage is blocked until the Platform hardware backend and relevant owner behavior have measured evidence in [[HW6_Brought_Up_Tracker]].

---

## Minimum Test Matrix

Per mode (`SHELL`, `LP_GRAPH`, `LP_MODULE`, `RT_SCENE`, `INSTALLER`):
- mode entry/exit
- owner-thread health
- display/audio/input behavior
- storage permissions enforcement
- suspend/resume behavior
- fault recovery path

---

## Display And Rendering Validation Cases

- logical `168 x 144` canvas maps correctly to native `144 x 168` panel orientation
- masked 1bpp assets render with correct opacity semantics
- tone5 assets render to deterministic 1-bit coverage output
- integer-scaled tone5 assets preserve stable coverage phase
- `UI -> GAME -> BG` layer order matches [[Rendering_API_Contract]]
- display payload DMA reads correctly from the approved SRAM4 display buffer region
- waiting-visual sequence assets resolve to bounded final 1bpp visual states before target-specific compilation or playback
- continued waiting-visual motion remains unavailable unless the selected measured profile grants `display.waiting_visual_animation`

---

## Power Validation Cases

- current for each measured reactive waiting backend and realtime workload class
- deterministic operating-point sweep across valid generated clock/voltage candidates with identical firmware workload and peripheral state
- reactive selection compares complete event-to-yield charge/energy, response latency, settled-presentation latency, and return-to-wait behavior
- realtime selection compares sustained current, frame-time distribution, worst-case deadline margin, audio underruns/glitches, and representative contention
- operating-point transition latency/energy and break-even interval are measured before dynamic switching or hysteresis is adopted
- wake source correctness for all enabled sources
- STOP2 residency ratio under expected workload
- repeated stop/resume cycle test (long-run)
- current captures follow [[Power_Measurement_and_Trace_Correlation_Runbook]]
- power-correlation evidence records sync strategy, including physical marker pin if available or timed/cue fallback if not
- operation energy costs are measured for display flush, wake/resume, sensor burst, BLE activity, audio output, storage save, USB enumeration, and MSC activity before those costs feed battery estimates
- trace-enabled power runs are treated as diagnostic unless a matching trace-disabled measurement confirms final current behavior

---

## Runtime Lifecycle Cases

For each host:
1. mount success path
2. suspend during active work
3. resume after sleep wake
4. unmount and switch to another host
5. failure injection during each lifecycle phase

Tracealyzer snapshot evidence should be used where practical to verify owner-thread scheduling and runtime lifecycle ordering. Snapshot trace policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].

---

## Runtime Logic And State Validation Cases

Package-facing runtime logic validation must prove the boundary defined by [[Runtime_Logic_State_API_Contract]].

Required cases:

1. valid `LP_GRAPH` state graph validates and runs from its declared entry node.
2. missing entry state fails package validation.
3. transition to undeclared state or undeclared runtime unit fails package validation.
4. unbounded action loop fails validation in every build profile.
5. `LP_GRAPH` high-frequency polling timer fails validation.
6. bounded local-calendar schedule and catch-up policy validate through the time contract.
7. `LP_MODULE` without approved `module_type` fails validation.
8. `RT_SCENE` without frame budget, meaningful-activity rules, suspend/resume policy, or a reactive fallback route fails validation.
9. `RT_SCENE` frame overrun emits package diagnostics where profile allows and follows lifecycle policy.
10. package runtime logic cannot receive hardware owner faults as normal gameplay branches.
11. suspend/resume preserves or reconstructs package state according to declared persistence classes.
12. digital twin replay of a fixed input/time/sensor trace produces identical runtime logic state and diagnostics output.

---

## Input And Focus Validation Cases

Package-facing input validation must prove the boundary defined by [[Input_Focus_API_Contract]].

Required cases:

1. button events map to logical package actions without exposing GPIO, EXTI, or debounce internals.
2. on a target that grants `input.encoder`, encoder events map to logical delta actions without exposing timer counters or enable signals; an HW6 profile must reject required encoder input.
3. joystick events map to normalized direction/vector actions without exposing TMAG registers or raw magnetic readings.
4. `BTN_BOOT` is unavailable to package input maps.
5. Start shipping and power-intent events suppress package focus and are not delivered as normal package actions.
6. focus scope push/pop/fallback behavior is bounded and deterministic.
7. optional input bindings fall back safely when the selected target profile does not provide the source.
8. low-power wake input intent resumes through the normal lifecycle path before package action delivery.
9. digital twin input replay produces the same package-visible action stream as hardware for the same logical trace.

---

## Sensor API Validation Cases

Package-facing sensor validation must prove the boundary defined by [[Sensor_API_Contract]].

Required cases:

1. on a target that grants `sensor.light`, the ambient-light package primitive resolves to level and band without exposing ADC or GPIO; an HW6 profile must reject required light sensing unless the package has an admitted fallback.
2. step session baseline reset changes package delta only and does not reset the hardware step counter.
3. IMU event package primitive receives normalized motion/tap/shake/tilt/orientation events without exposing registers.
4. high-rate motion stream in `LP_GRAPH` fails tool validation.
5. bounded motion stream in `RT_SCENE` validates only when the target profile grants `sensor.imu_motion_stream`.
6. per-step MCU wake behavior fails validation for normal packages.
7. optional sensor feature fails validation if content fallback behavior is missing.
8. required sensor primitive fault at runtime is logged as Platform degraded capability and handled through Engine lifecycle policy.
9. digital twin sensor replay produces deterministic package-visible values/events.
10. digital twin sensor fault injection produces diagnostics and lifecycle evidence, not physical-target bring-up evidence.

---

## Audio API Validation Cases

Package-facing audio validation must prove the boundary defined by [[Audio_API_Contract]].

Required cases:

1. symbolic music cue resolves to a valid package audio asset without exposing SAI, DMA, mixer, or amp control.
2. SFX priority/preemption is deterministic within the target profile voice limit.
3. on a target that grants `audio.bbb`, BBB patterns validate frequency, duration, step count, repeat count, curve, and envelope bounds; an HW6 profile must reject required BBB output.
4. when `audio.bbb` is granted, RTTTL BBB melody source compiles to bounded BBB pattern data before package export.
5. runtime package playback does not require firmware to parse RTTTL or other melody source text.
6. package volume/mute intent is overridden by global mute without corrupting package state.
7. muted or suppressed output does not fail package validation and does not require semantic fallback.
8. audio-centric package behavior validates when assets, contexts, and runtime behavior are bounded.
9. package audio path performs no FileX/FAT, host-path, or editor-source streaming during active runtime.
10. package logic cannot subscribe to DMA callbacks, SAI completion, LPTIM interrupts, or buffer refill timing.
11. digital twin audio timeline replay is deterministic with and without host audio output.
12. digital twin audio fault injection produces diagnostics and lifecycle evidence, not physical-target bring-up evidence.

---

## Communication API Validation Cases

Package-facing communication validation must prove the boundary defined by [[Communication_API_Contract]].

Required cases:

1. communication profile validates session contexts, role intent, message schema, payload bounds, and rate limits.
2. package cannot reference BLE, NINA, UART, GAP, GATT, module commands, pins, flow control, or bonding storage.
3. communication wake intent fails HW6 target-profile validation until measured HW6 evidence grants it.
4. an `LP_GRAPH` package that depends on receiving communication messages fails HW6 validation while communication wake is blocked.
5. session-required runtime unit remains in declared admission route when no session exists.
6. optional communication runtime unit follows declared fallback/route behavior.
7. peer disconnect and session timeout are delivered as package-visible session events.
8. BLE owner fault is logged as Platform/Engine diagnostic and not exposed as UART/NINA error to package gameplay code.
9. digital twin multi-instance communication replay is deterministic for a fixed trace.
10. digital twin delayed/drop/disconnect/fault injection validates package session behavior without acting as physical BLE bring-up evidence.
11. interactive peer-wait policy validates only when the target profile grants it and the context declares a wait-expiry route.
12. keepalive or unbounded communication chatter cannot bypass peer-wait grace, reactive yield, or enabled input-lock policy.

---

## Time And Power Intent Validation Cases

Package-facing time and power-intent validation must prove the boundary defined by [[Time_And_Power_Intent_API_Contract]].

Required cases:

1. package reads valid PeepOS local calendar time without RTC register or hardware timer access.
2. package cannot set, correct, resync, or directly program RTC/calendar time.
3. first-setup or recovery flow establishes valid system time before launching calendar-dependent packages.
4. delayed and local-calendar schedules produce bounded package events.
5. long sleep resumes package with elapsed suspended/calendar time and bounded missed-event summary.
6. unbounded catch-up policy fails validation.
7. reactive unit with polling or an awake input-wait loop fails validation.
8. `RT_SCENE` without meaningful-activity rules and a reactive fallback fails validation.
9. a settled reactive transaction yields immediately without waiting for an inactivity timeout.
10. package-authored gameplay inactivity executes as a normal schedule/state transition.
11. package may disable automatic input locking.
12. enabled input lock admits only Start for unlock; that press is consumed and followed by a symbolic unlock lifecycle event.
13. preserve-state, declared-state-transition, and shell-exit lock routes validate.
14. bounded lock deferral validates; unbounded deferral fails.
15. declared gyro activity may refresh enabled lock policy only when granted; cosmetic animation may not.
16. bounded interactive peer-wait behavior cannot bypass reactive yield or enabled input lock.
17. peer-wait expiry follows the declared session idle, pause, fallback, or reactive route.
18. HW6 communication wake intent fails validation until a measured HW6 profile grants it.
19. digital twin deterministic replay produces the same time, schedule, reactive-yield, lock/unlock, wake, lifecycle, and peer-wait sequence for a fixed trace.
20. digital twin accelerated sleep simulation is not used as physical-target current, wake-latency, RTC, or sleep evidence.
21. package or authoring content that requests literal clock frequency, voltage scale, or Platform operating point fails validation.
22. reactive and realtime semantics remain stable while Platform operating-point selection changes behind the Engine boundary.

---

## Diagnostics API Validation Cases

Package-facing diagnostics validation must prove the boundary defined by [[Diagnostics_API_Contract]].

Required cases:

1. package marker emits a bounded timeline record in dev/twin profile.
2. package counter emits fixed-schema numeric value within rate limits.
3. package timing scope reports bounded package/runtime timing without hardware callbacks.
4. shipping profile preserves minimal package fault code and runtime-unit evidence.
5. verbose trace values are rejected or stripped in shipping profile unless release policy allows them.
6. package diagnostics cannot reference SWD, SWO, UART, USB, BLE, storage regions, hardware registers, RTOS objects, raw pointers, memory dumps, or filesystem paths.
7. package fault routes through Engine lifecycle policy.
8. Platform fault remains Platform diagnostic and is not exposed as normal package diagnostic API.
9. digital twin replay produces deterministic diagnostics for a fixed trace.
10. exported diagnostic summary copies bounded package records without exposing protected storage directly.

---

## Installer Cases

- VBUS-only charger/power-bank attach shows charging/external-power behavior without MSC prompt or storage handoff
- USB protocol activity/enumeration gates the MSC installer/export offer
- local to installer handoff
- USB export with no competing storage users
- MSC personality exposes no CDC developer control in v1
- CDC developer personality exposes no MSC staging/export volume in v1
- CDC package upload routes through firmware-owned staging and package validation
- CDC live-safe tuning rejects non-live-safe Platform knobs and routes accepted edits through owner requests
- package stage/validate/commit
- package install rejects Platform firmware update artifacts
- safe rollback on failure
- return to shell and remount local storage

---

## Platform Update And Security Phasing Cases

Platform update and security phasing validation must prove the boundary defined by [[Platform_Firmware_Update_and_Development_Security_Policy]].

Required cases before Platform update support:

1. Platform update artifacts are distinguishable from `PeepPkg` packages.
2. package manager rejects Platform update artifacts as packages.
3. Platform update transfer over MSC staging, if supported, is detected only after firmware reclaims staging/export.
4. Platform update transfer over CDC, if supported, writes only to firmware-owned staging.
5. update validation rejects malformed, incompatible, or corrupted artifacts before apply.
6. failed update handling has a documented recovery route before production use.

Required cases before production security enforcement:

1. recovery/update path is proven before debug lock-down or irreversible option-byte protection is enabled.
2. dev and consumer security profiles are explicitly defined.
3. package signature enforcement, if enabled, is separate from schema, bounds, compatibility, and checksum validation.
4. production protection settings do not block the documented recovery/support path.

Required cases before watchdog release enablement:

1. reset-cause capture distinguishes watchdog reset where hardware allows.
2. watchdog refresh policy is supervisor-owned and deterministic.
3. sleep/resume tests pass with watchdog policy active.
4. interrupted save, settings, installer, package install, and Platform update scenarios preserve the last valid state.

---

## Development Tooling Validation Cases

Development tooling validation must prove the boundary defined by [[Dev_Orchestration_CLI_Contract]], [[USB_Development_Mode_Contract]], [[Live_Tuning_And_Knobs_Contract]], and [[Telemetry_And_Debug_Dashboard_Contract]].

Required cases:

1. live-tuning registry is generated from the same source as the compile-time Platform knobs schema.
2. `runtime_live_safe` Platform knob can be listed, described, set, and applied through the owning subsystem.
3. out-of-range live value is clamped or rejected according to metadata.
4. `compile_time`, `boot_applied`, and `protected_policy` Platform knobs are not exposed as normal live controls.
5. raw memory address write commands are unavailable.
6. owner apply failure preserves the previous valid value.
7. session overlay export records firmware commit, knobs hash, board revision, changed values, and tool version.
8. validation evidence records active tuning overlay and USB personality when live tuning is used.
9. digital twin replay records and reapplies the active tuning overlay for deterministic runs.
10. dashboard decoder accepts a capture manifest and telemetry stream with matching schema versions.
11. dashboard clearly identifies source type: hardware live, hardware exported, Tracealyzer-linked, or digital twin.
12. state vector, owner FSM, sleep/wake, display, storage/USB, runtime, package diagnostics, and live-tuning events appear in the expected panels.
13. dashboard controls use documented CDC/live-tuning/package-upload protocols and cannot bypass owner routing or package validation.
14. telemetry evidence records firmware commit, board revision, knobs hash, schema versions, source profile, and artifact path.
15. telemetry payloads do not expose protected storage, raw memory, HAL handles, RTOS objects, or arbitrary filesystem paths.
16. orchestration CLI package build refuses to emit installable output when validation fails.
17. orchestration CLI flash/debug/package/trace commands record active profiles, artifacts, and command line.
18. game-authoring tools can edit package content parameters but cannot list or edit `platform.knobs.*`.
19. orchestration CLI evidence command prepares tracker-ready metadata but does not mark bring-up completion automatically.

Tracealyzer snapshot validation must prove the boundary defined by [[Tracealyzer_Snapshot_Evidence_Contract]].

Required cases:

1. trace build exposes owner thread names.
2. each owner thread blocks when idle and wakes on its expected event path.
3. snapshot captures display/storage/input/sensor/audio/runtime scenarios inside the trace window.
4. sleep quiesce snapshot shows owner acknowledgements before sleep entry.
5. wake snapshot shows wake reason classification before owner resume validation completes.
6. trace-enabled evidence records trace configuration, buffer size, firmware commit, knobs hash, and artifact path.

---

## Digital Twin Parity Cases

Digital twin validation begins only after the corresponding Platform behavior is proven on the active physical target.

Required cases:

1. host twin exposes the same contract-visible state vector as the hardware backend.
2. same package runs through the same Engine lifecycle on host and hardware backends.
3. host twin simulates `SHELL`, `LP_GRAPH`, `LP_MODULE`, `RT_SCENE`, and `INSTALLER` contract behavior.
4. host twin yields settled reactive transactions and enforces source-profile cadence, waiting-visual, and optional input-lock bounds.
5. host twin simulates committed-frame hold, waiting-visual sequence, and realtime display behavior from the measured Platform profile.
6. waiting-visual animation capability appears only when measured evidence for the source target profile supports it.
7. host twin consumes Start on unlock and reproduces symbolic lock/unlock lifecycle ordering.
8. host twin provides deterministic input, sensor, communication, and fault-injection traces.
9. host twin deterministic replay produces stable state-vector and output artifacts.
10. host twin evidence is recorded separately from hardware bring-up evidence.

---

## Exit Criteria For Platform Freeze

Platform freeze can be approved only when:
1. all mandatory tests pass
2. no unresolved unknown wake causes remain
3. no cross-thread ownership violations remain
4. docs and implementation are in sync

---

## Evidence Requirements

Every test run record must include:
- hardware target, board revision/identity, assembly or rework state, and firmware commit
- test case ID and expected behavior
- pass/fail result
- artifact links (logs, captures, traces)
- current/power capture path and sync strategy, when physical power measurement was used
- issue IDs for failures

Evidence classes:

- HW6 hardware evidence: measured on an identified physical HW6 board; may satisfy HW6 bring-up and known-good requirements.
- HW5 hardware evidence: retained historical evidence measured on physical HW5 hardware; may guide regression scope but cannot satisfy an HW6 pass.
- Digital twin evidence: produced by the host runtime after hardware validation; may satisfy package, Engine, lifecycle, replay, and contract-parity requirements.

Digital twin evidence must not be used to mark physical wake, current, peripheral, storage-media, display-electrical, sensor-electrical, or BLE-module behavior known-good.
