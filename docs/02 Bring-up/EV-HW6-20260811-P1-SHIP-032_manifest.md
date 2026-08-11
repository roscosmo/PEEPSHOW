# EV-HW6-20260811-P1-SHIP-032

## Summary

HW6 unit 001 validated the FW0 guarded ADP5360 software shipment primitive.
The test used the debugger-only manual request flag to ask `thPower` to write
ADP5360 Shipment Mode register `0x36 = 1`.

## Result

PASS/PARTIAL.

Validated:

- The firmware-owned software shipment request flag can be consumed by the
  power owner.
- The ADP5360 software shipment primitive can shut the device down without
  waiting for the 12-second hardware MR threshold.
- The device lost target power/connection and restarted only after a START
  press, matching shipment-mode entry and recovery behavior.
- The test did not require enabling normal START or battery-critical software
  shipment gates.

Still open:

- automatic START-imminent software shipment with
  `KNOB_POWER_START_SOFTWARE_SHIP_ENABLE=true`
- automatic critical-battery software shipment with
  `KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE=true`
- automatic boot-low-battery software shipment with
  `KNOB_POWER_BOOT_LOW_BATTERY_SOFTWARE_SHIP_ENABLE=true`
- real save/quiesce behavior before shipment
- final shutdown UX, countdown timing, and recovery UX

## Captured Precondition

Before the manual request, the target was in normal active operation and no
software shipment request was pending.

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_start_power_prints.gdb
--- HW6 START shipping-prep scaffold ---
button api/edges/presses  = 5 / 0 / 0
last pin/button/event/lev = 0 / 0 / 0 / 0
START state/active       = 0 / 0
START hold checkpoint/live ticks = 0 / 0
START armed/live/next   = 0 / 1 / 0
START raw/stable/count  = 1 / 1 / 2
START samples/synth p/r = 30 / 0 / 0
power state/pmic state    = 2 / 3
power sw ship en/req/skip/status/tick = 0 / 0 / 0 / 0xffffffff / 0
UI page/shutdown/countdown = 1 / 0 / 0
display page/shutdown/countdown = 1 / 0 / 0
pmic MR/sw ship status   = 0x0 / 0xffffffff
pmic sw ship count/tick = 0 / 0
pmic sw ship request    = 0
```

## Manual Request

```text
set var g_ps_hw6_pmic_software_ship_request = 1
```

After the target was continued, the debugger reported:

```text
warning: Remote failure reply: E31
```

The user observed that the device lost power and restarted from a START press.

## Interpretation

This proves the ADP5360 `0x36` software shipment primitive is electrically and
firmware-path valid on HW6 unit 001. The volatile power-owner counters cannot
be printed after a successful shipment entry because the device has powered
down, so the useful evidence is the clean precondition plus the target losing
power immediately after the firmware request.

This evidence validates the primitive only. Product policy still requires
explicit gated tests for START-triggered shipment, critical-battery shipment,
boot-low-battery shipment, and real save/quiesce before power removal.
