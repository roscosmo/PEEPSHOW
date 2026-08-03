# EV-HW6-20260803-P5-FLASH-015

## Summary

- Test case: lifecycle-v13 FileX/LevelX local staging smoke test over AT25SL128A USB_STAGING region
- Result: `PASS`
- Date/time: `2026-08-03 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: fresh board flash; display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `2b8a758ebfba30090a01b027966ddb12ac3b6fad`
- Firmware state: uncommitted HW6 FW0 FileX/LevelX local smoke integration plus CubeMX-generated FileX/LevelX middleware
- Build profile: `Debug`
- FW0 ELF SHA-256: `E1EC4294A961C4CF5470BB4DF9C3962086B2B12940FA5D4F97991B9075433AD0`
- FW0 IOC SHA-256: `C6426E5F8EBEF7F3521406A530166C316A8733E0AEC22AFB910C8F927096E416`
- Instrumentation: ST-LINK SWD/GDB; operator armed `__fw0_owner_sm_start.gdb`, continued the target, waited for the bounded workflow, halted without reset, and sourced `__fw0_all_probe_prints.gdb`

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_filex_levelx_gdb.log` | `63032DC579209FBA3C69745333B435A2CC6A28A220FF6D787BDB860783CDA563` | Exact target-memory and transition-trace report from the operator transcript |

## Observations

- Physical owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Retained-peripheral lifecycle state machine reported version `0xD`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Final storage FSM state matched expected `2 / 2`; final flash FSM state matched expected `8 / 8`; both had zero rejected transitions.
- AT25SL128A remained healthy: JEDEC `1F 42 18`, identity match `1`, deep-power-down status `0x0`, OSPI state/error `0x2 / 0x0`.
- Raw block adapter and fixed flash layout still passed before the filesystem smoke test: geometry `16777216 / 4096 / 256 / 4096`, layout API/status/count `1 / 0 / 10`, and layout errors `0 / 0 / 0`.
- FileX/LevelX smoke test used storage region `6` (`USB_STAGING`) at `0x00AC0000`, full region length `5242880`, with test subrange `0x00AC0000` length `1048576`.
- Filesystem geometry was erase/sector/count `4096 / 512 / 2048`.
- LevelX init/open/close all returned `0x0 / 0x0 / 0x0`.
- FileX format/open/flush/close all returned `0x0 / 0x0 / 0x0 / 0x0`.
- File create/open/write returned `0x0 / 0x0 / 0x0`; file seek/read/close returned `0x0 / 0x0 / 0x0`.
- The test wrote and read `256 / 256` bytes with `0` mismatches.
- Boot-sector readback first 16 bytes were `EB 34 90 45 4C 20 46 49 4C 45 58 00 02 01 01 00`.
- FAT geometry readback was bytes-per-sector/sectors-per-cluster/reserved/FATs `512 / 1 / 1 / 1`, root/total/sectors-per-FAT/signature `32 / 2048 / 6 / 0xAA55`.
- File payload first 16 bytes were `48 57 36 2D 46 58 4C 58 49 4A 4B 4C 4D 4E 4F 50`, matching `HW6-FXLXIJKLMNOP`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 4273`.

## Conclusion

The HW6 storage-owner path now validates local FileX formatting, FileX mounting, LevelX-backed sector read/write behavior, and a FileX create/write/read/verify smoke test over the first 1 MiB of the host-exposed `USB_STAGING` region. This is local firmware ownership only; USB MSC export/reclaim, host ownership arbitration, package installation, protected fault-log policy, storage timing/current budgets, and storage fault injection/recovery remain open.
