# EV-HW6-20260731-P1-ADP5360-003

## Summary

- Test case: guarded no-cell/no-VBUS ADP5360 candidate write, readback, and
  exact restoration
- Result: `PASS`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Setup: naked PCB powered through the PPK2 battery path; ST-LINK attached;
  no production cell or VBUS connected
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- FW0 ELF SHA-256:
  `7032E27674CA2B8536E188635D0451319E20CB8AA076FF75E6E96359ABE917D5`
- FW0 IOC SHA-256:
  `F92EC587CCDE0261C6EC565447E38D627E5FB49B4FE1137890176C250DA195B4`

## Result

- All safety guards passed: PMIC identity matched, VBUS was absent, charger
  status was idle, fault was clear, and software charging was disabled.
- Register order: `0A 03 04 20 27 07`.
- Original values: `00 7A 29 32 50 2C`.
- Candidate values: `80 82 3F E1 53 AC`.
- Candidate readback matched all six values.
- Reverse restoration readback matched all six original values.
- Snapshot, write, verify, candidate-match, restore, restore-verify, and
  restore-match masks were all `0x3F`.
- Final fault was `0x00`; final PGOOD was `0x07`; VBUS remained absent.
- Total bounded transaction duration was `24 ms`.

## Scope

This validates register acceptance and the exact reversible transaction on
HW6 unit 001. It does not approve 320 mA real-cell charging, validate JEITA
temperature behavior, characterize the fuel gauge, change the VBUS input
limit, or validate the retained protection thresholds.
