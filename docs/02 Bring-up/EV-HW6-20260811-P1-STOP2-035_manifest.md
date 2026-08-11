# EV-HW6-20260811-P1-STOP2-035 - FW0 manual STOP2 START-wake scaffold

## Summary

HW6 unit 001 validated the first FW0 real STOP2 scaffold, the first baseline post-wake owner-liveness pass, and the first staged active-owner STOP2 resume pass. A debugger-triggered request was handled by `thPower`, all non-power owners acknowledged the bounded sleep-prep quiesce barrier, `thPower` entered real STOP2 through `HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI)`, and a physical START press woke the MCU. After wake, firmware restored the system clock, resumed the HAL tick, sent bounded post-STOP resume commands to all non-power owners, recorded owner ACK/liveness proof, and returned the power FSM to `PWR_ACTIVE_LP`. Probe API `27` added a debugger-only staged active-owner path so bring-up can prove active owner resume/quiesce completion before deliberately entering STOP2.

This is not production automatic sleep policy evidence. It validates the manual STOP2 entry/wake/resume scaffold only. It now includes both the inactive/parked baseline and a staged active-owner proof where audio, input, display, sensor, and comm owners were made active, quiesced, STOP2-entered, and then resumed or confirmed live after START wake. It does not yet prove automatic STOP admission, wake-source classification policy, LPBAM, current, repeated sleep/wake cycles, or fault-injection behavior.

## Procedure

1. Built and flashed the FW0 images containing probe API versions `23` and later `25`.
2. Halted the target after normal boot.
3. Sourced `firmware/peepshow_hw6_fw0/__fw0_stop2_power_request.gdb` to set `g_ps_hw6_power_stop2_request = 1`.
4. Continued the target so `thPower` could process the manual request.
5. Pressed START briefly to wake the MCU from STOP2.
6. Halted the target and sourced `firmware/peepshow_hw6_fw0/__fw0_stop2_power_prints.gdb`.
7. Repeated the same manual STOP2 path on probe API version `25` after adding the post-STOP owner resume/liveness barrier.
8. Repeated the STOP2 path on probe API version `27` with a staged active-owner debug flow: first `__fw0_stop2_active_prep_request.gdb` to bring selected owners active and quiesce them without entering STOP2, then `__fw0_stop2_active_enter_request.gdb` to enter STOP2 only after `prep_ready = 1`.
9. Pressed START to wake from the staged active-owner STOP2 pass, allowed post-wake resume to complete, and sourced `__fw0_stop2_power_prints.gdb`.

## Result

```text
--- HW6 STOP2 START-wake scaffold ---
api/status          = 23 / 0x0
stop2 count/start/wake/end = 1 / 229 / 229 / 229
stop2 quiesce/enter/clock/recover = 0x0 / 0x0 / 0x0 / 0x0
stop2 expected wake pin = 0x10
stop2 IDR before/after = 0x6055 / 0x6045
power state/last event = 2 / 5
barrier reason/count/status = 4 / 1 / 0x0
barrier ticks start/end = 229 / 229
barrier masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
barrier owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier send stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier ack stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
manual request flag = 0
```

Second pass with post-STOP owner resume/liveness barrier:

```text
--- HW6 STOP2 START-wake scaffold ---
api/status          = 25 / 0x0
stop2 count/start/wake/end = 1 / 835 / 835 / 835
stop2 quiesce/enter/clock/recover = 0x0 / 0x0 / 0x0 / 0x0
stop2 expected wake pin = 0x10
stop2 IDR before/after = 0x6055 / 0x6045
power state/last event = 2 / 5
barrier reason/count/status = 4 / 1 / 0x0
barrier masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
barrier owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
post-resume count/status = 1 / 0x0
post-resume masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
post-resume noop/action masks = 0x7e / 0x0
post-resume owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
post-resume send stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
post-resume ack stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
manual request flag = 0
```
## Interpretation

- Probe API version `23` identifies the initial STOP2 START-wake scaffold build; probe API version `25` identifies the follow-up build with post-STOP owner resume/liveness proof.
- `stop2_last_status = 0x0` means the full manual scaffold completed successfully.
- `stop2 quiesce/enter/clock/recover = 0x0 / 0x0 / 0x0 / 0x0` means owner quiesce passed, STOP-entry transition passed, clock restore was recorded as OK after wake, and recovery to active LP passed.
- `stop2 expected wake pin = 0x10` is START on PA4.
- `stop2 IDR before/after = 0x6055 / 0x6045` shows PA4 high before STOP entry and low after wake capture, matching START being pressed during wake.
- `power state/last event = 2 / 5` means final power state is `PWR_ACTIVE_LP` after `PWR_EV_LP_REQUEST` recovery.
- Barrier reason `4` is `SLEEP_PREP`.
- Barrier masks `0x7e / 0x7e / 0x7e / 0x7e / 0x0` prove all non-power owners were requested, commands were sent, ACKs were received, owner actions succeeded, and no owner failed before STOP2 entry.
- Post-resume masks `0x7e / 0x7e / 0x7e / 0x7e / 0x0` prove all non-power owners were requested again after wake, commands were sent, ACKs were received, owner liveness checks succeeded, and no owner failed.
- Post-resume noop/action masks `0x7e / 0x0` mean every non-power owner was already in an inactive or parked state for this baseline. This is valid liveness evidence, not active hardware resume evidence.

## Open Items

- Production automatic STOP2 admission is still open.
- Wake-source classification is still open beyond this expected START wake observation.
- RTOS/HAL tick compensation across STOP residency is still open.
- STOP2 current measurement is still open.
- LPBAM waiting display behavior is still open.
- Timeout/fault behavior for failed owner ACKs remains open.