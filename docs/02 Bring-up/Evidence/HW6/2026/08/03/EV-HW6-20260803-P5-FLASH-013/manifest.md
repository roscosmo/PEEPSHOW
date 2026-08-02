# EV-HW6-20260803-P5-FLASH-013

## Summary

- Test case: lifecycle-v11 raw AT25SL128A flash block adapter scratch-sector validation after storage-owner stack overflow fix
- Result: `PASS`
- Date/time: `2026-08-03 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: fresh board flash; display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `1c7bde001e72642643df748813d59aac81b845cd`
- Firmware state: uncommitted HW6 FW0 raw block adapter and storage stack fix
- Build profile: `Debug`
- FW0 ELF SHA-256: `9D0B9019CC1F751D4FC83C5C7792678D9607024437133E5470BC896BAA056FB0`
- Instrumentation: ST-LINK SWD/GDB; operator armed `__fw0_owner_sm_start.gdb`, continued the target, confirmed three tones, halted without reset, and sourced `__fw0_all_probe_prints.gdb`

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_flash_raw_block_gdb.log` | `235A2DDEFFA41500D3C7FBFB0E9FB0220297C1CC4DD52A5C2690F249D9F1EB0B` | Exact target-memory and transition-trace report from the operator transcript |

## Observations

- Physical output passed: all three expected 750 ms 1 kHz tones were heard.
- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Retained-peripheral lifecycle state machine reported version `0xB`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Final storage FSM state matched expected `2 / 2`; final flash FSM state matched expected `8 / 8` with zero rejected transitions.
- AT25 driver probe reported `driver API/init/state/ops/last = 1 / 0 / 3 / 75 / 0`.
- JEDEC telemetry reported `JEDEC status/ID/match = 0x0 / 1f 42 18 / 1`.
- Scratch range was `0x00FFF000`, within the final 4 KiB sector reserved only for bring-up scratch tests.
- Existing AT25 scratch still passed polling and single-page DMA paths: scratch status/address/length `0 / 0x00fff000 / 256`, polling mismatches `0`, DMA mismatches `0`, and cleanup mismatches `0`.
- Raw flash block adapter geometry was `16777216 / 4096 / 256 / 4096` for total bytes, erase size, page size, and logical block count.
- Raw block test reported status/index/address/length `0 / 4095 / 0x00fff000 / 4096`.
- Raw block initial erase passed: erase status/polls `0 / 634`.
- Raw block blank read passed: read count/status/mismatch `16 / 0 / 0`, first 16 bytes all `FF`.
- Raw block polling program passed: program status/pages/last polls `0 / 16 / 1`.
- Raw block verify read passed: read count/status/mismatch `16 / 0 / 0`, first 16 bytes `5A 5B 58 59 5E 5F 5C 5D 52 53 50 51 56 57 54 55`.
- Raw block cleanup passed: cleanup status/read/mismatch `0 / 0 / 0`, cleanup first 16 bytes all `FF`.
- Final raw block OSPI state/error was `0x2 / 0x0`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1314`.

## Conclusion

The HW6 storage-owner raw flash block adapter now validates the final 4 KiB scratch sector through bounded erase, 16 page programs, 16 blank reads, 16 verify reads, and cleanup erase/readback, while preserving the existing single-page DMA scratch coverage in `ps_dev_at25sl128a`.

The checkpoint also validates the storage-owner stack fix: the previous hardfault was a Cortex-M stack overflow during storage stabilization, and this run completed without hardfault after moving large storage scratch result structs out of the owner stack and increasing only `thStorage` stack to 2048 bytes.

This evidence does not close LevelX/FileX integration, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, or storage fault injection/recovery.