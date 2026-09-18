# Debug and Observability Contract

This document defines what must be observable in bring-up and runtime without destabilizing low-power behavior.

Package-facing diagnostics are defined in [[Diagnostics_API_Contract]]. Packages do not own debug transports or persistent fault storage.

Tracealyzer snapshot evidence policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].

Dashboard-facing telemetry is defined in [[Telemetry_And_Debug_Dashboard_Contract]].

---

## Performance Review Policy

Optimisation is periodic and evidence-led, not an open-ended prerequisite for
feature delivery. Revisit performance after meaningful integration milestones,
when users report noticeable lag, or when traces/current measurements indicate
a timing, energy, throughput or resource-budget regression. Safety and correctness
failures remain immediate work regardless of the likely performance saving.

Before each optimisation, state the observed end-to-end cost, the measured hot
path, the expected saving (or explicitly unknown), and the implementation/test
cost. Separate elapsed time from CPU time, energy and physical input latency.
Record firmware/artifact identity, clocks, completed work and observer limitations.
Do not add overlapping trace intervals or treat scheduling counters as completed work.

After measuring a change, report absolute end-to-end milliseconds and percentage,
not only the percentage improvement within a small substage. A few milliseconds
saved from a tens-of-milliseconds stage should trigger a value reassessment,
not automatically another micro-optimisation. Small gains can still matter in a
frequent hot path or against a hard deadline, but that justification must be explicit.

If gains are marginal, below measurement confidence, or not worth another
instrument/build/flash cycle, present alternatives: leave the path unchanged;
reduce duplicated work or change data representation; consider bounded caching
with its RAM/lifetime costs; or separately evaluate clock policy and its measured
energy tradeoff. No alternative is pre-approved by listing it. Do not remove
validation/ownership protection or change hardware policy merely to improve a trace.

Pause when no correctness/safety issue or agreed performance target justifies
further work. Retain the baseline, remaining hypotheses, potential upper bounds,
tradeoffs and the next discriminating measurement; resume feature rollout.
The paused V2 transition investigation is recorded in [[V2_Installed_SFX_Test]].

## Allowed Debug Channels

- SWD (required)
- SWO structured events (preferred)
- TraceX/Tracealyzer snapshot/ring-buffer traces for RTOS and owner-thread bring-up evidence
- HW6 `PH1` / `PWR_DBG` physical timing marker for PPK2 logic correlation in development evidence builds
- USB CDC developer personality optional, rate-limited, and mutually exclusive with MSC in v1

No UART printf console policy by default.

USB CDC developer mode is defined in [[USB_Development_Mode_Contract]]. Live tuning behavior is defined in [[Live_Tuning_And_Knobs_Contract]].

Rules:

- SWO/SWV is primarily observation from device to host.
- CDC developer mode is the structured control path for live-safe Platform tuning, telemetry queries, captures, and package upload.
- CDC commands must route through owner-thread or Engine service requests.
- CDC must not expose raw memory, protected storage, HAL handles, RTOS objects, or arbitrary filesystem paths.
- release/shipping builds must disable CDC developer control unless a future policy explicitly defines a limited diagnostic subset.

## HW6 Physical Marker

`PH1` / `PWR_DBG` is a spare output routed through the HW6 battery connector
to a PPK2 logic input. Platform diagnostics owns it.

Rules:

- default and idle level is low
- marker use is development-only and enabled by an explicit evidence build or bounded diagnostic scenario
- each scenario defines whether high means a pulse, interval, state, or edge; there is no global active polarity
- the evidence manifest records pin, voltage domain, marker semantics, pulse width or interval rule, firmware commit, build profile, and instrumentation connection
- the marker returns low when the scenario completes or faults
- Engine, packages, and authoring tools cannot access or configure the pin
- normal shipping builds keep it low and compile out marker-driving paths

See [[HW6_Pin_Ownership_Matrix]] and
[[Power_Measurement_and_Trace_Correlation_Runbook]].

---

## Required Event Visibility

Must be observable with low overhead:
- mode transitions
- subsystem state transitions
- runtime lifecycle transitions
- storage ownership transitions
- fault and recovery transitions
- owner-thread scheduling and queue/event wake paths during trace-enabled bring-up builds
- dashboard-facing state vector and bounded telemetry events where a dev/debug profile enables them

---

## HW6 FW0 SWO Lifecycle Tokens

HW6 FW0 may emit sparse three-letter lifecycle tokens over SWO/ITM stimulus port 0 when `KNOB_DEBUG_SWO_LIFECYCLE_ENABLE=1` and the debugger has SWO enabled.

Rules:

- tokens are observation-only and must not change firmware control flow
- token writes must be non-blocking; if SWO is not ready, the event is dropped and counted in `g_ps_hw6_trace_probe`
- tokens mark lifecycle boundaries only, not loop progress or high-rate data
- SWO tokens complement TraceX snapshots and GDB probes; they do not replace owner-thread probes or fault records

Current HW6 FW0 token meanings:

| Token | Meaning |
| --- | --- |
| `BTD` | boot/runtime owner self-tests completed |
| `RDY` | storage entered `STORAGE_FLASH_READY` |
| `REQ` | explicit storage flash initialization requested |
| `WAK` | flash wake succeeded during explicit flash init |
| `LAY` | storage layout validation succeeded during explicit flash init |
| `ERS` | USB staging erase started |
| `FMT` | FAT/FileX format started |
| `DON` | explicit storage flash initialization completed |
| `ERR` | current lifecycle command failed before completion |
| `EXP` | USB MSC export requested |
| `MOK` | MSC media open succeeded |
| `REC` | MSC media open found invalid media and requires explicit flash init |
| `REL` | USB MSC reclaim requested |
| `RDN` | USB MSC reclaim completed |

Target GDB status helper: `firmware/peepshow_hw6_fw0/__fw0_swo_lifecycle_prints.gdb`.

### HW6 FW0 Temporary Display Lifecycle Cues

Until live SWO capture or CDC developer status is validated, HW6 FW0 bring-up may use static on-screen cues for explicit flash provisioning and USB MSC export/reclaim. These cues are observation-only: they must be sent through `thDisplay`, must not access storage or USB directly, and must not decide whether MSC export, erase, format, or reclaim happens.

Current temporary cue meanings:

| Display text | Meaning |
| --- | --- |
| `FLASH INIT / USB STAGING / WAIT` | explicit USB staging provisioning is running |
| `FLASH INIT / USB STAGING / DONE` | explicit USB staging provisioning finished successfully |
| `FLASH INIT / USB STAGING / ERROR` | explicit USB staging provisioning failed; inspect GDB probes |
| `USB MSC / EXPORT / WAIT` | MSC export command has started |
| `USB MSC / ACTIVE / EJECT FIRST` | MSC export started and the host may mount the staging volume |
| `USB MSC / RECLAIM / WAIT` | firmware is reclaiming the exported staging volume |
| `USB MSC / RECLAIM / DONE` | reclaim completed and USB hardware/clock policy returned to firmware control |
| `USB MSC / ERROR / SEE GDB` | MSC export or reclaim failed; inspect GDB probes |
| `USB MSC / MSC NEEDS / FLASH INIT` | MSC export found invalid staging media; run the explicit flash init command before retrying export |

These cues are expected to be removed or replaced once the shell/installer UI and live debug transport are mature.

### HW6 FW0 Live SWO Tooling Block

Status as of 2026-08-12: firmware-side SWO lifecycle markers exist, but live SWO capture is blocked by the current VS Code Cortex-Debug + ST-LINK GDB server backend.

Observed debugger behavior:

- Adding a Cortex-Debug `swoConfig` block to `firmware/peepshow_hw6_fw0/.vscode/launch.json` produced the warning: `SWO support is not available from the probe when using the ST-Link GDB server. Disabling SWO.`
- After boot, `firmware/peepshow_hw6_fw0/__fw0_swo_lifecycle_prints.gdb` reported `swo emit/drop/disabled = 0 / 1 / 0`, `swo last token/status = 0x445442 / 0xfffffffd`, and `swo last text = BTD`.
- That proves the firmware called the SWO marker path for `BTD`, but the debugger had not enabled ITM/SWO, so the non-blocking writer correctly dropped the token as `PS_HW6_TRACE_STATUS_NOT_READY`.

Current rule for HW6 FW0 bring-up:

- Do not keep `swoConfig` enabled in the ST-LINK GDB server launch profile; Cortex-Debug disables it anyway.
- Keep the firmware SWO scaffold because it is harmless when SWO is not ready and will work with a backend that enables ITM stimulus port 0.
- Until live SWO is validated, use GDB status helpers, TraceX snapshots, display text, and `PH1` / `PWR_DBG` for operator feedback.

Candidate solution path for a separate debug-tooling task:

1. Validate a Cortex-Debug backend that supports live SWO with ST-LINK hardware, likely OpenOCD/ST-Link rather than the ST-LINK GDB server.
2. Confirm GDB script compatibility remains intact by sourcing the existing FW0 helpers after launch.
3. Confirm reset, flash load, halt, interrupt, low-power debug, and reconnect behavior are not worse than the current ST-LINK GDB server flow.
4. Confirm live text tokens `BTD` and `RDY` appear without pausing the target.
5. Confirm a destructive flash provisioning run shows the expected token sequence ending in `DON` or `ERR` without requiring a mid-operation GDB interrupt.
6. Only after that validation should the project launch profile grow a live-SWO variant.

This is a tooling limitation, not evidence of a firmware lifecycle failure.

---

## TraceX Runtime Scaffold

### One-Press V2 Raster Capture

Installed V2 object scenes and the HOME/AWAY development fixture support an
opt-in one-press TraceX capture through `__fw0_object_trace_enable.gdb` (object
capture API 2). The helper only writes a request;
thRuntime restarts the existing static trace ring through ThreadX APIs, verifies
that DWT CYCCNT advances and then arms the existing latency probe. No inferior
function calls, new trace allocation, clock change or STOP2 override is used.
Existing Platform trace/user-event knobs must be enabled. This diagnostic admits
a running V2 object scene with no outstanding candidate/display lease or lease
fault. It does not require the awake-only fixture: installed HELD/LPBAM sleep
selection and normal clock policy remain unchanged. Shell-suspended scenes and
overlapping trace/latency captures cannot arm.

Resume for one second before pressing the chosen scene-change or selection
button. For installed Lobby/Garden, A in quiet Lobby captures Garden entry;
B in quiet Garden captures Lobby return. A pending request is serviced before
the runtime records the next press even if the request waited through STOP2.
In that case there is no pre-receipt input history in the restarted ring. When
armed earlier, the ring may retain recent history. The trace freezes after the matching runtime
transaction completes, whether successful or rejected. Re-arm explicitly for
another capture; a reset cancels an armed capture. Do not halt during the press.
Let any audio finish before halting; the frozen transaction is not overwritten
by a later timer or sound. Do not launch a development fixture to measure an
installed package.
`__fw0_object_trace_prints.gdb` prints the capture state and latency summary;
`__fw0_tracex_dump.gdb` accepts both running and successfully frozen trace buffers.
The existing latest/timestamped `.trx` paths remain unchanged.

Only matching RECEIVE/DONE markers bound the measured transaction. DWT does not
measure STOP2 wall time, and a clock change before RECEIVE cannot be treated as
one continuous fixed-rate history. `hclk_start` is the ARM sample, not necessarily
the clock at runtime receipt. Inspect RECEIVE/DONE and intervening clock events;
use 24000 cycles/ms only for an unchanged 24 MHz transaction. Neither these
markers nor the cumulative WFI baseline establish physical-button-to-panel time
or low-current residency. Tracing adds overhead; compare with untraced captures.

The installed-capture extension passes 11 focused native tests for arm rejection,
awake/autonomous eligibility, pending-request protection, marker lifecycle and
freeze/rearm, plus the Debug firmware build. RAM/SRAM4 remain 553632/15480 bytes;
the existing 32768-byte trace buffer is reused. Retention of the complete installed
scene-change window still requires inspection of a hardware dump, not just a
successful freeze status.

Application markers use these ThreadX user-event IDs:

| ID | Information fields 1 / 2 / 3 / 4 |
| --- | --- |
| `0x5170` | latency stage / capture sequence / candidate token / HCLK Hz |
| `0x5171` | raster phase / begin=0,end=1 / capture sequence / reserved |
| `0x5172` | ARM=1,RECEIVE=2,DONE=3 / sequence / HCLK Hz / tick at ARM/RECEIVE, result at DONE |
| `0x5173` | owner operation / begin=0,end=1 / capture sequence / driver ps_status_t (begin=NOT_RUN) |
| `0x5174` | capture sequence / SysTick LOAD before / VAL before / target LOAD |
| `0x5175` | capture sequence / sampled DWT cycles before retune / ThreadX tick before / ICSR before |
| `0x5176` | capture sequence / sampled DWT cycles after retune / ThreadX tick after / ICSR after |

Latency stages match `ps_hw6_object_latency.h`. Raster phases are validation=1,
overlap closure=2, rectangle clearing=3, drawing=4, framebuffer copies=5 and cold
full-render fallback=6. DRAW includes a nested VALIDATE; do not sum overlapping
intervals. No per-pixel or continuous application marker loop is introduced.

Owner operations are PMIC snapshot=1, joystick wake=2, sample read=3 and suspend=4.
The PMIC marker encloses the driver call in `PS_HW6_PowerOwner_RunSnapshot`;
joystick markers enclose actual driver calls in the cardinal sampling path.
An already-active sensor does not emit a wake marker. They report driver success
or failure (zero is PS_STATUS_OK), not merely owner scheduling. Pair markers by
ThreadX context, operation and capture sequence; elapsed time includes driver
settling sleeps, mutex waits and preemption, not just bus or CPU time. These
markers do not report individual retry counts or prove why a snapshot was requested.

Begin returns the active capture sequence to the caller. End is emitted only
for that same still-active capture; work begun before capture or completed after
freeze/re-arm cannot produce a misleading end in another capture. An unfinished
pair at freeze is truncated work, not proof of failure. Markers are inactive
outside the existing one-press window and do not change polling, retries, driver
configuration, priorities or PMIC safety policy.

The current Cortex-M33 port timestamps from the 32-bit DWT cycle counter, not the
100 Hz kernel tick. The retune diagnostic uses the existing one-press window;
all three records are emitted after the original LOAD/VAL writes. Use the
explicit sampled DWT fields for before/after timing, not these records' insertion
timestamps. Pair ordered triples by thread and capture sequence and reject
incomplete triples or captures with marker errors. Reads are not atomic: pending
SysTick (ICSR bit 26), active SysTick (VECTACTIVE=15), a crossing tick or preemption
can make partial-tick accounting ambiguous. CTRL is not read, because that would
clear COUNTFLAG. At a stable HCLK and unchanged reload, LOAD minus VAL is an
estimate of the partial countdown discarded by the original VAL reset, not an
independent wall-time measurement. Tracing itself does not change interrupt masks;
its added observation cost must still be considered. With same-rate tick
preservation, matching LOAD requests leave SysTick untouched and emit no retune
triple. Actual reload changes retain the existing reset and diagnostic records.
A stable 24 MHz capture should therefore contain no retune triples even when
clock-policy requests occur. Compare the complete cycle/tick interval as well;
absence of markers alone does not prove accurate timekeeping.

The current Cortex-M33 port timestamps from the 32-bit DWT cycle counter, not the
100 Hz kernel tick. At unchanged 24 MHz, 24000 counts equal 1 ms. Check capture
start/end HCLK and existing clock-policy markers; do not apply one conversion
across clock changes. Use unsigned wrap arithmetic. Long halted intervals and
STOP2 are outside this capture's timing contract.

Require complete=1, arm/freeze status=0, marker_errors=0 and retained matching
RECEIVE/DONE events before interpreting the interval. A ring wrap alone does not
prove data loss, but overwritten/unpaired markers invalidate the affected phase.
ThreadX scheduling/queue events distinguish running, runnable and blocked work;
they do not prove pixels were drawn. Application phase markers and the existing
panel result provide that additional evidence. Interrupts without explicit trace
entry/exit hooks remain included in the apparent thread interval: this is not
a complete ISR or isolated CPU profile. Tracing perturbs timings; compare later
with an untraced run and debugger-detached PPK2 measurements.

TraceX is allowed for HW6 FW0 bring-up as a bounded, static RAM trace buffer. It is an observation tool for RTOS scheduling, object creation, event flags, queue activity, and owner-thread lifecycle behavior. It is not a package-facing diagnostic API and it must not become a hidden control path.

Rules:

- TraceX compile/runtime enable is development-only and must be controlled by Platform debug knobs.
- The trace buffer is statically allocated; no heap allocation is allowed.
- The runtime enable call must happen after ThreadX internal trace initialization and before Platform-owned ThreadX objects are created.
- The trace buffer size and object registry count are compile-time knobs, because they change RAM budget and capture depth.
- TraceX capture must not replace explicit owner-state probes, PMIC snapshots, fault records, or USB/MSC evidence.
- STOP2 evidence must state whether TraceX/SWD/debug-in-low-power changed the behavior being measured.

HW6 FW0 validated baseline:

- CubeMX generated Trace Async/SWO and `TX_ENABLE_EVENT_TRACE` support.
- FW0 calls `tx_trace_enable()` from the `tx_application_define()` user block in `AZURE_RTOS/App/app_azure_rtos.c`.
- Current knobs: `KNOB_DEBUG_TRACEX_ENABLE=1`, `KNOB_DEBUG_TRACEX_BUFFER_BYTES=32768`, `KNOB_DEBUG_TRACEX_REGISTRY_ENTRIES=64`, `KNOB_DEBUG_TRACEX_USER_EVENTS_ENABLE=1`, `KNOB_DEBUG_SWO_LIFECYCLE_ENABLE=1`.
- Target GDB status helper: `firmware/peepshow_hw6_fw0/__fw0_tracex_prints.gdb`.
- Target GDB dump helper: `firmware/peepshow_hw6_fw0/__fw0_tracex_dump.gdb`; while halted, it writes the live TraceX buffer to latest snapshot `firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx` and asks the host shell to copy that dump to a timestamped sibling named `__fw0_tracex_snapshot_YYYYMMDD_HHMMSS.trx`.
- First target status evidence returned `enable status/runtime = 0x0 / 1`, buffer `0x2000a7a0 / 32768`, trace start/end `0x2000b3d0 / 0x20012790`, and registry total/available `64 / 29`.
- First target dump evidence wrote `firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx` at `32768` bytes. TraceX viewer import was validated by user-provided screenshot showing Azure RTOS TraceX 6.4.0 opening the snapshot with named ThreadX/owner threads and scheduler activity.
- PeepShow app marker validation returned `trace api/count ok/skip/err = 1 / 35 / 35 / 0 / 0`, with last event `0x5110` and last status `0x0`, proving the FW0 user-event wrapper inserted owner/UI/input/power markers successfully during a menu interaction trace.

---

## PMIC Interrupt Observability

PMIC interrupt evidence must show both the MCU edge and the power-owner work it
caused. An ISR counter by itself only proves the MCU saw an edge; it does not
prove PMIC policy ran. Required PMIC_INT evidence includes:

- EXTI edge count and last edge timestamp
- pending/consumed count showing the event reached `thPower`
- normal ADP5360 snapshot status from `thPower` context
- interrupt enable registers and read statuses
- interrupt flag pre-clear values and write-one-clear statuses
- confirmation that VBUS/charger events did not trigger USB MSC export or
  storage ownership by themselves

On HW6 FW0, `EXTI15` is deliberately disarmed during early startup and armed
only after RTOS owner services exist. This prevents early PMIC_INT edges from
running owner-dependent work before ThreadX initialization is complete.

---

## HardFault Record Contract

On HardFault capture:
- PC, LR
- stacked registers
- CFSR, HFSR
- MMFAR, BFAR when valid
- current mode/runtime state identifiers

HardFault data must be captured before reset.

---

## Breakpoint Policy

- avoid high-frequency ISRs and DMA callbacks
- cap active breakpoints during runtime debugging
- prefer event markers over stop-heavy breakpoint sessions for STOP validation

---

## STOP2 Debug Policy

- enable debug-in-low-power when investigating STOP behavior, but do not treat that as physical sleep/wake proof
- classify every wake source with evidence
- avoid instrumentation that materially changes sleep behavior
- STOP2 evidence must record `DBGMCU->CR` before/after entry; on STM32U575 the debug MCU configuration register is at `0xE0044004`, not `0xE0044000`
- controlled physical START-wake proof on HW6 FW0 used `firmware/peepshow_hw6_fw0/__fw0_stop2_debug_low_power_off.gdb` to clear `DBG_STOP` and `DBG_STANDBY`, then restored them only when debug visibility was needed again
- current measurements used for power targets must state whether SWD was physically attached; HW6 FW0 STOP2 floor evidence uses debugger-detached PPK2 current as the trusted value, with GDB probes used only after wake/reconnect to inspect retained ledgers

HW6 FW0 evidence `EV-HW6-20260814-P1-STOP2WAKE-056` showed why this distinction matters: with `DBGMCU->CR = 0x6`, the controlled STOP2 helper entered and returned immediately with no EXTI, GPIO, NVIC, PMIC, or PWR wake-source evidence, so the result was classified as `UNKNOWN`. After clearing `DBGMCU->CR` to `0x0`, the same controlled helper entered STOP2 and woke from physical START with source mask `0x1`, primary cause `START`, PA4 IDR changing `0x6055 -> 0x6045`, START button edges `0 -> 1`, `PWR_SR.STOPF` set, and no unknown wake count.
---

## Production Build Guardrails

Release builds must:
- compile out verbose debug paths
- keep structured fault capture
- preserve deterministic timing behavior


