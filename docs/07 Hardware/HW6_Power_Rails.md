# HW6 Power Rails

This note records expected HW6 rail controls and safe defaults. First-unit
arrival evidence is partial; subsystem configuration and low-power behavior
remain `pending_validation` until their dedicated tests run.

| Rail / Control | Control | Boot Default | Waiting / STOP Behavior | Status |
|---|---|---|---|---|
| MCU/system rail | ADP5360 buck, expected 1.8 V baseline | hardware/PMIC defined | retained as required by selected sleep backend | unit 001: 1.8 V measured; register `0x2A=0x18` selects 1.8 V; VOUT1OK asserted |
| 3.3 V peripheral/display rail | ADP5360 buck-boost | hardware/PMIC defined | remains enabled for the retained display architecture unless measured policy changes | unit 001: 3.3 V measured; register `0x2C=0x13` selects 3.3 V; VOUT2OK asserted through hardware EN2 control |
| Display EXTCOMIN | `PC13 LCD_1HZ` | disabled until display policy | active while the panel holds an image | pending HW6 display proof |
| Display translator | retained TXU0104 path, hardwired enabled | hardware-enabled; no MCU OE control | remains enabled so EXTCOMIN can always pass while the display rail is powered | frozen design; verify electrically on HW6 |
| Joystick low-power mode | TMAG3001 register policy | off until owner init | threshold mode or off | pending measurement |
| IMU low-power mode | LIS2DUX12 register policy | off until owner init | lowest validated embedded-function mode | pending measurement |
| PMIC/fuel/charger | ADP5360 over I2C3 | `thPower` applies conservative charger profile, enables `EN_MR_SD`, prepares fuel gauge, then monitors | may notify through `PMIC_INT`; PMIC_INT edge capture is scaffolded, event behavior pending | unit 001 full read map, no-fault state, guarded six-byte reversible profile transaction, normal-boot `EN_MR_SD`, fuel-gauge prepare, valid VBAT reads, low/critical battery scaffold, real-cell/VBUS charger status, and boot-applied charger profile readback are proven; conservative profile readback is `0x02=0x81`, `0x03=0x82`, `0x04=0x29`, `0x07=0xAC`, `0x0A=0x80`; charge-current promotion, termination/full, JEITA substitution, protection fault tests, PMIC_INT event behavior, and production charging policy remain open |
| BLE reset | `PC6 NINA_NRST`, active low | asserted while module is off | validated module sleep or reset policy | pending HW6 proof |
| BLE auxiliary controls | `PC4/PC5/PC7/PC8` | analog high-Z/no-pull | high-Z unless validated mode requires drive | IOC confirmed |
| Speaker shutdown | `PC9 SD_MODE`, low shutdown | low | low before STOP | pending HW6 proof |
| External flash deep power-down | OCTOSPI command | idle | deep-power-down when policy permits | pending HW6 proof |
| Power debug marker | `PH1 PWR_DBG`, battery connector | low/idle | remains low unless an explicit development evidence procedure drives a timing marker | first-unit idle-low baseline and 250 ms PPK2 D7 route verified |

## Removed Loads And Enables

HW6 has no `PHOT_EN`, `ENC_EN`, or PAM/piezo load. Power policy must not retain
leases, wake floors, or shutdown sequencing for those absent circuits.

Related:

- [[Power_and_Sleep_Policy]]
- [[PMIC_and_Power_Contract]]
- [[HW6_Wake_Sources]]
