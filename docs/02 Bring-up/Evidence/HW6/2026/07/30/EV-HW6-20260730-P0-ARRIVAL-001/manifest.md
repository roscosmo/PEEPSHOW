# EV-HW6-20260730-P0-ARRIVAL-001

## Summary

- Test case: HW6 first-unit safe power, SWD recovery, minimal FW0 boot, and
  `PWR_DBG` timing-route validation
- Result: `PARTIAL`
- Date/time: `2026-07-30 AEST`
- Maintainer: `pending_record`
- Hardware target: `HW6`
- Board revision: `pending_record`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Assembly source/lot: `pending_record`
- Hardware rework state: `pending_record`
- Firmware commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Build profile: `Debug`
- Target profile: `pending_validation`
- Platform contract revision: `not_applicable_to_fw0`
- Knobs hash/version: `not_applicable_to_fw0`
- Active tuning overlay: `none`
- Instrumentation: Nordic PPK2 source meter and D7 logic input, target 1.8 V
  logic reference, STLINK-V3MINIE, and DMM

## Setup

- Hardware: naked HW6 PCB; display, speaker, and housing not attached
- Source: PPK2 source-meter mode at `3.300 V`; configured current limit was not
  recorded
- Debug: STLINK-V3MINIE attached; its loading is part of the active-current
  measurement
- Logic: `PH1 PWR_DBG` connected to PPK2 `D7`; target 1.8 V and ground connected
  to the PPK2 logic-reference header
- Firmware: minimal CubeMX FW0 at 4 MHz; temporary 250 ms `PWR_DBG` toggle
- PPK2 capture: nominal 100 kS/s integration, 1 kS/s saved CSV, five seconds

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `20260730T033642Z_hw6-fw0-pwr-dbg-heartbeat.csv` | `hw_measurement` | current and raw D0-D7 samples |
| `20260730T033642Z_hw6-fw0-pwr-dbg-heartbeat.json` | `hw_measurement` | PPK2 setup and integrated statistics |
| `d7_cadence_analysis.json` | `hw_measurement` | derived D7 edge-cadence result from the CSV |
| `ppk2_status_after_logic.json` | `hw_log` | live source, current, and logic-monitor status |
| `swd_identity_probe.log` | `hw_log` | MCU identity, target voltage, and probe-memory read |
| `artifact_hashes.txt` | `hw_log` | IOC, generated source, and ELF SHA-256 values |

## Observations

- Hardware shutdown current was observed at approximately `10 uA`.
- After START released hardware shutdown, the initial minimal image settled at
  approximately `2.65 mA` with ST-LINK connected.
- Both the 1.8 V and 3.3 V rails were present.
- START correctly released the board from hardware shutdown.
- Normal SWD and connect-under-reset recovery both worked.
- The SWD identity read reported STM32 device ID `0x482`, revision `Rev W`, and
  target voltage `1.80 V`.
- The five-second heartbeat capture integrated `499713` raw samples and measured
  mean `888.967 uA`, minimum `12.673 uA`, maximum `10100.769 uA`, and RMS
  `1320.397 uA`. These are instrumented FW0 values, not a sleep-current result.
- D7 produced 20 edges. The 19 measured edge intervals averaged `247.455 ms`,
  with `242.224 ms` minimum and `254.448 ms` maximum.

## Conclusion

The first HW6 unit has a working hardware shutdown/release path, both primary
rails, recoverable SWD, a buildable and flashable minimal FW0, and a verified
1.8 V `PWR_DBG` route into PPK2 D7. This is sufficient to proceed with a bounded,
read-only I2C3/ADP5360 identity and status probe.

This evidence does not close Phase 0. Board/fabrication identity, photos,
unpowered electrical measurements, display-path behavior, and formal PMIC
register readback remain open.

## Follow-Ups

- record PCB, schematic, BOM, assembly-lot, and rework identifiers
- add top/bottom and board-marking photos
- complete and record the unpowered electrical gate
- record the configured PPK2 current limit and DMM identity
- replace the temporary heartbeat with a phase-specific marker or idle-low
  policy after the timing route is no longer under test
- run the bounded read-only ADP5360 probe before any PMIC configuration write
