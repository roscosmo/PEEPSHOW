# EV-HW6-20260802-P5-FLASH-010

## Summary

- Test case: lifecycle-v9 `ps_dev_at25sl128a` driver-backed storage-owner JEDEC/release/deep-power-down cycle
- Result: `PASS`
- Date/time: `2026-08-02 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `94f58f85b0eb582647b9483589ee57fdef58f253`
- Firmware state: uncommitted/untracked HW6 FW0 `ps_dev_at25sl128a` wrapper integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `9E0C7D042130B89957E8BF79CEA1970B84B24AD0E95B37B4584DEB452EEB3070`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`, continued the target, halted without reset after completion, and sourced the read-only consolidated report `__fw0_all_probe_prints.gdb`.

The run used `ps_dev_at25sl128a` from the storage owner path. The wrapper owns non-destructive JEDEC identity reads, release from deep-power-down, deep-power-down entry, HAL status mapping, and OSPI state/error telemetry for this checkpoint. No erase, program, FileX/LevelX, package, or USB MSC behavior was exercised.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_flash_driver_gdb.log` | `89E0F01ABE17DDEC2898E6BF7B68F266908C822BB5984C8CBF0CF5849C1FDF42` | Exact target-memory and transition-trace report from the operator transcript |

The original operator transcript hash was `3A933EA91BE05ED7B4A60C3EBBC24108C5821ACB94998BCFB84A72BD6FF80290`; the normalized LF evidence log hash is `89E0F01ABE17DDEC2898E6BF7B68F266908C822BB5984C8CBF0CF5849C1FDF42`.

## Observations

- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Retained-peripheral lifecycle state machine reported version `0x7`, `complete/success = 1 / 1`, required/completed `0x7F / 0x7F`, and success/failure owners `0x7F / 0x0`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x0`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- Both flash cycles reported `flash release/JEDEC/match/DPD = 0 0 1 0`.
- The AT25 driver probe reported `driver API/init/state/ops/last = 1 / 0 / 3 / 8 / 0`.
- Baseline JEDEC telemetry reported `JEDEC status/ID/match = 0x0 / 1f 42 18 / 1`.
- Baseline deep-power-down status was `0x0`.
- Final OSPI state/error were `0x2 / 0x0`.
- Detached USB storage-owner park remained valid: VBUS `0`, PCD before/after `0x1 / 0x0`, clock before/after `1 / 0`, VDDUSB before/after `1 / 0`, and `USB parked = 1`.
- The operator-provided transcript preserved the expected physical workflow context for the three display-card presents and three tones.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1355`.

## Conclusion

The storage-owner lifecycle path now uses a typed `ps_dev_at25sl128a` wrapper instead of owner-local raw OSPI command helpers for non-destructive JEDEC identity, release from deep-power-down, and deep-power-down entry. The wrapper-backed path preserved the validated AT25SL128A identity `1F 42 18`, clean HAL command statuses, final deep-power-down state, and clean OSPI error state across the baseline workflow plus two bounded owner-routed resume/quiesce cycles.

This evidence does not close scratch erase/program/readback, DMA flash transfers, LevelX/FileX integration, USB MSC export/reclaim, protected fault-log layout, package installation, storage timing budgets, current, or storage fault injection/recovery.