# HW6 Battery Shutdown Validation

Status: healthy/warning/critical-preparation results recorded below. Recovery
cleanup revalidation, boot/restart and physical automatic shipment remain pending.

Authority: [[PMIC_and_Power_Contract]], [[Power_and_Sleep_Policy]] and
[[HW6_Hardware_Revision_Contract]]. This is a Platform bench test, not a package
capability or a cell-discharge experiment.

## Scope

First validate actual voltage classification and owner preparation with all
automatic software-shipment gates OFF. Physical automatic shutdown, restart
after shipment, charger recovery and failure fallback are later tests. A
debugger disconnect or a preparation counter is not physical shutdown proof.

The existing battery RTC test separately established one shortened wake and
successful due reading. It did not qualify the 30-minute interval or energy cost.

## Setup

- Flash the build containing battery preparation probe API 1 and bounded retries.
  Use its matching ELF and helpers. Preserve the installed package.
- Physically disconnect the cell. Connect the PPK2 to the board's battery input
  with verified polarity, in Source Meter mode. Begin at 3800 mV.
- Keep the device USB disconnected throughout this first test. The PPK2's own
  USB remains connected. Do not combine battery-input source testing with USB
  charging: the external source must not inadvertently become a charge sink.
- ST-Link may remain connected for debugging, with target-voltage sensing and
  common ground, not a second target power source. Stop if the board remains
  unexpectedly powered when the intended supply is off.
- Do not change charger settings, use a real cell at these test points, or test
  the emergency hardware undervoltage cutoff. Do not lower the source below
  3200 mV in this procedure.

Nordic documents the adjustable DUT supply in
[PPK2 Source Meter mode](https://docs.nordicsemi.com/r/bundle/ug_ppk2/page/ug/ppk/measure_current_source_meter.html).
The battery-input connection and USB exclusion above are the HW6 test setup,
not a claim that PPK2 emulates every electrical characteristic of a cell.

## Capture

At each point, run the target normally for several seconds before halting:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
```

Record the source setting, PMIC-read VBAT, snapshot/fuel/VBUS validity, policy
state, preparation attempt/result and quiesce masks. Use measured VBAT to judge
threshold crossings, not the source setting alone. Do not alter probe fields
to simulate successful readings or acknowledgements.

This test is not a wake-cadence measurement. Wake the package with a normal
button when necessary so the owner can acquire a fresh sample. The shell may
remain awake on this build; it is suitable for an awake read, not STOP2 proof.

## First Test: Gates Off

1. At 3800 mV, boot and verify normal package operation. Require valid snapshot
   and fuel data, PMIC VBUS absent, both battery shipment gates zero, and the
   preparation probe idle. Stop here if the read is invalid or VBUS is present.
2. With the device running, set 3450 mV. Wake normally if needed and allow a
   fresh read. A PMIC voltage between 3300 and 3500 mV should select WARNING,
   without starting critical preparation or requesting software shipment.
3. Set 3200 mV without resetting. Wake normally if needed; run for at least five
   seconds before halting. Require a valid PMIC voltage at/below 3300 mV and
   CRITICAL policy. On a healthy preparation path, expect attempts=1,
   prepared=1, exhausted=0, last status=0, successful owner quiesce, and one
   default-off shipment skip. Both policy software-shipment requests and
   physical driver shipment requests must remain zero. Do not expect power-off.
4. If attempts exceed one or exhausted=1, retain the full print and stop to
   investigate the failing admission/quiesce status. The target must not bypass
   that failure. Do not repeatedly reset it to hide the failure.
5. Restore 3800 mV before ending this runtime-critical test. Allow a valid
   recovery reading and capture it. Automatic package resume is not a pass
   requirement here; battery preparation does not use START's cancel/resume
   semantics. Reset at healthy voltage to establish a fresh normal baseline.
6. For a separate boot test, switch the source output off, select 3400 mV, then
   power on (press START if the PMIC requires it). Use a real reset/power cycle,
   not debugger reconnect. With valid VBAT below 3600 mV and no VBUS, expect
   BOOT_RESTART_BLOCKED, no package launch, and the default-off shipment skip
   after successful preparation. This point is above the critical threshold
   and specifically checks the separate restart threshold.
7. Raise the source to 3800 mV without resetting and allow a fresh sample.
   Require the boot gate to clear only once PMIC-read VBAT reaches at least
   3600 mV. Record recovery UI/runtime behaviour; do not assume package restart
   solely from the gate-clear counter. Leave the bench at healthy voltage.

The thresholds above are the existing provisional knobs, not newly qualified
cell limits. A high current during debug or gated-off shipment is not a
measurement of actual shipment current.

## Later Tests

Do not enable automatic shipment solely because the first test passes.

- Preparation retry/exhaustion is native-tested with fake owner failures.
  Target failure injection must be explicit and bounded; no live probe editing
  to force ACKs and no uncontrolled broken-bus experiment.
- Review and test failed physical PMIC shipment writes and exhausted preparation
  fallback before claiming complete protection. The current fix retries
  preparation, not the final shipment write.
- Enable automatic critical/boot shipment only in a separately identified test
  build. Confirm a physical current reduction, correct supply/rail behaviour,
  and no unintended repeated restart. GDB loss alone is insufficient.
- Test START below and above restart-allow, then charger-present recovery.
  Charger testing needs a separate approved power setup; do not plug device USB
  into the battery-input PPK2 setup used above.
- Measure charge per battery wake and select warning/retry cadence against
  discharge margin and shutdown behaviour. The 60-second interval is provisional.

## Result Record

Record firmware commit/build, unit ID, source mode/voltage, debugger state,
actual PMIC voltage, captures and physical observations for each step. Keep
preparation pass, physical shipment pass, restart pass and energy measurements
separate.

### 2026-09-14 Gated-Off Runtime Test

- At source 3800 mV, measured 3772 mV: policy OK, boot gate cleared, VBUS absent,
  valid full snapshot, no preparation or shipment requests.
- At source 3450 mV, measured 3443 mV: WARNING, no battery preparation or shipment
  requests. Shared sleep quiesce activity was not a battery-shutdown attempt.
- At source 3200 mV, measured 3156-3161 mV: CRITICAL, package suspended, one
  successful preparation, all owner masks `0x7e`, no failures, one gated-off
  shipment skip and no physical shipment request. The later timing capture
  below disqualifies this as a clean preparation pass: it depended on an
  internal clock timeout. Physical shipment and retry exhaustion were not tested.
- The barrier spanned ticks 588..1588 (1000 ticks at 100 Hz). Display was the
  last uncompleted owner in an intermediate halt. The owner send/ACK status
  record is published after the wait returns, so its intermediate NOT_RUN
  value does not prove that the command had not been sent.
- At source 3800 mV again, measured 3782 mV: policy OK, preparation tracker reset
  and power active, but PMIC remained `SHIP_PENDING`. The recovery code omitted
  that state. Package suspension is separate and does not imply auto-resume.

The new recovery cleanup is native-tested and built but not yet hardware-tested.
The completed timing capture confirmed the circular wait at measured 3163 mV:
barrier 484..1484, display 486..1484, clock waiting at both barrier begin and
display dispatch, one clock completion/failure with status `0x7` and elapsed
1000 ticks. All other owners completed by tick 486. Overall quiesce reported
success only after the display clock wait timed out. This is not clean shutdown
preparation, despite all final owner ACKs being successful.

The corrective increment uses the existing direct display-clock grant and
power/display handoff for battery-critical and boot-low barriers. Pending clock
waits can complete, and display clock requests during the handoff do not queue
back to blocked thPower. The existing stale-request handling prevents the old
queued request from reinstating its transfer capabilities afterward. Cleanup
releases the grant and ends the handoff; clock grant/release failures reject
preparation. Normal sleep and START barriers are unchanged.

Five focused battery tests, Debug build, V2 runtime stack and target-profile
checks pass. The native handoff test uses actual barrier/request/handoff code
with simulated queue boundaries, not a ThreadX scheduling or physical shutdown
test. It covers an outstanding clock request, new requests during the handoff,
grant/send/ACK/release failures and unchanged sleep/START paths. Build usage:
RAM 552912, ROM 880160, SRAM4 15480 bytes. Target confirmation remains pending.

### Next Capture

Flash the matching new build at 3800 mV with cell and device USB disconnected.
Boot normally, then lower the source to 3200 mV without resetting. Wake normally
if needed and allow 15 seconds running time without halting during preparation.
Then halt and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_quiesce_timing_prints.gdb
```

Require preparation status 0, prepared=1, all owner ACKs successful, no display
clock failures and no 1000-tick timeout plateau. Clock completion count may be
zero if no queued clock wait overlaps the capture; bypassed requests are not
counted as queue completions. Restore 3800 mV,
resume several seconds, and print battery power again. Recovery should now give
policy OK and power/PMIC `2/3`, with no shipment request. Do not require automatic
package resume, enable shipment, or proceed to charger testing yet.
