# HW5 CubeMX Pin Map

This note records the current non-virtual pin assignments from the reference/full-intent CubeMX project `firmware/peepshow_hw5_fw1/peepshow_hw5_fw1.ioc`.

> [!warning] Retired target map
> This is the historical HW5 map. It must not be copied into or used as authority for HW6; use [[HW6_CubeMX_Pin_Map]].

During phased hardware bring-up, the active generated target is `fw0` as defined by [[FW0_Phased_CubeMX_Bring-up_Plan]]. The `fw1` map remains the intended full hardware reference until `fw0` catches up or is promoted.

CubeMX is input, not authority. If this note conflicts with [[HW5_Pin_Ownership_Matrix]], [[HW5_Clock_Tree_Contract]], or [[HW5_DMA_Map]], resolve the conflict before firmware work continues.

## MCU

| Field | Value |
|---|---|
| MCU | `STM32U575RITx` |
| CPN | `STM32U575RIT6` |
| Package | `LQFP64` |
| Cube project | `peepshow_hw5_fw1` reference/full-intent |
| Toolchain | `CMake` |
| Firmware package | `STM32Cube FW_U5 V1.6.0` |

## Assigned Pins

| Pin | Label | Signal | Mode | Owner |
|---|---|---|---|---|
| `PA0` |  | `OCTOSPIM_P2_NCS` | `OCTOSPI1_Port2_NCS` | `thStorage` |
| `PA1` |  | `S_TIM2_CH2` | encoder interface | `thInput` |
| `PA2` |  | `LPUART1_TX` | asynchronous | `thComm` |
| `PA3` |  | `LPUART1_RX` | asynchronous | `thComm` |
| `PA4` | `BTN_START` | `GPXTI4` | EXTI | `thInput` |
| `PA5` |  | `S_TIM2_CH1` | encoder interface | `thInput` |
| `PA6` |  | `OCTOSPIM_P1_IO3` | `OCTOSPI1_IOL_Port1L` | `thStorage` |
| `PA7` |  | `OCTOSPIM_P1_IO2` | `OCTOSPI1_IOL_Port1L` | `thStorage` |
| `PA8` |  | `SAI1_SCK_A` | `SAI_A_Master` | `thAudio` |
| `PA9` |  | `USB_OTG_FS_VBUS` | `Activate_VBUS` | `thStorage` / `thPower` |
| `PA10` |  | `SAI1_SD_A` | `SAI_A_Master` | `thAudio` |
| `PA11` |  | `USB_OTG_FS_DM` | device only | `thStorage` |
| `PA12` |  | `USB_OTG_FS_DP` | device only | `thStorage` |
| `PA13` |  | `DEBUG_JTMS-SWDIO` | SWD trace | debug probe |
| `PA14` |  | `DEBUG_JTCK-SWCLK` | SWD trace | debug probe |
| `PA15` |  | `SPI3_NSS` | hard NSS output | `thDisplay` |
| `PB0` |  | `OCTOSPIM_P1_IO1` | `OCTOSPI1_IOL_Port1L` | `thStorage` |
| `PB1` |  | `OCTOSPIM_P1_IO0` | `OCTOSPI1_IOL_Port1L` | `thStorage` |
| `PB2` | `BUZZ` | `S_LPTIM1_CH1` | LPTIM output | `thAudio` |
| `PB3` |  | `DEBUG_JTDO-SWO` | SWO trace | debug probe |
| `PB4` | `ENC_EN` | `LPGPIO_Output` | low-power GPIO output | `thInput` |
| `PB5` | `BTN_A` | `GPXTI5` | EXTI | `thInput` |
| `PB6` | `BTN_B` | `GPXTI6` | EXTI | `thInput` |
| `PB7` | `BTN_L` | `GPXTI7` | EXTI | `thInput` |
| `PB8` | `BTN_R` | `GPXTI8` | EXTI | `thInput` |
| `PB9` |  | `SAI1_FS_A` | `SAI_A_Master` | `thAudio` |
| `PB10` |  | `OCTOSPIM_P1_CLK` | `O1_P1_CLK` | `thStorage` |
| `PB12` |  | `LPUART1_RTS` | RTS/CTS | `thComm` |
| `PB13` |  | `LPUART1_CTS` | RTS/CTS | `thComm` |
| `PB14` | `MPU_INT` | `GPXTI14` | EXTI rising/falling | `thSensor` |
| `PB15` | `PMIC_INT` | `GPXTI15` | EXTI rising/falling | `thPower` |
| `PC0` |  | `I2C3_SCL` | I2C | `thSensor` |
| `PC1` |  | `I2C3_SDA` | I2C | `thSensor` |
| `PC2` | `PHOT_EN` | `LPGPIO_Output` | low-power GPIO output | `thSensor` |
| `PC3` | `PHOT_ADC` | `ADC1_IN4` | single-ended ADC | `thSensor` |
| `PC4` | `NINA_SW1` | `GPIO_Analog` | analog / high-Z no-pull safe default | `thComm` when deliberately reconfigured |
| `PC5` | `NINA_SW2` | `GPIO_Analog` | analog / high-Z no-pull safe default | `thComm` when deliberately reconfigured |
| `PC6` | `NINA_NRST` | `GPIO_Output` | GPIO output | `thComm` |
| `PC7` | `NINA_DTR` | `GPIO_Analog` | analog / high-Z no-pull safe default | `thComm` when deliberately reconfigured |
| `PC8` | `NINA_DSR` | `GPIO_Analog` | analog / high-Z no-pull safe default | `thComm` when deliberately reconfigured |
| `PC9` | `SD_MODE` | `GPIO_Output` | GPIO output | `thAudio` |
| `PC10` |  | `SPI3_SCK` | TX-only master | `thDisplay` |
| `PC11` | `JOY_INT` | `GPXTI11` | EXTI rising/falling | `thInput` |
| `PC12` |  | `SPI3_MOSI` | TX-only master | `thDisplay` |
| `PC13` | `LCD_1HZ` | `RTC_OUT1` | RTC calibration 1 Hz output | `thDisplay` / `thPower` |
| `PC14` |  | `RCC_OSC32_IN` | LSE external clock source | `thPower` |
| `PD2` | `VLT_LCD` | `LPGPIO_Output` | low-power GPIO output | `thDisplay` |
| `PH3-BOOT0` | `BTN_BOOT` | `GPXTI3` | EXTI after app start; hardware BOOT0 at reset | ROM bootloader before app, then `thInput` maintenance policy |

## Notes

- `PC14` is connected to an external 32.768 kHz MEMS oscillator input, not a watch crystal.
- `PC12` is display `SPI3_MOSI`.
- ADC and LPUART interrupts are intentionally not enabled yet; they will be enabled when the light sensor and BLE owner implementations require them.
- `VLT_LCD` drives TXU0104RUTR active-high OE: high enables level translation, low disables/high-Z.
- `PHOT_EN` and `ENC_EN` are active-high power enables and should default low/off.
- `NINA_NRST` is active-low reset and should remain asserted while BLE is off.
- `NINA_SW1`/`NINA_SW2` are assigned as `GPIO_Analog` high-Z/no-pull safe defaults. `thComm` may reconfigure them only for a documented NINA startup/control mode.
- `NINA_DTR`/`NINA_DSR` are assigned as `GPIO_Analog` high-Z/no-pull safe defaults. Their direction/function is NINA-configuration-dependent and must be proven before firmware drives them.
