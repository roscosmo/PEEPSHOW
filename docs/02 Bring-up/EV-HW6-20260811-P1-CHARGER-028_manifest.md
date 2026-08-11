# EV-HW6-20260811-P1-CHARGER-028

## Summary

HW6 unit 001 validated FW0 boot-applied ADP5360 conservative charger profile write/readback with VBUS absent, and recorded PMIC_INT scaffold visibility.

## Result

PASS/PARTIAL.

Validated:

- `thPower` applies the conservative charger profile during normal PMIC stabilization.
- ADP5360 charger profile write status is `0x0`.
- Charger profile readback mask is `0x1f`, covering all five configured registers.
- Configured/read-back values are `0x02=0x81`, `0x03=0x82`, `0x04=0x29`, `0x07=0xAC`, and `0x0A=0x80`.
- VBUS absent classification agrees between ADP5360 and MCU `PA9`.
- PMIC_INT counters are visible in the probe output.

Still open:

- PMIC_INT edge generation, source enable/clear behavior, and `thPower` interrupt-driven snapshot proof were still open in this capture; later validated by `EV-HW6-20260811-P1-CHARGER-029`
- charge-current measurement and staged current promotion
- charge termination / full-state evidence
- JEITA hot/cold/cool/warm substitution evidence
- long-run thermal behavior and production charge policy

## Captured Probe

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_charger_power_prints.gdb
--- HW6 charger/VBUS power scaffold ---
owner api/snapshot      = 10 / 1 / 1
power state/pmic state  = 2 / 3
snapshot status/tick    = 0x0 / 828
driver read/exp/rails/fault = 0x7f / 0x7f / 1 / 1
pmic vbus/mcu/agree     = 0 / 0 / 1
vbus disagree count/tick = 0 / 0
charger raw status1/2   = 0x0 / 0x0
charger read mask       = 0x7
charger profile status = 0x0
charger cfg mask      = 0x1f
charger cfg addr      = 0x2 / 0x3 / 0x4 / 0x7 / 0xa
charger cfg value     = 0x81 / 0x82 / 0x29 / 0xac / 0x80
charger cfg status    = 0x0 / 0x0 / 0x0 / 0x0 / 0x0
profile/therm status/reg = 0x0 / 0x0 / 0x80
therm status bits      = 0
charger mode/status/type/health = 0 / 0 / 0 / 0
battery ok/present/full = 1 / 1 / 0
fuel vbat/SOC/mask      = 3759 / 27 / 0x1f
policy vbat/vbus/batt   = 3759 / 0 / 1
policy state/event      = 2 / 2
pmic int irq/pending/cons = 0 / 0 / 0
pmic int pin/level/irq/consume = 0 / 0 / 0 / 0
pmic int sm pending/snap/status = 0 / 0 / 0x0
```

## Interpretation

The charger profile is now a normal boot power-owner action, not a one-off debugger poke. The PMIC accepted and retained the conservative settings through readback. PMIC_INT did not fire in this capture, so this evidence did not validate interrupt-driven charger/battery/fault handling. That interrupt path was later validated by `EV-HW6-20260811-P1-CHARGER-029` for the charger/VBUS-safe interrupt subset.
