# HW5 Power Rails

This note records HW5 power rails, enables, safe defaults, and mode-transition behavior.

> [!warning] Retired target rail record
> This table applies only to historical HW5. Use [[HW6_Power_Rails]] for active work; HW6 intentionally replaces the HW5 `PD2` / `VLT_LCD` control with a hardwired-enabled translator path.

| Rail / Enable | Controlled By | Active Polarity | Boot Default | Sleep Behavior | Notes |
|---|---|---|---|---|---|
| LCD level translator OE | `PD2` `VLT_LCD` | active-high enable: drive high to enable TXU0104RUTR outputs, drive low to place outputs high-Z | firmware safe default is low / disabled until display owner enables | default high / enabled while display holds image; duty-cycle is experimental | TXU0104RUTR OE, controlled by `thDisplay`; generated init must avoid accidental high-enable unless display policy wants it |
| LCD EXTCOMIN / VCOM | `PC13` `LCD_1HZ` / RTC_OUT1 | RTC 1 Hz calibration output | disabled until display policy enables | active while display holds image | requires `VLT_LCD` active so signal reaches panel |
| Light sensor enable | `PC2` `PHOT_EN` | active high power enable | low / off | low / off in sleep unless bounded sample/stream lease is active | controlled by `thSensor`; requires settle time before ADC sample |
| Rotary encoder enable | `PB4` `ENC_EN` | active high power enable | low / off | low / off unless interactive mode or `ENC_WAKE_ARMED` requires it | controlled by `thInput` |
| Joystick threshold mode | TMAG3001A1YBGR register config | register-defined | off until requested | threshold mode allows joystick to sleep between readings | controlled by `thInput` |
| IMU low-power mode | LIS2DUX12TR register config | register-defined | off until requested | step-counter mode holds lowest sleep that preserves embedded functions | controlled by `thSensor`, power floor published to `thPower` |
| PMIC / charger / battery monitor | ADP5360 over I2C3 | register-defined | monitor after probe | may wake/notify through `PMIC_INT` | address `0x46`; controlled by `thPower` |
| Critical battery disconnect | ADP5360 ISOFET control | register/hardware-defined | inactive | disconnect at critical threshold | do not use shipping mode for critical battery |
| BLE module reset | `PC6` `NINA_NRST` | active-low reset | firmware safe default asserted low until `thComm` intentionally releases module | assert reset or use validated module sleep policy before deep sleep unless comm policy keeps active | controlled by `thComm` |
| BLE mode pins | `PC4` `NINA_SW1`, `PC5` `NINA_SW2` | NINA system-control inputs; low during startup has special behavior | high-Z / no-pull unless an intentional validated startup mode needs them | leave high-Z / no-pull unless required | do not drive low during reset release unless intentionally entering bootloader/default-reset behavior |
| BLE flow/control pins | `PC8` `NINA_DSR`, `PC7` `NINA_DTR`, `PB12` RTS, `PB13` CTS | DSR/DTR behavior is NINA configuration-dependent; RTS/CTS are UART flow control | leave DSR/DTR high-Z / no-pull unless configured; RTS/CTS owned by UART | quiesced before STOP | DSR may control command/connectable/UART/sleep behavior; DTR may indicate module connection/status depending on config |
| Speaker amp shutdown | `PC9` `SD_MODE` | active high enable, low shutdown | low / shutdown | low before deep sleep | MAX98357A `SD` pin, controlled by `thAudio` |
| External flash deep power-down | AT25SL128A command path over OCTOSPI1 | command-defined | idle after boot probe | deep power-down whenever idle and policy allows | controlled by `thStorage` |
| BBB piezo output | `PB2` `BUZZ` / LPTIM1_CH1 | signal active while pattern plays | off | off before deep sleep | PAM8904 auto-shutdown when no DIN signal is present |

Related:

- [[Power_and_Sleep_Policy]]
- [[HW5_Hardware_Revision_Contract]]
- [[HW5_Wake_Sources]]
