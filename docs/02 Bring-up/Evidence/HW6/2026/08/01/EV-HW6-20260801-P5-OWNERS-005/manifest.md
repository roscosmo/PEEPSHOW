# EV-HW6-20260801-P5-OWNERS-005

## Summary

- Test case: lifecycle-v3 two-cycle owner resume/quiesce validation
- Result: `FAIL` (diagnostic failure isolated to the IMU wake path)
- Date/time: `2026-08-01 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; housing and physical controls not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 lifecycle-v3 diagnostic
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `135404BE668A2AB89F1D040FF4CD60E98FF359CC3CCDE96E38775AC90081685D`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`,
continued the target, observed the expected three display presents and three
tones, halted without reset after `PWR_DBG` returned low, and sourced the
single consolidated GDB report.

The workflow first established the accepted seven-owner inactive baseline,
then attempted two bounded owner-routed `inactive -> active -> inactive`
cycles without entering STOP2.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_v3_gdb.log` | `727115DC962D925461A6590EC184A24C66393176E400C4B4A968301C3B01C3FB` | Exact target-memory and transition-trace report |

## Observations

- The baseline still passed all seven owners: required/completed and
  success/failure masks were `0x7F/0x7F` and `0x7F/0x00`.
- Every cycle queue send, bounded wait, and acknowledgement passed.
- Cycle 0 resume passed every owner except sensor: success/failure masks were
  `0x6F/0x10`. The IMU first wake transaction returned `HAL_ERROR`, identity
  and active configuration were not reached, and the FSM moved
  `IMU_PROBE -> IMU_ERROR`.
- Cycle 0 hardware quiesce writes returned success, but the active and inactive
  state masks were both `0x3BF`, missing only the IMU FSM bit `0x040`.
- Cycle 1 inherited `IMU_ERROR`; both resume and quiesce therefore failed only
  sensor with masks `0x6F/0x10`. The final IMU state was `IMU_ERROR` rather
  than `IMU_SUSPENDED`, with three rejected transitions.
- PMIC, display, audio, TMAG3001, flash/USB, and NINA-B112 actions passed both
  cycles. The TMAG wake retry, flash identity, and NINA `AT`/STOP paths all
  returned their expected values.
- Lifecycle v3 retained only the normalized first IMU transfer status, not the
  raw I2C error code. The observed `HAL_ERROR` occurs at the transaction where
  ST documents the intentional deep-power-down wake NACK, but this artifact
  alone does not prove `HAL_I2C_ERROR_AF`.

## Root Cause

The lifecycle-v3 implementation used `EN_DEVICE_CONFIG.SOFT_PD`, which is the
LIS2DUX12 **SPI** deep-power-down exit command. For I2C, AN5909 section 3.1.1.1
requires an address transaction that NACKs and initiates power-up, a maximum
25 ms wait, then another address transaction that must ACK. Firmware treated
the first expected failure as an I2C fault before waiting.

## Conclusion

This run is valid evidence for complete two-cycle owner transport and for the
non-IMU physical lifecycle paths. It is not evidence of an IMU hardware fault
and does not close lifecycle symmetry.

## Follow-Ups

Lifecycle v4 must retain raw first-transfer status/error, require the expected
address NACK, wait 30 ms, require ACK plus `WHO_AM_I=0x47`, and complete both
active and inactive state masks. Invalid FSM transitions must also propagate
as action failures rather than being hidden by a successful hardware write.
