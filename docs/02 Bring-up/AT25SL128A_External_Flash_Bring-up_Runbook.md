# AT25SL128A External Flash Bring-up Runbook

This runbook records the measured HW5 procedure for the external flash and storage base path.

> [!important] HW6 reuse
> Reuse the procedure on HW6 against the HW6 pin/DMA contracts. Begin with non-destructive identity/status reads, use only an approved scratch region for write/erase tests, and record all new evidence in [[HW6_Brought_Up_Tracker]].

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

The final flash layout is assigned later. Until then, the scratch range is a bring-up-only reservation, not an architectural region.

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
| device ID | JEDEC/device ID read | bytes match AT25SL128A datasheet | Non-destructive polling command read returned JEDEC ID `1F 42 18` | pass |
| status read | status/config registers | busy/protection/quad state understood | Non-destructive reads of commands `0x05`, `0x35`, and `0x15` returned `00 00 00`; write-enable/busy/protection bits clear in this snapshot | pass |
| scratch range | final 4 KiB sector, `0x00FFF000..0x00FFFFFF` | bring-up-only destructive scratch region; future flash-layout pass must not assume this remains available | User approved use of the final sector for immediate bring-up tests before final protected/fault-log layout is assigned | active |
| sector erase | scratch only | erased state readback | Initial erase of `0x00FFF000` completed; first 256 bytes read back as `0xFF` with `0` mismatches | pass |
| program pattern | deterministic pattern | write completes before timeout | Programmed 256-byte pattern `0xA5 ^ index` at `0x00FFF000`; write and busy-poll completed before timeout | pass |
| polling readback | scratch pattern | byte-for-byte match | Polling readback matched all 256 bytes; first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA` | pass |
| DMA readback | scratch pattern | byte-for-byte match and clean completion signaling | 256-byte GPDMA read of programmed scratch pattern passed after enabling OCTOSPI1 IRQ completion handling; `dma_read_status=0`, `dma_wait_status=0`, `dma_verify_mismatch_count=0`, first 16 bytes `A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA` | pass |
| DMA program | scratch page pattern | page program completes and readback matches | 256-byte page program via GPDMA passed after enabling OCTOSPI1 IRQ completion handling; `dma_program_status=0`, `dma_program_wait_status=0`, `program_wait_status=0`, and both DMA/polling readbacks matched with `0` mismatches | pass |
| deep power-down | flash DPD command `0xB9` | command accepted after active operations completed | DPD command returned `HAL_OK` after scratch cleanup erase and blank verification completed | pass |
| wake/revalidate | release from DPD command `0xAB` | ID/readback valid after wake | Release command returned `HAL_OK`; post-wake JEDEC ID remained `1F 42 18`; post-wake scratch readback had `0` non-blank bytes in the first 256 bytes at `0x00FFF000` | pass |
| local mount | FileX/LevelX after baseline | staging/export mount succeeds | TBD | open |
| USB export | staging/export only | host cannot see protected regions | TBD | open |

---

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
