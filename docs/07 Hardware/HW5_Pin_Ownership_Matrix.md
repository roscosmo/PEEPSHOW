# HW5 Pin Ownership Matrix

This matrix must agree with the schematic, PCB, CubeMX `.ioc`, and Platform owner-thread model.

> [!warning] Retired target matrix
> This matrix applies only to historical HW5. The active ownership authority is [[HW6_Pin_Ownership_Matrix]].

| Peripheral | Instance | Pins | Owner Thread | DMA | Wake Source | Safe Default |
|---|---|---|---|---|---|---|
| MCU | `STM32U575RIT6` / `STM32U575RITx` | LQFP64 | Platform | N/A | N/A | reset defaults |
| Display SPI | `SPI3` TX-only master to Sharp `LS013B7DH05` | `PC10` SCK, `PC12` MOSI, `PA15` NSS | `thDisplay` | `LPDMA1_CH0` TX | No | disabled |
| Display support | RTC EXTCOMIN output / TXU0104 level translator OE | `PC13` `LCD_1HZ`, `PD2` `VLT_LCD` | `thDisplay` / `thPower` | N/A | No | `VLT_LCD` low/disabled before display init; high/enabled with `LCD_1HZ` active while display holds image |
| Speaker audio | `SAI1_A` master to `MAX98357AETE+T` | `PA8` SCK, `PB9` FS, `PA10` SD, `PC9` `SD_MODE` | `thAudio` | `GPDMA1_CH3` TX | No | `SD_MODE` low shutdown |
| BBB piezo | `LPTIM1_CH1` to `PAM8904EGPR` | `PB2` `BUZZ` | `thAudio` | N/A | No | no output, PAM auto-shutdown |
| External flash | `AT25SL128A` via `OCTOSPI1` quad | `PA0` NCS, `PB10` CLK, `PB1` IO0, `PB0` IO1, `PA7` IO2, `PA6` IO3 | `thStorage` | `GPDMA1_CH4` RX, `GPDMA1_CH5` TX | No | idle / deep-power-down when allowed |
| Shared input/sensor/power I2C bus | `I2C3` | `PC0` SCL, `PC1` SDA | addressed device owner with bus serialization | N/A | optional through attached IRQ lines | bus idle |
| IMU | `LIS2DUX12TR` over `I2C3` plus EXTI14 | `PC0` SCL, `PC1` SDA, `PB14` `MPU_INT`, address `0x18` | `thSensor` | N/A | Yes | off unless requested; step-counter mode may hold sleep floor |
| PMIC / fuel / charger | `ADP5360` over `I2C3` plus EXTI15 | `PC0` SCL, `PC1` SDA, `PB15` `PMIC_INT`, address `0x46` | `thPower` | N/A | Yes | monitor/probe after power owner starts |
| Joystick / hall | `TMAG3001A1YBGR` over `I2C3` plus EXTI11 | `PC0` SCL, `PC1` SDA, `PC11` `JOY_INT`, address `0x34` | `thInput` | N/A | Yes | threshold armed or off; safe mode if uncalibrated |
| Light sensor | `ADC1_IN4` plus enable | `PC3` `PHOT_ADC`, `PC2` `PHOT_EN` | `thSensor` | N/A | No | sensor disabled |
| Rotary encoder | `TIM2` encoder interface plus enable | `PA5` CH1, `PA1` CH2, `PB4` `ENC_EN` | `thInput` | N/A | Conditional through `TIM2_IRQn` when `ENC_WAKE_ARMED` | encoder disabled |
| Buttons | GPIO EXTI plus BOOT0 hardware path | `PA4` `BTN_START`, `PB5` `BTN_A`, `PB6` `BTN_B`, `PB7` `BTN_L`, `PB8` `BTN_R`, `PH3` `BTN_BOOT` / `BOOT0` | `thInput` / `thPower` for Start power intent after application starts; ROM bootloader owns BOOT0 before app | N/A | Yes, policy-defined after app start | `A/B/L/R/BOOT` active high pulldown; Start active low pullup through ADP5360 `MR` path; BOOT0 can bypass firmware at reset |
| BLE module | `NINA-B112-04B` over `LPUART1` plus control GPIO | `PA2` TX, `PA3` RX, `PB12` RTS, `PB13` CTS, `PC4` `NINA_SW1`, `PC5` `NINA_SW2`, `PC6` `NINA_NRST`, `PC7` `NINA_DTR`, `PC8` `NINA_DSR` | `thComm` | none in `fw1` reference `.ioc`; interrupt/DMA deferred | No | `NINA_NRST` low/reset asserted while off; SW1/SW2/DTR/DSR high-Z/no-pull unless validated mode requires them |
| USB device | `USB_OTG_FS` detection path and USBX personality | `PA11` DM, `PA12` DP, `PA9` VBUS | `thStorage` / `thPower` | none in `fw1` reference `.ioc` | VBUS attach for power/detect policy | detached / not exported; USB activity/enumeration gates MSC availability |
| Debug | SWD / SWO | `PA13` SWDIO, `PA14` SWCLK, `PB3` SWO | debug probe | N/A | No | probe-controlled |
| RTC clock | LSE external source | `PC14` `RCC_OSC32_IN` | `thPower` | N/A | RTC wake internal | external MEMS oscillator input |

Related:

- [[HW5_Hardware_Revision_Contract]]
- [[RTOS_Ownership_and_Queue_Topology]]
- [[CubeMX_Configuration_Checklist]]
