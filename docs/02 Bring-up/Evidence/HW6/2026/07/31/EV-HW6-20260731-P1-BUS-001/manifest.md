# EV-HW6-20260731-P1-BUS-001

## Summary

- Test case: consolidated, non-destructive communication and identity probe for
  the populated HW6 external devices
- Result: `PASS`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: naked PCB; display, speaker, and housing not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted FW0 peripheral-probe diagnostic derived from the
  base commit
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `AAEE68FA6C019C6A9B3137F8B588E73B7094DDFA045348FB1ACF0CFBB5857B16`
- FW0 IOC SHA-256:
  `BE3528A142EECCB9F92E35FBAF23D3D876B1375ED34E9D1FB80FBF50C89740D0`
- Instrumentation: ST-LINK SWD/GDB; PPK2 used as the board source; no electrical
  timing or current claim is made by this artifact

## Setup

The HW6 FW0 image initialized the generated peripheral handles and ran a bounded
probe before remaining in its debugger-visible heartbeat loop. The result was
read using the single consolidated `__fw0_all_probe_prints.gdb` report.

The required communication mask covered ADP5360, LIS2DUX12, TMAG3001,
AT25SL128A, and NINA-B112. Display, audio, and USB physical-path tests were
explicitly skipped because the corresponding external hardware or host test was
not present in this naked-board setup.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `peripheral_probe_gdb.log` | `hw_log` | Exact consolidated target-memory report captured through GDB |

## Observations

- Required and attempted masks were both `0x1F`.
- Pass, failure, and skipped masks were `0x1F`, `0x00`, and `0xE0`.
- I2C3 remained ready with no residual error before and after the probe.
- ADP5360 address `0x46` returned identity `0x10`, fault `0x00`, and PGOOD
  `0x07`.
- LIS2DUX12 acknowledged at address `0x18` and returned WHO_AM_I `0x47`.
- LIS2DUX12 alternate address `0x19` returned the expected address NACK,
  `HAL_I2C_ERROR_AF (0x04)`.
- TMAG3001 address `0x34` returned manufacturer bytes `0x49 0x54`.
- AT25SL128A returned JEDEC ID `1F 42 18` and clear status bytes.
- NINA-B112 returned `OK` to the bounded `AT` handshake at the configured UART
  settings.
- RTC, SAI, display SPI, display LPTIM, and USB PCD handles initialized; this is
  software initialization evidence only.

## Conclusion

HW6 unit 001 passes the defined naked-board communication and identity baseline
for all five populated external devices. This is sufficient to begin RTOS owner
integration without repeating low-level identity-driver work from prior
revisions.

## Scope And Follow-Ups

This result does not validate display pixels, speaker output, USB enumeration,
joystick axes or calibration, IMU samples or interrupts, BLE pairing/data/NFC,
device low-power behavior, or current consumption. Those remain bounded
behavioral tests under their existing runbooks and can proceed alongside owner
integration as the required external hardware becomes available.
