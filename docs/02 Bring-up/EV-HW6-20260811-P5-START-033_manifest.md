# EV-HW6-20260811-P5-START-033

## Summary

HW6 unit 001 validated the FW0 START-owned power quiesce barrier and the PMIC
ship-pending ownership fix. START ship-prep now asks each physical owner to
quiesce through its own ThreadX queue, waits for bounded ACKs, records the
send/ACK/action result masks, and keeps PMIC in `PMIC_SHIP_PENDING` while
START owns shutdown prep.

## Result

PASS/PARTIAL.

Validated:

- START ship-prep calls the real owner quiesce barrier instead of the earlier
  counted no-op placeholder.
- `thPower` sends quiesce requests to audio, input, display, sensor, storage,
  and comm owner queues.
- Every owner ACKed and reported `HAL_OK` in the validated START-prep capture.
- START-owned shutdown prep holds power/PMIC as `PWR_SHIP_PREP` /
  `PMIC_SHIP_PENDING` (`8 / 8`).
- Battery monitor no longer recovers `PMIC_SHIP_PENDING` back to
  `PMIC_MONITOR` during START-owned ship prep.

Still open:

- persistent save schema and save-before-shipment policy
- enabled START software shipment with `KNOB_POWER_START_SOFTWARE_SHIP_ENABLE`
- enabled critical-battery software shipment
- enabled boot-low-battery software shipment
- final shutdown/recovery UX
- STOP/sleep-entry quiesce timeout tuning and fault injection

## Initial Barrier Evidence And PMIC Ownership Bug

The first clean START-prep capture proved the barrier itself worked but exposed
that PMIC was being recovered back to monitor by normal battery policy while
START still owned ship prep.

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_start_power_prints.gdb
--- HW6 START shipping-prep scaffold ---
START state/active       = 3 / 1
START hold checkpoint/live ticks = 505 / 656
START prep/warn/imm/rel   = 1 / 0 / 0 / 0
power state/pmic state    = 8 / 3
power last event/status   = 1 / 0x0
power prep/warn/imm/cancel = 1 / 0 / 0 / 0
power quiesce count/status/tick = 1 / 0x0 / 1240
barrier reason/count/status = 1 / 1 / 0x0
barrier ticks start/end = 1240 / 1240
barrier masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
barrier owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier send stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier ack stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
```

Interpretation: the owner barrier passed, but `PMIC_SHIP_PENDING` did not
persist. Firmware was fixed so normal battery-monitor recovery no longer treats
`PMIC_SHIP_PENDING` as a generic recoverable PMIC state. START release/cancel
still explicitly recovers PMIC.

## Passing Retest

After the ownership fix, a clean START hold produced the expected power/PMIC
state and retained the successful barrier result.

```text
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_start_power_prints.gdb
--- HW6 START shipping-prep scaffold ---
button api/edges/presses  = 5 / 1 / 0
last pin/button/event/lev = 16 / 5 / 1 / 0
START state/active       = 3 / 1
START hold checkpoint/live ticks = 505 / 656
START pend press/release = 0 / 0
START armed/live/next   = 1 / 0 / 928
START raw/stable/count  = 0 / 0 / 2
START samples/synth p/r = 28 / 0 / 0
START input owner tick   = 684
START checks            = 2
START press/release tick  = 28 / 0
START prep/warn/imm/rel   = 1 / 0 / 0 / 0
START pending/drop        = 0 / 0
pending tick/hold ticks   = 0 / 0
power state/pmic state    = 8 / 8
power last event/status   = 1 / 0x0
power return state       = 2
power count/hold ticks/tick = 1 / 505 / 533
power prep/warn/imm/cancel = 1 / 0 / 0 / 0
power quiesce count/status/tick = 1 / 0x0 / 533
barrier reason/count/status = 1 / 1 / 0x0
barrier ticks start/end = 533 / 533
barrier masks req/send/ack/ok/fail = 0x7e / 0x7e / 0x7e / 0x7e / 0x0
barrier owner stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier send stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
barrier ack stat A/I/D/S/ST/C = 0x0 / 0x0 / 0x0 / 0x0 / 0x0 / 0x0
power sw ship en/req/skip/status/tick = 0 / 0 / 0 / 0xffffffff / 0
UI page/shutdown/countdown = 8 / 1 / 0
display page/shutdown/countdown = 8 / 1 / 0
pmic MR/sw ship status   = 0x0 / 0xffffffff
pmic sw ship count/tick = 0 / 0
pmic sw ship request    = 0
```

## Interpretation

This closes the FW0 START-prep owner quiesce scaffold. It does not close product
save policy or automatic software shipment gates. The useful invariant now
validated is that START ship-prep can move `thPower` into shipment prep, keep
PMIC aligned in ship-pending, and obtain bounded owner acknowledgement from all
currently participating physical owners before any later software shipment
request is allowed.