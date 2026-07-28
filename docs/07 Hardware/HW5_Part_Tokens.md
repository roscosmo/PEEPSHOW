# HW5 Part Tokens

Canonical part tokens are used across hardware, Platform, Engine, bring-up, and issue notes.

> [!warning] Retired target parts
> This table applies only to historical HW5. Use [[HW6_Part_Tokens]] for active work.

| Function | Canonical Token | Part Number | Notes |
|---|---|---|---|
| MCU | `MCU_MAIN` | `STM32U575RIT6` | CubeMX device `STM32U575RITx`, LQFP64 |
| Display | `DISPLAY_PANEL` | `LS013B7DH05` | Sharp Memory LCD, native 144 x 168 portrait, used as 168 x 144 landscape |
| Display level translator | `DISPLAY_LEVEL_TRANSLATOR` | `TXU0104RUTR` | OE is active-high; `VLT_LCD` high enables output, low places outputs high-Z |
| External flash | `FLASH_EXT` | `AT25SL128A` | external serial NOR on OCTOSPI1 quad path |
| Speaker amp | `SPEAKER_AMP` | `MAX98357AETE+T` | SAI1/I2S mono speaker path to 1 W 20 mm speaker; `SD_MODE` low shutdown |
| Buzzer driver | `BUZZER_DRIVER` | `PAM8904EGPR` | piezo BBB path driven by `BUZZ`; auto-shutdown when no DIN signal is present |
| Hall/joystick | `JOY_SENSOR` | `TMAG3001A1YBGR` | I2C address `0x34`, threshold interrupt on `JOY_INT` |
| IMU | `IMU_SENSOR` | `LIS2DUX12TR` | I2C address `0x18`, embedded smart functions and step counter |
| Light sensor | `LIGHT_SENSOR` | `TEMT6000X01` | analog ambient light sensor powered by `PHOT_EN`, sampled through `PHOT_ADC` |
| Rotary encoder | `ROTARY_ENCODER` | TTC green encoder, exact model unknown | TIM2 quadrature encoder plus `ENC_EN`; exact model number not available |
| PMIC/fuel/charger | `PMIC_MAIN` | `ADP5360` | I2C address `0x46`, `PMIC_INT` on `PB15`, `BTN_START` connects to `MR` path |
| BLE module | `BLE_MODULE` | `NINA-B112-04B` | LPUART1 with RTS/CTS and NINA control pins |

Related: [[HW5_Hardware_Revision_Contract]]
