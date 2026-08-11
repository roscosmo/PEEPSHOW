# EV-HW6-20260811-P1-BATT-031

## Summary

HW6 unit 001 validated the FW0 VBUS-present boot charge-recovery path below
restart threshold and normal HOME recovery after the restart threshold was
restored.

## Result

PASS/PARTIAL.

Validated:

- The boot/restart gate suppresses HOME while waiting for valid fuel-gauge VBAT.
- With USB/VBUS present, charger active, and VBAT below the configured
  restart-allow threshold, `thPower` selects boot charge recovery instead of
  no-VBUS shipment-prep.
- Boot charge recovery shows the plain low-battery charging page and keeps
  software shipment requests at zero.
- Restoring the restart threshold below the measured VBAT clears the boot gate
  and returns UI/display to HOME while PMIC remains in charging state.

Still open:

- enabled boot software-shipment register write through ADP5360 `0x36`
- final low-battery/shutdown UX, animations, and assets
- real save/quiesce behavior before software shipment
- current measurement while held in boot charge recovery
- charge termination / full-state evidence

## Captured Probe: Forced Restart Threshold

The restart threshold was intentionally set to `4200 mV` for this test so the
available cell would be treated as below the restart-allow threshold while USB
was connected.

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
--- HW6 battery power policy scaffold ---
api/state/event       = 20 / 6 / 6
counts boot/monitor   = 1 / 7
boot home suppress/ui  = 1 / 1
boot gate pend/block/charge/clr = 1 / 1 / 1 / 0
snapshot status/tick  = 0x0 / 634
driver read/exp/rails/fault = 0x7f / 0x3f / 1 / 1
period/next tick      = 100 / 734
thresholds warn/crit/restart mV = 3500 / 3300 / 4200
policy vbat/fuel/vbus/batt = 3968 / 1 / 1 / 1
owner fuel vbat/SOC/mask = 3968 / 72 / 0x1f
owner fuel raw H/L    = 0x7c / 0x0
owner fuel cfg/mode   = 0x32 / 0x51
charger raw/status    = 0x22 / 0xe4 / 0x7
charger profile/cfg mask = 0x0 / 0x1f
charger cfg value   = 0x81 / 0x82 / 0x29 / 0xac / 0x80
profile/therm status/reg = 0x0 / 0x0 / 0x80
therm status bits    = 7
charger mode/stat/type/health = 2 / 1 / 2 / 0
vbus pmic/mcu/agree/disagree = 1 / 1 / 1 / 0
battery ok/present/full = 1 / 1 / 0
warn/crit/bootblock   = 0 / 0 / 0
quiesce count/status/tick = 0 / 0xffffffff / 0
ship gates crit/boot  = 0 / 0
ship req/skip/status/tick = 0 / 0 / 0xffffffff / 0
power state/pmic state = 2 / 4
ui page/shutdown/count = 8 / 6 / 2
display page/shutdown = 8 / 6
pmic sw ship status/count/tick = 0xffffffff / 0 / 0
```

## Captured Probe: Restored Restart Threshold

The restart threshold was restored to the provisional `3600 mV` value and the
same USB/VBUS charging setup returned to normal HOME.

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
--- HW6 battery power policy scaffold ---
api/state/event       = 20 / 2 / 2
counts boot/monitor   = 1 / 6
boot home suppress/ui  = 1 / 0
boot gate pend/block/charge/clr = 0 / 0 / 0 / 1
snapshot status/tick  = 0x0 / 533
driver read/exp/rails/fault = 0x7f / 0x3f / 1 / 1
period/next tick      = 100 / 633
thresholds warn/crit/restart mV = 3500 / 3300 / 3600
policy vbat/fuel/vbus/batt = 4049 / 1 / 1 / 1
owner fuel vbat/SOC/mask = 4049 / 82 / 0x1f
owner fuel raw H/L    = 0x7e / 0x88
owner fuel cfg/mode   = 0x32 / 0x51
charger raw/status    = 0x22 / 0xe4 / 0x7
charger profile/cfg mask = 0x0 / 0x1f
charger cfg value   = 0x81 / 0x82 / 0x29 / 0xac / 0x80
profile/therm status/reg = 0x0 / 0x0 / 0x80
therm status bits    = 7
charger mode/stat/type/health = 2 / 1 / 2 / 0
vbus pmic/mcu/agree/disagree = 1 / 1 / 1 / 0
battery ok/present/full = 1 / 1 / 0
warn/crit/bootblock   = 0 / 0 / 0
quiesce count/status/tick = 0 / 0xffffffff / 0
ship gates crit/boot  = 0 / 0
ship req/skip/status/tick = 0 / 0 / 0xffffffff / 0
power state/pmic state = 2 / 4
ui page/shutdown/count = 1 / 0 / 0
display page/shutdown = 1 / 0
pmic sw ship status/count/tick = 0xffffffff / 0 / 0
```

## Interpretation

The boot battery policy now has separate validated behavior for both sides of
the charger decision. Without VBUS, low battery blocks runtime into the
low-battery boot shutdown scaffold and records the default-off would-ship result.
With VBUS, low battery blocks runtime into a charge-recovery page, keeps PMIC in
charging state, and does not request shipment. Once VBAT is above the restart
threshold, the boot gate clears and HOME is allowed.