# EV-HW6-20260730-P1-ADP5360-001

## Summary

- Test case: bounded read-only ADP5360 identity, status, regulator, fault, and
  PGOOD probe
- Result: `PASS` for I2C transport, identity, and live rail status; full PMIC
  configuration remains `PARTIAL`
- Date/time: `2026-07-30 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware setup: naked PCB powered from PPK2 source mode; ST-LINK attached
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- FW0 ELF SHA-256:
  `7DFCFBE2F4995F1D9597D17E7C42AD65E77F15FBDFA4DF5567ED5014DA00775A`
- FW0 IOC SHA-256:
  `F92EC587CCDE0261C6EC565447E38D627E5FB49B4FE1137890176C250DA195B4`

## Procedure

1. Flash and run the HW6 FW0 debug image.
2. Allow the one-shot boot probe to complete.
3. Halt the target through VS Code Cortex-Debug.
4. Run `source __fw0_adp5360_probe_prints.gdb`.
5. Preserve the complete readback without writing any ADP5360 register.

The probe reads 14 registers. It performs no I2C writes.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `adp5360_readonly_probe_vscode_gdb.log` | `hw_log` | authoritative register and decoded probe output |
| `20260730T072639Z_hw6-fw0-adp5360-readonly-reset.csv` | `hw_measurement` | current and D7 transaction-marker capture |
| `20260730T072639Z_hw6-fw0-adp5360-readonly-reset.json` | `hw_measurement` | PPK2 setup and integrated statistics |
| `flash_stdout.txt` / `flash_stderr.txt` | `hw_log` | image programming attempt log |
| `reset_stdout.txt` / `reset_stderr.txt` | `hw_log` | reset attempt log |

The other GDB-server logs in this directory record unsuccessful standalone CLI
attachment attempts and are diagnostic history, not register evidence. The
successful register readback came from VS Code Cortex-Debug.

## Observations

- The ADP5360 acknowledged at public 7-bit address `0x46`.
- All 14 reads completed with `HAL_OK`; the read mask is `0x3fff`.
- Manufacturer/model ID `0x10` and silicon revision `0x8` match ADP5360.
- Buck setting `0x18` decodes to `1.8 V`.
- Buck-boost setting `0x13` decodes to `3.3 V`.
- Buck configuration is `0x31`; buck-boost configuration is `0x18`.
- Buck-boost register enable is low, consistent with hardware `EN2` control;
  ADP5360 combines `EN2` and the register enable with OR logic.
- PGOOD `0x07` reports `VOUT1OK`, `VOUT2OK`, and `BATOK`.
- Fault is `0x00`.
- `VBUSOK=0`, consistent with no validated USB/VBUS test in this run.
- Charger status, SOC, and VBAT readback are zero. This run does not establish
  fuel-gauge enablement, battery characterization, or valid battery telemetry.

## Conclusion

HW6 unit 001 has a working MCU-to-ADP5360 I2C3 path, correct PMIC identity,
factory-programmed 1.8 V and 3.3 V regulator targets, both regulator PGOOD
states, and no reported PMIC fault.

This evidence does not approve PMIC writes, cell charging, fuel-gauge
configuration, interrupt clearing, threshold policy, or shipment-mode testing.
