# HW6 Brought-Up Tracker

This is the active tracker for HW6 hardware and firmware bring-up.

HW5 is retired. Its evidence is preserved in [[HW5_Brought_Up_Tracker]] and is
not proof of HW6 behavior.

## Metadata

- Status: `pre_arrival`
- Last updated: `2026-07-24`
- Hardware target: `HW6`
- PCB revision: `pending_record`
- Schematic revision: `pending_record`
- BOM revision: `pending_record`
- Assembly source/lot: `pending_record`
- Initial board IDs: `pending_arrival`
- Active firmware target: `not_created`
- Full-intent IOC: `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc`
- Full-intent IOC SHA-256: `40801363273BB8ABD0072EFC5FFFF55E6878625B687033E03A6A5259E3DF179A`
- Maintainer: `pending_record`

## Phase Status

| Phase | Status | Primary Authority | Exit Summary |
|---|---|---|---|
| 0 - intake, safe power, recovery | blocked by remaining pre-arrival gates and delivery | [[HW6_Arrival_Phase0_Checklist]] | identified unit, safe rails, recoverable SWD, evidence initialized |
| 1 - PMIC and clocks | blocked by arrival | [[HW6_Power_Rails]], [[HW6_Clock_Tree_Contract]] | rails, faults, clocks, reset causes proven |
| 2 - display and storage | blocked by Phase 1 | display/flash runbooks, [[HW6_DMA_Map]] | retained display/flash behavior proven |
| 3 - speaker audio | blocked by Phase 1 | [[Audio_Output_Bring-up_Runbook]] | speaker-only SAI/DMA/output/shutdown proven |
| 4 - input and sensors | blocked by Phase 1 | button/joystick/IMU runbooks, [[HW6_Wake_Sources]] | buttons, joystick, IMU, PMIC IRQ proven |
| 5 - RTOS owner integration | blocked by retained hardware proof | owner/state-machine contracts | queues, ownership, faults, and quiesce proven |
| 6 - STOP2, LPBAM, wake, power | blocked by owners | sleep/LPBAM/power runbooks | waiting current, wake/resume, LPBAM, operating points proven |
| 7 - USB, BLE, NFC, installer | blocked by owners | communication/USB runbooks | transport, recovery, sessions, NFC, and power proven |
| 8 - runtime host lifecycle | blocked by Platform | Engine/runtime contracts | package lifecycle proven on HW6 |
| 9 - target profile and Platform freeze | blocked by evidence | [[HW6_Revalidation_Matrix]], target-profile contracts | all grants and limits evidence-backed |
| 10 - digital twin parity | blocked by corresponding HW6 proof | digital-twin contract | host behavior matches adopted HW6 contracts |

## Removed HW6 Capabilities

| Capability | Status | Evidence Rule |
|---|---|---|
| `input.encoder` | `not_present` | fabrication/IOC delta, not an electrical bring-up target |
| `sensor.light` | `not_present` | fabrication/IOC delta, not an electrical bring-up target |
| `audio.bbb` | `not_present` | fabrication/IOC delta, not an electrical bring-up target |

## Evidence Ledger

Use [[Evidence_Artifact_Convention]] and target-qualified evidence IDs.

| Date | Board ID | Evidence ID | Test Case | Result | Artifact | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | HW6-UNIT-NNN | EV-HW6-YYYYMMDD-PHASE-SUBSYSTEM-NNN | test ID | PASS/FAIL/PARTIAL/BLOCKED/INFO | evidence folder | concise conclusion |

Every row records:

- board ID and revision
- assembly lot/source and rework state
- firmware commit and artifact hash
- IOC revision/hash where generated configuration matters
- instruments and instrumentation state
- target/profile/knob versions where applicable

## Temporary Measures

| ID | Introduced | Scope | Exit Criteria | Owner | Status |
|---|---|---|---|---|---|
| TMP-HW6-XXX | YYYY-MM-DD | TBD | TBD | TBD | inactive |

## Open Issues

| ID | Blocking Phase | Summary | Next Action | Status |
|---|---|---|---|---|
| DOC-HW6-001 | pre-generation | `PD2 VLT_LCD` absent from IOC | resolved by frozen design: translator is hardwired enabled so EXTCOMIN always passes; verify electrically at arrival | resolved_design |
| DOC-HW6-002 | pre-generation | `PH1 PWR_DBG` behavior not documented | resolved as idle-low Platform diagnostic output to the battery connector for PPK2 logic capture; verify route/default at arrival | resolved_design |
| DOC-HW6-003 | pre-generation | fabrication revision identifiers not recorded | record schematic, PCB, BOM, assembly, and lot identifiers | open |
| GEN-HW6-001 | pre-generation | stale ADC1/TIM2 initializer names in IOC function-order list | inspect first generated output and remove invalid generator state | open |
| GEN-HW6-002 | pre-generation | no repository-contained safe HW6 arrival target | define minimal target, bounded build/flash artifact, and recovery path | open |
| CLK-HW6-001 | pre-generation | IOC reports 16 kHz SAI result with `-91.66%` serialized error warning | audit generated SAI/PLL2 registers before speaker validation | open |

## Rule

No phase or capability is known-good until measured on an identified HW6 board
and linked here.
