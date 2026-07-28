# HW5 Clock Tree Contract

This note records the intended HW5 clock tree and the CubeMX baseline.

> [!warning] Retired target clock record
> HW5 clock values and experiments are historical baselines only. Active operating-point work belongs in [[HW6_Clock_Tree_Contract]].

## Operating-Point Families

The `.ioc` frequencies below are configuration and bring-up baselines. They are not final PeepOS performance policy. Shipping reactive/realtime points must be selected by measured HW5 energy, latency, and deadline evidence.

| Family | Purpose | Current Baseline Or Candidate Set | Status |
|---|---|---|---|
| CubeMX baseline | generated initial clock configuration | 24 MHz MSI for the full/reference profile; `fw0` early bring-up currently uses 4 MHz MSI | configuration fact, not final policy |
| Boot/init | deterministic early startup | 4 MHz MSI in `fw0` Phase 0; tune only when a measured bring-up/runtime requirement justifies it | provisional |
| Reactive active burst | complete an admitted event transaction, prepare presentation/LPBAM work, quiesce, and return to the waiting backend | sweep valid generated points beginning with existing 4 MHz and 24 MHz baselines; add higher candidates only after clock-tree validation | pending measurement |
| Realtime sustained | meet frame, audio, sensor, display, and owner deadlines with margin | sweep valid generated points; 24 MHz is only the current baseline, not a guaranteed final point | pending measurement |
| USB installer | host transport mode | 24 MHz SYSCLK baseline with valid 48 MHz USB clock | baseline; validate transport deadlines and power |

## Operating-Point Selection Contract

`thPower` owns active operating-point selection and transitions. Engine semantics identify the optimization objective, not a frequency:

- reactive selection minimizes measured charge/energy per complete event-to-yield transaction while satisfying response-latency limits. A faster race-to-sleep point is valid only if total transaction cost improves.
- realtime selection uses the lowest measured sustained point that satisfies worst-case frame, audio, sensor, display, and owner deadlines with required margin.
- package/runtime code never selects SYSCLK, HCLK, voltage scaling, PLLs, flash latency, or kernel clocks.
- an operating point is the validated combination of SYSCLK/HCLK, voltage scale, flash latency/cache state, and all affected kernel clocks.
- peripheral kernel clocks such as SAI, SPI, USB, RTC, and autonomous-domain clocks retain their own functional constraints; they do not blindly track CPU policy.
- no clock/voltage transition occurs during active DMA or an active bus transaction.
- transition latency and energy are measured; dynamic switching and hysteresis are adopted only when their break-even behavior is proven.
- one conservative point per semantic is acceptable when workload-dependent switching is not measurably beneficial.

Candidate sweeps, selection criteria, and evidence requirements are defined by [[Power_and_Sleep_Policy]], [[Power_Validation]], [[Power_Measurement_and_Trace_Correlation_Runbook]], and [[Pending_Measured_Constants_Register]].

## PLL2 Baseline

Current `.ioc` PLL2 configuration:

| Output | Frequency | Use |
|---|---|---|
| `PLL2P` | `4.096 MHz` | required SAI1 audio clock for 16 kHz mono playback |
| `PLL2Q` | `64 MHz` | current OCTOSPI1 kernel clock source |
| `PLL2R` | `256 MHz` | possible later OCTOSPI/performance source after validation |

## Kernel Clocks

| Peripheral | Kernel Clock | Profile Constraints | STOP/Resume Notes |
|---|---|---|---|
| SYSCLK / HCLK | MSI baseline | `fw1` reference `.ioc` targets SYSCLK/HCLK 24 MHz; `fw0` Phase 0 intentionally starts at 4 MHz MSI and should be retuned only when a bring-up phase needs more clock | validate after STOP resume |
| RTC | external 32.768 kHz MEMS oscillator on `PC14` LSE input | must remain low-power safe | wake source authority lives in `thPower` |
| Display SPI3 | HSI 16 MHz kernel, SPI calculated 8 Mbit/s | no clock change during active SPI/LPDMA transfer | display owner must quiesce before STOP unless a validated LPBAM display scenario owns the transfer |
| Display EXTCOMIN | RTC 1 Hz calibration output on `PC13` | must remain valid while display holds image | coordinated by display/power policy |
| Audio SAI1 | `PLL2P = 4.096 MHz`, SAI real audio frequency `16.0 kHz` | required valid clock tree for 16 kHz mono, 16-bit output to MAX98357A | audio owner stops DMA before STOP |
| BBB LPTIM1 | reference CubeMX LPTIM path via `PB2` `BUZZ` | procedural BBB tone/sweep timing must be bounded | audio owner stops BUZZ output before STOP |
| External flash OCTOSPI1 | `PLL2Q = 64 MHz` kernel, `OCTOSPI1.ClockPrescaler = 8` | conservative bring-up baseline; later tuning may evaluate PLL2R-derived 128/256 MHz options only after reliable ID/read/write/erase | storage owner quiesces before STOP |
| Sensor/power bus I2C3 | HSI 16 MHz kernel | timing `0x00303D5B`; validate against bus speed target; hosts TMAG3001A1YBGR at `0x34`, LIS2DUX12TR at `0x18`, and ADP5360 at `0x46` | input/sensor/power owners validate bus after wake; IMU step counter should run in embedded logic without active MCU I2C |
| Light sensor ADC1 | 16 MHz ADC clock | sampled while awake; no ADC IRQ required yet | sensor owner controls `PHOT_EN` settle/sample/off |
| BLE LPUART1 | HSI 16 MHz kernel, 9600 baud baseline | interrupt/DMA policy deferred until BLE owner pass | comm owner quiesces UART before STOP |
| USB OTG FS | 48 MHz USB clock | USB installer mode blocks deep sleep as required | storage/power owners arbitrate attach/detach |

Related:

- [[Power_and_Sleep_Policy]]
- [[Power_Validation]]
- [[Pending_Measured_Constants_Register]]
- [[HW5_Hardware_Revision_Contract]]
- [[CubeMX_Configuration_Checklist]]
