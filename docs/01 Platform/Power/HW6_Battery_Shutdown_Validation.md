# HW6 Battery Shutdown Validation

Status: preparation/recovery, automatic low-boot shipment, runtime-critical
shipment and shortened unattended battery-wake shutdown pass at the bench
points below. Returned-command failure handling has native regression coverage;
permanent-failure fallback, charger recovery and complete discharge protection
remain pending.

## Current Build Defaults (2026-09-15)

Normal Debug now enables automatic critical-battery and boot-low-battery
shipment. START software shipment remains off. This promotes the tested
battery policy without changing voltage thresholds, wake periods, retry
budgets, physical owner checks or fault fallback. Normal-build low-boot and
shortened unattended critical-shutdown/recovery checks now pass by user bench
observation; earlier evidence used the isolated BatteryShutdownTest preset.

Verification: nine focused battery tests pass, including actual policy tests
with enabled/disabled gates, schema/generated-default agreement and CMake
snapshot admission/rejection. Both Debug and BatteryShutdownTest build
successfully: RAM 553632, ROM 883944, SRAM4 15480 bytes. Existing unused-function
and unused-parameter warnings remain. No hardware was flashed by the agent.

Historical gates-off procedures below describe the builds tested at those
checkpoints, not today's normal Debug defaults. Manual shipment and fault-test
helpers intentionally refuse enabled normal builds. They require an explicitly
identified diagnostic build with all shipment gates disabled. Do not bypass
their guards. The BatteryShutdownTest preset still requires both battery gates
on and permits no other differences from the current generated knobs snapshot;
its gate comparison now works with either enabled or disabled normal defaults.

Normal-build bench confirmation uses the isolated PPK2 battery-input supply,
cell and device USB disconnected:

1. Flash `HW6 FW0: Debug with ST-LINK` at 3.8 V. Resume and confirm normal boot.
2. Intentionally restart at 3.4 V. It should return to shipment after successful
   preparation; a screen update is not required. Confirm low current, not merely
   debugger loss. START while still at 3.4 V should repeat that refusal.
3. Raise to 3.8 V and START: normal boot should recover.
4. For the separate unattended-critical check, use the existing
   `__fw0_battery_wake_enable.gdb` at healthy voltage to shorten one deadline to
   15 seconds. Resume into ordinary STOP2, lower the isolated source to 3.2 V
   and observe shipment after the due battery check. No post-shutdown RAM print
   is expected. Collect current/rail evidence, then restore 3.8 V and START.

Actual healthy sleep cadence remains 30 minutes unless deliberately shortened
for this test. Earlier fault-test wake peaks were only visually estimated;
the user clarified that the apparent 10-second spacing was not a measurement.
Regular wakes were confirmed, but exact wall-clock cadence remains unmeasured.

### Normal-Build Bench Confirmation

Against the promoted normal-build defaults committed at `7a97e3858b8fab5a0e39c216b6c0bbcd7a5fb97f`,
the user confirmed shutdown at 3.4 V and normal boot on START at 3.8 V.
For the separate shortened battery-wake test, the user followed the STOP2
procedure above and reported shutdown after approximately 15 seconds, then
confirmed normal boot on START after restoring 3.8 V. These are physical bench
observations, not inferred from debugger disconnection or queue counters.
No new numeric shipment-current, rail trace or post-shutdown RAM capture was
provided for this normal-build confirmation. The 15-second deadline is a test
override, not a change to the 30-minute healthy-battery default.

This closes the normal-build low-boot and unattended critical-shutdown/restart
checks at those bench points. It does not qualify exact long-interval timing,
charger-connected recovery, permanently failed hardware or complete cell
discharge protection.

Authority: [[PMIC_and_Power_Contract]], [[Power_and_Sleep_Policy]] and
[[HW6_Hardware_Revision_Contract]]. This is a Platform bench test, not a package
capability or a cell-discharge experiment.

## Scope

First validate actual voltage classification and owner preparation with all
automatic software-shipment gates OFF. The prepared one-shot shipment/restart
result and subsequent automatic-shutdown results are recorded below. Charger
recovery and failure fallback remain later tests. A
debugger disconnect or a preparation counter is not physical shutdown proof.

The existing battery RTC test separately established one shortened wake and
successful due reading. It did not qualify the 30-minute interval or energy cost.

## Setup

- Use the matching build and helpers; preparation-only evidence used probe API 1,
  while returned shipment-command result handling uses API 2.
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

## Prepared One-Shot Physical Test

This first physical step uses the already-built, gates-off firmware. It does
not enable automatic battery or START shipment. The helper checks successful
battery preparation, the matching owner barrier, clock cleanup, low voltage,
absent VBUS, PMIC boot setup, and no earlier shipment attempt. It only sets the
existing `g_ps_hw6_pmic_software_ship_request`; thPower performs the PMIC write.
It does not call target functions or overwrite readings/ACKs. The checks use
the last completed target readings, not new measurements while halted.
The helper loads against the matching ARM ELF offline. A host-only GDB fixture
executes the actual helper across 13 cases: prepared boot/critical acceptance,
API/voltage/read/VBUS/preparation/owner/ACK/clock/pending/duplicate/default-gate
refusals, including rejection of a second source after queuing. Only the request
flag is written. This validates helper behaviour, not the physical PMIC result.

1. Keep the isolated setup above: cell disconnected, device USB disconnected,
   PPK2 supplying the battery input. Begin at 3800 mV. Do not reflash just for
   this helper; use the matching ELF from the passed preparation build.
2. Boot at source 3400 mV and let the warning appear. Run several seconds before
   halting. Save the battery power and quiesce timing prints. Require successful
   preparation and zero automatic gates, as in the recorded pass.
3. Start PPK2 recording and note the pre-shutdown current. Keep the source
   voltage and wiring unchanged, halt, then source:

   ```gdb
   source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_ship_once_enable.gdb
   ```

4. If it prints NOT armed, retain that output; do not force the request flag.
   If it queues the request, resume immediately and leave all buttons alone.
   Observe the current transition and sustained post-shutdown level, plus an
   accessible system supply rail if measurable. The warning image may remain;
   a retained frame or debugger disconnect does not settle the power verdict.
5. If current does not fall or the board repeatedly restarts, stop this test.
   If still powered, halt and use the battery power print for the actual PMIC
   shipment result. Do not repeatedly source the helper after a failed attempt.
6. After a sustained power-off observation, raise the source to 3800 mV and
   press START to test healthy restart. Record whether the package boots and
   works. Raising voltage alone is not assumed to leave PMIC shipment mode.
   If it does not restart, stop and report rather than repeatedly cycling it.

Report the PPK2 source settings, pre/post current, debugger connection state,
rail observation if available, and restart behaviour. Debug wiring can alter
low-current readings; any suspected back-power invalidates the current result.
No absolute shipment-current limit is qualified by this procedure.

The request flag is consumed once by thPower; the helper rejects another
attempt in the same boot. Power loss resets RAM, so retain the pre-request
prints and PPK2 trace rather than expecting post-mortem counters to survive.
This establishes only the prepared-state -> manual physical shipment ->
healthy restart path. Low-voltage restart with automatic re-shipment, fully
automatic critical/boot triggering, final-write failure and charger recovery
remain separate tests. Automatic-gate enablement still requires the identified
test-build procedure, not arbitrary writes to status probes.

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

The recovery cleanup was initially native-tested; subsequent target results
are recorded below.
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
RAM 552912, ROM 880160, SRAM4 15480 bytes for that increment.

### Runtime Revalidation and Low-Boot Follow-Up

- At measured 3175 mV, preparation completed on attempt 1 in ticks 479..482
  (30 ms). All owner masks were 0x7e with no failures. A pending display clock
  request completed successfully in 3 ticks, with no clock failures. Preparation
  succeeded without the previous timeout. Shipment remained disabled.
- Restoring source 3800 mV produced measured 3782 mV, policy OK, power/PMIC 2/3,
  reset preparation tracking and no physical shipment request. Runtime
  preparation/recovery passes; this does not prove physical power-off.
- Booting at source 3400 mV produced measured 3374 mV, no VBUS, boot blocked and
  CHARGE BATTERY (LOW_BOOT) displayed. This message does not mean charging.
  Preparation succeeded on attempt 2; the retained second barrier was
  1384..1384 with all owner ACKs/actions successful and no clock failures.
  The first attempt's failure was overwritten. Low-boot preparation is not yet
  a clean pass. The disabled shutdown gate leaves power/PMIC 8/8, which blocks
  ordinary STOP2: staying awake is a gated-test condition, not acceptable final
  depleted-battery behaviour.

Timing API 2 now preserves a separate first failed battery barrier until reset,
including owner action/ACK results and clock grant/release results. Success,
later failures, sleep barriers and voltage recovery cannot overwrite it.
No admission-before-barrier failures are captured by this record. No scheduling,
retry, shipment or voltage policy is changed. Five focused battery tests and
Debug build pass; RAM 553296, ROM 880832, SRAM4 15480 bytes. The readonly helper
loads against the matching ELF offline; live first-failure capture is pending.

The API-2 capture subsequently preserved the first boot failure:
barrier 258..1260, storage 260..1260 with ACK status 0x7/action NOT_RUN, input
ACK success but action 0x1. After resuming, barrier 2 at tick 1384 reported all
owner actions/ACKs successful and the warning appeared. The first failure was
unchanged, proving retention across the retry. A late storage action success in
the live power probe must not be mistaken for a timely first-attempt ACK.

At that checkpoint the first-failure mechanism was unresolved. Storage boot calibration
requests clocks from thPower; power can evaluate low-battery policy while that
work is pending. Input's quiesce path calls joystick sleep without first using
its ordinary stabilization/recovery path. These are candidate dependencies,
not proof of the exact failed operations. API 3 adds storage-clock wait markers,
boot/calibration progress and input driver state/results to the same retained
record. No policy, retries, clocks or owner dispatch behaviour is changed.
Five focused battery tests and the Debug build pass; the helper loads offline
against the matching ELF. RAM 553456, ROM 881320, SRAM4 15480 bytes.

### API-3 Result and Corrective Increment

At measured 3371 mV with no VBUS, the first barrier again took ticks 258..1260.
Power boot was done, calibration load had started but was unresolved, and
storage clock wait was active at barrier begin, dispatch and completion. The
request sampled at dispatch started at tick 28 with capabilities 0x2. This
confirms the power/storage circular wait; the flags alone do not prove that one
unchanged clock request persisted at all three sampling points. The successful
retry had calibration resolved and no outstanding storage clock wait.

Input remained JOY_OFF (0), while the driver went READY (1) to WAKE_SLEEP (5).
Ready/identity/sleep-write/driver status and I2C error were zero, with terminal
sleep committed. Its action HAL_ERROR came from requesting an undefined
JOY_OFF -> QUIESCE transition after hardware success, not from failed sleep.
The user observed a long blank period before the warning appeared.

The corrective build holds flash clocks under thPower during battery barriers,
including early boot before FLASH_READY. It answers the outstanding storage
clock request, avoids further circular clock waits during quiesce, and consumes
the superseded queued request without reapplying it or generating another ACK.
The real storage owner still performs its work and acknowledges quiesce.
Unsupported capabilities and grant failures are not reported as successful;
cleanup cannot overwrite the pending caller's failed grant result.
Joystick first-sleep success now handles OFF/SUSPENDED like the existing cached
proof path. Actual sleep-write or required-transition failures remain failures.

Focused native tests exercise the actual request/barrier and joystick quiesce
functions with mocked hardware/queue boundaries. They cover pending storage
work, flash grant retention through a local release, unsupported capabilities,
grant/send/ACK/release failures, stale request consumption, ordinary sleep/START,
first and repeated joystick sleep, and real sleep failure propagation. These
are not a ThreadX scheduling or physical shutdown test. Debug build passes:
RAM 553472, ROM 881680, SRAM4 15480 bytes. Shipment gates remain disabled.

### Low-Boot Revalidation Pass

The 2026-09-14 corrective-build capture at measured 3373 mV, no VBUS, passed:

- Admission and preparation succeeded on attempt 1, with no exhaustion.
- Barrier ticks 258..261: 30 ms rather than the previous 10-second timeout.
- All six owner sends, ACKs and actions succeeded; masks were 0x7e with no
  failed bits. FIRST FAILED had no record.
- Storage clock wait was 1 at barrier begin and 0 at dispatch/completion.
  Required/grant/release were 1/0/0; storage completed at tick 261.
- Input remained JOY_OFF, while the driver reached WAKE_SLEEP. Ready, identity,
  sleep-write and action statuses were zero, terminal sleep committed was 1,
  and I2C error was zero.
- The user confirmed the warning appeared directly after boot, and the UI and
  display probes both showed LOW_BOOT. The 30 ms is preparation time, not a
  measured reset-to-visible-warning interval.
- The user confirmed recovery after raising voltage. No post-recovery probe
  was supplied in this run, so this is observed recovery rather than a new
  numerical policy/PMIC recovery capture.

This closes the delayed-warning/first-preparation failure. Shipment gates were
0/0, shipment requests remained zero and the gate skip incremented once.
Physical shutdown, shutdown current and automatic discharge protection remain
unqualified; the device still stays awake in this gated test condition.

### Repeat Procedure

Flash the corrective build at 3800 mV with cell and device USB disconnected.
Set the source to 3400 mV and reset to perform a low-voltage boot. Allow 15
seconds uninterrupted execution, observing when the warning appears, then halt
and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_power_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_quiesce_timing_prints.gdb
```

Expect attempt 1 prepared successfully, all owner actions/ACKs successful,
storage clock required/grant/release 1/0/0, and no 1000-tick stall. The warning
must be observed; queue counters alone do not prove it was drawn promptly.
Capture both LATEST and FIRST FAILED sections, even if the retry succeeded.
A missing first-failure record means no qualifying barrier failure was captured,
not proof that no admission failed. Do not reset between boot and printing.
Restore 3800 mV, resume several seconds, and print battery power again. Recovery should give
policy OK and power/PMIC `2/3`, with no shipment request. Do not require automatic
package resume, enable shipment, or proceed to charger testing yet.

### Prepared One-Shot Shipment and Restart Pass

On 2026-09-14, booting with source 3400 mV produced a valid prepared low-boot
state. `__fw0_battery_ship_once_enable.gdb` accepted measured 3369 mV and queued
one request. After resuming, the user observed PPK2 current fall and settle at
5.6 uA. This is physical current evidence, not merely a queued-command counter
or debugger disconnect. No separate system-rail voltage measurement was supplied;
5.6 uA is the observed bench result, not a qualified production current limit.

Pressing START restarted the device while the source was still 3400 mV. It
remained on LOW BATTERY at approximately 2.5 mA. This is expected for the
gates-off build: the one-shot request does not survive reboot and does not
enable automatic re-shipment. It is not acceptable final low-battery behaviour.
The user then confirmed normal boot after restoring source 3800 mV.

Pass scope: successful preparation -> manually requested physical shipment ->
START wake, low-voltage boot block and normal boot at healthy source voltage.
Automatic critical/boot triggering and repeated low-voltage restart shutdown
remain untested. Next is a separately identified automatic-shutdown test build;
do not treat the one-shot result as completed automatic discharge protection.

### Isolated Automatic-Shutdown Build

The `BatteryShutdownTest` configure/build preset is a bench-only increment.
It enables critical-battery and low-boot shipment through the existing knobs
pipeline. Normal Debug remains gates-off. START shipment, thresholds, owner
preparation, retry limits and the final PMIC write path are unchanged.
The test ELF is `build/BatteryShutdownTest/peepshow_hw6_fw0.elf`.

The generated snapshot is a local build artifact, not another configuration
source. To recreate it, from `firmware/peepshow_hw6_fw0`:

1. Stop builds. Temporarily set only `power_critical_software_ship_enable` and
   `power_boot_low_battery_ship_enable` to `true` in `config/knobs.json`.
2. Generate and retain the test header:

```powershell
python tools/gen_knobs.py
New-Item -ItemType Directory -Force build/BatteryShutdownTest
Copy-Item Core/Inc/knobs_autogen.h build/BatteryShutdownTest/battery-shutdown-knobs.h
```

3. Restore those two JSON values to `false`, then regenerate normal knobs and
   build the isolated profile:

```powershell
python tools/gen_knobs.py
cmake --preset BatteryShutdownTest
cmake --build --preset BatteryShutdownTest
```

Do not commit temporary enabled values or manually edit either header. CMake
rejects missing/stale snapshots, any difference beyond those two gates, and
attempts to use this profile in the normal Debug directory. The snapshot is
force-included in C compilation; its generated include guard prevents normal
knobs from replacing it. Changing normal knobs requires snapshot regeneration.

On 2026-09-14, isolated and normal builds passed. Offline GDB macro inspection
of their actual ELFs confirmed critical/boot/START gates `1/1/0` and `0/0/0`
respectively. Seven focused battery tests passed, including eight build-profile
admission cases. These are build checks, not automatic-shutdown hardware proof.

### Automatic-Shutdown Bench Procedure

Use the isolated PPK2 setup above: cell and device USB disconnected. Do not
use a real cell for forced low-voltage tests.

1. At source 3800 mV, select VSCode
   `HW6 FW0: BATTERY SHUTDOWN TEST (flash)`, then resume through normal boot.
   Halt and run `__fw0_battery_power_prints.gdb`; require critical/boot gates
   `1/1`. Use the matching test attach configuration for later reconnects.
2. Set source 3400 mV and intentionally reset or power-cycle. Resume without
   halting during preparation. No one-shot helper is required. Expect automatic
   preparation followed by shipment and a sustained current reduction.
3. Press START while still at 3400 mV. This new low-voltage boot must shut down
   again without another debugger request. It must not remain drawing mA or
   repeatedly restart without a button press.
4. Restore 3800 mV and press START. Expect normal boot and package operation.
5. After the low-boot result is confirmed, test runtime critical voltage:
   boot normally at 3800 mV, then lower the source to 3200 mV without resetting.
   Expect the critical path to prepare and shut down. Do not lower further.

Observe current and, where accessible, the system rail. Debugger loss alone
does not prove shutdown. The warning may be brief or not finish drawing before
power removal; this increment adds no warning dwell. The earlier 5.6 uA result
is a comparison, not a new acceptance limit. Record restart behaviour as well
as the settled current. If shutdown fails or a restart loop appears, restore
3800 mV and capture the existing battery power/preparation probes if accessible;
do not force a manual shipment to turn a failed automatic test into a pass.

The flashed test remains automatic-shutdown enabled across restarts. At the
end, restore 3800 mV and flash normal Debug; confirm gates `0/0`. Editing or
restoring source knobs alone does not change firmware already on the device.
Final-write failure handling and charger interaction remain separate tests;
successful automatic shipment alone is not complete discharge-protection proof.

### Automatic-Shutdown Hardware Results

On 2026-09-14, with the isolated automatic-shutdown build and cell/device USB
disconnected, the user confirmed:

- Source 3400 mV boot immediately entered shipment before visible screen work.
  START at the same voltage repeated the shutdown; source 3800 mV booted normally.
- After a healthy boot and STOP2 entry, source 3200 mV did not immediately cause
  shutdown. One button wake returned to STOP2; a later wake caused automatic
  shipment, with settled PPK2 current 4.7 uA. An arbitrary input wake does not
  force a battery read; the monitor's read cadence still applies.
- With the existing one-shot battery deadline shortened to 15 seconds, the
  device woke from STOP2 without input and shut down at source 3200 mV. Settled
  current was 4.8 uA. The user restored source 3800 mV afterward.

These are physical current and observed behaviour results, not just request
counters. No rail-voltage capture or production current limit is claimed.
The 15-second result does not qualify a full 30-minute residency or its energy
cost. These results precede the returned-command retry implementation below.

### Returned Shipment Failure Handling

The 2026-09-14 review found that the power loop cleared a shipment request and
discarded the returned owner status. Prepared state then suppressed subsequent
attempts, even after a failed final command. Normal STOP2 rejects the remaining
shipment-preparation state, so that was not an automatic low-power fallback.

Battery requests now use their own pending field and result handling in
`thPower`. Failed calls clear prepared state and wait for a subsequent valid
qualifying sample plus the existing retry spacing. Every attempted write must
follow successful admission/quiesce, including retries. All preparations share
the existing three-attempt episode budget; no separate unlimited write loop is
introduced. Manual/START one-shots remain separate and are not retried by this
policy. Valid recovery clears only the battery episode and its pending request.

Probe API 2 exposes actual call attempts, failures, latest status and first
failure. The queued policy status is NOT_RUN until the owner returns. HAL_OK
means a returned command result, not measured power removal. The existing
`__fw0_battery_power_prints.gdb` prints both preparation and shipment results;
the manual one-shot helper now requires the matching API 2 build.

Seven focused battery tests pass. Native tests compile the actual policy and
shipment consumer with fake owner results and cover both gates-off/on, transient
write failure, successful retry, failed re-quiesce, permanent failure exhaustion,
invalid readings, wraparound spacing, healthy/boot-charge cancellation, and
manual/START one-shot isolation. Normal Debug and BatteryShutdownTest builds
pass: RAM 553488, ROM 881896, SRAM4 15480 bytes. No device flash or physical
failure injection was performed for this increment.

At that checkpoint, exhaustion remained a reported fault condition, NOT an
energy-safe terminal state. The following increment implements the separately
reviewed fallback; physical qualification remains outstanding.
Persistent PMIC communication loss may prevent software shipment entirely;
hardware battery-protection qualification is required independently. Do not
enable normal automatic gates or claim complete discharge protection on the
strength of these retry tests.

## Exhausted-Shutdown Fault Wait (2026-09-14)

Exhausted preparation or returned shipment failures now latch a dedicated fault
wait in `thPower`. The exhausted episode and first/last shipment failure remain
available. START/manual requests cannot trigger additional shipment writes while
latched. Normal successful shipment behavior and all default-off gates are unchanged.

Each fallback sleep requires admission, terminal owner quiesce and the existing
clock/GPIO/RTC/final-input checks. Pending package work, candidate leases and
unfinished renders also refuse entry. Failed attempts are spaced by the existing
60-second battery retry interval. This does not force unsafe owners into STOP2.
Display-owner quiesce explicitly aborts autonomous animation. Normal UI/input,
runtime service and presentation paths are suppressed while the fault is latched.

Fault wakes restore clocks/timebases but skip normal physical-owner resume and
package time reconciliation. Only the battery deadline selects the RTC wake;
scene timers and interaction deadlines do not fire or advance through the sleep.
A fresh reading is requested after each WFI return, and the check deadline is
capped at the battery retry interval. Valid voltage at/above the restart threshold,
successful base-clock restoration and successful owner resume are required for
recovery. VBUS or buttons alone cannot clear exhaustion. Package execution remains
suspended for its normal recovery action.

The existing `__fw0_battery_power_prints.gdb` now includes the separate API 1
`g_ps_hw6_battery_fault_wait_probe`: active, attempts, WFI returns, latest status,
next attempt tick, forced reading and recovery status. Shipment API stays 2.
These counters are cumulative except active/deadline/read state. WFI returns do
not prove sustained STOP2 or measured low current. Do not use this updated helper
against an older ELF lacking the fault-wait probe.

Eight focused battery tests pass, including native compilation of the actual
policy, retry scheduler, state tables, monitor and shared RTC prepare/finish.
They cover gates off/on, exhausted retry retention, admission/sleep refusal and
spacing, no manual bypass, invalid recovery samples, failed clock/owner recovery,
restart hysteresis, forced post-wake reads, and no scene/interaction timer work
during fault RTC sleep. Hardware sleep is stubbed in the policy tests; structural
checks verify integration with the real STOP2 safety checks and resume exclusion.

Normal Debug and BatteryShutdownTest builds pass: RAM 553520, ROM 883304,
SRAM4 15480 bytes. The two existing unused-function warnings remain. No device
flash, PMIC failure injection or fallback-current measurement was performed.

Next bench checkpoint: controlled fault injection using the isolated battery-input
supply, with cell and device USB disconnected. Verify retained exhaustion, no
new shipment attempts on button wakes, no animation/input dispatch, battery RTC
checks, measured low-current residency when owners can park, and valid-voltage
recovery. Also verify an intentionally refused owner prevents STOP2 and reports
the failure. The following procedure covers synthetic preparation-return failure;
it does not cover a genuinely failed physical owner. Do not disconnect the live
PMIC bus or treat an ordinary successful shipment test as this proof.

### Fault-Wait Bench Capture With Retained RAM

Use the **normal Debug** configuration `HW6 FW0: Debug with ST-LINK`, NOT
`HW6 FW0: BATTERY SHUTDOWN TEST (flash)`. All three shipment gates must be off.
Keep the cell and device USB disconnected and use only the isolated PPK2
battery-input supply. This test does not write shipment mode or deliberately
break a bus. It simulates a returned preparation failure after working owners
have actually acknowledged quiesce. This distinction must accompany results.

1. Flash normal Debug at 3.8 V. While stopped at `main`, set the isolated source
   to 3.4 V and resume. Wait for LOW BATTERY and completed preparation, then halt.
2. Queue the one-shot test:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_fault_test_enable.gdb
```

3. Resume at 3.4 V with all buttons released. The owner rechecks fresh voltage,
   then runs the existing three-attempt budget with synthetic failed returns
   after real successful admission/quiesce. After exhaustion, the checked fault
   fallback should sleep. Observe current for about 25 seconds; this test caps
   the battery RTC interval at 15 seconds. No screen animation is expected.
4. Record current **before** pressing a button. Tap A once. The first classified
   button wake opens a 60-second **running-time** awake inspection window, without
   enabling gameplay or shipment. RTC wakes do not open it. Reconnect using
   `HW6 FW0: Attach with ST-LINK` if needed, without reset/reflash, then halt:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_fault_test_prints.gdb
```

5. Expected before recovery: accepted=1, injections=3, last REAL preparation
   status=0, preparation attempts/exhausted=3/1, injected status=HAL_ERROR,
   fault active=1, successful fault sleep status, positive WFI/RTC-expiry/due-read
   deltas, no actual shipment calls, and measured low current outside inspection.
   A queued request or WFI counter alone is not physical low-current proof.
6. Keep 3.4 V and resume if checking window expiry: it ends after 60 seconds of
   running kernel time from the first button wake. Halting freezes that countdown.
   Later button wakes cannot reopen/extend it. Once capture is complete, raise
   the source to 3.8 V and resume; a valid reading and successful owner recovery
   should clear fault/test active. Injection counts remain until reset.

The helper writes only the request mailbox; firmware checks admission again.
Native tests exercise actual request admission, enabled battery/START gate
refusal, injected retry exhaustion, shipment suppression, RTC-vs-button wake
selection, one-shot inspection, deadline wraparound/expiry and recovery cleanup.
Host GDB executes both helpers against a memory-only fixture to check names,
formatting and refusal paths.

### Fault-Wait Scheduling Failure and Correction (2026-09-15)

The first bench run did NOT enter STOP2: measured current remained about
3.73 mA, falling to 3.12 mA after a minute. Test admission succeeded, all three
synthetic failures followed successful real preparation, and exhaustion latched.
Fault sleep attempts/WFI returns were 1/0; inspection never opened and no
shipment call occurred. This is a failed fallback test, not low-power proof.

The first failed barrier ran from tick 664 to 3664. COMM, SENSOR and STORAGE
commands all queued successfully, but each ACK timed out after 1000 ticks with
status 0x7 and owner action NOT_RUN. AUDIO, INPUT and DISPLAY acknowledged.
Clock grants/releases succeeded. The display deadline was 663, suppression was
zero, its queue wait was zero, and the overdue counter reached 1684893.

Fault mode suppressed the display service without suppressing its deadline in
the owner receive-wait calculation. The expired deadline therefore made the
display thread spin, starving the three lower-priority owners. The correction
ignores display and runtime service deadlines while fault mode disables those
services, using the existing bounded heartbeat queue wait instead. Queued owner
commands still wake the receiver immediately. Button scheduling, audio servicing,
normal scheduling and all physical STOP2 checks remain unchanged; no owner ACK
is fabricated or bypassed.

A native regression compiles the actual receive-wait function and reproduces
deadline 663 at tick 664. It checks bounded fault waits, no overdue-counter churn,
ignored runtime deadlines, preserved button/audio waits and normal scheduling
after fault clears. This proves the wait calculation, not target scheduling or
physical sleep. Repeat the retained-RAM bench procedure above after flashing the
corrected normal Debug build. Require all owner results successful, positive WFI
and RTC deltas, and measured low current before the inspection button wake.

### Fault-Wait Bench Pass and Voltage Recovery (2026-09-15)

The corrected normal Debug build passed the synthetic preparation-failure bench
sequence. The user observed two automatic wakes and STOP2 at the normal sleep
current; no numeric current value was supplied for this run.

- At 3395 mV: accepted/test active=1/1, injections=3, last real preparation
  status=0, preparation attempts/exhausted=3/1. Fault sleep attempts/WFI returns
  were 3/3, battery RTC expiry/due-success deltas were 2/2, and all STOP2
  quiesce/clock prepare/clock restore statuses were zero.
- All six owners acknowledged and completed successfully: required/ACK/success
  masks=0x7e/0x7e/0x7e, failures=0. The latest barrier began and ended at tick 741
  (below the 10 ms tick resolution); no first failed barrier was recorded.
- The button wake opened inspection once: active/count=1/1. Actual shipment
  calls and pending requests remained zero.
- After raising the isolated supply: valid VBAT=3794 mV, recovery status=0,
  fault/test/inspection active=0/0/0. Preparation attempts/exhaustion reset,
  retained injections stayed at 3, and actual shipment calls remained zero.
  Power state returned to ACTIVE_LP (2); runtime lifecycle was 2.

Nine focused battery tests and both Debug/BatteryShutdownTest firmware builds
passed before this hardware run. This qualifies retry exhaustion, checked
fallback sleep, periodic battery reads, inspection and valid-voltage recovery
with working physical owners. It does not qualify a genuinely failed owner,
a failed PMIC shipment write, or permanent bus failure. Inspection-window expiry
without voltage recovery was not separately confirmed in this capture.

### One-Shot Owner Refusal Test (2026-09-15, Functional Pass; Cadence Pending)

Fault-test probe API is now 2; shutdown API remains 2 and fault-wait API remains
1. Existing mode 1 behaviour is unchanged, but its helpers require the matching
new ELF. Mode 2 adds exactly one SENSOR result refusal after preparation
exhaustion. Its actual sensor quiesce still runs; only a successful real result
is replaced with HAL_ERROR before normal result/mask publication and ACK.
A real error is preserved, consumes the injection, and is not counted as a
successful synthetic refusal. No physical fault or shipment is injected.

Use the normal `HW6 FW0: Debug with ST-LINK` build, all shipment gates OFF,
cell/device USB disconnected, isolated PPK2 supply only. Flash at 3.8 V, boot
at 3.4 V, let LOW BATTERY preparation complete, then halt and source:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_fault_owner_reject_enable.gdb
```

Resume and leave controls released at 3.4 V for about 90 seconds. The first
fault sleep attempt MUST be refused. Awake current during the existing
60-second backoff is intentional; battery reads continue. The next barrier
must succeed before sleep resumes. Thereafter the test's 15-second battery
RTC checks apply. Record the current transition and sleep current, then tap A
once for inspection, attach without resetting/reflashing if necessary, halt:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_fault_test_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_battery_quiesce_timing_prints.gdb
```

Require mode=2, owner pending/refusals/REAL status=0/1/0, retry seen=1,
refusal-to-retry >=6000 ticks at 100 Hz, intervening WFI=0 and positive
successful due reads during backoff. FIRST FAILED barrier must show SENSOR
send/ACK=0/0, flags=0x10, action=1; this is not an ACK timeout. The latest
successful barrier must have all six real ACK/action successes before STOP2.
Require measured sleep current, RTC reads, and no actual shipment calls.
Then raise the isolated supply to 3.8 V, resume and confirm valid-voltage
recovery clears fault/test active without erasing the retained refusal.

Native tests compile the actual result injection, aggregate quiesce decision
and fault retry scheduler with fake hardware/queue completions. They verify
no injection before fault mode, no other-owner injection, preservation of real
errors, one-shot consumption, no WFI during backoff, battery reads, fresh
successful retry and recovery. Host GDB executes the new request and print
paths, including old-API, shipment-gate, USB and duplicate-request refusals.
Nine focused tests and both firmware builds pass. This does not qualify a
permanently unresponsive owner or failed physical shipment.

Bench result: the user confirmed current fell to normal STOP2 levels after
about a minute. No numeric current was supplied. The first barrier at tick 664
retained SENSOR send/ACK/action=0/0/1, with all other owner actions successful.
The next attempt was at tick 6668: 6004 ticks (60.04 seconds), zero intervening
WFI returns, and four successful due battery reads. The injected owner refusal
occurred exactly once after real quiesce status=0; the injection was consumed.

The later capture recorded 46 fault sleep attempts, 45 WFI returns and 44 RTC
expiries, with 48 successful due-read deltas. Latest quiesce required/ACK/success
masks were all 0x7e, failure mask=0, and quiesce/clock prepare/clock restore
statuses were all zero. Inspection opened once on the button wake. No actual
shipment calls occurred. The first failed barrier remained available alongside
the latest successful one.

At valid VBAT=3774 mV, recovery status=0, fault/test/inspection active=0/0/0,
and preparation attempts/exhaustion reset. Power returned to ACTIVE_LP (2).
The one-shot refusal and retry evidence remained intact, due-read delta reached
49, and shipment calls stayed zero. This passes synthetic transient-owner
refusal, checked retry, battery monitoring, observed low-current sleep and
valid-voltage recovery. It does not establish behaviour with physically broken
hardware or independently qualify inspection-window expiry.

Follow-up: the user reported regular wake-current peaks approximately every
10 seconds in the PPK log. The armed test configures a 15000 ms battery wake
interval, so physical cadence is not yet qualified. RTC expiry/read counters
establish that battery work occurred but do not establish the wall-clock
interval or identify every current peak. Obtain cursor measurements of
consecutive peaks before attributing this to RTC timing or another wake source.
