# EV-HW6-20260801-P5-OWNERS-006

## Summary

- Test case: lifecycle-v5 two-cycle owner resume/quiesce validation with the
  official LIS2DUX12 driver path
- Result: `PASS`
- Date/time: `2026-08-01 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; USB/VBUS detached for the storage
  owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 lifecycle-v5 diagnostic
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `E704C7A95BD7AE59DA7E5CAE7BF4DADC9B0C7A0BEBAD7380EF206FACC33CF92B`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`,
continued the target, observed the expected three display-card presents and
three speaker tones, halted without reset after the workflow completed, and
sourced the read-only consolidated report `__fw0_all_probe_prints.gdb`.

The workflow first established the accepted seven-owner inactive baseline, then
ran two bounded owner-routed `inactive -> active -> inactive` cycles without
entering STOP2.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_v5_gdb.log` | `EF40CB170B87E1FF400DAC244D67E2C3BD1BED62A44E07587E234CC09A8BA962` | Exact target-memory and transition-trace report |

## Observations

- Top-level lifecycle completed with `complete/success = 1/1`,
  required/completed masks `0x7F/0x7F`, and success/failure masks `0x7F/0x00`.
- Both cycles completed with `requested/completed/success = 2/2/1`.
- Both cycles had resume and quiesce success/failure masks `0x7F/0x00`.
- Both cycles matched the expected active and inactive ten-FSM masks
  `0x3FF/0x3FF`.
- The LIS2DUX12 wake path accepted the expected first address NACK in both
  cycles: `IMU wake status/error/accepted = 1 4 1`.
- The LIS2DUX12 identity and low-rate active configuration passed in both
  cycles: `WHO_AM_I = 0x47` and `CTRL5 = 0x10`.
- The LIS2DUX12 driver baseline reported
  `driver API/init/state/ops/last = 1 / 0 / 3 / 5 / 0`, with final driver
  state `3` (`SUSPENDED`) and last status `0`.
- The LIS2DUX12 shutdown baseline passed with
  `snapshot/write/verify = 0x7FF / 0x7FF / 0x3FF`, terminal deep-power-down
  value/status/committed `0x01 / 0x0 / 1`, and no post-deep-power-down read.
- Physical observations matched the workflow: three diagnostic display
  presents and three approximately 750 ms speaker tones.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1158`.

## Conclusion

Lifecycle-v5 closes the prior lifecycle-v3 IMU wake defect for the bounded
owner-routed cycle probe. The official LIS2DUX12 driver path correctly handles
the I2C deep-power-down wake NACK, revalidates identity, configures low-rate
active mode with `CTRL5 = 0x10`, and returns to deep-power-down through the
terminal sleep write.

This evidence does not close STOP2 entry, interrupt wake, embedded step-counter
retention, active sensing calibration, current measurement, cancellation,
fault-injection, saturation, or production runtime message contracts.
