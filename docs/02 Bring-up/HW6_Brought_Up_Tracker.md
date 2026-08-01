# HW6 Brought-Up Tracker

This is the active tracker for HW6 hardware and firmware bring-up.

HW5 is retired. Its evidence is preserved in [[HW5_Brought_Up_Tracker]] and is
not proof of HW6 behavior.

## Metadata

- Status: `owner_lifecycle_v5_passed`; all seven physical owner domains reached
  inactive states and completed two bounded inactive-active-inactive cycles with
  success/failure masks `0x7F / 0x00`; lifecycle-v5 validates the LIS2DUX12
  I2C deep-power-down wake NACK, `WHO_AM_I=0x47`, and low-rate `CTRL5=0x10`;
  formal Phase 0 intake, fault policy, STOP2, active sensing, interrupts,
  current evidence, and production contracts remain open
- Last updated: `2026-08-01`
- Hardware target: `HW6`
- PCB revision: `pending_record`
- Schematic revision: `pending_record`
- BOM revision: `pending_record`
- Assembly source/lot: `pending_record`
- Initial board IDs: `HW6-UNIT-001` (provisional; formal intake identity remains open)
- Active firmware target: `firmware/peepshow_hw6_fw0`
- Safe-arrival IOC: `firmware/peepshow_hw6_fw0/peepshow_hw6_fw0.ioc`
- Original safe-arrival IOC SHA-256: `BE3528A142EECCB9F92E35FBAF23D3D876B1375ED34E9D1FB80FBF50C89740D0`
- Active FW0 IOC SHA-256: `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Active FW0 ELF SHA-256: `E704C7A95BD7AE59DA7E5CAE7BF4DADC9B0C7A0BEBAD7380EF206FACC33CF92B`
- Active FW0 baseline commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Full-intent IOC: `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc`
- Full-intent IOC SHA-256: `40801363273BB8ABD0072EFC5FFFF55E6878625B687033E03A6A5259E3DF179A`
- Maintainer: `pending_record`

## Phase Status

| Phase | Status | Primary Authority | Exit Summary |
|---|---|---|---|
| 0 - intake, safe power, recovery | in progress; safe-power/runtime subset measured, intake records remain open | [[HW6_Arrival_Phase0_Checklist]] | identified unit, safe rails, recoverable SWD, evidence initialized |
| 1 - PMIC and clocks | complete read-only ADP5360 map and guarded six-byte reversible profile transaction pass on unit 001; persistent profile, fuel-gauge sweep, NTC/JEITA behavior, VBUS policy, protection thresholds, controlled charging, IRQs, and clock characterization remain open | [[HW6_Power_Rails]], [[HW6_Clock_Tree_Contract]] | rails, faults, clocks, reset causes proven |
| 2 - display and storage | partial: AT25SL128A identity/status and RTOS-owned full-frame SPI3/LPDMA diagnostic display pass; partial updates and destructive flash behavior remain open | display/flash runbooks, [[HW6_DMA_Map]] | retained display/flash behavior proven |
| 3 - speaker audio | partial: RTOS-owned 4.096 MHz SAI, 16 kHz DMA tone, audible output, and clean `SD_MODE` shutdown pass; current, refill/underrun, and quiesce remain open | [[Audio_Output_Bring-up_Runbook]] | speaker-only SAI/DMA/output/shutdown proven |
| 4 - input and sensors | partial: LIS2DUX12 identity at `0x18`, TMAG3001 identity at `0x34`, owner-routed terminal inactive entry, and LIS2DUX12 lifecycle wake/low-rate/suspend pass; active sampling, calibration, interrupts, event wake, current, embedded step-counter retention, and buttons remain open | button/joystick/IMU runbooks, [[HW6_Wake_Sources]] | buttons, joystick, IMU, PMIC IRQ proven |
| 5 - RTOS owner integration | partial: topology, bounded waits, `WFI`, queue ownership, acknowledgements, physical power/display/audio actions, one complete seven-owner inactive lifecycle pass, and lifecycle-v5 two-cycle wake/revalidate/quiesce pass with masks `0x7F / 0x00`; saturation, injected faults, cancellation, STOP2 handoff, and production quiesce remain open | [[RTOS_Ownership_and_Queue_Topology]], [[Subsystem_State_Machines]] | queues, ownership, faults, and quiesce proven |
| 6 - STOP2, LPBAM, wake, power | staged work unblocked by the passing inactive owner baseline; STOP2 entry, owner resume, wake sources, LPBAM, current, and operating points remain open | sleep/LPBAM/power runbooks | waiting current, wake/resume, LPBAM, operating points proven |
| 7 - USB, BLE, NFC, installer | partial: NINA-B112 bounded AT handshake and owner-routed `AT&D4`/DSR STOP entry pass; detached USB shutdown passes; BLE data/NFC/power, resume, and connected USB behavior remain open | communication/USB runbooks | transport, recovery, sessions, NFC, and power proven |
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
| 2026-07-30 | HW6-UNIT-001 (provisional) | EV-HW6-20260730-P0-ARRIVAL-001 | safe power, SWD recovery, FW0 boot, and PWR_DBG timing route | PARTIAL | `docs/02 Bring-up/Evidence/HW6/2026/07/30/EV-HW6-20260730-P0-ARRIVAL-001/` | rails, safe flash/recovery, and D7-correlated 250 ms heartbeat proven; formal intake, photos, and unpowered measurements remain open |
| 2026-07-30 | HW6-UNIT-001 (provisional) | EV-HW6-20260730-P1-ADP5360-001 | read-only ADP5360 identity, regulator, fault, and PGOOD probe | PASS/PARTIAL | `docs/02 Bring-up/Evidence/HW6/2026/07/30/EV-HW6-20260730-P1-ADP5360-001/` | I2C, identity, 1.8/3.3 V targets, PGOOD, and no-fault pass; full configuration, fuel gauge, charger, IRQ, and policy remain open |
| 2026-07-30 | HW6-UNIT-001 (provisional) | EV-HW6-20260730-P1-ADP5360-002 | complete read-only ADP5360 register-map inventory | PASS/PARTIAL | `docs/02 Bring-up/Evidence/HW6/2026/07/30/EV-HW6-20260730-P1-ADP5360-002/` | 55/55 reads pass; factory 100 mAh profile, disabled fuel gauge and IRQs, charger defaults, rail status, and protection baseline captured; writes and cell policy remain open |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P1-ADP5360-003 | guarded no-cell/no-VBUS profile write, readback, and exact restoration | PASS | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P1-ADP5360-003/` | all six candidate and restored bytes matched; fault `0x00`, PGOOD `0x07`, VBUS absent; charging and behavioral validation remain open |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P1-BUS-001 | consolidated retained-device communication and identity baseline | PASS | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P1-BUS-001/` | required mask `0x1F` passed: ADP5360, LIS2DUX12 at `0x18`, TMAG3001, AT25SL128A, and NINA AT; physical display/audio/USB and behavioral tests remain open |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P5-RTOS-001 | ThreadX topology, startup queue envelopes, bounded owner idle, and scheduler `WFI` path | PASS/PARTIAL | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P5-RTOS-001/` | owner/queue/event masks `0x1FF/0x1FF/0x0F` pass with zero object or message errors; real routing, exclusivity, saturation/fault, and quiesce/resume tests remain open |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P5-OWNERS-002 | queued power/display/audio owner workflow with physical outputs and bounded acknowledgements | PASS/PARTIAL | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P5-OWNERS-002/` | serialized PMIC snapshot, full-frame display card, 16 kHz SAI DMA tone, queue routing, acknowledgements, and clean `SD_MODE`/`PWR_DBG` final states pass; saturation/fault, quiesce, partial display, and audio-current work remain open |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P5-OWNERS-003 | first seven-owner inactive lifecycle and terminal-command diagnosis | FAIL | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P5-OWNERS-003/` | all transport passed; post-terminal TMAG/LIS reads and unsupported NINA-B1 `UPWRMNG` caused three software-contract failures |
| 2026-07-31 | HW6-UNIT-001 (provisional) | EV-HW6-20260731-P5-OWNERS-004 | corrected seven-owner inactive lifecycle baseline | PASS | `docs/02 Bring-up/Evidence/HW6/2026/07/31/EV-HW6-20260731-P5-OWNERS-004/` | complete/success `1/1`, owner masks `0x7F/0x00`, no rejected transitions, terminal sensor writes committed, NINA `AT&D4` STOP path passed without reset fallback |
| 2026-08-01 | HW6-UNIT-001 (provisional) | EV-HW6-20260801-P5-OWNERS-005 | lifecycle-v3 two-cycle owner resume/quiesce diagnosis | FAIL | `docs/02 Bring-up/Evidence/HW6/2026/08/01/EV-HW6-20260801-P5-OWNERS-005/` | all queue transport and non-IMU actions passed; IMU failed because firmware treated the expected I2C deep-power-down wake NACK as a fault |
| 2026-08-01 | HW6-UNIT-001 (provisional) | EV-HW6-20260801-P5-OWNERS-006 | lifecycle-v5 official LIS2DUX12 driver wake/low-rate/suspend cycle | PASS | `docs/02 Bring-up/Evidence/HW6/2026/08/01/EV-HW6-20260801-P5-OWNERS-006/` | complete/success `1/1`, two cycles passed with masks `0x7F/0x00` and `0x3FF/0x3FF`; IMU wake NACK accepted `1 4 1`, `WHO_AM_I=0x47`, active `CTRL5=0x10`, deep-power-down recommitted |

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
| TMP-HW6-001 | 2026-07-30 | FW0 drove `PH1 PWR_DBG` as a 250 ms heartbeat for PPK2 D7 correlation | heartbeat removed; `PWR_DBG` restored idle-low except for explicit bounded probe markers | Platform | closed |
| TMP-HW6-002 | 2026-07-31 | RTOS scaffold `thPower` toggled `PWR_DBG` after each diagnostic 25-tick queue timeout | replaced by event-driven owner routing and one bounded workflow marker | Platform | closed |
| TMP-HW6-003 | 2026-07-31 | FW0 automatically ran the PMIC/display/audio owner workflow once after RTOS startup | physical diagnostics are now gated by `__fw0_owner_sm_start.gdb`; normal boot has no automatic owner-lifecycle run | Platform | closed |

## Open Issues

| ID | Blocking Phase | Summary | Next Action | Status |
|---|---|---|---|---|
| DOC-HW6-001 | pre-generation | `PD2 VLT_LCD` absent from IOC | resolved by frozen design: translator is hardwired enabled so EXTCOMIN always passes; verify electrically at arrival | resolved_design |
| DOC-HW6-002 | pre-generation | `PH1 PWR_DBG` behavior not documented | idle-low baseline and routed diagnostic toggling verified on the first unit through PPK2 D7 | verified_unit_001 |
| DOC-HW6-003 | pre-generation | fabrication revision identifiers not recorded | record schematic, PCB, BOM, assembly, and lot identifiers | open |
| GEN-HW6-001 | pre-generation | stale ADC1/TIM2 initializer names in IOC function-order list | inspect first generated output and remove invalid generator state | open |
| GEN-HW6-002 | pre-generation | no repository-contained safe HW6 arrival target | `firmware/peepshow_hw6_fw0` built, flashed, probed, and correlated with PPK2 evidence | resolved_measured |
| CLK-HW6-001 | pre-generation | IOC reports 16 kHz SAI result with `-91.66%` serialized error warning | audit generated SAI/PLL2 registers before speaker validation | open |
| P0-HW6-001 | Phase 0 | first-unit revision/lot identity, photos, and unpowered measurements are not yet recorded | complete the remaining intake fields and artifacts before closing Phase 0 | open |
| P5-HW6-001 | Phase 5 | first seven-owner run exposed destructive post-terminal sensor reads and unsupported NINA-B1 `UPWRMNG` | lifecycle v2 verified readable state before terminal writes, omitted wake-causing reads, used `AT&D4`, and passed all owners with mask `0x7F` | resolved_measured |
| P5-HW6-002 | Phase 5 | the accepted baseline proved one-way inactive entry only | lifecycle v5 passed two bounded owner-routed cycles with expected LIS2DUX12 I2C wake NACK, ACK/identity retry, `CTRL5=0x10`, and terminal deep-power-down recommit; separately inject timeout/device failures and verify recovery plus resource cleanup | resolved_measured_lifecycle_v5 |

## Rule

No phase or capability is known-good until measured on an identified HW6 board
and linked here.
