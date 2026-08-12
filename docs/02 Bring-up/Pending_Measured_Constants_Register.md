# Pending Measured Constants Register

This document tracks constants, limits, thresholds, timings, budgets, and capability gates that must be measured or revalidated on the active HW6 target before they become authoritative Platform, Engine, target-profile, or tooling values.

A value listed here is intentionally not final.

It becomes known-good for HW6 only when target-qualified measured evidence is linked from [[HW6_Brought_Up_Tracker]] and the owning contract/profile is updated. HW5 measurements may establish a regression baseline but do not grant an HW6 value.

Related:

- [[Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[Validation_Plan]]
- [[HW6_Hardware_Documentation_Readiness]]
- [[Target_Profile_Schema_Contract]]
- [[Knobs_and_Tuning_Contract]]
- [[Memory_and_Budgeting_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Power_Validation]]
- [[HW6_Clock_Tree_Contract]]
- [[Display_and_Rendering_Contract]]

---

## Rules

- Do not hardcode pending values into shipping policy.
- Provisional values may be used for tool design only.
- Every finalized value must link to evidence.
- Every finalized value must identify the contract, Platform knob, target profile, or tool schema it updates.
- If a measured value changes a package-facing limit, update the relevant target profile.
- If a measured value affects PeepOS implementation policy, update the relevant Platform contract or Platform knob.
- If a measured value affects package authoring, update package/tool validation rules.

---

## Status Values

| Status | Meaning |
|---|---|
| `Pending` | not measured yet |
| `Provisional` | placeholder used for design/tooling only |
| `Measured` | measured but not yet promoted into authority docs |
| `Adopted` | promoted into the owning contract/profile/knob/tool rule |
| `Rejected` | measurement shows this value or capability is not viable |
| `Superseded` | replaced by a newer measurement or policy |
| `Not Applicable` | the hardware capability is absent from the active target |

---

## Promotion Rule

A pending measured constant becomes authoritative only when:

1. target-qualified evidence exists in [[HW6_Brought_Up_Tracker]]
2. evidence artifacts follow [[Evidence_Artifact_Convention]]
3. the owning contract is updated
4. the relevant target profile or Platform knob is updated where applicable
5. package/tool validation impact is reviewed
6. the register row is marked `Adopted`, `Rejected`, `Superseded`, or `Not Applicable`

---

## Register

| ID | Value | Domain | Feeds | Evidence Source | Status | Notes |
|---|---|---|---|---|---|---|
| PMC-POWER-001 | steady waiting current per physical backend and armed context | Power | [[Power_and_Sleep_Policy]], [[Target_Profile_Schema_Contract]] | [[Sleep_Wake_Integration_Bring-up_Runbook]], power measurement artifacts | Pending | execution semantic alone does not determine waiting current; record sleep backend, retained peripherals, and wake sources |
| PMC-POWER-002 | wake latency per wake source | Power/Input/Sensor | [[Power_and_Sleep_Policy]], [[Input_Focus_API_Contract]], [[Target_Profile_Schema_Contract]] | [[Sleep_Wake_Integration_Bring-up_Runbook]] | Pending | includes buttons, RTC, and retained sensor IRQs where supported on HW6 |
| PMC-POWER-003 | resume latency per physical backend and execution path | Power/Runtime | [[Runtime_Host_Contract]], [[Digital_Twin_Host_Runtime_Contract]] | sleep/wake telemetry and trace artifacts | Pending | include wake source, clock restore, reactive/realtime destination, and lifecycle path |
| PMC-POWER-004 | owner quiesce timeout values | Power/RTOS | [[Subsystem_State_Machines]], [[RTOS_Ownership_and_Queue_Topology]] | START shutdown barrier, sleep/wake telemetry, Tracealyzer/SWO evidence | Partial | FW0 START shutdown prep, pre-STOP sleep-prep, and manual STOP2 START-wake all validate the bounded owner-ACK barrier with required/send/ACK/success/failure masks `0x7e/0x7e/0x7e/0x7e/0x0` and all owner statuses `0x0`; sleep-prep validates active-LP recovery with STOP intentionally skipped, STOP2-035 validates real STOP2 entry/START wake/clock restore/power-FSM recovery, and probe API `27` validates staged active-owner resume/quiesce plus post-wake liveness for audio/input/display/sensor/comm with storage/flash intentionally excluded; final timeout values, current, repeated cycles, and fault-injection evidence remain pending |
| PMC-POWER-005 | optional automatic input-lock timeout/default/min/max | Power/Engine | [[Time_And_Power_Intent_API_Contract]], [[Target_Profile_Schema_Contract]] | integration testing and UX review | Provisional | package may disable locking but does not author the timeout; final system default and bounds remain evidence/policy gated |
| PMC-POWER-006 | reactive schedule cadence and input-response caps | Power/Display | [[Power_and_Sleep_Policy]], [[Target_Profile_Schema_Contract]] | current/display tests | Pending | separate logical scheduled-event cadence from admitted input-response latency |
| PMC-POWER-007 | baseline reactive wake/update/yield cap | Power/Display | [[Power_and_Sleep_Policy]], [[Display_and_Rendering_Contract]] | current/display sleep tests | Pending | baseline-profile wake/update/yield cost must be measured |
| PMC-POWER-008 | waiting-visual autonomous cadence | Power/Display | [[Target_Profile_Schema_Contract]], [[Digital_Twin_Host_Runtime_Contract]] | [[LPBAM_Autonomous_Display_Validation_Plan]] | Provisional | HW5 baseline approximately 250 ms / 4 Hz; repeat the representative playback and power measurement on HW6 before profile adoption |
| PMC-POWER-009 | 303040 LiPo battery profile and ADP5360 charge/fuel settings | Power/PMIC | [[PMIC_and_Power_Contract]], [[ADP5360_Power_Bring-up_Runbook]] | PPK2 traces, cell review, ADP5360 register readback, charging tests | Provisional | profile basis recorded: 3.7 V nominal, 4.20 V terminal, 450 mAh/1.665 Wh label, up to 1 C cell permission, two-wire cell with board-mounted 100 kOhm NTC, and mandatory ADP5360 protection; PMIC ceiling is 320 mA (0.711 C); six-byte no-cell/no-VBUS candidate write/readback/restore passed as `EV-HW6-20260731-P1-ADP5360-003`; FW0 boot now applies/readbacks conservative charger profile `0x02=0x81`, `0x03=0x82`, `0x04=0x29`, `0x07=0xAC`, `0x0A=0x80` in `EV-HW6-20260811-P1-CHARGER-028`; fuel prepare/VBAT reads and room-temperature real-cell charging status are target-validated; VBUS-limit promotion, JEITA substitution, protection tests, current/termination charging, PMIC_INT event behavior, and measured capacity remain open |
| PMC-POWER-010 | measured effective battery capacity / runtime basis | Power/Product | [[PMIC_and_Power_Contract]], target profile UX estimates | controlled discharge/runtime tests | Pending | use for battery indicator/runtime claims; not required before initial PMIC probe |
| PMC-POWER-015 | low battery warning threshold | Power/PMIC/UI | [[PMIC_and_Power_Contract]], Platform knobs | ADP5360 power runbook, PPK2/source-voltage evidence | Provisional | FW0 knob value `3500 mV`; runtime warning path target-validated at PMIC `3421 mV` from source `3.46 V`; final value still depends on UX/load-shed and current evidence |
| PMC-POWER-016 | critical battery controlled-shipment threshold | Power/PMIC | [[PMIC_and_Power_Contract]], Platform knobs | ADP5360 power runbook, PPK2/source-voltage evidence | Provisional | FW0 knob value `3300 mV`; runtime critical path target-validated at PMIC `3232 mV` from source `3.27 V`, with quiesce count and default-off shipment skip; START path validates the real owner-ACK quiesce barrier used by shipment prep; enabled ADP5360 `0x36 = 1` test remains separate |
| PMC-POWER-017 | post-shipment restart-allow threshold | Power/Boot/PMIC | [[PMIC_and_Power_Contract]], [[Boot_and_Fault_Supervisor_State_Machine]], Platform knobs | boot/restart tests with and without VBUS | Provisional | FW0 knob value `3600 mV`; runtime recovery from critical path target-validated when PMIC returned to `3814 mV`; boot/restart behavior with and without VBUS remains open |
| PMC-POWER-018 | critical-battery and boot-low-battery software-shipment enable gates | Power/PMIC | [[PMIC_and_Power_Contract]], Platform knobs | ADP5360 power runbook | Provisional | FW0 knobs default to `false`; disabled tests record would-ship, enabled tests are separate explicit shipment-entry validations |
| PMC-POWER-019 | battery monitor cadence | Power/PMIC | [[PMIC_and_Power_Contract]], Platform knobs | ADP5360 power runbook, PMIC snapshot/current evidence | Provisional | FW0 knob value `1000 ms`; target probes showed `period=100` ticks, valid VBAT after fuel-gauge prepare, warning/critical/recovery decisions over consecutive monitor samples, and no false shipment with gates disabled; final cadence depends on current impact and UX needs |
| PMC-POWER-011 | operation energy profiles for Platform actions | Power/Tooling | [[Power_Measurement_and_Trace_Correlation_Runbook]], [[Power_and_Sleep_Policy]], [[Target_Profile_Schema_Contract]] | PPK2 traces correlated with Tracealyzer/SWO/telemetry | Pending | cost table for wake/resume, display flush, sensor burst, BLE activity, audio output, save write, USB enumeration, MSC activity |
| PMC-POWER-012 | reactive active operating-point selection | Power/Clock/Runtime | [[Power_and_Sleep_Policy]], [[HW6_Clock_Tree_Contract]], [[Power_Validation]] | deterministic reactive workload sweep with PPK2 plus bounded timing/trace evidence | Pending | choose transaction-energy minimum subject to response-latency and correctness limits; include logic-only, render, display-program preparation, and representative owner work |
| PMC-POWER-013 | realtime sustained operating-point selection and deadline margin | Power/Clock/Runtime/Audio | [[Power_and_Sleep_Policy]], [[HW6_Clock_Tree_Contract]], [[Target_Profile_Schema_Contract]] | representative light/typical/worst-case realtime sweeps with frame/audio diagnostics and PPK2 | Pending | choose lowest-power point meeting frame, audio, sensor, display, and owner deadlines with margin; fastest point is not assumed |
| PMC-POWER-014 | operating-point transition cost and switching hysteresis | Power/Clock | [[Power_and_Sleep_Policy]], [[HW6_Clock_Tree_Contract]] | measured clock/voltage transition latency and charge/energy captures | Partial | FW0 USB `CLK_IO_HIGH` apply/base-restore correctness is target-validated by `EV-HW6-20260812-P1-CLOCKUSB-037`; `EV-HW6-20260812-P1-CLOCKSTORAGE-039` validates the named storage requester wrapper, storage-owned USB/OCTOSPI caps during MSC, release back to base, STOP2-ready readback after reclaim, and PLL2 autogate-off behavior after reclaim; `EV-HW6-20260812-P1-FLASHINIT-040` validates the OCTOSPI-only flash provisioning request and clean release back to base with PLL2 off. Transition latency, charge/energy, break-even behavior, and non-USB profile switching remain pending |
| PMC-DISPLAY-001 | LS013B7DH05 pixel polarity | Display | [[Display_and_Rendering_Contract]], renderer implementation | [[LS013B7DH05_Display_Bring-up_Runbook]] | Pending | confirm native bit meaning with pattern test |
| PMC-DISPLAY-002 | row order and line address format | Display | [[Display_and_Rendering_Contract]] | display pattern tests | Pending | validates logical/native coordinate mapping |
| PMC-DISPLAY-003 | byte order and row payload format | Display | [[Display_and_Rendering_Contract]] | display pattern tests | Pending | needed before partial update policy |
| PMC-DISPLAY-004 | dirty granularity and full-frame fallback threshold | Display/Rendering | [[Rendering_API_Contract]], [[Display_and_Rendering_Contract]] | display timing tests | Pending | package tools never control dirty regions |
| PMC-DISPLAY-005 | SRAM4 DMA reachability for display payloads | Display/Memory | [[Memory_and_Budgeting_Contract]], [[HW6_DMA_Map]] | DMA/display flush evidence | Pending | required before the HW6 DMA/LPBAM display grant |
| PMC-DISPLAY-006 | waiting-visual animation support through LPBAM | Display/Power | [[Target_Profile_Schema_Contract]], [[Digital_Twin_Host_Runtime_Contract]] | [[LPBAM_Autonomous_Display_Validation_Plan]], [[HW6_Revalidation_Matrix]] | Provisional | HW5 validated full-frame and partial-diff STOP2 playback, wake/abort/restore, and seeded handoff; repeat the bounded HW6 regression before profile adoption |
| PMC-DISPLAY-007 | compiled waiting-visual slice payload/chunk/changed-row limits | Display/Memory | [[Display_and_Rendering_Contract]], [[Memory_and_Budgeting_Contract]] | [[LPBAM_Autonomous_Display_Validation_Plan]] | Provisional | HW5 model: 16 chunks, 11,456 B payload arena, 553 conservative changed rows; verify the HW6 linker/map result before profile adoption |
| PMC-MEM-001 | SRAM4 display-DMA/autonomous arena partition | Memory/Display | [[Memory_and_Budgeting_Contract]], linker script | [[LPBAM_Autonomous_Display_Validation_Plan]], map file and SRAM4 retention/DMA evidence | Provisional | HW5 assigned all 16 KiB to the arena with TX scratch overlaying payload; reproduce the linker placement and retention/DMA proof on HW6 |
| PMC-MEM-002 | owner thread stack sizes | Memory/RTOS | [[Memory_and_Budgeting_Contract]], Platform knobs | map file and stack watermark evidence | Pending | per owner thread |
| PMC-MEM-003 | owner queue depths | Memory/RTOS | [[RTOS_Ownership_and_Queue_Topology]], Platform knobs | queue stress tests | Pending | per owner queue |
| PMC-MEM-004 | Tracealyzer snapshot buffer size | Debug/Memory | [[Tracealyzer_Snapshot_Evidence_Contract]], [[Memory_and_Budgeting_Contract]] | trace capture tests | Pending | snapshot only unless streaming becomes necessary |
| PMC-MEM-005 | dashboard telemetry ring/event budget | Debug/Telemetry | [[Telemetry_And_Debug_Dashboard_Contract]] | telemetry capture tests | Pending | profile/build gated |
| PMC-MEM-006 | package runtime RAM limit | Engine/Memory | [[Target_Profile_Schema_Contract]], [[Package_Contract]] | map/budget report | Pending | package-facing abstract limit only |
| PMC-MEM-007 | package asset/blob size limits | Assets/Storage | [[Package_Blob_Format_Contract]], [[Target_Profile_Schema_Contract]] | storage/package validation | Pending | depends on flash layout and installer policy |
| PMC-INPUT-001 | button debounce timing | Input | [[Button_Input_Contract]], Platform knobs | [[Button_Input_Bring-up_Runbook]] | Partial | A/B/L/R hardware debounce works for shell navigation; START PA4 software live-level reconciliation is knob-backed for stable sample count in HW6 FW0; next PCB should add PA4-side hardware debounce/filtering without changing ADP5360 MR timing |
| PMC-INPUT-002 | long press and repeat timing | Input/Shell | [[Input_Focus_API_Contract]], [[Shell_Settings_Calibration_Contract]] | button runbook | Partial | START shipping scaffold thresholds promoted to HW6 FW0 knobs: `1000 ms` long, current `5000 ms` prep, `9000 ms` warning, `11000 ms` imminent; release-cancel scaffold target-validated with UI/display returning HOME; clean START prep now validates owner-ACK quiesce and PMIC ship-pending persistence; normal non-START long press/repeat remains pending |
| PMC-INPUT-003 | rotary encoder sign convention and detent ratio | Input | [[Rotary_Encoder_Input_Contract]] | [[Rotary_Encoder_Bring-up_Runbook]] | Not Applicable | rotary encoder hardware is absent from HW6; retain the row only as an HW5 historical reference |
| PMC-INPUT-004 | rotary encoder settle/filter timing | Input/Power | [[Rotary_Encoder_Input_Contract]], Platform knobs | encoder runbook | Not Applicable | rotary encoder hardware is absent from HW6 |
| PMC-INPUT-005 | joystick center/deadzone/hysteresis | Input/Sensor | [[Joystick_Hall_Input_Contract]], [[Input_Focus_API_Contract]] | [[TMAG3001_Joystick_Bring-up_Runbook]] | Pending | package sees normalized vector/direction only |
| PMC-INPUT-006 | joystick wake threshold and polarity | Input/Power | [[HW6_Wake_Sources]], [[Power_and_Sleep_Policy]] | joystick + sleep runbooks | Pending | if supported by measured HW6 hardware |
| PMC-POWER-001 | START software shipment enable policy | Power/Input | [[PMIC_and_Power_Contract]], [[Button_Input_Contract]], Platform knobs | ADP5360 power runbook | Partial | `KNOB_POWER_START_SOFTWARE_SHIP_ENABLE` added with generated default `false`; default-off gate target-validated with enable/request/skip `0/0/1` and PMIC sw ship count `0`; START prep now validates owner-ACK quiesce and PMIC ship-pending persistence; when enabled, START imminent can request ADP5360 `0x36 = 1`; enabled START register-entry validation and final product timing remain pending |
| PMC-SENSOR-001 | TEMT6000 dark/room/bright ADC bands | Sensor | [[Light_Sensor_Contract]], [[Sensor_API_Contract]] | [[TEMT6000_Light_Sensor_Bring-up_Runbook]] | Not Applicable | TEMT6000 and its ADC path are absent from HW6; preliminary HW5 readings remain in [[HW5_Brought_Up_Tracker]] |
| PMC-SENSOR-002 | light sensor settle/filter timing | Sensor/Power | [[Light_Sensor_Contract]], Platform knobs | light sensor runbook | Not Applicable | light sensor hardware is absent from HW6 |
| PMC-SENSOR-003 | LIS2DUX12 lowest-power step counting mode | Sensor/Power | [[IMU_Contract]], [[Target_Profile_Schema_Contract]] | [[LIS2DUX12_IMU_Bring-up_Runbook]] | Pending | ST baseline first, optimize later |
| PMC-SENSOR-004 | IMU event thresholds | Sensor/Input | [[Sensor_API_Contract]] | IMU runbook | Pending | tap/shake/tilt/orientation behavior |
| PMC-SENSOR-005 | IMU motion stream context limits | Sensor/Runtime | [[Sensor_API_Contract]], [[Target_Profile_Schema_Contract]] | IMU + power tests | Pending | sample rate, event rate, duration, wake behavior, and runtime-class validity |
| PMC-AUDIO-001 | MAX98357A enable settle timing | Audio/Power | [[Audio_Contract]], Platform knobs | [[Audio_Output_Bring-up_Runbook]] | Pending | needed for pop-free output policy |
| PMC-AUDIO-002 | BBB safe frequency/duration bounds | Audio | [[Audio_API_Contract]], [[Audio_Contract]] | audio runbook | Not Applicable | the physical PAM/piezo path is absent from HW6; `audio.bbb` must be false in the HW6 target profile |
| PMC-AUDIO-003 | mixer voice budget | Audio/Engine | [[Audio_API_Contract]], [[Target_Profile_Schema_Contract]] | audio runtime tests | Pending | music plus SFX budget |
| PMC-AUDIO-004 | audio buffer sizes and sample rate confirmation | Audio/Memory | [[Memory_and_Budgeting_Contract]], [[Audio_Contract]] | audio DMA/playback evidence | Pending | profile/build gated |
| PMC-STORAGE-001 | AT25SL128A JEDEC/device ID and status meanings | Storage | [[Storage_and_Installer_Contract]] | [[AT25SL128A_External_Flash_Bring-up_Runbook]] | Pending | driver baseline |
| PMC-STORAGE-002 | erase/program/readback timing | Storage | [[Storage_and_Installer_Contract]], package installer policy | flash runbook | Pending | package install UX and timeout policy |
| PMC-STORAGE-003 | deep power-down and wake timing | Storage/Power | [[Storage_and_Installer_Contract]], [[Power_and_Sleep_Policy]] | flash + sleep tests | Pending | affects suspend/resume and installer behavior |
| PMC-STORAGE-004 | protected fault-log ring offset and size | Storage/Diagnostics | [[Memory_and_Budgeting_Contract]], [[Debug_and_Observability]] | flash layout pass | Pending | protected, not host-exposed |
| PMC-STORAGE-005 | save write budget | Save/Storage | [[Package_Save_Settings_API_Contract]], [[Target_Profile_Schema_Contract]] | storage wear/policy review | Pending | package-facing limit |
| PMC-USB-001 | MSC mount/reclaim timing | USB/Storage | [[Storage_and_Installer_Contract]], [[USB_MSC_Bring-up_and_Recovery_Runbook]] | USB MSC runbook | Partial | FW0 manual debug export mounted and serviced host traffic, then reclaim closed FileX/LevelX and restored the base clock profile in `EV-HW6-20260812-P1-CLOCKUSB-037`; `EV-HW6-20260812-P1-CLOCKSTORAGE-039` validates the menu/service MSC export/reclaim clock requester counters and release behavior; `EV-HW6-20260812-P1-FLASHINIT-040` validates explicit destructive staging provisioning followed by an expected empty MSC mount. Precise timing, reconnect soak, host write/read/delete smoke, rescan/install handling, and fault injection remain pending |
| PMC-USB-002 | CDC packet size/rate limits | USB/Dev Tools | [[USB_Development_Mode_Contract]], [[Dev_Orchestration_CLI_Contract]] | CDC dev-mode tests | Pending | developer personality only |
| PMC-COMM-001 | NINA reset/boot timing | Communication | [[BLE_Communication_Contract]] | [[NINA_B112_BLE_Bring-up_Runbook]] | Pending | HW5 established a preliminary reset/AT-response baseline; exact boot-ready timing/current and quiesce behavior must be measured on HW6 |
| PMC-COMM-002 | BLE UART ring buffer sizing | Communication/Memory | [[BLE_Communication_Contract]], [[Memory_and_Budgeting_Contract]] | BLE runbook | Pending | interrupt-driven v1 baseline |
| PMC-COMM-003 | BLE payload and message-rate limits | Communication/Engine | [[Communication_API_Contract]], [[Target_Profile_Schema_Contract]] | BLE/session tests | Pending | package-facing abstract limits |
| PMC-COMM-004 | BLE session timeout/reconnect behavior | Communication | [[Communication_API_Contract]] | BLE runbook | Pending | package sees session events, not NINA faults |
| PMC-COMM-005 | BLE current impact | Communication/Power | [[Power_and_Sleep_Policy]], [[Communication_API_Contract]] | power measurement artifacts | Pending | HW6 communication wake remains blocked unless HW6 evidence grants it |
| PMC-COMM-006 | interactive session peer-wait grace and refresh policy | Communication/Power/Engine | [[Communication_API_Contract]], [[Time_And_Power_Intent_API_Contract]], [[Target_Profile_Schema_Contract]] | BLE session UX and power tests | Pending | bounded wait for remote turns; must not become keepalive-driven stay-awake policy |
| PMC-TOOL-001 | SWO event rate limit | Debug/Tooling | [[Debug_and_Observability]], [[Telemetry_And_Debug_Dashboard_Contract]] | debug workflow tests | Pending | avoid destabilizing timing |
| PMC-TOOL-002 | dashboard telemetry schema/event rate | Tooling | [[Telemetry_And_Debug_Dashboard_Contract]] | dashboard capture validation | Pending | build/profile gated |
| PMC-TOOL-003 | evidence artifact folder convention adoption | Bring-up | [[Evidence_Artifact_Convention]], [[HW6_Brought_Up_Tracker]] | first HW6 evidence entry | Pending | validate the target-qualified convention with the first HW6 artifact |
| PMC-TOOL-004 | power trace correlation sync strategy | Debug/Power | [[Power_Measurement_and_Trace_Correlation_Runbook]], [[Tracealyzer_Snapshot_Evidence_Contract]] | board pin review and first correlated power capture | Pending | physical GPIO sync if a safe pin exists; otherwise timed/cue fallback with stated precision |

---

## Domain Notes

### Power And Sleep

Power constants should remain profile-gated until measured on HW6.

Do not promote STOP, wake, or current assumptions from the digital twin.

Power estimates should be derived from measured operation energy profiles, battery profile evidence, and target-profile runtime behavior. PPK2 traces and sync markers are Platform evidence only; package tools may consume resulting profile limits or estimates, not raw measurement internals.

### Display And Rendering

Display constants must distinguish:

- panel-native behavior
- renderer logical behavior
- DMA/SRAM4 requirements
- LPBAM/autonomous display support

Package tools may consume resulting target profile limits but must not control dirty rows, transfer mode, SRAM4 placement, or LPBAM descriptors.

### Memory And Budgets

Budget constants require map-file evidence or runtime watermark/stress evidence.

Development/instrumented build budgets must remain separate from release/shipping budgets.

### Input And Sensors

Measured physical behavior should update Platform normalization and calibration policy.

Packages consume logical input and normalized sensor events only.

Sensor values that affect packages should be promoted as target-profile sensor contexts, not flat sensor-wide rate limits.

### Audio

Audio constants should distinguish creative package limits from hardware safety/settle behavior.

Sound may be muted by user/system policy; audio-centric packages are still valid.

### Storage, USB, And Communication

Package-facing limits must remain abstract.

Do not expose flash offsets, FAT paths, BLE/NINA details, or USB internals to package tools.

---

## Review Cadence

Review this register:

- before each bring-up phase starts
- after each evidence entry is added
- before creating or updating a target profile
- before Platform freeze
- before digital twin implementation begins

---

## Rule

Pending measured constants are tracked here so uncertainty stays explicit.

Do not pretend an HW6 value is known until HW6 evidence proves it.
