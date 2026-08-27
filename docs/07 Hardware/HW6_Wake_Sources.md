# HW6 Wake Sources

This note records the HW6 wake-capable sources represented by the fabricated
design and imported IOC.

| Wake Source | Electrical Path | Owner | Allowed Context | Validation State |
|---|---|---|---|---|
| RTC | internal RTC wake using external source on `PC14` | `thPower` | scheduled low-power wake | pending HW6 proof |
| Boot button | `PH3 BTN_BOOT`; BOOT0 at reset, EXTI3 after app starts | ROM bootloader, then `thInput` | boot/maintenance | pending HW6 proof |
| Start | `PA4 BTN_START`, EXTI4, ADP5360 MR path | `thInput` / `thPower` | initial system activation gesture and primary power intent | target-proven STOP2 wake and wake classification; system `ACTIVE`/`INACTIVE` consumption remains to be implemented |
| A | `PB5`, EXTI5 | `thInput` | policy-defined input wake | target-proven ordered STOP2 wake with the press delivered once |
| B | `PB6`, EXTI6 | `thInput` | policy-defined input wake | target-proven ordered STOP2 wake with the press delivered once |
| L | `PB7`, EXTI7 | `thInput` | policy-defined input wake | target-proven ordered STOP2 wake with the press delivered once |
| R | `PB8`, EXTI8 | `thInput` | policy-defined input wake | target-proven ordered STOP2 wake with the press delivered once |
| Joystick | `PC11 JOY_INT`, EXTI11 | `thInput` / `thPower` | Platform-admitted STATE and shell movement wake | target-proven X/Y omnipolar wake for all four cardinals, persistent calibration, bounded post-wake direction confirmation, and logical delivery; long neutral false-wake soak remains open |
| IMU | `PB14 MPU_INT`, EXTI14 | `thSensor` / `thPower` | admitted motion contexts | pending HW6 proof |
| PMIC | `PB15 PMIC_INT`, EXTI15 | `thPower` | charger/battery/fault policy | active-runtime EXTI15 routing, ADP5360 interrupt enable/read/flag-clear, and `thPower` snapshot consumption are target-proven; STOP2 wake classification remains pending |
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

