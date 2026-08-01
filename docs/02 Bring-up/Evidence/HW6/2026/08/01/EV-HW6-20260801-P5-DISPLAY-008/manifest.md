# EV-HW6-20260801-P5-DISPLAY-008

## Summary

- Test case: lifecycle-v7 LS013B7DH05 display-driver-backed owner full-frame DMA present
- Result: `PASS`
- Date/time: `2026-08-01 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `8740974f46770a9f475ea92fd46860a912ff8f24`
- Firmware state: uncommitted/untracked HW6 FW0 LS013B7DH05 display driver-wrapper integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `C62C439407DFDCE56F052DC20C07F668C7FF8E969B72DBC84915617829B06DD0`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`,
continued the target, halted without reset after completion, and sourced the
read-only consolidated report `__fw0_all_probe_prints.gdb`.

The run used `ps_dev_ls013b7dh05` from the display owner path. The wrapper
preserved the existing proven `LCD_Init` and `LCD_PresentFull_DMA` packetization
and SPI3/LPDMA transfer behavior while adding typed device state and probe
telemetry around the operation.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_display_driver_gdb.log` | `F38C70EDC794CB334EA65A6E557431A13561D339FE81863B607940A37837F693` | Exact target-memory and transition-trace report from the operator transcript |

The original operator transcript hash was
`F38C70EDC794CB334EA65A6E557431A13561D339FE81863B607940A37837F693`.

## Observations

- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Display owner completed with `command/complete/success = 676 / 1 / 1`.
- The display driver probe reported
  `display driver API/init/state/ops/last = 1 / 0x0 / 2 / 3 / 0x0`.
- The deterministic full-frame card matched `144x168 / 0x54455354 / 0x360cda71 / 1725`.
- RTC state was ready and RTC `CR=0x40880000`, preserving the enabled `LCD_1HZ` / EXTCOMIN path.
- SPI3 and LPDMA completed cleanly: SPI before/init/present `0x1 / 0x0 / 0x0`, DMA done/state/error `1 / 0x1 / 0x0`, and SPI state/error after `0x1 / 0x0`.
- Top-level lifecycle completed with `complete/success = 1 / 1`.
- Required/completed masks were `0x7F / 0x7F`; success/failure owner masks were `0x7F / 0x00`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x00`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1603`.

## Conclusion

The display-owner lifecycle path now uses a typed LS013B7DH05 device wrapper
instead of direct owner-local LCD calls, while preserving the existing proven
full-frame SPI3/LPDMA packetization. The wrapper-backed path presented the
known diagnostic card three times across the baseline workflow plus two bounded
owner-routed resume cycles.

This evidence does not close partial updates, dirty-region policy, detailed
pixel-orientation regression, display current, STOP2 static hold, LPBAM
autonomous playback, EXTCOMIN electrical probing, or display fault injection.