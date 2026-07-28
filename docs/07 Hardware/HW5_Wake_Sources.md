# HW5 Wake Sources

This note records all wake-capable sources and their Platform handling path.

> [!warning] Retired target wake map
> This table applies only to historical HW5. Use [[HW6_Wake_Sources]]; the HW5 rotary wake path does not exist on HW6.

| Wake Source | Electrical Path | Owner | Debounce / Filter | Allowed Modes | Notes |
|---|---|---|---|---|---|
| RTC alarm | internal RTC wake, `PC14` external MEMS LSE source | `thPower` | N/A | all low-power modes | CubeMX RTC wake internal enabled |
| Boot button | `PH3` `BTN_BOOT` / `BOOT0`, `EXTI3` only after application firmware is running | ROM bootloader before app, then `thInput` maintenance policy | required for application-visible edges | bootloader before app; maintenance only after app starts | BOOT0 pin, pulldown, active high; firmware cannot intercept ROM bootloader entry at reset |
| Start button | `PA4` `BTN_START`, `EXTI4` | `thInput` / `thPower` | required | primary wake/sleep/power-intent path | pullup, active low through BAT54 into ADP5360 `MR`; long hold enters shipping mode |
| A button | `PB5` `BTN_A`, `EXTI5` | `thInput` | required | optional/contextual | pulldown, 0.1 uF debounce cap, active high |
| B button | `PB6` `BTN_B`, `EXTI6` | `thInput` | required | optional/contextual | pulldown, 0.1 uF debounce cap, active high |
| L button | `PB7` `BTN_L`, `EXTI7` | `thInput` | required | optional/contextual | pulldown, 0.1 uF debounce cap, active high |
| R button | `PB8` `BTN_R`, `EXTI8` | `thInput` | required | optional/contextual | pulldown, 0.1 uF debounce cap, active high |
| Rotary encoder activity | `TIM2_CH1/CH2` on `PA5`/`PA1`, `TIM2_IRQn`, powered by `PB4` `ENC_EN` | `thInput` | timer/counter filtering required | only when `ENC_WAKE_ARMED` and selected sleep class supports TIM2 activity | STOP-compatible wake must be validated on HW5 |
| Joystick / hall IRQ | `PC11` `JOY_INT`, `EXTI11` | `thInput` / `thPower` | TMAG threshold filter | policy-defined | threshold interrupt from TMAG3001; read over I2C3 to determine cardinal direction(s) |
| IMU IRQ | `PB14` `MPU_INT`, `EXTI14`, LIS2DUX12 `INT1` | `thSensor` / `thPower` | required | policy-defined | motion, tap/shake, tilt/orientation, diagnostics, or future policy events; step counting is normally polled and must not wake on every step |
| PMIC / fuel IRQ | `PB15` `PMIC_INT`, `EXTI15` | `thPower` | required | policy-defined | ADP5360 charger, battery, or fault notification |
| USB VBUS attach | `PA9` VBUS plus PMIC VBUS classification | `thPower` / USB policy | required | power/USB detection policy | external power wake/detect only; USB protocol activity or enumeration must gate MSC availability |

## Not Wake Sources In Current `.ioc`

- Light sensor uses `PHOT_EN` and `ADC1_IN4`; no threshold interrupt is assigned.
- Rotary encoder wake is conditional through `ENC_WAKE_ARMED`; exact sleep depth support must be proven during bring-up.
- BLE/NINA has no dedicated module IRQ or wake pin assigned.

Unknown wake reasons are defects until explained with evidence.

Related:

- [[Power_and_Sleep_Policy]]
- [[Brought_Up_Tracker]]
- [[Debug_and_Observability]]
