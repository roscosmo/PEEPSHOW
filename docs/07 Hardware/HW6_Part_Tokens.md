# HW6 Part Tokens

Canonical part tokens are used across hardware, Platform, Engine, bring-up, and
issue notes.

| Function | Canonical Token | Part Number | Notes |
|---|---|---|---|
| MCU | `MCU_MAIN` | `STM32U575RIT6` | CubeMX device `STM32U575RITx`, LQFP64 |
| Display | `DISPLAY_PANEL` | `LS013B7DH05` | Sharp Memory LCD, native 144 x 168 portrait |
| Display level translator | `DISPLAY_LEVEL_TRANSLATOR` | `TXU0104RUTR` | retained from HW5 and hardwired enabled so EXTCOMIN always passes; no MCU OE GPIO |
| External flash | `FLASH_EXT` | `AT25SL128A` | OCTOSPI1 quad path |
| Speaker amp | `SPEAKER_AMP` | `MAX98357AETE+T` | SAI1 mono speaker path; `SD_MODE` low shutdown |
| Hall/joystick | `JOY_SENSOR` | `TMAG3001A1YBGR` | I2C address `0x34`, threshold interrupt on `JOY_INT` |
| IMU | `IMU_SENSOR` | `LIS2DUX12TR` | I2C address `0x18`, embedded smart functions |
| PMIC/fuel/charger | `PMIC_MAIN` | `ADP5360` | I2C address `0x46`, `PMIC_INT` on `PB15`, Start/MR path |
| BLE/NFC module | `BLE_MODULE` | `NINA-B112-04B` | LPUART1 with RTS/CTS plus NFC capability |

## Removed HW5 Tokens

The following tokens are not populated HW6 parts:

| HW5 Token | HW5 Part | HW6 Status |
|---|---|---|
| `LIGHT_SENSOR` | `TEMT6000X01` | removed |
| `ROTARY_ENCODER` | TTC rotary encoder | removed |
| `BUZZER_DRIVER` | `PAM8904EGPR` | removed |

Related:

- [[HW6_Hardware_Revision_Contract]]
- [[HW6_Delta_From_HW5]]
