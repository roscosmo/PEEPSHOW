# EV-HW6-20260811-P1-CHARGER-027

## Summary

HW6 unit 001 validated FW0 ADP5360 charger/VBUS monitor behavior with a real cell, board-mounted `100 kOhm` NTC at room temperature, and USB/VBUS plugged.

## Result

PASS/PARTIAL.

Validated:

- FW0 power owner probe API version `9` completed and succeeded.
- ADP5360 and MCU `PA9` VBUS paths agreed that VBUS was present.
- ADP5360 thermistor control register `0x0A` was configured and read back as `0x80`, selecting the 6 uA thermistor bias used for the board-mounted `100 kOhm` NTC.
- ADP5360 charger status reported thermistor OK, charger healthy, and active fast charging.
- PeepOS power state stayed active while the PMIC state machine reported charging.

Still open:

- charge-current measurement and staged current promotion evidence
- charge termination / full-state evidence
- JEITA hot/cold/cool/warm substitution evidence
- VBUS input-current policy beyond the retained 100 mA baseline
- long-run thermal behavior and production charge policy

## Captured Probe

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_charger_power_prints.gdb
--- HW6 charger/VBUS power scaffold ---
owner api/snapshot      = 9 / 1 / 1
power state/pmic state  = 2 / 4
snapshot status/tick    = 0x0 / 1226
driver read/exp/rails/fault = 0x7f / 0x3f / 1 / 1
pmic vbus/mcu/agree     = 1 / 1 / 1
vbus disagree count/tick = 0 / 0
charger raw status1/2   = 0x22 / 0xe4
charger read mask       = 0x7
therm cfg/status/reg   = 0x0 / 0x0 / 0x80
therm status bits      = 7
charger mode/status/type/health = 2 / 1 / 2 / 0
battery ok/present/full = 1 / 1 / 0
fuel vbat/SOC/mask      = 3732 / 17 / 0x1f
policy vbat/vbus/batt   = 3732 / 1 / 1
policy state/event      = 2 / 2
```

## Interpretation

- `therm status bits = 7` means the PMIC accepted the thermistor input as OK.
- `charger mode/status/type/health = 2 / 1 / 2 / 0` means fast-charge mode, charging status, fast-charge type, and good health.
- `power state/pmic state = 2 / 4` means the system remained in active low-power runtime while the PMIC FSM entered `PMIC_CHARGING`.