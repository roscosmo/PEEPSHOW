# EV-HW6-20260730-P1-ADP5360-002

## Summary

- Test case: complete read-only ADP5360 register-map inventory, addresses
  `0x00..0x36`
- Result: `PASS` for full register accessibility and configuration inventory;
  battery-profile adoption, PMIC writes, charging, and interrupts remain open
- Date/time: `2026-07-30 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware setup: naked PCB powered from PPK2 source mode; ST-LINK attached;
  no production cell connected
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- FW0 ELF SHA-256:
  `A0B47D0E492461B5C1CF1EC505CE8FAC54E75F3D0526A92F9A82EE7770701A1D`
- FW0 IOC SHA-256:
  `F92EC587CCDE0261C6EC565447E38D627E5FB49B4FE1137890176C250DA195B4`

## Procedure

1. Flash and run the HW6 FW0 full-map read-only image.
2. Halt after the one-shot probe completes.
3. Print the decoded summary and all 55 register results.
4. Preserve the complete readback without writing an ADP5360 register.

The initial capture used four temporary print helpers. The consolidated
user-facing helper `source __fw0_adp5360_map_prints.gdb` was then run against
the same target image and reproduced the complete result. The artifact stores
that canonical one-command output.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `adp5360_full_map_vscode_gdb.log` | `hw_log` | authoritative decoded summary and complete register map |

## Observations

- All 55 addresses from `0x00` through `0x36` read with `HAL_OK`; there were
  no I2C or register-read failures.
- Identity `0x10`, silicon revision `0x08`, regulator targets, PGOOD `0x07`,
  and fault `0x00` reproduce the bounded probe result.
- `BAT_CAP=0x32` configures `100 mAh`. This is the factory/default application
  profile and does not match the provisional `450 mAh` HW6 cell assumption.
- `FUEL_GAUGE_MODE=0x50` has `EN_FG=0`, so the fuel gauge is disabled. Its
  stored policy fields select an 11% low-SOC threshold, 10 mA sleep-current
  threshold, one-minute sleep update interval, and active mode if enabled.
- `BAT_SOC=0` and `VBAT_READ_H/L=0` are therefore not valid battery telemetry.
- `CHARGER_FUNCTION_SETTING=0x2C` leaves software charging disabled, enables
  the charger LDO and end-of-charge function, and leaves JEITA disabled.
- `BATTERY_THERMISTOR_CONTROL=0x00` selects the 60 uA source and does not force
  thermistor sampling while VBUS is absent.
- `BATTERY_PROTECTION_CONTROL=0x03` enables battery protection and permits
  recovery charging from undervoltage; overcurrent responses retain latch mode.
- Both interrupt-enable registers and both interrupt-flag registers are zero.
  No PMIC interrupt behavior can be expected from this baseline.
- Shipment-mode request is clear and supervisory configuration is `0x80`.
- This test performed no PMIC writes and does not authorize charging a cell.

## Conclusion

HW6 unit 001 has a completely readable ADP5360 configuration map and healthy
live rail status. The result identifies configuration work rather than a
hardware fault: the fuel gauge is disabled, its capacity profile is still
100 mAh, PMIC interrupts are disabled, and charger/cell policy has not been
reviewed for the intended HW6 battery.
