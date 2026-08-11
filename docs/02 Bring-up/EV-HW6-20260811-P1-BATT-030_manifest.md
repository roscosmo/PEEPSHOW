# EV-HW6-20260811-P1-BATT-030

## Summary

HW6 unit 001 validated the FW0 no-VBUS boot-low-battery restart block and
the UI/display recovery path once the controlled source rose above the
restart-allow threshold.

## Result

PASS/PARTIAL.

Validated:

- `thPower` keeps the boot/restart gate pending until a valid fuel-gauge VBAT
  sample is available.
- With VBUS absent and PMIC-read VBAT below the `3600 mV` restart-allow
  threshold, normal runtime is blocked.
- The blocked path enters the existing controlled-shutdown scaffold
  (`PWR_SHIP_PREP` / `PMIC_SHIP_PENDING`) and uses the default-off boot
  software-shipment gate.
- `thUI` and `thDisplay` show the plain low-battery boot page instead of HOME.
- Raising VBAT above the restart-allow threshold clears the boot gate, returns
  power/PMIC monitoring to active, and returns UI/display to HOME.

Still open:

- VBUS-present boot recovery shell below restart-allow threshold
- enabled boot software-shipment register write through ADP5360 `0x36`
- final low-battery/shutdown UX, animations, and assets
- real save/quiesce behavior before software shipment
- current measurement while held in boot-low-battery block

## Captured Probe

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
--- HW6 battery power policy scaffold ---
api/state/event       = 19 / 5 / 5
counts boot/monitor   = 1 / 4
boot home suppress/ui  = 0 / 1
boot gate pend/block/clr = 1 / 1 / 0
snapshot status/tick  = 0x0 / 331
driver read/exp/rails/fault = 0x7f / 0x7f / 1 / 1
period/next tick      = 100 / 431
thresholds warn/crit/restart mV = 3500 / 3300 / 3600
policy vbat/fuel/vbus/batt = 3270 / 1 / 0 / 1
owner fuel vbat/SOC/mask = 3270 / 0 / 0x1f
owner fuel raw H/L    = 0x66 / 0x30
owner fuel cfg/mode   = 0x32 / 0x51
charger raw/status    = 0x0 / 0x0 / 0x7
charger profile/cfg mask = 0x0 / 0x1f
charger cfg value   = 0x81 / 0x82 / 0x29 / 0xac / 0x80
profile/therm status/reg = 0x0 / 0x0 / 0x80
pmic irq cfg/status = 0x0 / 0xf
vbus pmic/mcu/agree/disagree = 0 / 0 / 1 / 0
battery ok/present/full = 1 / 1 / 0
warn/crit/bootblock   = 0 / 0 / 1
quiesce count/status/tick = 1 / 0x0 / 129
ship gates crit/boot  = 0 / 0
ship req/skip/status/tick = 0 / 1 / 0xffffffff / 0
power state/pmic state = 8 / 8
ui page/shutdown/count = 8 / 5 / 1
display page/shutdown = 8 / 5

Program received signal SIGINT, Interrupt.
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
--- HW6 battery power policy scaffold ---
api/state/event       = 19 / 2 / 2
counts boot/monitor   = 1 / 15
boot home suppress/ui  = 0 / 1
boot gate pend/block/clr = 0 / 0 / 1
snapshot status/tick  = 0x0 / 1442
driver read/exp/rails/fault = 0x7f / 0x7f / 1 / 1
period/next tick      = 100 / 1542
thresholds warn/crit/restart mV = 3500 / 3300 / 3600
policy vbat/fuel/vbus/batt = 3707 / 1 / 0 / 1
owner fuel vbat/SOC/mask = 3707 / 0 / 0x1f
owner fuel raw H/L    = 0x73 / 0xd8
owner fuel cfg/mode   = 0x32 / 0x51
charger raw/status    = 0x0 / 0x0 / 0x7
charger profile/cfg mask = 0x0 / 0x1f
charger cfg value   = 0x81 / 0x82 / 0x29 / 0xac / 0x80
profile/therm status/reg = 0x0 / 0x0 / 0x80
pmic irq cfg/status = 0x0 / 0xf
vbus pmic/mcu/agree/disagree = 0 / 0 / 1 / 0
battery ok/present/full = 1 / 1 / 0
warn/crit/bootblock   = 0 / 0 / 1
quiesce count/status/tick = 1 / 0x0 / 129
ship gates crit/boot  = 0 / 0
ship req/skip/status/tick = 0 / 1 / 0xffffffff / 0
power state/pmic state = 2 / 3
ui page/shutdown/count = 1 / 0 / 1
display page/shutdown = 1 / 0
```

## Interpretation

The restart gate is now a real boot policy path rather than a one-shot early
sample. It waits for a valid PMIC/fuel snapshot, blocks normal runtime below
the restart threshold with no VBUS, records the default-off would-ship result,
and recovers cleanly once the battery reading is safely above the restart
threshold.

This evidence does not validate the final product power-off UX or the ADP5360
software shipment write. Those remain intentionally gated until save/quiesce and
the enabled-shipment test plan are ready.