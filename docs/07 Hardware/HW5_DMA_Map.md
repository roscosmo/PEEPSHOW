# HW5 DMA Map

This note records the HW5 DMA paths, buffer placement rules, alignment requirements, and STOP compatibility.

> [!warning] Retired target DMA map
> This map applies only to historical HW5. Use [[HW6_DMA_Map]] for active work and revalidate all retained paths.

| Path | DMA | Buffer Region | Alignment | STOP-safe | Owner |
|---|---|---|---|---|---|
| Display flush | `LPDMA1_CH0`, `LPDMA1_REQUEST_SPI3_TX`, memory-to-peripheral | SRAM4 display line/full-frame payload buffers; exact section/budget TBD | source incremented, final alignment TBD | No normal active transfer across STOP; LPBAM scenarios are separately validated | `thDisplay` |
| Speaker audio TX | `GPDMA1_CH3`, `GPDMA1_REQUEST_SAI1_A`, memory-to-peripheral, circular enabled | PCM mix buffer, region TBD | source/dest halfword, 16-bit PCM | No active transfer across STOP | `thAudio` |
| AT25SL128A external flash read | `GPDMA1_CH4`, `GPDMA1_REQUEST_OCTOSPI1`, peripheral-to-memory | storage buffers, region TBD | destination incremented; data exchange configured by CubeMX | No active transfer across STOP | `thStorage` |
| AT25SL128A external flash program | `GPDMA1_CH5`, `GPDMA1_REQUEST_OCTOSPI1`, memory-to-peripheral | storage buffers, region TBD | source incremented; data exchange configured by CubeMX | No active transfer across STOP | `thStorage` |
| USB MSC transfer | none assigned in `fw1` reference `.ioc`; unavailable in early `fw0` | N/A | N/A | USB active blocks deep sleep by policy | `thStorage` |
| Light sensor ADC | none assigned in `fw1` reference `.ioc`; unavailable in early `fw0` | N/A | N/A | sampled while awake | `thSensor` |
| BLE UART | none assigned in `fw1` reference `.ioc`; first bring-up uses interrupt-driven RX/TX static rings | static comm RX/TX rings, not DMA buffers | byte ring entries; exact ring sizes from BLE knobs | no active transfer across STOP | `thComm` |

Rules:

- no clock changes during active DMA
- no STOP entry while critical DMA paths are active
- owner threads must quiesce DMA before low-power transitions
- display DMA source buffers and LPBAM display sequence payloads must use the approved DMA-safe SRAM4 placement once validated
- BLE UART DMA is deferred until interrupt-driven static rings are proven insufficient by measurement

Related:

- [[Authority_and_Invariants]]
- [[Display_and_Rendering_Contract]]
- [[Audio_Contract]]
- [[Storage_and_Installer_Contract]]
