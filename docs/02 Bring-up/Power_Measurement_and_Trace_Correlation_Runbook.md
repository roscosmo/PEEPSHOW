# Power Measurement and Trace Correlation Runbook

This runbook defines how PeepShow uses external current measurement, firmware trace markers, Tracealyzer snapshots, and telemetry to diagnose and optimize PeepOS power behavior on the active hardware target.

This is PeepOS Platform development tooling.

It is not part of the package/game developer API.

Related:

- [[HW6_Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[Pending_Measured_Constants_Register]]
- [[Validation_Plan]]
- [[Debug_Workflows]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Power_and_Sleep_Policy]]
- [[PMIC_and_Power_Contract]]
- [[Target_Profile_Schema_Contract]]

---

## Purpose

PPK2 or equivalent current measurement shows what the hardware consumed.

Tracealyzer, SWO, and telemetry show what PeepOS was doing.

The goal is to correlate those sources so power anomalies can be traced to concrete Platform behavior:

- mode transitions
- owner-thread activity
- display flushes
- sensor reads
- BLE/NINA activity
- audio output
- storage writes
- USB attach/enumeration/MSC
- sleep entry and wake paths

The output of this work feeds:

- measured constants
- Platform power policy
- target-profile limits
- future battery/runtime estimates
- regression evidence

---

## Non-Goals

This runbook does not define:

- game-facing power controls
- package-visible hardware telemetry
- user-facing battery UX details
- PPK2 software operation in detail
- Tracealyzer licensing or streaming setup
- final production test procedure

Package authors should see a power-optimized PeepOS API and target-profile limits, not current probes, GPIO sync pins, PMIC internals, or Tracealyzer artifacts.

---

## Measurement Sources

| Source | Proves | Does Not Prove |
|---|---|---|
| PPK2 current trace | current draw, spikes, plateaus, operation energy cost, average current | software cause by itself |
| Tracealyzer snapshot | ThreadX scheduling, owner ordering, queue/timer behavior | electrical current or STOP current |
| SWO/telemetry | Platform state vector, event timeline, firmware timestamps | electrical current by itself |
| optional sync GPIO | alignment cue between current trace and firmware events | power behavior by itself |

Rules:

- PPK2 or equivalent physical measurement is required for current and energy claims.
- Tracealyzer snapshots explain scheduling and ordering only.
- Telemetry can explain state and event context.
- Final low-power current evidence should be confirmed with trace overhead disabled unless the test explicitly accepts instrumentation overhead.

---

## Sync Strategy

### Preferred: Physical Sync Marker

Use a physical marker pin only when the active hardware contract assigns an electrically safe, accessible development signal for that purpose. Do not guess a pin from package availability or an old hardware revision; record the selected test point and owner in the evidence manifest.

On HW6, the preferred marker is `PH1` / `PWR_DBG`, routed through the battery
connector to a PPK2 logic input. It is idle-low and owned by Platform
diagnostics. A high level has no global product meaning: each bounded evidence
procedure defines whether it represents a pulse, interval, state, or edge and
records that definition in the manifest.

Recommended behavior:

- connect sync pin to a PPK2 digital input or logic capture channel
- emit a short pulse at major Platform markers
- mirror the same marker into Tracealyzer/SWO/telemetry
- document the pin, polarity, pulse width, and firmware build profile in the evidence manifest
- restore `PWR_DBG` low before the diagnostic scenario exits or faults

Example marker flow:

```text
firmware emits POWER_SYNC_STOP_PREP
  -> Tracealyzer custom event
  -> SWO/telemetry event
  -> optional GPIO pulse visible in PPK2 timeline
```

Rules:

- the sync pin is dev-only instrumentation.
- it must not be required by package logic.
- it must not be used for product behavior.
- if the pin is shared with future hardware function, the evidence must record the temporary routing.

### Fallback: Timed/Cue-Based Correlation

If no safe pin is available, use deliberate timing cues:

- run a focused single-action scenario
- record a firmware timestamped marker stream
- include a clear preamble pattern supported by the selected target, such as three display flushes or three short speaker cues
- align PPK2 trace features to matching firmware markers
- record the expected correlation precision in the evidence manifest

Rules:

- fallback correlation is valid for diagnosis and coarse attribution.
- do not claim sub-millisecond marker alignment without a physical sync or equivalent shared timebase.
- unresolved ambiguity should remain in the evidence notes.

---

## Required Marker Classes

Power correlation builds should emit bounded Platform markers for:

| Marker Class | Examples |
|---|---|
| boot | `POWER_SYNC_BOOT`, `BOOT_PHASE_BEGIN`, `BOOT_PHASE_END` |
| mode | `MODE_SHELL`, `MODE_REALTIME`, `MODE_REACTIVE_EVENT`, `MODE_REACTIVE_WAIT`, `MODE_STOP_PREP`, `MODE_STOP_ENTER`, `MODE_WAKE` |
| operating point | transition requested, clocks/voltage valid, transition complete, transition failed; payload uses a bounded internal point ID |
| owner | owner wake, owner idle, owner fault, quiesce begin/ack |
| display | flush requested, full flush begin/end, partial flush begin/end, LPBAM sequence arm/disarm |
| input | input wake, focus delivery, meaningful-activity admission, lock timer reset, lock, consumed Start unlock |
| sensor | IMU sample burst, step session update, and any additional profile-granted sensor sample |
| audio | speaker stream begin/end, amplifier enable/disable, and target-granted procedural-output markers |
| communication | NINA wake/sleep, BLE advertise, connected, message TX/RX |
| storage | flash read/write/erase, save commit, package install step |
| USB | VBUS present, USB activity, enumerated, MSC entry, MSC exit |
| sleep/wake | quiesce begin, all owners acked, sleep attempted, wake reason classified, resume complete |

Rules:

- marker names must be stable once used in evidence.
- marker payloads must be bounded.
- high-rate marker classes must be filterable.
- markers must not expose raw pointers, register dumps, protected storage, private package data, or host filesystem paths.

---

## Capture Scenarios

Start with narrow captures. Each run should focus on one behavior.

Baseline scenarios:

| Scenario | Purpose |
|---|---|
| boot to shell idle | baseline startup cost and idle plateau |
| shell idle | steady interactive baseline |
| reactive hold wait | sleep baseline while an unlocked package waits for input |
| reactive input transaction | input wake, bounded state/render work, and return-to-wait cost |
| reactive operating-point sweep | compare identical event-to-yield transactions across valid candidate points |
| reactive waiting-visual sequence | autonomous waiting motion and fallback cost |
| `REALTIME` active loop | frame cadence and CPU/display cost |
| realtime operating-point sweep | compare frame/audio deadline margin and sustained power across valid candidate points |
| operating-point transition | measure transition latency/charge and establish switching break-even/hysteresis |
| optional input-lock expiry and Start unlock | lock overlay, consumed unlock press, and lifecycle cost |
| STOP entry and wake | physical sleep entry cost, STOP current, wake cost |
| display full flush | worst-case display transfer cost |
| display partial flush | normal changed-region transfer cost |
| LPBAM waiting-visual test | compiled waiting-visual playback cost and cadence against the measured LPBAM profile |
| profile-granted procedural audio | procedural-output current and timing, only on targets that provide that path |
| speaker cue | amplifier and DMA path cost |
| IMU low-power step mode | always-on step counter cost |
| IMU motion stream | high-rate sensor context cost |
| storage save commit | flash write current and duration |
| BLE advertise/connect/message | NINA current behavior and session cost |
| VBUS charger attach | charger/power-bank behavior without MSC prompt |
| USB host enumeration | lightweight data-host detection cost |
| MSC active | heavy USB/storage transport cost |

Run only scenarios granted by the selected target profile. HW6 omits rotary-encoder, ambient-light, and PAM/piezo/BBB scenarios; their old HW5 captures are historical references, not HW6 measurements.

---

## Active Operating-Point Sweep

Use one firmware commit, deterministic input/event trace, asset set, owner configuration, peripheral state, source voltage, and instrumentation profile across every point in a sweep. A candidate point includes SYSCLK/HCLK, voltage scale, flash latency/cache state, and relevant kernel-clock configuration; frequency alone is not a complete test condition.

Reactive sweep workloads must include:

- minimal logic-only event transaction
- representative state change plus partial rendering
- representative or worst admitted presentation update and waiting-visual/display-program preparation
- representative owner work such as bounded audio cue, sensor result, or storage request where the reactive contract permits it

The reactive measurement window begins at the admitted event/wake marker and ends only after the transaction has settled and the selected waiting backend is stable. Record response latency, settled-presentation latency, active duration, charge/energy, return-to-wait latency, and failures. Select the lowest transaction-energy point that satisfies the required response/correctness bounds; do not select from active current or execution time alone.

Realtime sweep workloads must include light, representative, and worst admitted frame work, with required audio and sensor contexts active. Record sustained current, frame-time distribution, worst-case frame time, deadline misses, audio underruns/glitches, owner contention, and headroom. Select the lowest-power point that meets every required deadline with margin.

Transition tests must cover wake-to-reactive point, reactive-to-realtime, realtime-to-reactive, and return-to-wait where those transitions are part of the intended policy. Measure transition duration and charge/energy separately. Dynamic switching is accepted only when a measured break-even interval supports a bounded hysteresis rule.

Debug/trace-assisted runs explain behavior, but final current and energy comparisons require a matching instrumentation-minimized run. Record selections as pending in [[Pending_Measured_Constants_Register]] until promoted through the target-specific tracker, currently [[HW6_Brought_Up_Tracker]].

---

## Interpreting Current Traces

Use operation energy, not just peak current.

For each scenario, record:

- peak current
- steady plateau current
- duration
- average current over the scenario window
- estimated charge cost where practical
- runtime class, execution semantic, and deterministic workload ID
- internal operating-point ID and full clock/voltage configuration
- instrumentation profile

Typical interpretations:

| PPK2 Shape | Likely Cause To Check |
|---|---|
| short sharp spike | display flush, radio packet, wake transition, flash write |
| regular periodic spike | timer wake, sensor poll, BLE interval, RTC wake, display cadence |
| long plateau | peripheral left enabled, owner not idle, NINA awake, audio/display path active |
| current fails to drop after `STOP_PREP` | sleep not entered, pending wake source, unparked peripheral |
| current rises only after input | input path, redraw, audio feedback, sensor burst |
| repeated sawtooth | radio interval, polling loop, charger/fuel gauge behavior |

Every conclusion should identify both:

- the electrical observation from PPK2
- the matching firmware state/event evidence where available

---

## Battery Estimate Pipeline

Game developers should not work with PPK2 traces.

The useful package-facing output is a Platform/Engine estimate derived from measured profiles.

Intended path:

```text
PPK2 current traces
  + Tracealyzer/telemetry correlation
  + measured battery capacity/profile
  + measured runtime-class costs
  -> Platform power model
  -> target-profile budgets and estimates
  -> package validator/runtime compatibility reports
```

Possible future package-facing outputs:

- broad battery state or icon level
- estimated runtime class cost category
- compatibility warning for expensive contexts
- package validation report showing power-relevant declarations

Rules:

- battery estimates are advisory unless a later contract says otherwise.
- estimates must identify the target profile and measured constants used.
- package tools may consume profile limits and estimates.
- package tools must not consume raw PPK2 traces or Platform power internals.

---

## Evidence Requirements

Each power-correlation evidence entry should include:

- evidence ID
- board revision
- firmware commit
- build profile
- target profile if applicable
- Platform knobs hash/version
- active tuning overlay
- instrumentation state
- PPK2 or source-meter model
- PPK2 software version where practical
- sample rate / capture resolution
- source voltage and current limit
- measurement range or calibration notes
- sync strategy: physical pin, logic channel, or timed/cue fallback
- marker list enabled
- Tracealyzer artifact path if used
- telemetry/SWO artifact path if used
- PPK2 raw capture path
- derived plot/export path if used
- conclusion and follow-ups

Use [[Evidence_Artifact_Convention]] for folder and manifest format.

---

## Acceptance Rules

A power measurement is usable bring-up evidence only when:

1. the measurement source and setup are recorded.
2. firmware build/instrumentation profile is recorded.
3. the scenario window is clear.
4. the current trace is preserved or exported.
5. marker/correlation method is described.
6. any promoted constant is updated in [[Pending_Measured_Constants_Register]].
7. the tracker entry distinguishes measured data from interpretation.

Trace-correlated evidence is stronger when:

- the PPK2 trace has a physical sync marker, or
- the firmware event timeline contains a deliberate alignment cue, and
- the trace/telemetry window covers the same scenario.

---

## Rule

Use PPK2 to measure what PeepShow consumed.

Use Tracealyzer and telemetry to explain why.

Promote only measured, reproducible power behavior into Platform policy, target profiles, and package-facing estimates.
