# EV-HW6-20260810-P5-BOOT-021

## Scope

Normal FW0 boot lifecycle evidence for HW6 unit 001 after the UI router,
display bootstrap, power-owner sequencing, and ADP5360 `EN_MR_SD` enable work.

## Source

User-provided GDB capture from:

`source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_all_probe_prints.gdb`

Raw capture:

`docs/02 Bring-up/EV-HW6-20260810-P5-BOOT-021_normal_boot_power_display_mr_shipping_gdb.log`

## Result

PASS for the current FW0 normal boot slice.

## Measured Facts

- RTOS init/runtime complete: `1 / 1`
- normal boot power/display complete: `1 / 1`
- owner threads started: `0x1ff / 0x1ff`
- queue self-test: `0x1ff / 0x1ff`
- event-group self-test: `0xf / 0xf`
- power-owner PMIC snapshot command/complete/success: `0 / 1 / 1`
- ADP driver API/init/MR/state/ops/last: `4 / 0 / 0 / 2 / 2 / 0`
- ADP functions/read/match: `0xf / 0x7f / 0x7f`
- ADP register addresses: `00 29 2a 2b 2c 2e 2f`
- ADP register values: `10 31 18 18 13 00 07`
- I2C lease, HAL transfer, HAL error, and lease release arrays all zero
- ADP identity/rails/fault checks: `1 / 1 / 1`
- display owner command/complete/success: `0 / 1 / 1`
- display driver API/init/state/ops/last: `1 / 0x0 / 2 / 2 / 0x0`
- display hash: `0x554dd845`
- audio diagnostic command/complete/success: `0 / 0 / 0`
- retained lifecycle diagnostic start request: `0`
- retained lifecycle required/completed: `0x7f / 0x1`

## Interpretation

This capture proves the current FW0 normal boot path, not the full retained
peripheral diagnostic lifecycle. Only the power owner is expected to appear as
completed in the retained lifecycle section during this normal boot capture.
Display bootstrap has run, audio diagnostic work has not run, and sensor,
storage, and communication diagnostic cycles were not requested.

The ADP driver API version and MR status show the firmware attempted and
completed the ADP5360 MR shipment-enable configuration before the PMIC snapshot.
The snapshot and all I2C lease/status rows passed.

## Still Open

- full START shipping-prep UX and save/quiesce behavior
- START release cancellation before hardware shipment threshold
- wake/recovery after actual shipment entry
- low-battery forced-sleep path
- critical-battery ISOFET disconnect path
- STOP2 owner quiesce/resume and current evidence
- full production boot supervision and fault policy