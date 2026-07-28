# HW6 CubeMX Pin Map

This note records the non-virtual pin assignments from
`firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc`.

The IOC is a full-intent design input. It is not permission to generate or
flash firmware until [[HW6_Hardware_Documentation_Readiness]] is satisfied.

CubeMX is input, not authority. Conflicts with [[HW6_Pin_Ownership_Matrix]],
[[HW6_Clock_Tree_Contract]], [[HW6_DMA_Map]], or the fabricated schematic must
be resolved before generation.

## MCU

| Field | Value |
|---|---|
| MCU | `STM32U575RITx` |
| CPN | `STM32U575RIT6` |
| Package | `LQFP64` |
| Cube project | `PEEPSHOW_HW6_FW1` full-intent |
| Toolchain | `CMake` |
| Firmware package | `STM32Cube FW_U5 V1.6.0` |
| SYSCLK/HCLK baseline | `24 MHz` |

## Assigned Pins

| Pin | Label | Signal | Mode | Owner |
|---|---|---|---|---|
| `PA0` |  | `OCTOSPIM_P2_NCS` | OCTOSPI1 NCS | `thStorage` |
| `PA2` |  | `LPUART1_TX` | asynchronous | `thComm` |
| `PA3` |  | `LPUART1_RX` | asynchronous | `thComm` |
| `PA4` | `BTN_START` | `EXTI4` | EXTI | `thInput` / `thPower` |
| `PA6` |  | `OCTOSPIM_P1_IO3` | OCTOSPI1 IO | `thStorage` |
| `PA7` |  | `OCTOSPIM_P1_IO2` | OCTOSPI1 IO | `thStorage` |
| `PA8` |  | `SAI1_SCK_A` | SAI master | `thAudio` |
| `PA9` |  | `USB_OTG_FS_VBUS` | VBUS detect | `thPower` / `thStorage` |
| `PA10` |  | `SAI1_SD_A` | SAI master | `thAudio` |
| `PA11` |  | `USB_OTG_FS_DM` | USB device | `thStorage` |
| `PA12` |  | `USB_OTG_FS_DP` | USB device | `thStorage` |
| `PA13` |  | `SWDIO` | debug | debug probe |
| `PA14` |  | `SWCLK` | debug | debug probe |
| `PA15` |  | `SPI3_NSS` | hardware NSS output | `thDisplay` |
| `PB0` |  | `OCTOSPIM_P1_IO1` | OCTOSPI1 IO | `thStorage` |
| `PB1` |  | `OCTOSPIM_P1_IO0` | OCTOSPI1 IO | `thStorage` |
| `PB3` |  | `SWO` | asynchronous trace | debug probe |
| `PB5` | `BTN_A` | `EXTI5` | EXTI | `thInput` |
| `PB6` | `BTN_B` | `EXTI6` | EXTI | `thInput` |
| `PB7` | `BTN_L` | `EXTI7` | EXTI | `thInput` |
| `PB8` | `BTN_R` | `EXTI8` | EXTI | `thInput` |
| `PB9` |  | `SAI1_FS_A` | SAI master | `thAudio` |
| `PB10` |  | `OCTOSPIM_P1_CLK` | OCTOSPI1 clock | `thStorage` |
| `PB12` |  | `LPUART1_RTS` | hardware flow control | `thComm` |
| `PB13` |  | `LPUART1_CTS` | hardware flow control | `thComm` |
| `PB14` | `MPU_INT` | `EXTI14` | rising/falling EXTI | `thSensor` |
| `PB15` | `PMIC_INT` | `EXTI15` | rising/falling EXTI | `thPower` |
| `PC0` |  | `I2C3_SCL` | I2C | serialized bus owners |
| `PC1` |  | `I2C3_SDA` | I2C | serialized bus owners |
| `PC4` | `NINA_SW1` | `GPIO_Analog` | high-Z/no-pull default | `thComm` when reconfigured |
| `PC5` | `NINA_SW2` | `GPIO_Analog` | high-Z/no-pull default | `thComm` when reconfigured |
| `PC6` | `NINA_NRST` | `GPIO_Output` | active-low reset | `thComm` |
| `PC7` | `NINA_DTR` | `GPIO_Analog` | high-Z/no-pull default | `thComm` when reconfigured |
| `PC8` | `NINA_DSR` | `GPIO_Analog` | high-Z/no-pull default | `thComm` when reconfigured |
| `PC9` | `SD_MODE` | `GPIO_Output` | speaker shutdown/enable | `thAudio` |
| `PC10` |  | `SPI3_SCK` | TX-only master | `thDisplay` |
| `PC11` | `JOY_INT` | `EXTI11` | rising/falling EXTI | `thInput` |
| `PC12` |  | `SPI3_MOSI` | TX-only master | `thDisplay` |
| `PC13` | `LCD_1HZ` | `RTC_OUT1` | 1 Hz calibration output | `thDisplay` / `thPower` |
| `PC14` |  | `RCC_OSC32_IN` | external LSE clock input | `thPower` |
| `PH1` | `PWR_DBG` | `GPIO_Output` | idle-low development timing marker routed through the battery connector to a PPK2 logic input | Platform diagnostics |
| `PH3-BOOT0` | `BTN_BOOT` | `EXTI3` after app start | BOOT0 at reset | ROM bootloader, then `thInput` |

## Deliberately Absent Assignments

| Pins | Removed HW5 Function | HW6 Rule |
|---|---|---|
| `PA1`, `PA5`, `PB4` | rotary encoder and `ENC_EN` | remain unassigned; no `TIM2` encoder path |
| `PC2`, `PC3` | `PHOT_EN` and `ADC1_IN4` | remain unassigned; no ambient-light ADC path |
| `PB2` | `BUZZ` / physical `LPTIM1_CH1` | remain unassigned; no PAM/piezo output |

Unassigned removed-function pins retain reset-safe behavior unless a future
hardware contract explicitly assigns them. They are not spare package-facing
GPIO.

## Reconciliation Notes

- `LPTIM1` remains as an internal/no-IO LPBAM timing resource.
- The MCU/IP list omits `TIM2` and `ADC1`, but the serialized function-order
  list still names their initializers. Generated code must not emit them.
- `PD2 VLT_LCD` is intentionally absent. The HW6 display translator is
  hardwired enabled so EXTCOMIN always reaches the powered panel.
- `PH1 PWR_DBG` is an idle-low, development-only Platform diagnostic output to
  the battery connector. Each evidence procedure defines any pulse/high-level
  meaning; normal packages cannot drive it.
- NINA SW1/SW2/DTR/DSR remain analog high-Z/no-pull defaults.
