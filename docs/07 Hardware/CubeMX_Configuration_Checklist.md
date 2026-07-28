# CubeMX Configuration Checklist

Use this checklist before generating an HW6 firmware base from `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc`.

The imported IOC is the final full-intent design input supplied before board arrival. It has not yet been generated, built, flashed, or validated on HW6. Clear the blockers in [[HW6_Hardware_Documentation_Readiness]] before treating generated output as an active bring-up target.

## Before Generation

- [[HW6_Hardware_Revision_Contract]] identifies the fabricated target.
- [[HW6_Delta_From_HW5]] identifies removed hardware and required software/profile consequences.
- [[HW6_Pin_Ownership_Matrix]] assigns retained pins and records unresolved signals.
- [[HW6_Clock_Tree_Contract]] defines the imported clock assumptions.
- [[HW6_DMA_Map]] defines expected DMA paths.
- [[HW6_Power_Rails]] defines safe boot defaults.
- [[HW6_Wake_Sources]] defines wake-capable EXTI and RTC paths.
- [[HW6_Arrival_Phase0_Checklist]] defines the first hardware-attached sequence.

## Confirmed Removals In The HW6 IOC

- no `ADC1` peripheral and no `PC2` / `PC3` light-sensor path
- no `TIM2` peripheral and no `PA1` / `PA5` / `PB4` encoder path
- no physical `PB2` PAM/piezo output

These removals are intentional. Do not restore them from HW5 generated code or CubeMX settings.

`LPTIM1` is not an orphaned piezo peripheral. It remains configured with internal/no-IO channels for LPBAM timing and must be preserved unless the autonomous-display architecture is explicitly replaced.

## Pre-Generation Blockers

- `ProjectManager.functionlistsort` still names `MX_TIM2_Init` and `MX_ADC1_Init` although TIM2 and ADC1 are absent. Remove or regenerate this stale metadata and verify that no generated initialization is emitted.
- SAI reports a real audio frequency of `16.0 kHz` while serialized `ErrorAudioFreq` reports `-91.66%`. Confirm the intended SAI kernel/sample-rate calculation before audio validation.

Resolved HW6 design inputs:

- `PD2` / `VLT_LCD` is intentionally absent. The display translator is
  hardwired enabled so EXTCOMIN always passes while the display rail is on.
- `PH1` / `PWR_DBG` is a spare output routed to the battery connector for a
  PPK2 logic input. Platform diagnostic builds use an idle-low default and
  define marker meaning per evidence procedure.

## Imported Clock And Bus Checks

- SYSCLK/HCLK: `24 MHz`
- APB1/APB2 peripheral clocks: `12 MHz`; timer clocks: `24 MHz`
- APB3: `3 MHz`
- MSIK: `4 MHz`
- I2C3 kernel: MSIK `4 MHz`, timing `0x00000E14`
- SPI3 kernel: MSIK `4 MHz`, prescaler `2`, approximately `2 Mbit/s`, LSB first, hardware NSS active high
- LPUART1 kernel: HSI16, `115200` baud, RTS/CTS
- PLL2P: `4.096 MHz` for SAI1
- PLL2Q: `128 MHz`; OCTOSPI1 prescaler `8`
- USB kernel: `48 MHz`
- RTC source: external `32.768 kHz` input

These are imported design values, not measured HW6 operating points. Final reactive and realtime operating points remain pending in [[Pending_Measured_Constants_Register]].

## Retained Peripheral Checks

- `BTN_BOOT` remains on `PH3-BOOT0`; BOOT0 can select the ROM bootloader before application firmware runs.
- `PC14` is the external 32.768 kHz clock input, not a crystal connection.
- SPI3 uses `PC10` SCK, `PC12` MOSI, and `PA15` hardware NSS for the display.
- LPUART1 retains NINA RTS/CTS. BLE owner implementation remains interrupt-driven unless [[HW6_DMA_Map]] is explicitly revised.
- USBX MSC, FileX, LevelX, ThreadX, ICACHE, LPBAM, GPDMA1, and LPDMA1 remain present in the full-intent IOC.
- ADP5360 remains at I2C address `0x46`, with `PMIC_INT` on `PB15` and VBUS cross-check on `PA9`.
- LIS2DUX12 remains at I2C address `0x18`, with `MPU_INT` on `PB14`.
- TMAG3001 remains at I2C address `0x34`, with `JOY_INT` on `PC11`.
- NINA-B112 remains on LPUART1. No communication wake capability is granted until HW6 evidence proves one.

## Required GPIO Initial-State Checks

- Display translator: no GPIO initial state exists; HW6 hardware keeps the translator enabled continuously.
- `NINA_NRST` / `PC6`: active low; BLE-off policy holds the module in the Platform-defined safe state until `thComm` begins validated sequencing.
- `NINA_SW1` / `PC4`, `NINA_SW2` / `PC5`, `NINA_DTR` / `PC7`, and `NINA_DSR` / `PC8`: high impedance/analog/no-pull unless a validated NINA mode requires ownership and output drive.
- `SD_MODE` / `PC9`: low shuts down MAX98357A.
- `PWR_DBG` / `PH1`: initialize low/idle. Only bounded Platform diagnostics may drive it, and every capture must document marker polarity and meaning.

There are no HW6 `PHOT_EN`, `ENC_EN`, encoder-channel, light-ADC, or piezo-output initial-state requirements because those physical paths do not exist.

## After Generation

Verify generated code and the resulting `.ioc` against:

- pin ownership and removed-path absence
- alternate functions and GPIO reset levels
- interrupt priorities and wake-source masks
- DMA request mappings and SRAM reachability
- peripheral kernel clocks
- RTC and low-power settings
- USB device settings
- ThreadX/Cube middleware settings
- absence of generated ADC1/TIM2 initialization
- preservation of internal LPTIM1 LPBAM timing

Record the generated-file diff and build result in [[HW6_Brought_Up_Tracker]]. Do not flash merely because generation succeeds.

## Rule

CubeMX is a generator. The vault is the architectural source of truth, and measured target-qualified evidence determines known-good behavior.
