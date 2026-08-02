# EV-HW6-20260802-P5-FLASH-011

## Summary

- Test case: lifecycle-v10 `ps_dev_at25sl128a` storage-owner polling scratch erase/program/readback/cleanup cycle
- Result: `PASS`
- Date/time: `2026-08-02 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: fresh board flash; display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `853204be3605244a2c0d26b144b37426beb17b6f`
- Firmware state: uncommitted HW6 FW0 `ps_dev_at25sl128a` polling scratch-sector integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `057C6FA7191BD2926E3F277D59D284EBBA3F78536DD70219AF92BD31A0AB4FEF`
- Instrumentation: ST-LINK SWD/GDB; operator armed `__fw0_owner_sm_start.gdb`, continued the target, halted without reset, and sourced `__fw0_all_probe_prints.gdb`

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_flash_scratch_gdb.log` | `E68AB3713A76996B087A03EF27ECDF7218A6F9C392768771B58853D6F37C830B` | Exact target-memory and transition-trace report from the operator transcript |

## Observations

- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Retained-peripheral lifecycle state machine reported version `0x8`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Final storage FSM state matched expected `2 / 2`; final flash FSM state matched expected `8 / 8` with zero rejected transitions.
- AT25 driver probe reported `driver API/init/state/ops/last = 1 / 0 / 3 / 9 / 0`.
- JEDEC telemetry reported `JEDEC status/ID/match = 0x0 / 1f 42 18 / 1`.
- Scratch range was `0x00FFF000`, length `256` bytes, within the final 4 KiB sector reserved only for bring-up scratch tests.
- Initial scratch erase succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 635`, blank read status `0x0`, blank mismatches `0`.
- Scratch page program succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 10`, polling read status `0x0`, pattern mismatches `0`.
- Program first 16 bytes were `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`.
- Cleanup erase succeeded: WREN/cmd/wait/polls `0x0 / 0x0 / 0 / 620`, blank read status `0x0`, blank mismatches `0`.
- Cleanup first 16 bytes were all `FF`, so the scratch page was left erased.
- Deep-power-down status was `0x0`; both lifecycle cycles reported `flash release/JEDEC/match/DPD = 0 0 1 0`.
- Final scratch OSPI state/error and final OSPI state/error were both `0x2 / 0x0`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1815`.

## Conclusion

The HW6 storage-owner path now validates the AT25SL128A polling scratch-sector baseline through `ps_dev_at25sl128a`: JEDEC identity, scratch sector erase, blank verify, 256-byte page program, polling readback verify, cleanup erase, cleanup blank verify, release from deep-power-down, and recommit to deep-power-down across the baseline workflow plus two bounded owner-routed resume/quiesce cycles.

This evidence closes the HW6 owner-routed polling scratch erase/program/readback baseline. It does not close DMA flash transfers, LevelX/FileX integration, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, or storage fault injection/recovery.
