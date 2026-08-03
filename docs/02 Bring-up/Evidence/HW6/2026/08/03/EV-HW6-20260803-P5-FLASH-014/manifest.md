# EV-HW6-20260803-P5-FLASH-014

## Summary

- Test case: lifecycle-v12 AT25SL128A reset-normalized raw block and storage layout validation
- Result: `PASS`
- Date/time: `2026-08-03 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: fresh board flash; display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `55035f9774bfe683ae04bbd6e12e7793f12ccc81`
- Firmware state: uncommitted HW6 FW0 AT25SL128A reset normalization, raw block validation, and storage layout validation
- Build profile: `Debug`
- FW0 ELF SHA-256: `EBE0BD03BB4D97518C71F1FF6BE8280C49B8349467FA12AEF892C46CC481EF1A`
- FW0 IOC SHA-256: `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Instrumentation: ST-LINK SWD/GDB; operator armed `__fw0_owner_sm_start.gdb`, continued the target, confirmed the physical workflow completed, halted without reset, and sourced `__fw0_all_probe_prints.gdb`

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_flash_layout_gdb.log` | `B11DE0BB230858FBA481C24167E77426001E7FCBCF1B091648144C346E62619E` | Exact target-memory and transition-trace report from the operator transcript |

## Observations

- RTOS topology and runtime completed with `init/runtime complete = 1 / 1`; owner and queue start/self-test masks were `0x1FF / 0x1FF`.
- Physical owner workflow completed with `complete/success/init = 1 / 1 / 0x0`, display acknowledgement `0x0`, and audio acknowledgement `0x0`.
- Retained-peripheral lifecycle state machine reported version `0xC`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Final storage FSM state matched expected `2 / 2`; final flash FSM state matched expected `8 / 8` with zero rejected transitions.
- AT25SL128A driver probe reported `driver API/init/state/ops/last = 1 / 0 / 3 / 75 / 0`.
- JEDEC telemetry reported `JEDEC status/ID/match = 0x0 / 1f 42 18 / 1`.
- The AT25 init path now reset-normalizes the flash with reset-enable/reset and bounded SR1 polling; this run avoided the earlier pre-RTOS `HAL_Delay()` hang and completed the owner workflow.
- Scratch range was `0x00FFF000`, length `256` bytes, within the final 4 KiB sector reserved only for bring-up scratch tests.
- Scratch erase passed with WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 664`, accepted command status `0x01`, blank-read HAL status `0x0`, and `0` blank mismatches.
- Scratch polling program passed with WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 1`, read status `0x0`, `0` mismatches, and first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`.
- Scratch DMA program/readback passed with DMA program WREN/cmd/xfer wait/polls `0x0 / 0x0 / 0 / 1`, flash wait/polls `0 / 1`, DMA read cmd/xfer wait/polls `0x0 / 0 / 27`, and `0` mismatches.
- Scratch cleanup erase passed with WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 664`; cleanup blank read returned all `FF` in the first 16 bytes with `0` mismatches.
- Raw flash block adapter reported API/init/ops/last `1 / 0 / 1 / 0` and geometry `16777216 / 4096 / 256 / 4096`.
- Raw block test reported status/index/address/length `0 / 4095 / 0x00FFF000 / 4096`.
- Raw block erase, blank read, program, verify read, and cleanup all passed: erase status/polls `0 / 664`, blank mismatch `0`, program status/pages/last polls `0 / 16 / 1`, verify mismatch `0`, and cleanup mismatch `0`.
- Storage layout validation reported API/status/count `1 / 0 / 10`, total/erase/end `16777216 / 4096 / 0x01000000`, alignment/overlap/range errors `0 / 0 / 0`, host/protected masks `0x40 / 0x3BF`, and scratch index/start/length `9 / 0x00FFF000 / 4096`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1093`.

## Conclusion

The HW6 storage-owner path now validates AT25SL128A reset-normalized operation, single-page scratch erase/program/readback through polling and DMA paths, raw 4 KiB block erase/program/readback/cleanup, and the fixed 10-region flash layout. The layout reserves only `USB_STAGING` as host-exposed and keeps the bring-up scratch sector protected from host exposure.

This evidence closes the raw block plus storage layout checkpoint before FileX/LevelX. It does not close FileX/LevelX integration, USB MSC export/reclaim, package installation, storage current/timing budgets, protected fault-log policy, or storage fault injection/recovery.
