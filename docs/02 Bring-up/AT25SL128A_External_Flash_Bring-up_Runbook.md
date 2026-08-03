# AT25SL128A External Flash Bring-up Runbook

This runbook records the measured HW5 procedure for the external flash and storage base path.

> [!important] HW6 reuse
> Reuse the procedure on HW6 against the HW6 pin/DMA contracts. Begin with non-destructive identity/status reads, use only an approved scratch region for write/erase tests, and record all new evidence in [[HW6_Brought_Up_Tracker]]. `EV-HW6-20260802-P5-FLASH-010` validates the HW6 storage-owner JEDEC/release/deep-power-down wrapper path. `EV-HW6-20260802-P5-FLASH-011` validates owner-routed polling scratch erase/program/readback/cleanup in the final 4 KiB scratch sector. `EV-HW6-20260802-P5-FLASH-012` validates owner-routed DMA program/readback on the same scratch page. `EV-HW6-20260803-P5-FLASH-013` validates the raw 4 KiB block adapter against the same scratch sector after increasing only the storage-owner stack. `EV-HW6-20260803-P5-FLASH-014` validates AT25 reset-normalized write/erase operation plus the fixed 10-region storage layout; FileX/LevelX, package, and USB MSC behavior remain open.

Related:

- [[Storage_and_Installer_Contract]]
- [[HW6_DMA_Map]]
- [[HW6_Pin_Ownership_Matrix]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `AT25SL128A` device identity over `OCTOSPI1`
- safe OCTOSPI bring-up clock: the `fw1` reference `.ioc` uses `PLL2Q = 64 MHz` kernel with `OCTOSPI1.ClockPrescaler = 8`; enable this in `fw0` only when Phase 2 storage validation begins
- JEDEC/device ID readback
- read/write/erase behavior
- deep power-down and wake revalidation
- DMA read/write path where enabled
- protected storage region assumptions
- staging/export storage basis used by USB MSC

---

## CubeMX / Bus Baseline

| Function | MCU resource | Required baseline |
| --- | --- | --- |
| External flash | `AT25SL128A` serial NOR | `OCTOSPI1` quad path |
| Chip select | `PA0` | `OCTOSPIM_P2_NCS` / `OCTOSPI1_Port2_NCS` |
| Clock | `PB10` | `OCTOSPIM_P1_CLK` |
| IO0..IO3 | `PB1`, `PB0`, `PA7`, `PA6` | quad data lines |
| DMA read | `GPDMA1_CH4`, `GPDMA1_REQUEST_OCTOSPI1` | peripheral-to-memory |
| DMA program | `GPDMA1_CH5`, `GPDMA1_REQUEST_OCTOSPI1` | memory-to-peripheral |

First bring-up uses the conservative clock path already recorded in the hardware docs: `PLL2Q = 64 MHz` kernel with `OCTOSPI1.ClockPrescaler = 8`. Later performance experiments may try a PLL2R-derived 128 MHz or 256 MHz kernel only after ID/read/write/erase/deep-power-down behavior is reliable.

---

## Scratch Region Rule

Destructive erase/program tests must use one explicitly selected scratch region only.

Before any erase:

1. Record the scratch address range in this runbook and [[Brought_Up_Tracker]].
2. Confirm the range is outside boot/configuration data, calibration data, saved data, installed package data, USB staging/export, and the protected fault-log ring.
3. Confirm the test is not running while USB MSC export is active.
4. Confirm no package/runtime asset reader is active.

`EV-HW6-20260803-P5-FLASH-014` validates the current fixed 10-region flash layout. The final 4 KiB sector remains a protected bring-up scratch region and is not host-exposed.

---

## Baseline State Sequence

This sequence proves the flash path before FileX/LevelX or installer behavior is trusted.

1. Boot with storage owner offline and OCTOSPI inactive.
2. Initialize OCTOSPI1 at the conservative clock baseline.
3. Read JEDEC/device identity and record all returned bytes.
4. Read status/configuration registers and record write-enable, busy, protection, quad-enable, and deep-power-down relevant bits.
5. Select and record the scratch test sector.
6. Erase the scratch sector and poll busy until completion or timeout.
7. Read back the erased state.
8. Program a deterministic test pattern.
9. Read back by polling path and compare byte-for-byte.
10. Read back by DMA path and compare byte-for-byte.
11. Erase the scratch sector again and confirm blank state.
12. Enter deep power-down, wait the required interval, wake/release, and re-read device ID.
13. Repeat a small readback after wake to confirm the command path recovered.

Every erase/program/read operation must have an explicit timeout. A timeout is a storage fault, not an infinite polling condition.

---

## Storage Integration Gate

Only after the baseline flash path passes:

1. Bring up the LevelX custom NOR interface against the proven flash operations.
2. Mount the local FileX staging/export volume.
3. Confirm local ownership works before any USB export.
4. Export only the staging/export volume through USB MSC.
5. Reclaim and rescan staging/export after host release.
6. Confirm protected regions are not host-visible or host-writable.
7. Confirm fault-log export is firmware-copy into staging/export, never direct exposure.

The installed game/package raw blob storage is not the FAT/FileX staging volume. Runtime reads must use [[Package_Asset_Loading_API_Contract]] or bounded package-managed caches.

---

## Command / Configuration Ledger

Populate this table during bring-up. The current values are placeholders until measured on HW5 hardware.

| Step | Configuration | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| conservative clock | `PLL2Q = 64 MHz`, prescaler `8` | stable OCTOSPI command path | `fw0` generated with `OCTOSPIMFreq_Value=64000000`, `OCTOSPI1.ClockPrescaler=8`; GDB probe read `ospi_kernel_hz=64000000` | pass |
| device ID | JEDEC/device ID read | bytes match AT25SL128A datasheet | Non-destructive polling command read returned JEDEC ID `1F 42 18`; `EV-HW6-20260802-P5-FLASH-010` and `EV-HW6-20260802-P5-FLASH-011` revalidated this through `ps_dev_at25sl128a` in the storage owner with JEDEC status/match `0x0/1` | pass |
| status read | status/config registers | busy/protection/quad state understood | Non-destructive reads of commands `0x05`, `0x35`, and `0x15` returned `00 00 00`; write-enable/busy/protection bits clear in this snapshot | pass |
| scratch range | final 4 KiB sector, `0x00FFF000..0x00FFFFFF` | bring-up-only destructive scratch region; future flash-layout pass must not assume this remains available | User approved use of the final sector for immediate bring-up tests; `EV-HW6-20260802-P5-FLASH-011` touched the first `256` bytes and left them erased; `EV-HW6-20260803-P5-FLASH-013` exercised the same sector through the raw 4 KiB block adapter and left it erased; `EV-HW6-20260803-P5-FLASH-014` validates this sector as protected layout region index `9`, start `0x00FFF000`, length `4096` | active |
| sector erase | scratch only | erased state readback | HW6 owner-routed polling initial erase of `0x00FFF000` completed with WREN/cmd/wait/polls `0x0/0x0/0/635`; first 256 bytes read back as `0xFF` with `0` mismatches | pass |
| program pattern | deterministic pattern | write completes before timeout | HW6 owner-routed polling programmed 256-byte pattern `0xA5 ^ index` at `0x00FFF000`; WREN/cmd/wait/polls `0x0/0x0/0/10` | pass |
| polling readback | scratch pattern | byte-for-byte match | HW6 owner-routed polling readback matched all 256 bytes with `0` mismatches; first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA` | pass |
| DMA readback | scratch pattern | byte-for-byte match and clean completion signaling | HW6 owner-routed GPDMA read of programmed scratch pattern passed with DMA read cmd/xfer wait/polls `0x0/0/27`, `0` mismatches, first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`, RX DMA state/error `0x1/0x0`, and OSPI state/error `0x2/0x0`; wrapper services OSPI transfer completion locally after DMA channel completion | pass |
| DMA program | scratch page pattern | page program completes and readback matches | HW6 owner-routed 256-byte page program via GPDMA passed with DMA program WREN/cmd/xfer wait/polls `0x0/0x0/0/1`, flash wait/polls `0/1`, TX DMA state/error `0x1/0x0`, DMA readback mismatches `0`, and cleanup blank verify `0` mismatches | pass |
| raw block adapter | `ps_storage_flash_block`, logical block size `4096` | geometry is sane; one scratch block erase/program/readback/cleanup completes without fault | HW6 lifecycle-v12 raw block adapter returned API/init/ops/last `1/0/1/0`, geometry `16777216/4096/256/4096`, test block `4095`, address `0x00FFF000`, erase status/polls `0/664`, blank/verify/cleanup mismatches `0`, programmed 16 pages, verify first 16 bytes `5A 5B 58 59 5E 5F 5C 5D 52 53 50 51 56 57 54 55`, cleanup first 16 bytes all `FF`, final OSPI state/error `0x2/0x0`; initial hardfaults were diagnosed as storage-owner stack overflow and fixed by moving large storage result buffers static plus increasing only `thStorage` stack to `2048` bytes | pass |
| storage layout | fixed 10-region map | geometry, range, alignment, overlap, host exposure, and scratch reservation validate before FileX/LevelX | HW6 lifecycle-v12 layout validation returned API/status/count `1/0/10`, total/erase/end `16777216/4096/0x01000000`, alignment/overlap/range errors `0/0/0`, host/protected masks `0x40/0x3BF`, and scratch index/start/length `9/0x00FFF000/4096` | pass |
| deep power-down | flash DPD command `0xB9` | command accepted after active operations completed | DPD command returned `HAL_OK`; `EV-HW6-20260802-P5-FLASH-011` revalidated owner-routed DPD after scratch cleanup with baseline status `0x0` and final OSPI state/error `0x2/0x0` | pass |
| wake/revalidate | release from DPD command `0xAB` | ID/readback valid after wake | Release command returned `HAL_OK`; post-wake JEDEC ID remained `1F 42 18`; `EV-HW6-20260802-P5-FLASH-010` revalidated both owner-routed lifecycle cycles with release/JEDEC/match/DPD `0/0/1/0` | pass |
| local mount | FileX/LevelX after baseline | staging/export mount succeeds | TBD | open |
| USB export | staging/export only | host cannot see protected regions | TBD | open |

---


## HW6 Measured Evidence

`EV-HW6-20260802-P5-FLASH-010` validates the non-destructive storage-owner flash wrapper path on `HW6-UNIT-001`: `ps_dev_at25sl128a` read JEDEC identity, released the AT25SL128A from deep-power-down, re-read identity, and recommitted deep-power-down across the baseline workflow plus two bounded owner-routed resume/quiesce cycles.

`EV-HW6-20260802-P5-FLASH-011` validates the HW6 owner-routed polling scratch path on `HW6-UNIT-001`: the storage owner erased `0x00FFF000`, verified the first `256` bytes blank, programmed deterministic pattern `0xA5 ^ index`, verified readback with `0` mismatches, erased the scratch sector again, verified cleanup blank state with `0` mismatches, and entered deep-power-down. The probe reported flash driver API/init/state/ops/last `1 / 0 / 3 / 9 / 0`, scratch status/address/length `0 / 0x00fff000 / 256`, erase/program/cleanup statuses all `0`, program first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`, cleanup first 16 bytes all `FF`, deep-power-down status `0x0`, both cycle release/JEDEC/match/DPD lines `0 / 0 / 1 / 0`, and final OSPI state/error `0x2 / 0x0`.

This evidence closes the HW6 owner-routed polling scratch erase/program/readback baseline. DMA flash transfers are closed by `EV-HW6-20260802-P5-FLASH-012`; LevelX/FileX, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, and storage fault injection/recovery remain open.
`EV-HW6-20260802-P5-FLASH-012` validates the HW6 owner-routed DMA scratch path on `HW6-UNIT-001`: polling erase/program/readback/cleanup still passed, DMA page program completed with xfer wait/polls `0 / 1`, DMA flash wait/polls `0 / 1`, DMA read completed with xfer wait/polls `0 / 27`, DMA readback matched all 256 bytes with `0` mismatches, DMA first 16 bytes were `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA`, cleanup first 16 bytes were all `FF`, deep-power-down status was `0x0`, both lifecycle cycles reported `flash release/JEDEC/match/DPD = 0 / 0 / 1 / 0`, and final OSPI state/error was `0x2 / 0x0`.

This evidence closes the HW6 owner-routed flash DMA transfer checkpoint. LevelX/FileX, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, and storage fault injection/recovery remain open.

`EV-HW6-20260803-P5-FLASH-013` validates the HW6 raw flash block adapter on `HW6-UNIT-001`: `ps_storage_flash_block` reported geometry `16777216 / 4096 / 256 / 4096`, erased logical block `4095` at `0x00FFF000`, verified blank state with `0` mismatches, programmed and verified a 16-byte deterministic pattern, erased the sector again, verified cleanup blank state with `0` mismatches, and left the flash command path healthy with OSPI state/error `0x2 / 0x0`. The full owner lifecycle completed two cycles with masks `0x7F / 0x00` and `0x3FF / 0x3FF`, and the user heard all three tones with no hardfault.

This evidence closes the raw block-level flash adapter checkpoint before FileX/LevelX. `EV-HW6-20260803-P5-FLASH-014` additionally validates the fixed 10-region flash layout: total size `16777216`, erase size `4096`, layout end `0x01000000`, zero alignment/overlap/range errors, host-exposed mask `0x40`, protected mask `0x3BF`, and protected scratch region `9` at `0x00FFF000` length `4096`. It does not create a filesystem, validate wear leveling, expose USB MSC storage, or validate package installation.

## Validation Procedure

1. Confirm OCTOSPI pins and idle levels.
2. Read device ID at the conservative CubeMX baseline: `PLL2Q = 64 MHz`, `OCTOSPI1.ClockPrescaler = 8`.
3. Read status/configuration registers.
4. Erase a test sector in an allowed scratch area.
5. Program a known pattern.
6. Read back by polling path.
7. Read back by DMA path if enabled.
8. Validate erase returns expected blank state.
9. Enter deep power-down and measure/observe safe behavior.
10. Wake flash and revalidate ID/readback.
11. Validate storage owner can isolate staging/export from protected regions.
12. Validate no FileX/FAT reads occur in active runtime/audio asset loops.
13. Validate storage failure routes to safe mode rather than normal shell/runtime launch.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- device ID readback
- selected conservative bring-up clock and any later PLL2R-derived performance test clock
- test address range used
- erase/program/readback result
- DMA result if used
- deep-power-down and wake result
- any failed status register observations

Do not test destructive writes outside an explicitly designated scratch region. Later OCTOSPI performance tuning may evaluate PLL2R-derived 128/256 MHz options only after reliable ID/read/write/erase/deep-power-down behavior is proven.
