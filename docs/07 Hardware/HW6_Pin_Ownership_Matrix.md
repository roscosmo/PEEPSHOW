# HW6 Pin Ownership Matrix

This matrix must agree with the fabricated schematic, PCB, imported HW6 IOC,
and Platform owner-thread model.

| Peripheral | Instance / Pins | Owner | DMA | Wake | Safe Default |
|---|---|---|---|---|---|
| MCU | `STM32U575RIT6`, LQFP64 | Platform | N/A | N/A | reset defaults |
| Display SPI | `SPI3`: `PC10` SCK, `PC12` MOSI, `PA15` NSS | `thDisplay` | `LPDMA1_CH0` TX | no | disabled until owner init |
| Display EXTCOMIN | `PC13 LCD_1HZ`, RTC output | `thDisplay` / `thPower` | N/A | no | disabled until display policy |
| Display translator | retained TXU0104 path, hardwired enabled | hardware | N/A | no | always enabled while the display rail is powered; no MCU OE GPIO |
| LPBAM cadence | internal/no-IO `LPTIM1` | `thDisplay` config, `thPower` transition coordination | LPDMA trigger path | no | stopped until admitted autonomous sequence |
| Speaker | `SAI1_A`: `PA8` SCK, `PB9` FS, `PA10` SD, `PC9 SD_MODE` | `thAudio` | `GPDMA1_CH3` TX | no | `SD_MODE` low |
| External flash | `OCTOSPI1`: `PA0`, `PB10`, `PB1`, `PB0`, `PA7`, `PA6` | `thStorage` | CH4 RX, CH5 TX | no | idle/deep-power-down by policy |
| Shared I2C bus | `I2C3`: `PC0` SCL, `PC1` SDA | addressed owner with bus serialization | N/A | attached IRQs only | bus idle |
| IMU | `LIS2DUX12TR`, `PB14 MPU_INT`, address `0x18` | `thSensor` | N/A | yes, policy-defined | lowest validated mode |
| PMIC | `ADP5360`, `PB15 PMIC_INT`, address `0x46` | `thPower` | N/A | yes | monitor after owner start |
| Joystick | `TMAG3001A1YBGR`, `PC11 JOY_INT`, address `0x34` | `thInput` | N/A | yes, policy-defined | threshold mode or off |
| Buttons | `PA4`, `PB5`, `PB6`, `PB7`, `PB8`, `PH3` | `thInput`; Start power intent coordinated with `thPower` | N/A | yes | board-defined pulls; BOOT0 remains hardware-owned at reset |
| BLE/NFC | `LPUART1` on `PA2/PA3/PB12/PB13`; controls `PC4..PC8` | `thComm` | none in IOC | no dedicated wake | reset asserted while off; auxiliary controls high-Z |
| USB device | `PA11` DM, `PA12` DP, `PA9` VBUS | `thStorage` / `thPower` | none in IOC | VBUS detect policy | detached/not exported |
| Debug | `PA13` SWDIO, `PA14` SWCLK, `PB3` SWO | debug probe | N/A | no | probe-controlled |
| Power debug | `PH1 PWR_DBG`, battery-connector PPK2 logic output | Platform diagnostics | N/A | no | low/idle; dev-only marker transitions require an explicit evidence procedure |
| RTC source | `PC14` external 32.768 kHz input | `thPower` | N/A | internal RTC wake | external source input |

## Removed Paths

HW6 has no light-sensor, rotary-encoder, or PAM/piezo owner path. Their former
pins and peripherals are not available to Engine or packages and must not be
claimed by another owner without a hardware-contract change.

Related:

- [[HW6_Hardware_Revision_Contract]]
- [[RTOS_Ownership_and_Queue_Topology]]
