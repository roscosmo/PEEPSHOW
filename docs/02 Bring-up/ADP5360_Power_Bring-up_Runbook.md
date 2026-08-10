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
- HW6 `303040` LiPo pouch-cell configuration and capacity assumptions
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

The intended HW6 cell is a `303040` flat LiPo pouch cell reported as marked
`3.7 V`, `450 mAh`, and `1.665 Wh`. The energy marking is consistent with the
nominal voltage and capacity (`3.7 V x 0.450 Ah = 1.665 Wh`). Treat capacity as
a label/specification value until measured. The selected cell has a confirmed
`4.20 V` charge terminal and is accepted for charging at up to `1 C`
(`450 mA`). The ADP5360 charger is limited to `320 mA`, so `320 mA`
(`0.711 C`) is the absolute Platform charge-current ceiling before VBUS and
thermal derating. The previous `LIR2540` coin-cell assumptions must not be
reused for charge current, fuel-gauge capacity, low-battery thresholds,
runtime estimates, or UX claims.

The cell has two electrical leads. A board-mounted `100 kOhm` NTC physically
contacts the pouch and provides the ADP5360 temperature input. Current
development cells contain a protection circuit for handling/assembly, but
production cells may be unprotected. ADP5360 battery protection is therefore
mandatory and must be validated as the authoritative protection path rather
than relying on a cell-integrated PCM.

Per the ADP5360 datasheet, the `BAT_CAP` register encodes battery capacity as `BAT_CAP x 2 mAh`, so a nominal `450 mAh` profile is representable as code `225` if the cell is accepted as the configured profile. Do not treat the configured profile as proof of real cell capacity.

### Reviewed Register Profile And Validation State

The full read-only map on HW6 unit 001 established the present register state.
The table below is the selected delta for the intended cell. Its six bytes have
passed a guarded reversible write/readback/restore test, but the profile is not
yet approved for persistent application or real-cell charging.

| Register | Present | Candidate | Meaning / gate |
|---|---:|---:|---|
| `0x02 CHARGER_VBUS_ILIM` | `0x81` | dynamic; initially retain `0x81` | present VBUS limit is `100 mA`; any increase must follow USB/source classification and not merely the cell's `1 C` permission |
| `0x03 CHARGER_TERMINATION_SETTING` | `0x7A` | `0x82` | accepted/read back/restored; controlled `4.20 V` validation remains open |
| `0x04 CHARGER_CURRENT_SETTING` | `0x29` | `0x3F` maximum | accepted/read back/restored; `320 mA` charging remains unapproved pending staged thermal/current testing |
| `0x07 CHARGER_FUNCTION_SETTING` | `0x2C` | `0xAC` | accepted/read back/restored; JEITA behavior remains open |
| `0x0A BATTERY_THERMISTOR_CONTROL` | `0x00` | `0x80` | accepted/read back/restored; `100 kOhm` NTC temperature-zone behavior remains open |
| `0x20 BAT_CAP` | `0x32` | `0xE1` | accepted/read back/restored; nominal `450 mAh` fuel-gauge behavior remains open |
| `0x27 FUEL_GAUGE_MODE` | `0x50` | `0x53` | accepted/read back/restored; sleep/active mode and representative-load behavior remain open |
| `0x11..0x15` protection | `03 90 E6 78 E8` | provisionally retain | protection enabled; `2.50 V` UV, `600 mA` discharge OC / `5 ms`, `4.30 V` OV / `0.5 s`, `400 mA` charge OC / `10 ms`; peak-load and fault tests required |

The candidate encodings are derived from the
[ADP5360 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ADP5360.pdf).
That reversible test passed on HW6 unit 001 as
`EV-HW6-20260731-P1-ADP5360-003`: all six candidate and restored bytes matched,
PGOOD remained `0x07`, fault remained `0x00`, and VBUS remained absent.
Protection-register changes require a separate power-up-only procedure because
the data sheet forbids changing protection selection during a battery fault.

---

## Baseline State Sequence

This sequence proves the PMIC monitor path before full sleep policy depends on it.

1. Boot from a known-good controlled power source with START released.
2. Initialize I2C3 and probe the ADP5360 at `0x46`.
3. Confirm the firmware driver uses public 7-bit I2C address `0x46` through `ps_hw_i2c3`.
4. Read PMIC identity/status, charger/input status, battery/fuel state, interrupt status, and fault status registers.
5. Configure only the minimum required PMIC settings for safe monitor operation.
6. Use `PPK2` or equivalent to step through representative battery voltages and record PMIC/fuel-gauge readback without a real cell connected.
7. Validate the reviewed `303040` candidate register delta with no cell and no VBUS before enabling any real-cell charging.
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

### Active Step 6 Fuel-Gauge Sweep

The active FW0 test temporarily writes only `BAT_CAP=0xE1` and
`FUEL_GAUGE_MODE=0x51`. The latter enables the gauge in active mode so battery
voltage is sampled every second and SOC is updated every ten seconds. It then
writes `SOC_RESET=0x80` followed by `0x00`, records 13 voltage/SOC samples over
12 seconds, restores the original `0x27` and `0x20` values in reverse order,
and verifies final fault, PGOOD, and VBUS state. `PWR_DBG` is high only during
the bounded measurement window.

Run one PPK2 source-voltage point per reset with no cell and no VBUS connected.
Begin at the known-good source voltage, then use representative points
`3.3 V`, `3.5 V`, `3.7 V`, `3.9 V`, `4.1 V`, and `4.2 V`. After `PWR_DBG`
returns low, halt and run
`source __fw0_adp5360_fuel_sweep_prints.gdb`. Do not continue to a lower
source voltage if rail stability, SWD recovery, fault state, or exact register
restoration fails.

---

### HW6 FW0 EN_MR_SD Normal-Boot Result

FW0 now enables the ADP5360 hardware START/MR shipment-entry option during
normal boot through the power owner. The firmware sets Supervisory Setting
register `0x2D`, bit `1` (`EN_MR_SD`), then records the normal ADP5360 status
snapshot. The passing HW6 unit 001 result is recorded as
`EV-HW6-20260810-P5-BOOT-021`.

Measured evidence from that capture:

- RTOS init/runtime complete `1 / 1`
- normal boot power/display complete `1 / 1`
- ADP driver API/init/MR/state/ops/last `4 / 0 / 0 / 2 / 2 / 0`
- PMIC snapshot command/complete/success `0 / 1 / 1`
- I2C lease, HAL transfer, and release statuses all `0`
- ADP identity, rail, and fault checks all passed

This confirms the protective hardware path for a 12-second START/MR hold is
enabled. Subsequent HW6 unit 001 FW0 captures validated the software
shipping-prep, warning, and imminent scaffold at 5 s, 9 s, and 11 s, and
the user confirmed ADP5360 shipment entry after a long START/MR hold. This
does not authorize routine intentional shipment-entry testing as part of button
navigation work, and it does not close save/quiesce, release-cancel,
wake/recovery, or first-boot policy tests.

## Threshold / Policy Ledger

Populate this table during bring-up. Values are placeholders until selected and measured.

| Policy item | Initial value | Evidence required | Status |
| --- | --- | --- | --- |
| battery simulator/source | `PPK2` | source-voltage/current capture setup recorded | open |
| cell family | `303040 LiPo pouch`, two wire | cell marking, polarity, protection, and physical fit record | electrical profile recorded; polarity/fit evidence open |
| nominal voltage / energy marking | `3.7 V` / `1.665 Wh` | marking record and arithmetic consistency | marking_recorded_unverified |
| seller-stated capacity | `450 mAh` | record as unverified label/spec claim | marking_recorded_unverified |
| measured effective capacity | TBD | controlled discharge or supplier-verified evidence | open |
| ADP5360 `BAT_CAP` code | measured factory `50` (`100 mAh`); target `225` for 450 mAh | reviewed cell profile, register acceptance, then fuel-gauge characterization | reversible_write_pass_unit_001; behavior open |
| charge terminal voltage | `4.20 V`; register `0x03=0x82` | no-cell write/readback, then controlled charge-voltage evidence | reversible_write_pass_unit_001; controlled charge open |
| charge current limit | `320 mA` absolute ceiling (`0.711 C`); first-cell commissioning at `100 mA` | VBUS contract plus staged current/thermal evidence | profile_ceiling_recorded; promotion open |
| VBUS input current limit | present `100 mA`; dynamic target TBD | USB/source classification and input-current evidence | open; do not raise solely from cell rating |
| charge termination current | provisional `5 mA` retained | controlled CV/termination evidence and cell behavior review | provisional |
| thermistor arrangement | two-wire cell; board `100 kOhm` NTC physically contacts pouch | `6 uA` bias readback, room-temperature resistance/THR status, and hot/cold substitution test | arrangement_recorded; present `10 kOhm` bias mismatch open |
| temperature charge policy | JEITA enabled | register readback plus cold/cool/normal/warm/hot charge-state tests | reversible_write_pass_unit_001; behavior open |
| cell-integrated protection | development cell protected for handling; production TBC | procurement/BOM record | open; not a Platform safety dependency |
| ADP5360 protection | mandatory authoritative path; present `2.50 V` UV, `600 mA` discharge OC, `4.30 V` OV, `400 mA` charge OC | controlled threshold/fault tests and peak-load margin | baseline_decoded; validation open |
| low battery warning threshold | TBD | warning event and log | open |
| low battery forced-sleep threshold | TBD | forced-sleep transition evidence | open |
| critical battery ISOFET-disconnect threshold | TBD | controlled disconnect or safe simulation evidence | open |
| charger-present debounce/filter | TBD | USB connect/disconnect logs | open |
| VBUS disagreement timeout | TBD | ADP5360 vs `PA9` mismatch handling | open |
| PMIC read retry limit | TBD | transient failure recovery test | open |
| START ship-prep threshold | `5 s` FW0 scaffold | power owner receives prep before hardware cutoff | pass_unit_001; save/quiesce policy open |
| START warning threshold | `9 s` FW0 scaffold | warning starts early enough before hardware cutoff | pass_unit_001; warning UI open |
| START imminent threshold | `11 s` FW0 scaffold | final warning before hardware cutoff | pass_unit_001; close to PMIC threshold |

Thresholds are Platform tuning constants, not Reference Game policy.

---

## Command / Configuration Ledger

| Step | Configuration | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| PPK2 setup | source mode, current capture | device powers as battery simulator; current trace captured | TBD | open |
| battery voltage sweep | selected voltage points | PMIC/fuel readback tracks voltage safely | TBD | open |
| cell profile | `303040` provisional profile | charge/fuel settings match reviewed cell assumptions | six-byte profile delta accepted and exactly restored; persistent application and behavior remain open | reversible_pass_unit_001 |
| BAT_CAP | code `225` for 450 mAh | register accepts/readbacks configured value | `0xE1` accepted/read back/restored exactly | reversible_pass_unit_001; sweep open |
| charger config | `4.20 V`; staged `100 mA` then up to `320 mA`; provisional `5 mA` termination; JEITA/`100 kOhm` NTC | no-cell readback first, then controlled real-cell current/voltage/temperature evidence | `03=82`, `04=3F`, `0A=80`, and `07=AC` accepted/read back/restored; no charging exercised | reversible_pass_unit_001; physical tests open |
| battery protection | authoritative ADP5360 protection regardless of cell PCM | decoded settings survive power cycle and controlled fault tests behave safely | enabled baseline decoded; thresholds not yet stimulated | partial_unit_001 |
| I2C probe | address `0x46` | ADP5360 ACKs | HW6 unit 001 ACKed; complete 55/55 read-only map passed | pass_unit_001 |
| address representation | public 7-bit `0x46` through `ps_hw_i2c3` | convention confirmed | public `0x46`, STM32 HAL shifted `0x8c` | pass_unit_001 |
| status read | PMIC status registers | charger/battery/fault state readable | identity `0x10`, revision `0x8`, PGOOD `0x07`, fault `0x00`; full map captured; fuel gauge disabled and telemetry invalid | partial_unit_001 |
| configuration map | read-only `0x00..0x36` | every register readable before write planning | 55/55 reads; factory 100 mAh profile, fuel gauge/IRQs disabled, charger/protection baseline captured | pass_inventory_unit_001 |
| PMIC_INT | safe event or pending clear | EXTI15 event and owner handling | TBD | open |
| VBUS cross-check | USB attach/detach | ADP5360 and `PA9` agree or log diagnostic; no VBUS-only MSC prompt | TBD | open |
| charging | USB attached | charging/charge-done state reported | TBD | open |
| low battery | simulated or measured threshold | forced sleep selected | TBD | open |
| critical battery | simulated or controlled threshold | ISOFET disconnect selected, not shipping mode | TBD | open |
| EN_MR_SD enable | normal boot through `thPower` | ADP5360 Supervisory Setting `0x2D[1]` set so START/MR 12 s shipment entry is enabled | power/display boot complete `1/1`, ADP driver API `4`, MR status `0`, PMIC snapshot success `1` | pass_unit_001; software prep UX open |
| START prep | sustained hold below hardware cutoff | warning/save/quiesce path starts | 5 s START hold reached `START_SHIP_PREP`, power event `1`, power state `8/8`, raw/stable PA4 `0/0` | scaffold_pass_unit_001; save/quiesce open |
| START warning/imminent | sustained hold below/near hardware cutoff | warning and imminent events route to `thPower` | 9-10 s hold reached `START_SHIP_WARNING`; >11 s hold reached `START_SHIP_IMMINENT`; prep/warn/imm counters `1/1/1`; user confirmed PMIC shipment after long hold | scaffold_pass_unit_001; warning UI and release-cancel open |
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
11. Validate normal boot enables ADP5360 `EN_MR_SD` through `thPower` before intentional START shipping-entry tests.
12. Validate normal START short/long press remains firmware-observable before hardware shipping threshold.
13. Validate START hold warning/prep path can run before the ADP5360 shipping threshold.
14. Validate first-boot START shipping intent is ignored by firmware policy.
15. Validate PMIC read failure uses bounded recovery and does not silently trust stale power state.
16. Validate VBUS disagreement blocks installer/storage ownership decisions until explained.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- I2C probe log
- PPK2/source-meter setup, source voltage, current limit, and current trace
- battery cell marking, claimed capacity, connector polarity, and protection status
- board NTC resistance, physical contact, room-temperature THR readback, and
  temperature-zone substitution evidence
- ADP5360 battery profile register write/readback evidence
- PMIC status readback
- PMIC interrupt observation
- VBUS cross-check result, including no VBUS-only MSC prompt/storage handoff
- charging state result
- low-battery forced-sleep test result
- critical-battery ISOFET disconnect result if safely testable
- START / shipping-mode timing observations

Do not mark power behavior known-good without measured HW6 evidence.
