# HW6 Wake Sources

This note records the HW6 wake-capable sources represented by the fabricated
design and imported IOC.

| Wake Source | Electrical Path | Owner | Allowed Context | Validation State |
|---|---|---|---|---|
| RTC | internal RTC wake using external source on `PC14` | `thPower` | scheduled low-power wake | pending HW6 proof |
| Boot button | `PH3 BTN_BOOT`; BOOT0 at reset, EXTI3 after app starts | ROM bootloader, then `thInput` | boot/maintenance | pending HW6 proof |
| Start | `PA4 BTN_START`, EXTI4, ADP5360 MR path | `thInput` / `thPower` | initial system activation gesture and primary power intent | pending HW6 proof |
| A | `PB5`, EXTI5 | `thInput` | policy-defined input wake | pending HW6 proof |
| B | `PB6`, EXTI6 | `thInput` | policy-defined input wake | pending HW6 proof |
| L | `PB7`, EXTI7 | `thInput` | policy-defined input wake | pending HW6 proof |
| R | `PB8`, EXTI8 | `thInput` | policy-defined input wake | pending HW6 proof |
| Joystick | `PC11 JOY_INT`, EXTI11 | `thInput` / `thPower` | threshold-armed contexts | pending HW6 proof |
| IMU | `PB14 MPU_INT`, EXTI14 | `thSensor` / `thPower` | admitted motion contexts | pending HW6 proof |
| PMIC | `PB15 PMIC_INT`, EXTI15 | `thPower` | charger/battery/fault policy | FW0 ISR-to-`thPower` scaffold implemented; no PMIC_INT edge has been target-validated yet |
| USB VBUS | `PA9` plus PMIC VBUS classification | `thPower` / USB policy | external power/detect | active-runtime ADP5360/PA9 agreement validated for absent and present VBUS; STOP/wake behavior remains pending |

## Explicitly Absent Wake Paths

- no rotary-encoder activity or `TIM2_IRQn`
- no ambient-light threshold or ADC wake
- no BLE/NINA dedicated module wake pin
- `LPTIM1` autonomous cadence is not a package wake source

Unknown wake reasons remain defects until explained by HW6 evidence.

Related:

- [[Power_and_Sleep_Policy]]
- [[HW6_Brought_Up_Tracker]]
- [[Debug_and_Observability]]

