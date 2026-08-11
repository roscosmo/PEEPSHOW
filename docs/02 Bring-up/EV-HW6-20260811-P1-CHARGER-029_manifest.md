# EV-HW6-20260811-P1-CHARGER-029

## Summary

HW6 unit 001 validated FW0 PMIC_INT pull-up behavior, guarded EXTI15 arming,
ADP5360 interrupt enable/read/flag-clear handling, and interrupt-driven
`thPower` PMIC snapshot routing.

## Result

PASS/PARTIAL.

Validated:

- HW6 unit 001 needs the MCU internal `PB15` pull-up for the ADP5360 `PMIC_INT` line.
- Early `EXTI15` handling is guarded until RTOS owner services are initialized and the interrupt is explicitly armed.
- The boot-time HardFault caused by an early PMIC_INT edge is resolved by the early disarm/clear plus explicit arm sequence.
- `thPower` applies the FW0 PMIC interrupt profile during normal PMIC stabilization.
- ADP5360 interrupt enable readback is `INTERRUPT_ENABLE1=0x03`, `INTERRUPT_ENABLE2=0x00`.
- ADP5360 interrupt flag registers are read and then cleared by write-one-to-clear handling.
- The MCU EXTI edge was recorded and consumed, then `thPower` took a normal PMIC snapshot.
- VBUS/charger interrupt handling stays in power policy and does not imply USB MSC export or storage ownership.

Still open:

- non-charger PMIC interrupt sources: MR, watchdog, rails, faults, battery protection events
- enabled software-shipment register write through `0x36`
- charge-current measurement and staged current promotion
- charge termination / full-state evidence
- JEITA hot/cold/cool/warm substitution evidence
- long-run thermal behavior and production charge policy
- STOP/wake behavior using PMIC_INT as an armed wake source

## Captured Probe

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_charger_power_prints.gdb
--- HW6 charger/VBUS power scaffold ---
owner api/snapshot      = 12 / 1 / 1
power state/pmic state  = 2 / 3
snapshot status/tick    = 0x0 / 130
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
pmic irq cfg/status = 0x0 / 0xf
pmic irq addr       = 0x32 / 0x33 / 0x34 / 0x35
pmic irq value      = 0x3 / 0x0 / 0x0 / 0x0
pmic irq read status = 0x0 / 0x0 / 0x0 / 0x0
pmic irq clear     = 0x3 / 0x0 / 0x0
pmic irq clr stat  = 0x0 / 0x0
therm status bits      = 0
charger mode/status/type/health = 0 / 0 / 0 / 0
battery ok/present/full = 1 / 1 / 0
fuel vbat/SOC/mask      = 3748 / 24 / 0x1f
policy vbat/vbus/batt   = 3748 / 0 / 1
policy state/event      = 2 / 2
pmic int irq/pending/cons = 1 / 0 / 1
pmic int pin/level/irq/consume = 32768 / 1 / 1440 / 26
pmic int sm pending/snap/status = 1 / 1 / 0x0
```

## Interpretation

The interrupt path is now a real power-owner path, not just visible counters.
The PMIC interrupt line is electrically usable with the MCU pull-up, the ISR can
record an edge without doing PMIC I2C work, and `thPower` consumes that edge by
taking the normal snapshot and clearing ADP5360 flags correctly.

This evidence only covers the charger/VBUS-safe interrupt profile configured by
FW0. Other ADP5360 interrupt sources must stay disabled until each source is
individually validated.
