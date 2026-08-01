# EV-HW6-20260731-P5-OWNERS-003

## Summary

- Test case: first seven-owner retained-peripheral inactive lifecycle pass
- Result: `FAIL`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; housing and physical controls not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: superseded uncommitted/untracked HW6 FW0 lifecycle-v1 diagnostic
- Build profile: `Debug`
- FW0 ELF SHA-256: `not_recorded_for_superseded_v1`
- FW0 IOC SHA-256:
  `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Instrumentation: ST-LINK SWD/GDB; PPK2 supplied the target and observed the
  bounded `PWR_DBG` marker, but this artifact makes no current claim

## Setup

After the passing pre-kernel device and ThreadX topology probes, the operator
armed one lifecycle pass with `__fw0_owner_sm_start.gdb`, continued execution,
waited for `PWR_DBG` to return low, halted without reset, and sourced the single
consolidated GDB report.

The pass routed commands through all seven physical owners. It attempted to
place the PMIC, display, audio, TMAG3001, LIS2DUX12, flash/USB, and NINA-B112 in
their defined inactive baseline states.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `owner_lifecycle_v1_gdb.log` | `hw_log` | Exact failed lifecycle-v1 target-memory report |

## Observations

- Queue transport completed for every owner: required and completed masks were
  both `0x7F`, with all send, wait, and acknowledgement statuses successful.
- Power, display, audio, storage, flash deep-power-down, and detached USB park
  actions passed. Owner success/failure masks were `0x2B / 0x54`.
- TMAG3001 accepted its shutdown writes, then deliberately NACKed the
  post-sleep verification transaction. That transaction was itself a wake
  request, leaving I2C error `0x04` and creating a false lifecycle failure.
- LIS2DUX12 accepted all 11 shutdown writes, then NACKed the first read after
  `SLEEP.DEEP_PD=1` because its register map was inaccessible. That address
  transaction also began waking the device.
- NINA-B112 returned `ERROR` to `AT+UPWRMNG=1,10`. The command is not supported
  by NINA-B1, so the lifecycle stopped before its supported `AT&D4` path and
  asserted reset as the bounded fallback.
- No queue, scheduler, PMIC, display, audio, flash, or USB hardware fault was
  implicated by the failed result.

## Conclusion

The lifecycle-v1 failure was a valid diagnostic result. It proved the complete
owner transport path and exposed three incorrect software assumptions: two
post-terminal sensor reads and one unsupported NINA-family command. It is not
evidence that the three physical devices failed.

## Follow-Ups

Lifecycle v2 must verify readable sensor configuration before each terminal
sleep write, perform no post-terminal address transaction, and use the
NINA-B1-supported `AT&D4` plus DSR transition. The corrected rerun is preserved
as `EV-HW6-20260731-P5-OWNERS-004`.
