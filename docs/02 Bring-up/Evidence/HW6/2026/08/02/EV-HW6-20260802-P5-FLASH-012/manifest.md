# EV-HW6-20260802-P5-FLASH-012

## Summary

- Test case: lifecycle-v10 `ps_dev_at25sl128a` storage-owner polling plus DMA scratch erase/program/readback/cleanup cycle
- Result: `PASS`
- Date/time: `2026-08-02 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: fresh board flash; display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `201bd9101c5f11f0fd66445bace52c82c12e59d4`
- Firmware state: uncommitted HW6 FW0 `ps_dev_at25sl128a` DMA completion integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `2C2B79B18D71D218B629B614C469849D6D874C84E4DB158EE34684C79F1970EC`
- Instrumentation: ST-LINK SWD/GDB; operator armed `__fw0_owner_sm_start.gdb`, continued the target, halted without reset, and sourced `__fw0_all_probe_prints.gdb`

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_flash_dma_gdb.log` | `7489E1B9436D51B7A586A32498D04E9B82E43AC99C9E46AE7FF8CC87640BD2C9` | Exact target-memory and transition-trace report from the operator transcript |

## Observations

- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Retained-peripheral lifecycle state machine reported version `0xA`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Final storage FSM state matched expected `2 / 2`; final flash FSM state matched expected `8 / 8` with zero rejected transitions.
- AT25 driver probe reported `driver API/init/state/ops/last = 1 / 0 / 3 / 9 / 0`.
- JEDEC telemetry reported `JEDEC status/ID/match = 0x0 / 1f 42 18 / 1`.
- Scratch range was `0x00FFF000`, length `256` bytes, within the final 4 KiB sector reserved only for bring-up scratch tests.
- Initial scratch erase succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 619`, status1 after WREN/cmd `0x02 / 0x01`, blank read status `0x0`, blank mismatches `0`, and blank first 16 bytes all `FF`.
- Polling page program succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 1`, polling read status `0x0`, pattern mismatches `0`, and first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`.
- DMA page program/readback succeeded: DMA program WREN/cmd/xfer wait/polls `0x0 / 0x0 / 0 / 1`, DMA program flash wait/polls `0 / 1`, DMA read cmd/xfer wait/polls `0x0 / 0 / 27`, DMA mismatches `0`, and DMA first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`.
- DMA handles ended ready and error-free: TX state/error `0x1 / 0x0`, RX state/error `0x1 / 0x0`.
- Cleanup erase succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 619`, blank read status `0x0`, blank mismatches `0`, and cleanup first 16 bytes all `FF`.
- Deep-power-down status was `0x0`; both lifecycle cycles reported `flash release/JEDEC/match/DPD = 0 0 1 0`.
- Final scratch OSPI state/error and final OSPI state/error were both `0x2 / 0x0`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1568`.

## Conclusion

The HW6 storage-owner path now validates AT25SL128A scratch-sector erase/program/readback/cleanup through both polling and DMA paths in `ps_dev_at25sl128a`. The wrapper services the OSPI transfer-complete path after DMA channel completion without changing CubeMX-generated interrupt configuration. This evidence closes the HW6 owner-routed flash DMA transfer checkpoint.

This evidence does not close LevelX/FileX integration, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, or storage fault injection/recovery.