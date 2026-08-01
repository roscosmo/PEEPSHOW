# EV-HW6-20260731-P5-OWNERS-004

## Summary

- Test case: corrected seven-owner retained-peripheral inactive lifecycle pass
- Result: `PASS`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; housing and physical controls not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 lifecycle-v2 diagnostic derived
  from the base commit
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `C9C5F77B2328A3D6E87EE560BDB4F99947E6E76C4C729E417CDEC31E36396BC7`
- FW0 IOC SHA-256:
  `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Instrumentation: ST-LINK SWD/GDB; PPK2 supplied the target and exposed the
  bounded `PWR_DBG` marker, but this artifact makes no current claim

## Setup

The operator flashed lifecycle v2, reset once, armed one pass with
`__fw0_owner_sm_start.gdb`, continued execution, waited for the bounded pass to
finish, halted without reset in the scheduler `WFI` path, and sourced
`__fw0_all_probe_prints.gdb`.

The corrected sensor paths verified every readable register before issuing the
terminal low-power write and deliberately performed no later device-addressed
transaction. The NINA path skipped unsupported `UPWRMNG` slots, required the
NINA-B1 commands through `AT&D4`, deasserted host DSR, waited 1.1 seconds, and
then deinitialized LPUART1.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `owner_lifecycle_v2_gdb.log` | `hw_log` | Exact corrected boot, RTOS, owner, peripheral, and final-state report |

## Observations

- Lifecycle probe identity/version/phase was `0x48364653 / 2 / 0x68FF`.
- Complete/success were `1 / 1`; required/completed and success/failure owner
  masks were `0x7F / 0x7F` and `0x7F / 0x00`.
- Every owner command send, bounded acknowledgement wait, acknowledgement flag,
  and physical action status was successful. No transition was rejected.
- The total pass took 301 ThreadX ticks, or 3.01 seconds at 100 Hz. The NINA
  owner accounted for 221 ticks, including boot and documented STOP settling.
- TMAG3001 pre-terminal write/verify masks were `0x07 / 0x03`; terminal sleep
  value/status/commit were `0x01 / HAL_OK / 1`. I2C ended ready with no error.
- LIS2DUX12 snapshot/write/pre-terminal-verify masks were
  `0x7FF / 0x7FF / 0x3FF`; deep-power-down value/status/commit were
  `0x01 / HAL_OK / 1`. I2C ended ready with no error.
- AT25SL128A identity passed before deep power-down. Detached USB PCD, clock,
  and VDDUSB were all disabled successfully.
- NINA required/attempted/skipped masks were `0x79 / 0x79 / 0x06`; OK/error
  masks were `0x79 / 0x00`. NRST and host DSR ended high, UART deinit passed,
  and reset fallback was not used.
- Final FSM states matched every defined inactive baseline, including
  `JOY_SUSPENDED`, `IMU_SUSPENDED`, `FLASH_DEEP_POWER_DOWN`, and
  `BLE_SUSPENDED`.
- `PWR_DBG` recorded exactly two transitions and ended low at tick 1877.

## Conclusion

HW6 unit 001 passes the first complete owner-routed inactive lifecycle baseline
for all seven physical owner domains. This proves one-way entry into the
defined inactive peripheral states and validates the corrected terminal-command
contracts on the target.

## Follow-Ups

This artifact does not prove inactive current, STOP2, LPBAM, wake/resume,
repeated cycling, fault injection/recovery, active sensor sampling, BLE data,
or USB-host behavior. The next owner-level test is bounded
`inactive -> active -> inactive` cycling with state and resource verification
after each transition.
