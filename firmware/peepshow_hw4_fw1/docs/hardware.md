# Hardware Architecture

Authoritative specification for the hardware layer of PeepShow V5 (ThreadX).

This document defines electrical domains, peripheral topology, clock tree
(finalized), DMA domains, and hardware-level invariants.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- MCU and peripheral topology
- Electrical enable signals / level translators
- Clock tree (finalized) and kernel clock selections
- DMA domains and STOP2-capable datapaths
- SRAM placement constraints for DMA
- Hardware invariants and forbidden operations

Does NOT define:
- RTOS ownership rules (see rtos_architecture.md)
- STOP2 policy and governors (see power_management.md)
- Rendering algorithms (see display_and_rendering.md)
- Blob/storage architecture (see storage_and_updates.md)

---

## Hardware Overview

| Component | Interface | Notes |
|---|---|---|
| MCU | STM32U575 | 1.8 V core, SmartRun/STOP2 capable |
| Display | Sharp Memory LCD (MIP) | SPI3 + LPDMA, EXTCOMIN management |
| External Flash | AT25SL128A | OCTOSPI1, deep power-down for STOP2 |
| PMIC | ADP5360 | charger + fuel gauge (I2C) |
| Hall joystick | TMAG5273 | I2C (single-owner) |
| Accelerometer | LIS2* | I2C (single-owner), step counter use-case |
| Audio amp | MAX98357A | SAI1 (I2S TX), SD_MODE control |
| LoRa module | Wio E5 | LPUART1 (LSE clocked) |
| Buttons | A/B/L/R/BOOT | EXTI wake capable |
| RTC | LSE-driven | STOP tick authority |

(Naming: “LIS2*” reflects that the exact ST part may change; the electrical/RTOS contracts do not.)

---

## Power Domains and STOP2 Behavior

PeepShow is designed for aggressive STOP2 usage.

Key facts:
- STOP2 disables the CPU core execution domain and SYSCLK-driven activity.
- SmartRun-capable datapaths (LPDMA, kernel clocks, retained SRAM) can remain operational.
- Register access requires APB clocks, but datapaths are driven by *kernel clocks*.

Rule:
APB clock availability does NOT imply datapath functionality.

---

## Clock Tree (Finalized)

The clock tree is finalized in `PeepshowV5.ioc` and is considered authoritative.

### System Clock Modes

Baseline (low power default):
- SYSCLK source: **MSIS**
- SYSCLK: **16 MHz**
- AHB (HCLK): **16 MHz** (`/1`)
- APB1: **8 MHz** (`/2`) — timers x2 → **16 MHz**
- APB2: **8 MHz** (`/2`) — timers x2 → **16 MHz**
- APB3: **2 MHz** (`/8`)

Turbo (REALTIME / performance mode):
- SYSCLK source: **PLL1**
- PLL1 SYSCLK capability: **up to 160 MHz**
- Prescaler steps may be used per governor policy (see power_management.md)

SysTick rule:
- SysTick must be reprogrammed immediately after any SYSCLK/HCLK change to keep
  timeouts and scheduling correct.

### Low-Speed Oscillators

- LSE: **32.768 kHz** (RTC source)
- LSI: **~32 kHz** (fallback / internal low-speed uses)

### Kernel Clock Selections (Critical)

These are intentionally chosen to decouple performance scaling from peripheral timing:

- **MSIK** (kernel MSI) is enabled and used for low-power kernel clocks:
  - SPI3 kernel clock source: **MSIK**
  - I2C3 kernel clock source: **MSIK**
  - Target frequency: **~4 MHz** for both

- **PLL2** is dedicated to “performance peripherals”:
  - PLL2Q: **64 MHz** → OCTOSPI kernel clock (external flash)
  - PLL2P: **4.096 MHz** → SAI1 kernel clock (audio MCLK for 16 kHz @ 256×Fs)

- **HSI48** provides CLK48 domain:
  - USB FS / RNG / CK48 as required

- **LPUART1** kernel clock source: **LSE**
  - Low-power LoRa UART operation

- **USART1** kernel clock source: **HSI**
  - Debug-only UART; should remain disabled in low power unless explicitly requested

PLL3 is configured but unused at this time.

---

## DMA Domains

| DMA | Domain | STOP2-capable | Intended use |
|---|---|---:|---|
| GPDMA | Core | No | High-speed RUN-mode transfers |
| LPDMA | SmartRun | Yes | STOP2-safe SPI/UART paths, low-power transfers |
| LPBAM | SmartRun | Yes | Autonomous low-power sequences (when used) |

Rules:
- STOP2-compatible datapaths must use LPDMA/SmartRun resources.
- ISRs must not perform heavy work; they signal threads only (see rtos_architecture.md).

---

## Peripheral Constraints (Electrical + Clock + DMA)

### Display (Sharp Memory LCD)

- Interface: SPI3
- SPI3 kernel clock: **MSIK (~4 MHz)**
- DMA: **LPDMA only**
- DMA source buffers: **SRAM4** (required)
- Level translator enable: **VLT_LCD**
  - Must be driven to the active level *before* any SPI transaction.
  - On current board revision, active level is **LOW**.
  - **Must remain enabled continuously** (including QUIESCE/STOP paths), because
    EXTCOM passes through this path. Disabling it can leave the panel without EXTCOM.
- Single-flush-in-flight invariant is enforced in the rendering layer.

### External Flash (AT25SL128A)

- Interface: OCTOSPI1
- OCTOSPI kernel clock: **PLL2Q (64 MHz)**
- Flash must enter deep power-down before STOP2 when idle.
- Clock switching or OCTOSPI reconfiguration during an active transfer is forbidden.

### Audio (SAI1 + MAX98357A)

- Interface: SAI1 (I2S TX)
- Kernel clock: **PLL2P (4.096 MHz)**
- Target: **16 kHz, mono**
- Amp SD_MODE:
  - LOW = shutdown
  - HIGH = active
- SD_MODE must default LOW at boot and be asserted only after valid audio data is ready.

### LoRa (LPUART1 + Wio E5)

- Kernel clock: **LSE**
- Baud: 9600
- Intended STOP2-safe operation
- Level translator enable: **VLT_E5**
  - Must be driven to the active level *before* UART activity.
- Any autonomous STOP2 RX behavior must be implemented via SmartRun mechanisms.

### I2C Bus (Sensors + PMIC)

- Devices: TMAG + LIS + ADP5360 (shared bus)
- Kernel clock: **MSIK** (stable across SYSCLK scaling)
- Single-owner policy is mandatory (see peripheral_robustness.md)
- Bus recovery capability is mandatory (stuck SDA/SCL recovery)

---

## Level Translators / Enables

| Signal | Active level | Purpose |
|---|---:|---|
| VLT_LCD | (board-defined) | Enables LCD interface level shifting |
| VLT_E5 | (board-defined) | Enables LoRa UART level shifting |

Rules:
- Translators must be enabled prior to any peripheral communication.
- Default boot states must be known and documented (GPIO init must enforce safe defaults).
- `VLT_LCD` is a special case: keep it continuously enabled during runtime and low-power transitions
  so EXTCOM remains connected to the panel.

(If “active level” differs from earlier revisions, the board schematic wins.)

---

## SRAM Placement Constraints

- SRAM4 is the required placement for LPDMA source buffers used by the display pipeline.
- STOP2-retained buffers must reside in SRAM regions retained across STOP2 (per U5 retention config).

Misplacing DMA buffers can cause:
- hard faults (bus errors)
- silent corruption
- intermittent wake failures

---

## Hardware Invariants (Do Not Violate)

- No peripheral accessed from multiple threads (single-owner model).
- No DMA started from ISR (ISR signals only).
- No clock source or prescaler change during active DMA or bus transfer.
- No STOP2 entry with OCTOSPI active.
- No display SPI transaction unless display translator is enabled.
- Do not disable `VLT_LCD` during normal operation, QUIESCE, or STOP sequencing.
- No LoRa UART transaction unless LoRa translator is enabled.
- Audio amplifier must default to shutdown and only be enabled after valid audio data is queued.

---

## Cross-References

- Peripheral robustness (boot/resume/recovery): `docs/peripheral_robustness.md`
- Power governor, STOP2 sequencing, clock transitions: `docs/power_management.md`
- Peripheral/thread ownership model: `docs/rtos_architecture.md`

---

Last updated: 2026-02-20
