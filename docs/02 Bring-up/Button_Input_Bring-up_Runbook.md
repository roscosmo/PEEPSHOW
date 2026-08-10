# Button Input Bring-up Runbook

This runbook records the measured HW5 procedure for button electrical behavior, EXTI routing, debounce, and power-intent handling.

> [!important] HW6 reuse
> Repeat the complete procedure on HW6 using [[HW6_Wake_Sources]] and record the results in [[HW6_Brought_Up_Tracker]]. The removed rotary encoder is not an alternate input or wake source on HW6.

Related:

- [[Button_Input_Contract]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[HW6_Wake_Sources]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `BTN_A`, `BTN_B`, `BTN_L`, `BTN_R` active-high pulldown inputs with debounce capacitors
- `BTN_START` active-low pullup through BAT54 into ADP5360 `MR` path
- `BTN_BOOT` / `PH3-BOOT0` hardware limitation
- EXTI routing for application-visible button edges
- debounce timing
- long press and repeat classification
- chord classification
- Start shipping-prep warning path
- wake behavior where supported

---

## CubeMX Baseline

The HW5 `.ioc` baseline should expose each application-visible button as an EXTI source:

| Signal | MCU pin | Expected CubeMX role | Expected interrupt |
| --- | --- | --- | --- |
| `BTN_START` | `PA4` | `GPXTI4` | `EXTI4_IRQn` |
| `BTN_A` | `PB5` | `GPXTI5` | `EXTI5_IRQn` |
| `BTN_B` | `PB6` | `GPXTI6` | `EXTI6_IRQn` |
| `BTN_L` | `PB7` | `GPXTI7` | `EXTI7_IRQn` |
| `BTN_R` | `PB8` | `GPXTI8` | `EXTI8_IRQn` |
| `BTN_BOOT` / `BOOT0` | `PH3-BOOT0` | `GPXTI3` after application boot only | `EXTI3_IRQn` |

Generated firmware must confirm the EXTI edge polarity before bring-up is marked complete. The electrical polarities are not uniform across all buttons.

---

## Electrical Expectations

| Signal group | Idle level | Pressed level | Hardware note | Platform role |
| --- | --- | --- | --- | --- |
| `BTN_A`, `BTN_B`, `BTN_L`, `BTN_R` | low | high | pulldown plus `0.1 uF` debounce capacitor | normal user input |
| `BTN_BOOT` / `BOOT0` | low | high | MCU `BOOT0` pin with pulldown | system maintenance/recovery only |
| `BTN_START` | high | low | pullup; button pulls down through BAT54 into ADP5360 `MR` path | normal input plus power-intent detection |

`BTN_BOOT` has two different meanings depending on timing. If it is held early enough during reset, the MCU enters the ROM bootloader and no application firmware runs. Firmware can only classify `BTN_BOOT` presses that occur after the application has already booted.

`BTN_START` also has two different meanings depending on hold duration. Short and normal long presses are ordinary Platform button events. A sustained hold approaches the ADP5360 `MR` shipping-mode threshold. Firmware may warn, save, and quiesce before that hardware threshold, but firmware must not assume it can prevent shipping mode once the PMIC threshold is reached.

---

## Baseline State Sequence

This sequence describes the intended bring-up behavior, not final UI mapping.

1. Boot with all buttons released.
2. Record the idle GPIO level for each button.
3. Press and release each button individually while logging raw GPIO level and EXTI event arrival.
4. Confirm `BTN_A`, `BTN_B`, `BTN_L`, and `BTN_R` classify a press on the active-high transition.
5. Confirm `BTN_START` classifies a press on the active-low transition.
6. Confirm `BTN_BOOT` can be observed only when firmware is already running.
7. Confirm `BTN_BOOT` is filtered out of normal Engine and Reference Game input streams.
8. Confirm debounce suppresses capacitor/bounce artifacts without losing intentional fast presses.
9. Confirm button events include press, release, held duration, long-press threshold, and optional repeat events.
10. Confirm chord events preserve raw button masks and timing evidence.
11. Hold `BTN_START` long enough to trigger the firmware shipping-prep warning path below the ADP5360 shipping threshold.
12. Release `BTN_START` during the warning/prep window and confirm the firmware cancels the pending software-side warning state.

Do not intentionally enter ADP5360 shipping mode from this runbook until the power-management bring-up runbook is ready to capture PMIC behavior and recovery.

---

## Command / Configuration Ledger

Populate this table during bring-up. The current values are placeholders until measured on HW5 hardware.

| Step | Input | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| idle level scan | all released | A/B/L/R/BOOT low; Start high | `fw0` raw probe booted with `idle_mask=0x0` and `current_mask=0x0`, meaning no active buttons with Start active-low and A/B/L/R active-high polarity | pass |
| A/B/L/R press | each button individually | active-high press event, release event, debounce evidence | `fw0` raw EXTI probe recorded A `2` presses / `2` releases, B `3` / `3`, L `2` / `2`, R `2` / `2`; all returned to `current_mask=0x0` | pass |
| Start press | short press | active-low press event, release event, debounce evidence | `fw0` raw EXTI probe recorded Start `3` presses / `3` releases; all returned to `current_mask=0x0` | pass |
| BOOT press after app boot | short press | system-only maintenance event, not game input | TBD | open |
| BOOT held during reset | early hold | ROM bootloader entry; no app-level event claimed | TBD | open |
| long press | each normal input where enabled | held-duration event at configured threshold | TBD | open |
| repeat | each normal input where enabled | repeat events only when policy allows repeats | TBD | open |
| chord | selected multi-button combinations | raw mask plus ordered edge evidence | TBD | open |
| Start ship prep | sustained Start hold | warning/prep event before PMIC threshold | TBD | open |
| Start release during prep | release before hardware threshold | warning/prep cancelled | TBD | open |
| wake test | supported low-power state | only armed buttons wake the system | TBD | open |

---

### HW6 FW0 UI Navigation Status

FW0 has proven A/B/L/R as usable shell navigation inputs on HW6 unit 001. The
Platform still emits generic button impulses; the shell currently maps A/B/L/R
contextually for entering, backing out, and moving focus while joystick
calibration is missing or invalid. This belongs to UI/input routing evidence,
not to final game binding policy.

START remains a dual-purpose input. The ADP5360 `EN_MR_SD` bit is now enabled
during normal boot by `thPower`; intentional tests that approach or cross the
12-second shipment threshold belong to [[ADP5360_Power_Bring-up_Runbook]], not
ordinary button-navigation validation.

## Validation Procedure

1. Confirm idle and pressed logic levels for A/B/L/R.
2. Confirm idle and pressed logic levels for Start.
3. Confirm application-visible `BTN_BOOT` behavior only after firmware is running.
4. Confirm early BOOT0 ROM bootloader behavior is not claimed as firmware-handled.
5. Validate EXTI event for each application-visible button.
6. Validate debounce suppresses bounce for each button type.
7. Validate long press and repeat timing knobs.
8. Validate chord masks are raw masks, not Platform action mappings.
9. Validate Start shipping-prep event fires before hardware shipping threshold.
10. Validate Start wake behavior from supported low-power state.
11. Validate optional/contextual wake buttons only wake when Platform policy has armed them.
12. Validate stuck-button handling suppresses unbounded repeat spam.
13. Validate first-boot/no-settings behavior does not rely on save data existing when Start shipping-prep fires.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- logic-level observations
- EXTI event logs
- debounce timing observations
- chord/repeat observations
- Start/MR behavior notes
- wake test result
- BOOT0 limitation note

Do not expose `BTN_BOOT` as normal Engine or Reference Game input.
