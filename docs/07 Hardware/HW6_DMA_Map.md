# HW6 DMA Map

This note records the HW6 DMA topology from the imported full-intent IOC.
Buffer placement and production budgets remain `pending_validation`.

| Path | DMA | Buffer Rule | STOP / LPBAM Rule | Owner |
|---|---|---|---|---|
| Display and autonomous display | `LPDMA1_CH0`, SPI3 TX request | display TX scratch, retained payloads, and linked-list nodes use the approved SRAM4 arena | normal transfers quiesce before STOP; only a validated LPBAM queue runs autonomously | `thDisplay` |
| Speaker audio TX | `GPDMA1_CH3`, SAI1_A request, memory-to-peripheral circular | aligned 16-bit PCM mix buffers outside SRAM4 unless measured design requires otherwise | stop and drain before STOP | `thAudio` |
| External flash read | `GPDMA1_CH4`, OCTOSPI1 request, peripheral-to-memory | owner-managed storage buffers | no active transfer across STOP | `thStorage` |
| External flash program | `GPDMA1_CH5`, OCTOSPI1 request, memory-to-peripheral | owner-managed storage buffers | no active transfer across STOP | `thStorage` |
| USB MSC | no explicit IOC DMA channel | USBX-owned buffers | USB activity blocks incompatible sleep | `thStorage` |
| BLE UART | no IOC DMA channel | interrupt-driven static rings first | UART quiesced before STOP | `thComm` |

## Removed DMA Candidates

- no light-sensor ADC path
- no rotary-encoder DMA path
- no piezo DMA path

## Rules

- no clock or voltage transition during active DMA
- no STOP entry while a non-autonomous critical DMA transfer is active
- owner threads quiesce their DMA before acknowledging a power transition
- SRAM4 belongs to display DMA/LPBAM according to the validated HW6 linker and
  memory report
- HW5 SRAM4 measurements are provisional sizing inputs only until repeated on
  HW6 firmware

Related:

- [[HW6_Clock_Tree_Contract]]
- [[HW6_Hardware_Revision_Contract]]
- [[Memory_and_Budgeting_Contract]]

