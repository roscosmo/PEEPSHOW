# PMIC and Power Contract

This document defines the active HW6 Platform contract for the ADP5360 PMIC, battery policy, VBUS detection, shipping-mode handling, and system power state ownership. HW5 measurements remain historical evidence only; retained behavior must be revalidated on HW6 before it is published as a granted target capability.

Related:

- [[Power_Architecture_Index]]
- [[Power_and_Sleep_Policy]]
- [[Button_Input_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[HW6_Revalidation_Matrix]]

---

## Hardware

PMIC: `ADP5360`.

Current HW6 battery planning basis, pending incoming-board validation:

- Bring-up battery simulator / source meter: Nordic Power Profiler Kit II (`PPK2`) or equivalent controlled source.
- Previous cell family: `LIR2540` rechargeable coin cell.
- Current cell family: `303040` flat LiPo pouch cell.
- Current seller-stated capacity: `450 mAh`, treated as unverified until measured.

Connections carried into the final HW6 design/IOC intent and requiring board-level revalidation:

- ADP5360 is connected to MCU I2C at address `0x46`.
- ADP5360 interrupt is `PMIC_INT` on `PB15` / `EXTI15`.
- `BTN_START` is connected to the ADP5360 `MR` path.
- VBUS can be detected through ADP5360 status and through the MCU USB VBUS sense path.
- `USB_OTG_FS_VBUS` is on `PA9`.

Per [[Platform_Hardware_Abstraction_Contract]], the PMIC driver uses the public 7-bit address `0x46`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer.

---

## Ownership

- `thPower` owns PMIC configuration, battery policy, charger state, VBUS classification, sleep entry, wake classification, and power fault escalation.
- `thInput` owns START button edge/hold classification and publishes shipping-intent events to `thPower`.
- Other owners publish activity blockers or quiesce acknowledgements; they do not directly enter STOP or change power policy.
- The Reference Game and Engine express power intent only.

---

## Power Policy Decisions

- Low battery threshold causes forced sleep.
- Critical battery threshold disconnects the ISOFET.
- Critical battery must not enter shipping mode as the battery response, because START can wake the PMIC/device and create a shipping/wake loop.
- The device may run normally while charging.
- Flashing/install mode is the exception: charging does not imply normal runtime use while the device is in flashing mode.
- All main power rails may remain active; power savings come primarily from MCU sleep, peripheral low-power modes, local enables, and device-specific shutdown/deep-power-down.

---

## START / Shipping-Mode Rule

START is both a normal system button and the ADP5360 `MR` shipping-mode path.

Rules:

- firmware may detect START hold intent early
- firmware should save state and quiesce before the hardware shipping threshold
- firmware may display warning/countdown UI
- firmware must not assume it can prevent shipping mode once the ADP5360 threshold is reached
- low-battery and critical-battery policy must not use shipping mode as the automatic response

---

## VBUS Detection

VBUS may be classified from:

- ADP5360 charger/input status
- MCU `USB_OTG_FS_VBUS` on `PA9`

Both paths should be reconciled by `thPower`.

Disagreement between the PMIC VBUS view and MCU VBUS view is a diagnostic event until explained. It must not silently change installer/storage ownership.

VBUS classification means external USB power is present. It does not by itself prove a USB data host exists.

Rules:

- VBUS may wake the device, update charger policy, and notify USB policy.
- VBUS alone must not prompt for MSC flashing/export mode.
- USB protocol activity or successful host enumeration must gate MSC availability through [[Storage_and_Installer_Contract]].
- power-only chargers and USB-C power banks remain charger/external-power cases when no usable USB data-host activity is observed.

---

## System Power FSM

This state machine describes what the whole device is doing from a power-management perspective.

| State | Meaning |
|---|---|
| `PWR_BOOTING` | MCU has started, but clocks, rails, PMIC state, reset reason, and owner threads are not trusted yet. |
| `PWR_RAIL_VALIDATE` | Firmware validates power conditions, reset cause, required rails, and PMIC status before normal operation. |
| `PWR_ACTIVE_LP` | Awake low-power operation. Display/UI may update slowly, sensors are mostly off, and STOP residency is preferred. |
| `PWR_ACTIVE_RT` | Awake realtime operation. Used for gameplay, realtime display, audio, sensor streaming, install activity, or other high-duty work. |
| `PWR_SLEEP_PREP` | Owners quiesce DMA/peripherals, save required state, and arm approved wake sources. |
| `PWR_STOP_RESIDENT` | MCU is in STOP/low-power sleep. Display hold, RTC, and armed wake sources remain valid. |
| `PWR_WAKE_RESUME` | MCU woke and is restoring clocks, classifying wake reason, and resuming owners. |
| `PWR_FORCED_SLEEP` | Battery or policy requires sleep regardless of normal runtime intent. |
| `PWR_SHIP_PREP` | START hold indicates shipping mode may be reached soon; firmware saves and warns before hardware cutoff. |
| `PWR_FAULT` | Power state is unsafe or incoherent; boot/fault supervisor takes over. |

Rules:

- Only `thPower` transitions this FSM.
- STOP entry requires owner quiesce acknowledgements or explicit timeout/fault policy.
- Wake resume must classify wake source before handing control back to runtime policy.
- Forced sleep is the low-battery response.
- ISOFET disconnect is the critical-battery response.

---

## ADP5360 / Battery FSM

This state machine describes what firmware knows about the PMIC, charger, and battery.

| State | Meaning |
|---|---|
| `PMIC_OFFLINE` | PMIC interface is not initialized or PMIC status is unavailable. |
| `PMIC_PROBE` | Firmware checks that the ADP5360 responds at I2C address `0x46`. |
| `PMIC_CONFIG` | Firmware applies required PMIC configuration and verifies it. |
| `PMIC_MONITOR` | Normal monitoring state for battery, charger, VBUS, interrupt, and fault status. |
| `PMIC_CHARGING` | Charger/input is present and battery is charging. Runtime may continue unless another mode blocks it. |
| `PMIC_CHARGE_DONE` | PMIC reports full or charge termination state. |
| `PMIC_LOW_BATT` | Battery crossed the low threshold; firmware warns where possible and forces sleep. |
| `PMIC_CRITICAL_BATT` | Battery crossed the critical threshold; firmware saves what it can and disconnects the ISOFET. |
| `PMIC_SHIP_PENDING` | START hold indicates ADP5360 shipping-mode threshold may be reached soon. |
| `PMIC_RECOVERING` | Firmware is retrying or revalidating after a transient PMIC/status fault. |
| `PMIC_ERROR` | PMIC or battery state cannot be trusted. System must degrade or fault depending on severity. |

Rules:

- PMIC register access must be serialized by `thPower`.
- `PMIC_INT` may wake or notify, but handling occurs in `thPower` context.
- Charging state does not grant storage/installer ownership.
- Low battery and START shipping intent are different paths and must stay separate.
- Critical battery must prefer ISOFET disconnect over shipping mode.

---

## Threshold Policy

Threshold values are tuning constants, not game policy.

Required thresholds:

- selected battery profile / cell family
- configured ADP5360 battery capacity value
- charger terminal voltage
- charger current
- charge termination current
- low battery warning threshold
- low battery forced-sleep threshold
- critical battery ISOFET-disconnect threshold
- charger-present debounce/filter timing
- VBUS disagreement timeout
- PMIC read retry limit

Thresholds must be logged with firmware version during bring-up tests.

Rules:

- real-cell charging must not begin until the selected cell chemistry, polarity, protection status, charge voltage, and charge-current limit are confirmed.
- seller-stated pouch-cell capacity is not a design fact until measured or otherwise verified.
- ADP5360 fuel-gauge capacity configuration is a Platform battery-profile setting, not a package or game setting.
- if the physical cell capacity exceeds the ADP5360 fuel-gauge coding range, charger safety policy still follows the cell datasheet and PMIC limits; package-visible battery estimates must be treated as approximate until characterized.

---

## Failure Policy

PMIC/power faults are potentially fatal.

Fault handling depends on severity:

- transient I2C/status read failure: retry in `PMIC_RECOVERING`
- repeated PMIC read failure: degrade and report power-monitor fault
- low battery: force sleep
- critical battery: save where possible, then disconnect ISOFET
- incoherent VBUS/charger/install state: block installer ownership until resolved
- unsafe rail or reset condition: enter `PWR_FAULT`

---

## HW6 Validation Cases

1. ADP5360 probe at I2C address `0x46`
2. PMIC interrupt path from `PMIC_INT` on `PB15`
3. VBUS detected through ADP5360
4. VBUS detected through `USB_OTG_FS_VBUS` on `PA9`
5. VBUS path disagreement handling
6. PPK2 or equivalent battery-simulator operation across selected voltage points
7. ADP5360 battery-profile configuration for the selected cell
8. VBUS-only charger/power-bank attach does not trigger MSC prompt or storage handoff
9. charging while normal runtime is active
10. charging while flashing/install mode is active
11. low-battery forced sleep
12. critical-battery ISOFET disconnect
13. START hold shipping-prep handoff from input to power

Evidence for these cases belongs in [[HW6_Brought_Up_Tracker]]. A passing HW5 result may define the initial procedure or expected value, but it does not close the corresponding HW6 row.
