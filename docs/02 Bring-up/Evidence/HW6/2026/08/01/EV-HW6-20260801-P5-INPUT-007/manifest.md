# EV-HW6-20260801-P5-INPUT-007

## Summary

- Test case: lifecycle-v6 TMAG3001 driver-backed owner wake/configure/sleep cycle
- Result: `PASS`
- Date/time: `2026-08-01 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 TMAG3001 driver integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `D43B24747A293379044361B88AF6649CA43AFF035F93F259E3979891678FF512`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`,
continued the target, halted without reset after completion, and sourced the
read-only consolidated report `__fw0_all_probe_prints.gdb`.

The run used the HW6-native `ps_dev_tmag3001` wrapper through the I2C3 input
lease. The wrapper performed identity validation, pre-terminal configuration
readback, active continuous-mode setup, and terminal sleep writes from the
input owner path.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_tmag_driver_gdb.log` | `9A96FFD4E013258527C7C45ADC63B202767D37E4B04BDCD8B36F53865C15B65B` | Key target-memory and transition-trace excerpt from the operator transcript |

The original operator transcript hash was
`1C84856537D2966510900A34FEE83FCFBD821AD5E2FC415EE7F7A6668A39E7BE`.

## Observations

- Top-level owner lifecycle completed with `complete/success = 1 / 1`.
- Required/completed masks were `0x7F / 0x7F`; success/failure owner masks were
  `0x7F / 0x00`.
- Both bounded cycles completed with resume and quiesce success/failure masks
  `0x7F / 0x00`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- The TMAG3001 driver probe reported
  `driver API/init/state/ops/last = 1 / 0 / 3 / 5 / 0`.
- TMAG3001 identity passed with `ready/identity/match = 0x0 / 0x0 / 1`,
  `Device_ID = 0x00`, and manufacturer ID `0x49 / 0x54`.
- The readable suspended baseline passed with
  `SENSOR_CONFIG1 before/verified = 00 / 00`,
  `DEVICE_CONFIG2 before/verified/sleep = 00 / 00 / 01`,
  write/verify masks `0x7 / 0x3`, terminal sleep status `0x0`, and committed
  flag `1`.
- Both active cycles configured `SENSOR_CONFIG1 = 0x70` and
  `DEVICE_CONFIG2 = 0x02`.
- The first TMAG wake probe in each cycle returned status `5`, followed by
  retry/active/sleep status `0 / 0 / 0`; this is the expected sleeping-device
  wake behavior where the first address transaction wakes the TMAG and NACKs.
- Final I2C state/error were `0x20 / 0x0`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1479`.

## Conclusion

The input-owner lifecycle path now uses the HW6-native TMAG3001 driver wrapper
instead of raw owner-local register operations. The driver-backed path validates
identity, configures continuous XYZ active mode, and returns the device to the
terminal sleep baseline across two bounded owner-routed cycles.

This evidence does not close joystick axis mapping, signed scaling,
normalization, threshold IRQ behavior, calibration, wake-current behavior,
button integration, fault injection, or production input-event publication.
