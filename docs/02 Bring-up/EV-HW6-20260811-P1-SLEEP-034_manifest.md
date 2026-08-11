# EV-HW6-20260811-P1-SLEEP-034 - FW0 pre-STOP sleep-prep scaffold

## Summary

HW6 unit 001 validated the FW0 pre-STOP sleep-prep scaffold after a local build/flash from Codex. This is not STOP2 entry evidence. It proves that `thPower` can accept a debugger-triggered sleep-prep request, enter `PWR_SLEEP_PREP`, run the Platform owner-ACK quiesce barrier, intentionally skip real STOP entry, and recover to `PWR_ACTIVE_LP`.

## Procedure

1. Built Debug firmware with CubeCLT CMake/Ninja.
2. Flashed `build/Debug/peepshow_hw6_fw0.elf` through STM32CubeProgrammer over ST-LINK V3 MINI-E.
3. Attached GDB through ST-LINK GDB server.
4. Set `g_ps_hw6_power_sleep_prep_request = 1`.
5. Detached to let the target run.
6. Reattached and sourced `firmware/peepshow_hw6_fw0/__fw0_sleep_power_prints.gdb`.

## Result

```text
--- HW6 sleep-prep scaffold ---
api/status          = 22 / 0x0
sleep count/start/end = 1 / 23384 / 23384
sleep quiesce/recover/stop-skip = 0x0 / 0x0 / 1
power state/last event = 2 / 5
barrier reason/count/status = 4 / 1 / 0x0
barrier ticks start/end = 23384 / 23384
barrier masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
barrier owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier send stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier ack stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
manual request flag = 0
```

## Interpretation

- Probe API version `22` identifies the sleep-prep scaffold build.
- `sleep_prep_last_status = 0x0` means the scaffold completed successfully.
- `sleep quiesce/recover/stop-skip = 0x0 / 0x0 / 1` means owner quiesce passed, recovery to active LP passed, and real STOP entry was intentionally skipped.
- `power state/last event = 2 / 5` means the final power state is `PWR_ACTIVE_LP` after `PWR_EV_LP_REQUEST` recovery.
- Barrier reason `4` is `SLEEP_PREP`.
- Barrier masks `0x7e / 0x7e / 0x7e / 0x7e / 0x0` prove all non-power owners were requested, commands were sent, ACKs were received, owner actions succeeded, and no owner failed.

## Open Items

- Real STOP2 entry is not validated by this evidence.
- Wake-source arming/classification is still open.
- Resume-after-STOP owner liveness is still open.
- Current measurement for waiting backends is still open.
- Fault-injection and timeout behavior for failed owner ACKs remain open.