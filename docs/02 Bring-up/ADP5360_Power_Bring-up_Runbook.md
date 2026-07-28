# ADP5360 Power Bring-up Runbook

This runbook records the measured HW5 procedure for PMIC, charger, battery, VBUS, ISOFET, and shipping-mode validation.

> [!important] HW6 reuse
> Reuse the procedure on HW6, but execute it against [[HW6_Power_Rails]] and [[HW6_Wake_Sources]] and record every new result in [[HW6_Brought_Up_Tracker]]. No HW5 pass transfers to HW6.

Related:

- [[PMIC_and_Power_Contract]]
- [[Power_and_Sleep_Policy]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- Nordic Power Profiler Kit II (`PPK2`) or equivalent battery-simulator/source-meter bring-up
- I2C probe of ADP5360 at address `0x46`
- `PMIC_INT` behavior on `PB15` / `EXTI15`
- VBUS detection through ADP5360 and `USB_OTG_FS_VBUS` on `PA9`
- HW5 `303040` LiPo pouch-cell configuration and capacity assumptions
- charge and charge-done state reporting
- low-battery forced sleep threshold behavior
- critical-battery ISOFET disconnect behavior
- START / ADP5360 `MR` shipping-mode path
- first-boot START shipping-intent ignore behavior

---

## CubeMX / Electrical Baseline

| Function | MCU resource | Required baseline |
| --- | --- | --- |
| PMIC bus | `I2C3` on `PC0` SCL, `PC1` SDA | ADP5360 responds at `0x46` |
| PMIC interrupt | `PB15` `PMIC_INT` | `GPXTI15` / `EXTI15_IRQn` |
| USB VBUS sense | `PA9` `USB_OTG_FS_VBUS` | alternate VBUS classification path |
| Start / MR path | `BTN_START` through ADP5360 `MR` path | firmware-visible active-low Start before hardware shipping threshold |

Per [[Platform_Hardware_Abstraction_Contract]], the driver-facing address is the public 7-bit address `0x46`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer. Bring-up evidence should confirm the convention used by the firmware under test.

---

## Safety Gates

Power bring-up can physically shut the device down or disconnect the battery path. Do not run the hazardous tests until the recovery method is known.

| Test | Hazard | Required gate |
| --- | --- | --- |
| START shipping-mode entry | device may enter shipping mode | prove normal Start warning/prep first; confirm 200 ms wake/recovery path |
| critical-battery ISOFET disconnect | device may lose power | perform only with controlled supply/battery setup and recovery plan |
| charger/VBUS edge tests | storage/USB policy may change | ensure no install/export/write operation is active |
| low-battery forced sleep | runtime may be stopped | ensure save/quiesce behavior is observable |

For early bring-up, prefer register-controlled, threshold-simulated, or bench-supply-controlled tests where the ADP5360 supports them. Record whether each result is measured physically or simulated through safe configuration.

Use `PPK2` source mode or an equivalent controlled source as the first battery-path supply where possible. Do not connect the real pouch cell until polarity, connector orientation, rail behavior, ADP5360 configuration, and recovery path are understood.

The HW5 cell target is a `303040` flat LiPo pouch cell with seller-stated `450 mAh` capacity. Treat this capacity as unverified. The previous `LIR2540` coin-cell assumptions must not be reused for charge current, fuel-gauge capacity, low-battery thresholds, runtime estimates, or UX claims.

Per the ADP5360 datasheet, the `BAT_CAP` register encodes battery capacity as `BAT_CAP x 2 mAh`, so a nominal `450 mAh` profile is representable as code `225` if the cell is accepted as the configured profile. Do not treat the configured profile as proof of real cell capacity.

---

## Baseline State Sequence

This sequence proves the PMIC monitor path before full sleep policy depends on it.

1. Boot from a known-good controlled power source with START released.
2. Initialize I2C3 and probe the ADP5360 at `0x46`.
3. Confirm the firmware driver uses public 7-bit I2C address `0x46` through `ps_hw_i2c3`.
4. Read PMIC identity/status, charger/input status, battery/fuel state, interrupt status, and fault status registers.
5. Configure only the minimum required PMIC settings for safe monitor operation.
6. Use `PPK2` or equivalent to step through representative battery voltages and record PMIC/fuel-gauge readback without a real cell connected.
7. Select the provisional `303040` battery profile only after charge voltage, charge current, termination policy, and capacity coding are reviewed.
8. Validate `PMIC_INT` by reading/clearing a known pending condition or producing a safe charger/input event.
9. Compare VBUS classification from ADP5360 status and `PA9` USB VBUS sense.
10. Connect USB power and confirm charging/input-present state without showing an MSC prompt or changing storage ownership by itself.
11. Disconnect USB and confirm charger/input-absent state.
12. Trigger or simulate low-battery threshold and confirm forced-sleep policy is selected.
13. Trigger or simulate critical-battery threshold and confirm ISOFET-disconnect policy is selected instead of shipping mode.
14. Hold START long enough to generate firmware shipping-prep intent below the ADP5360 hardware threshold.
15. Release START during prep and confirm firmware cancels software-side warning/prep.
16. Validate first-boot/no-settings policy ignores save/backup work for START shipping intent because there is nothing valid to preserve yet.

Do not intentionally cross the ADP5360 shipping-mode threshold until shipping entry and recovery are part of the active test plan.

---

## Threshold / Policy Ledger

Populate this table during bring-up. Values are placeholders until selected and measured.

| Policy item | Initial value | Evidence required | Status |
| --- | --- | --- | --- |
| battery simulator/source | `PPK2` | source-voltage/current capture setup recorded | open |
| cell family | `303040 LiPo pouch` | cell marking, polarity, protection, and physical fit record | open |
| seller-stated capacity | `450 mAh` | record as unverified label/spec claim | open |
| measured effective capacity | TBD | controlled discharge or supplier-verified evidence | open |
| ADP5360 `BAT_CAP` code | provisional `225` for 450 mAh | register write/readback and rationale | open |
| charge terminal voltage | TBD | cell spec and PMIC register evidence | open |
| charge current limit | TBD | conservative initial value and thermal/current evidence | open |
| charge termination current | TBD | PMIC register evidence | open |
| low battery warning threshold | TBD | warning event and log | open |
| low battery forced-sleep threshold | TBD | forced-sleep transition evidence | open |
| critical battery ISOFET-disconnect threshold | TBD | controlled disconnect or safe simulation evidence | open |
| charger-present debounce/filter | TBD | USB connect/disconnect logs | open |
| VBUS disagreement timeout | TBD | ADP5360 vs `PA9` mismatch handling | open |
| PMIC read retry limit | TBD | transient failure recovery test | open |
| START ship-prep warning threshold | TBD | warning starts early enough for save/quiesce | open |
| START imminent threshold | TBD | final warning before hardware cutoff | open |

Thresholds are Platform tuning constants, not Reference Game policy.

---

## Command / Configuration Ledger

| Step | Configuration | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| PPK2 setup | source mode, current capture | device powers as battery simulator; current trace captured | TBD | open |
| battery voltage sweep | selected voltage points | PMIC/fuel readback tracks voltage safely | TBD | open |
| cell profile | `303040` provisional profile | charge/fuel settings match reviewed cell assumptions | TBD | open |
| BAT_CAP | provisional code `225` for 450 mAh | register accepts/readbacks configured value | TBD | open |
| charger config | terminal voltage/current/termination TBD | real-cell charging remains disabled until reviewed | TBD | open |
| I2C probe | address `0x46` | ADP5360 ACKs | TBD | open |
| address representation | public 7-bit `0x46` through `ps_hw_i2c3` | convention confirmed | TBD | open |
| status read | PMIC status registers | charger/battery/fault state readable | TBD | open |
| PMIC_INT | safe event or pending clear | EXTI15 event and owner handling | TBD | open |
| VBUS cross-check | USB attach/detach | ADP5360 and `PA9` agree or log diagnostic; no VBUS-only MSC prompt | TBD | open |
| charging | USB attached | charging/charge-done state reported | TBD | open |
| low battery | simulated or measured threshold | forced sleep selected | TBD | open |
| critical battery | simulated or controlled threshold | ISOFET disconnect selected, not shipping mode | TBD | open |
| START prep | sustained hold below hardware cutoff | warning/save/quiesce path starts | TBD | open |
| START release | release during prep | software warning/prep cancelled | TBD | open |
| first boot | no settings/calibration | no save/backup dependency during ship prep | TBD | open |

---

## Validation Procedure

1. Confirm board powers safely from controlled input and USB input.
2. Use `PPK2` or equivalent battery simulation to power the battery path and capture current draw before real-cell use.
3. Probe ADP5360 over I2C3 at `0x46`.
4. Read basic PMIC/fuel/charger status registers.
5. Confirm `PMIC_INT` edge/level behavior and EXTI routing.
6. Compare VBUS classification from ADP5360 and `PA9` VBUS divider/path; confirm VBUS-only power does not offer MSC mode.
7. Validate provisional `303040` cell profile settings without charging a real cell.
8. Validate charging and charge-done reporting only after real-cell charge configuration is reviewed and safe.
9. Validate low-battery threshold routes to forced sleep policy.
10. Validate critical-battery threshold disconnects ISOFET, not shipping mode.
11. Validate normal START short/long press remains firmware-observable before hardware shipping threshold.
12. Validate START hold warning/prep path can run before the ADP5360 shipping threshold.
13. Validate first-boot START shipping intent is ignored by firmware policy.
14. Validate PMIC read failure uses bounded recovery and does not silently trust stale power state.
15. Validate VBUS disagreement blocks installer/storage ownership decisions until explained.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- I2C probe log
- PPK2/source-meter setup, source voltage, current limit, and current trace
- battery cell marking, claimed capacity, connector polarity, and protection status
- ADP5360 battery profile register write/readback evidence
- PMIC status readback
- PMIC interrupt observation
- VBUS cross-check result, including no VBUS-only MSC prompt/storage handoff
- charging state result
- low-battery forced-sleep test result
- critical-battery ISOFET disconnect result if safely testable
- START / shipping-mode timing observations

Do not mark power behavior known-good without measured HW5 evidence.
