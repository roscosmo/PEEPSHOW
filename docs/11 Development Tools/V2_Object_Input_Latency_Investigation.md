# V2 Object Input Latency Investigation

## Evidence and Scope

The labelled HOME/AWAY hardware fixture behaves correctly, including fresh
input/timer scene replacement and local animation continuity. The user reports
an obvious delay between a button press and marker movement. Successful
candidate/display completion counters do not establish acceptable latency.

`PS_SceneRuntime_HandleStateSceneEventId` requires synchronous exact candidate
admission before a local transaction commits. `CandidateOwnedCheck` copies the
package, `CandidateCheck` decodes/validates it and builds a waiting schedule,
and `CandidateDisplay` rasters/packs the full cycle privately. Only afterward
does `ObjectPresent` build the presentation and publish its waiting schedule.
The awake-only target capture below confirms substantial elapsed time in this
path. It does not separate CPU work from preemption within each stage.

## Measured Baseline

HOME local R selection, LPBAM disabled, scene 1 -> 1, all 21 stages valid,
event/panel status zero. Runtime receipt tick 1071, event commit 1096, panel
completion 1099 at 100 Hz:

| Stage | Elapsed |
| --- | ---: |
| Package decode/validation | 100 ms |
| Graph/schedule | 10 ms |
| Candidate raster/packing | 140 ms |
| Receipt to event commit | 250 ms |
| Panel render/transfer | 30 ms |
| Receipt to panel completion | 280 ms |

HCLK samples were 24 MHz throughout. The user reports other presses felt worse;
this is one successful sample, not a maximum or percentile. Physical-edge,
debounce, input routing and pre-runtime wake latency are additional, unmeasured
costs. STOP2 cannot be the sole cause because this capture was awake-only.

The original helper failed because passing a quoted label through `%s` caused
bare-metal GDB to request inferior `malloc`. Native-host GDB had concealed this
problem. Labels now use literal format strings and the row helper accepts only
numeric indices. No target allocator is added.

## First Optimisation

Runtime retains the last successful private candidate's descriptor/catalog and
profile, borrowing only its existing 64 KiB owned byte buffer. Reuse requires
the same byte count, admission mode, selected scene and an exact `memcmp` against
the incoming bytes before any copy. Pointer equality or a package ID/hash is not
used as proof. Changed bytes, size, mode or scene force full decoding. Borrowed
diagnostics do not populate the cache. Failure, busy rejection or timeout/late
completion discards the reusable metadata; the existing matching-token lease
still prevents overwriting a display-owned candidate.

Each hit still initializes/checks the candidate graph, builds its actual waiting
schedule and performs full private raster/packing before committing the event.
There is no cached capacity verdict, delayed validation or unchecked mutation.
The retained descriptor/catalog reuse existing storage; the key, profile and
hit/miss counters add 44 static bytes on ARM. Loader probes describe the last
actual decode; candidate probes describe the current transaction. Cache counters
printed by the helper are cumulative, not frozen per-press measurements.

Sprite rasterization now computes the rotated destination row/bit once per
logical row, rather than calling two checked pixel functions for every pixel.
Bounds and asset resolution remain checked before drawing. Opaque white,
transparent preservation, clear-bounds and black-pixel count semantics are
unchanged; clear-bounds is folded into the same pass. Full frames and payloads
are still composed, including the wrap frame. Clocks, power policy, ownership,
animation cadence and scene/state lifetime are unchanged.

This is the first measured-path optimisation, not a claim that responsiveness
is solved. The single-entry cache does not accelerate the first decode after a
scene change. Full-cycle packing and pre-runtime latency remain investigation
targets. Compare fresh target captures before choosing the next optimisation.

## First Optimisation Target Results

The user reported noticeably improved responsiveness. These are completed
HOME local-selection captures, all with successful event/panel status and
24 MHz HCLK boundary samples:

| Mode / sequence / button | Decode | Raster/packing | Panel | Receipt to panel |
| --- | ---: | ---: | ---: | ---: |
| Awake / 1 / R | <10 ms | 110 ms | 20 ms | 140 ms |
| Awake / 2 / R | <10 ms | 110 ms | 20 ms | 130 ms |
| Autonomous / 1 / R | <10 ms | 80 ms | 40 ms | 120 ms |
| Autonomous / 2 / L | <10 ms | 90 ms | 30 ms | 120 ms |

The first awake sample additionally spent 10 ms in runtime clock/setup. A
reprint of sequence 1 was not counted as another sample. Autonomous WFI
baselines were 3 and 7; they show intervening sleep returns, not a measured
physical wake-to-input delay for these particular presses. No worst-case,
percentile or power claim follows from four samples. Full-cycle raster/packing
remains the largest measured stage. Fresh cross-scene entry is not covered.

## One-Press Capture

`g_ps_object_latency_probe` API 2 is independent of the existing probe layouts.
It occupies 368 bytes, has no heap or background task, and is dormant unless
explicitly armed. One runtime-delivered press consumes the request. Runtime
owns the capture lifetime; candidate and display stages are matched to their
transaction tokens. Releases, timer work and later animation frames cannot
replace a completed capture. An ignored press still completes, but correctly
lacks admission/panel stages. Change selection to get a useful sample.

Stages timestamp runtime receipt/advance, private copy, runtime clock request,
package validation, graph/schedule preparation, clock release, candidate queue,
display clock request, candidate raster/packing, admission return, event commit,
presentation preparation, panel render/transfer, playback publication and end.
HCLK is read at each recorded boundary without requesting a different clock.
The firmware's 100 Hz ThreadX tick gives 10 ms resolution: zero means below this
resolution, not zero cost. Durations include preemption and bounded waits.
Some totals overlap and must not be summed. Panel success must be checked;
elapsed time on a failing call does not prove a completed drawing.

The clock starts at **runtime receipt**, not the physical edge. Physical wake,
debouncing, input routing and earlier queue delay are excluded. The WFI count
is cumulative and does not associate a particular press with a wake. Comparing
an explicitly awake run against the autonomous run helps separate paths, but
does not itself measure physical-edge-to-pixel latency. Do not halt during a
captured press or infer normal STOP2 timing from debugger interference.

## Candidate Work Breakdown

API 2 adds five non-overlapping timing groups inside the matching candidate's
private display check. The existing helpers retain the one-shot arming workflow;
reflash and load the matching ELF before using their API-2 versions.

| Group | Included work |
| --- | --- |
| Frame projection | Waiting-step snapshot to render model |
| Frame raster | Model/asset validation, all drawing, framebuffer save/restore |
| Dirty-band comparison | Full-frame band comparisons and dirty-band marking |
| Payload | Wire construction, deduplication and private payload storage |
| Packing copies | Workspace reset and previous/target frame copies |

These are elapsed ThreadX ticks, including preemption and observer overhead,
not isolated CPU time. Raster is not a sprite-only measurement. Per-group
accumulation over several short intervals has tick quantization error; zero
does not mean no work. The groups fit inside the existing candidate total;
do not add them to that total. Small bookkeeping gaps are not attributed.

A successful four-step cycle has calls 5/5/4/4/5 (including the wrap frame);
a rejected job may have partial counts. Calls show the code ran; only successful
result/panel status and observation establish successful output. The optional
observer is passed synchronously through the private checker, with no retained
caller pointer, heap or peripheral access. It makes 46 clock reads for four
steps, at most 126 for twelve, never per pixel or row. The clock is `tx_time_get`;
no clock policy or scheduling change is requested. A NULL observer/clock makes
no timing reads. Ordinary playback/public packing uses the untimed wrapper.

Runtime resets the breakdown on a new capture/candidate token. Display copies
totals into the capture only while its token still matches an active capture;
late completions cannot overwrite a finished capture. The display scratch
observer occupies 44 static bytes on ARM, in addition to the probe's 48-byte
increase. This changes neither private workspace limits nor SRAM4 allocation.

## Band Comparison Optimisation

The first API-2 awake HOME/R target capture completed at 130 ms, with 110 ms
inside admission. Substage totals were 80 ms raster across five calls and
30 ms row comparison across four calls. Projection, payload construction and
packing copies recorded zero ticks. Calls were 5/5/4/4/5, status zero, with
24 MHz HCLK samples. These elapsed totals include preemption and quantization;
they do not prove 30 ms of CPU execution inside `memcmp`.

The private full-scene checker only emits whole 28-row bands, but previously
called `RowIsDirty` separately on all 168 rows per transition. It now compares
each complete band once: six 504-byte spans rather than 168 separate 18-byte
spans. Four steps require 24 comparisons rather than 672. The predicate is
identical: a band is dirty if any of its bytes differs. No row or pixel is
excluded, and unchanged transitions still refresh band zero. Full-update mode
still marks every band. Wrap composition, chunk/payload limits, deduplication,
clock claims and token ownership are unchanged. No extra RAM is allocated.

The existing row-based production packer is deliberately unchanged and serves
as an independent test oracle. Native tests change every one of the 3024 frame
bytes, covering every row and byte position, and check both forward and wrap
transitions. Additional unchanged, adjacent-band and all-band cases compare
admission results and private payload bytes against that packer. Existing
overflow/rejection tests remain in force.

Rendering inspection confirmed that `CopySceneModelFrame` validates a model,
then `DrawSceneModel` validates it again. Raster timing also includes layer
ordering, static labels/outlines, sprites, and framebuffer preservation. These
costs are not isolated yet. Drawing is unchanged in the band-comparison build
so its target timing comparison is not confounded by another rendering change.
The existing API-2 helper now labels the group "Dirty-band comparison"; the
call count remains one comparison phase per transition, not the number of bands.
Reflash before comparing against the 30 ms row-comparison result.

### Band Comparison Target Result

The subsequent awake capture reported 140 ms runtime receipt to panel completion:
110 ms candidate work (90 ms raster, 10 ms comparison, 10 ms packing copies),
then 30 ms panel render/transfer. Status and panel status were zero at 24 MHz.
This is not an overall speedup over the earlier 130-140 ms samples. The 10 ms
clock resolution and preemption prevent treating small differences as isolated
CPU savings. Full-frame candidate rasterization remained the main measured cost.

## Incremental Candidate Raster

The next increment retains one private framebuffer and its render model in the
display-owned admission workspace. It reuses pixels across the candidate cycle,
and across successful same-scene candidate transactions only after the existing
exact-byte metadata cache establishes immutable package identity. Runtime passes
that decision under the existing lease; it does not modify the display cache.

Changed old/new element rectangles seed a bounded overlap closure (at most 12
old plus 12 new elements). The renderer clears those rectangles and redraws all
affected visible objects in the original layer/z/index order, restoring both
opaque white pixels and masked content. Disconnected unchanged content is left
intact. Overlap with a large background can conservatively expand to a full
redraw. Text uses a full render because legacy text is not clipped to its declared
bounds; legacy focus remains rejected. Model and sprite validation still run on
every frame, even on a cache hit and for hidden objects.

Cold package/scene/mode changes invalidate reuse. Failed raster/packing invalidates
the private frame, and timeout/late completion still invalidates metadata before
the next transaction. Borrowed diagnostic buffers never establish reuse across
transactions. No cached admission verdict is accepted in place of real work.

All frames, including wrap, still undergo full band comparison, wire construction,
deduplication and exact capacity checks. Packed payload reuse is a later increment,
not part of this change. Live DMA payloads, actual panel presentation, animation
phase, timer semantics, clock policy and STOP2 ownership are unchanged. The cache
adds 3560 bytes of ordinary static RAM, with no new SRAM4 allocation or heap.

The latency probe is now API 3 and captures full/reused frame counts and elements
drawn for the matching candidate. A warm four-step local transaction should show
0 full / 5 reused frames; a cold one normally shows 1 full / 4 reused. Reused
frames may still redraw overlapping content: the count does not mean zero work.
Use the raster elapsed time and element count together. Hardware latency improvement
and visual continuity remain to be measured; do not infer a speedup from cache hits.

## Target Sequence

Flash the new firmware once and use its matching ELF. Installed package bytes
remain unchanged. Boot, HOLD START for shell MENU, then halt and launch the
existing autonomous fixture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_exits_enable.gdb
continue
```

Let HOME finish its two-second reveal. Wake/halt as needed, then arm while
playing with no outstanding candidate/display transaction:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_latency_enable.gdb
continue
```

Let controls settle, then press the opposite L/R selection once. After the
visible move, halt and print. A further wake press will not overwrite the
completed capture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_latency_prints.gdb
```

For the comparison, reset deliberately, boot, open shell MENU and halt. Launch
the identical fixture with automatic STOP2 blocked:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_exits_awake_enable.gdb
continue
```

Repeat the same capture after HOME's reveal. Do not force STOP2 in this mode.
Keep the tested scene/button consistent; initial launch, reveal work, local
selection and scene replacement are different operations. Re-arm separately
for additional samples or an A/B scene exit. Do not clear leases to force a
capture. Capture multiple local changes in each mode, including the first after
scene entry, and A/B cross-scene changes separately. Report the range rather
than selecting only the fastest result.

## Trace and Power Follow-Up

The first incremental-raster target capture completed successfully but took
160 ms: 140 ms inside raster work and 20 ms panel render/transfer at 24 MHz.
It recorded 0 full / 5 reused frames and only 8 elements drawn. Reuse is proven;
latency improvement is not. The grouped 10 ms timestamps cannot attribute that
raster interval to clearing, validation, drawing, copying or preemption.

The next build adds the bounded TraceX capture documented in
`docs/01 Platform/Debug/Debug_and_Observability.md`. From the awake HOME/AWAY
fixture, after HOME's reveal, halt and use the new trace helper instead of the
ordinary latency enable helper:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_trace_enable.gdb
continue
```

Wait one second, press the opposite L/R selection once, then halt after the move:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_trace_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_tracex_dump.gdb
```

Open the resulting `.trx` in TraceX and retain it with the printed probe. Inspect
the matching capture sequence from RECEIVE to DONE, the display thread's
preemptions/waits and raster begin/end markers. Missing markers or changed clocks
must be resolved before attributing elapsed time. This first capture is awake
only; it is not evidence of STOP2 timing or low-current residency.

Trace capture verification: **354 authoring/native tests pass**. The Debug build
uses RAM 555376 bytes, ROM 875792 bytes and unchanged SRAM4 15480 bytes. Capture
tests cover counter-not-running, refused/failed arm, marker errors, freeze,
inactive no-ops and re-arm. The existing runtime stack check passes, but treats
the trace wrapper as an external leaf under its reserve; it is not a measured
trace-path or display-stack bound. Actual target trace and observer overhead
remain pending.

Use bounded TraceX captures to separate execution from waits/preemption during
boot, steady playback, local state changes, scene replacement, timer expiry,
audio and installation. Correlate owner work and clocks with PPK2 captures;
record trace/debug configuration and compare an untraced, debugger-detached
power baseline. Record latency distributions, stack high-water evidence and
awake residency rather than inferring headroom from one saved stack pointer or
successful queue counters. The one-shot probe remains useful for repeatable
before/after comparisons but does not replace this broader analysis.

## Trace Result and Byte-Level Clearing

The awake one-press capture
`__fw0_tracex_snapshot_20260913_095702.trx` retained the complete matching
RECEIVE-to-DONE transaction: no object marker errors, no ring wrap during the
transaction, and reported HCLK remained 24 MHz. The coarse latency probe reported
160 ms receipt-to-panel and 170 ms transaction total. Cycle-derived timings do
not exactly match kernel ticks; they are not independent wall-clock evidence.

Raster markers measured approximately 66 ms clearing, 21 ms framebuffer copies,
14 ms drawing and 2.4 ms validation. These include preemption, and validation
inside drawing overlaps that total. Observed thInput intervals occupied about
36 ms across the transaction, including repeated 5-6 ms I2C3-related intervals.
Subtracting observed other-thread intervals leaves approximately 48 ms in clearing,
still including unmarked interrupts. This establishes expensive clearing, not
isolated CPU time or a justification to change input scheduling.

The next implementation replaces cached-candidate rectangle clearing through
per-pixel helpers with masked byte operations. Logical coordinates are clipped
once and mapped to reversed panel rows; interior bytes become white with memset,
while edge masks preserve neighbouring pixels. Dirty-object selection, overlap
closure, layer order, validation, framebuffer copies, payload packing and active
DMA buffers are unchanged. No clock, input, sleep-policy or allocation changes.

Native comparisons cover every panel-column interval, single-byte and multi-byte
masks, rotation, clipping, empty/outside rectangles and buffer guards against the
per-pixel oracle. Existing full-render and full-payload equivalence tests remain
required. All **355 authoring/native tests pass**; target-profile and whitespace
checks pass. The runtime stack check remains 2432 bytes including reserve, not
a measured display-stack high-water mark. Debug build uses RAM 555376 bytes,
ROM 876072 bytes and SRAM4 15480 bytes.
Target capture `__fw0_tracex_snapshot_20260913_102755.trx` subsequently measured
clearing at 1.625 ms versus 66.320 ms, with framebuffer-copy sections still at
19.893 ms versus 20.918 ms. Kernel-tick receipt-to-panel improved from 160 to
100 ms. These are separate time domains; both traces retained the complete
transaction with no marker errors and unchanged reported 24 MHz clocks. The new
capture drew seven elements versus eight and experienced different preemption,
so this is not an isolated CPU-cost comparison or a latency distribution.

The user confirmed repeated awake L/R changes preserved the digits and static
content. The subsequent autonomous HOME/AWAY regression also passed visually:
one marker move, continuous local animation, fixed outlines and automatic HOME
return six seconds after AWAY entry. Probe evidence: replacement attempts/failures
2/0, timers due/applied/errors 3/3/0, WFI returns/measured/reconciled 7/7/7,
wake statuses zero and schedule four steps at 400 ms. The final render was halted
in flight (request/complete 15/14); that snapshot is not a completed render
failure. No new low-current or energy measurement was supplied.

## Direct Private Raster Destination

The next increment removes the dependency on the normal software framebuffer
as candidate scratch. Drawing selects a private destination only within bounded,
synchronous thDisplay composition. Shape pixels and package sprites use that
destination; rotation and the normal destination are restored before returning.
Nested private composition is rejected. Focus remains excluded from private
composition because it changes cursor bookkeeping.

Warm cached composition clears/draws in place and retains one final frame copy
to the packer instead of five copies per changed frame. Full composition draws
directly into its supplied destination too, removing its static saved-frame
buffer. There is no new framebuffer, allocation or linker reservation. Packer
previous/target copies, admission checks, active DMA storage, clocks and input
polling are unchanged. COPY markers now occur once per composed cached frame,
including the model update and final output copy, rather than three groups.

Native tests check live-frame preservation at raster trace markers, destination
and rotation restoration, normal rendering after private composition, all shape
types and opaque/masked sprites, missing assets, focus rejection, invalid buffers
and nested-scope refusal. Full pixel and packed-payload comparisons remain in
place. Target timing and visual regression for this increment are pending;
reflash and repeat the same awake one-press capture before testing autonomous
playback. Do not infer the latency gain from the number of copies removed.

Verification: **355 authoring/native tests pass**, as do the Debug build and
target-profile check. Build totals: ordinary RAM 552360 bytes (3016 bytes less),
ROM 875984 bytes, SRAM4 unchanged at 15480 bytes. The runtime stack check remains
2432 bytes including reserve; it is not measured display-thread stack high water.

### Direct Raster Target Result

Capture `__fw0_tracex_snapshot_20260913_123219.trx` retained a complete transaction
with no marker errors and reported 24 MHz throughout. Raster COPY groups fell
from 15 to 5 and their elapsed cycle-derived time fell from 19.893 to 4.523 ms.
Clearing remained 1.845 ms. Kernel-tick receipt-to-panel was 80 ms versus 100 ms
in the prior capture. The runtime transaction was 110.174 ms in cycle-derived
time versus 123.296 ms previously; the difference between time domains remains
unresolved and neither is independent physical-edge timing. Drawing counts were
8 versus 7 and owner interruptions differed. The user confirmed unchanged awake
marker, digit and static-content behaviour.

The supplied autonomous probe reported replacement attempts/failures 2/0,
timers due/applied/errors 3/3/0, WFI returns/measured/reconciled 5/5/5, wake statuses
zero and display request/complete/result 7/7/0. This is successful subsystem-work
evidence, not a new low-current measurement.

## Owner Work Attribution

The same trace showed the first runtime clock request acknowledged near 3.9 ms,
followed by thPower holding I2C3 near 4.3-15.6 ms before yielding. Periodic battery
monitoring calls a broad PMIC snapshot including power, charger configuration,
interrupt and fuel-gauge reads. This is the matching code-path hypothesis, not
yet a named-operation marker proof. thInput execution intervals totalled about
12.6 ms. Its cardinal sampling path wakes a suspended sensor, reads and then
suspends it again; the raw reader also sleeps one tick while retaining its lease.
Those sleeps release the CPU but not I2C3. HAL transfers are blocking.

The next diagnostic adds event 0x5173 around the actual PMIC snapshot and
joystick cardinal wake/read/suspend calls. Fields are operation, begin/end,
capture sequence and driver ps_status_t; zero on end means success. Begin uses
NOT_RUN. Completion is recorded only if the same capture remains active. Pair
by thread, operation and sequence; a missing end at freeze is truncated work,
not a timeout verdict. Absence of a PMIC marker does not prove battery monitoring
is disabled: it may not have been due inside this one transaction.

This adds no polling, retry, priority, sensor configuration or power-policy
changes. It distinguishes real successful/failed driver work from scheduling,
but does not count internal retries or separate bus time from settling/preemption.
Reflash, run the same awake HOME/AWAY helper and collect a one-press trace with
the existing enable/prints/dump helpers. Use it to choose the next change rather
than weakening input reliability or PMIC safety checks based on elapsed time alone.

Owner-marker verification: **356 authoring/native tests pass**, including inactive
no-ops, matching sequence/status fields, stale completion suppression after freeze
and re-arm, marker insertion failures and source checks around the actual calls.
Debug build, target-profile and whitespace checks pass. Ordinary RAM is unchanged
at 552360 bytes, ROM is 876216 bytes and SRAM4 remains 15480 bytes. The runtime
stack check remains 2432 bytes including reserve; this is not owner-stack high
water or hardware marker validation. Two unused-function warnings remain in
untouched owner state-machine code. Target owner-marker capture is pending.

## Successful Owner Work and Awake Joystick Lifetime (2026-09-13)

The owner-marker capture `__fw0_tracex_snapshot_20260913_130422.trx` retained
matching RECEIVE/DONE markers with zero insertion errors. Visual behaviour was
unchanged. The completed first joystick wake/read/suspend calls took approximately
15.6/5.6/3.9 ms in cycle timestamps, all with driver status zero. A second wake
completed successfully; its following read was truncated by trace freeze, not
reported as a failure. No PMIC snapshot was captured in this transaction.

These are elapsed call durations, including settling and preemption, not isolated
CPU usage. The kernel probe reported 70 ms receipt-to-panel while the retained
cycle interval was approximately 100 ms; that discrepancy remains unresolved.
The change from the earlier 80 ms probe is not a new optimisation: this build
only added markers. Neither measurement includes physical input/debounce latency.

The user approved retaining active TMAG conversions while awake. The bounded
cardinal path now bypasses diagnostic parking for the existing SLOW_POLL/ACTIVE
pair and leaves successful polls, including wake confirmation, in that pair.
It still performs full wake setup when coming from suspended/wake-and-sleep,
clears terminal-sleep proof on wake, and preserves recovery for inconsistent or
failed states. Existing diagnostic preparation still parks the live device.
STOP2/shutdown quiesce, threshold derivation/verification, terminal-write order,
poll eligibility, sample delay and poll-period knobs are unchanged. No driver,
clock, priority, probe-layout, calibration or package-format changes are made.

Native coverage executes the production poll/preparation functions and joystick
transition table with a fake driver: repeated awake reads have one initial wake
and no suspend; diagnostic handoff parks; simulated terminal sleep requires a
new wake; bounded stable/fallback confirmation is retained; injected read, wake,
normalization and FSM failures preserve cleanup/recovery. Source checks retain
the STOP2 preparation and polling-admission paths, but do not prove physical
quiesce or movement wake.

Target checks pending: reflash with matching ELF, repeat the awake one-press trace
after normal polling has started, and expect READ markers without per-poll WAKE
or SUSPEND. Then test all four joystick directions waking STOP2, correct release
and repeat behaviour, and the unchanged HOME/AWAY animation/timers. Use existing
`__fw0_joystick_input_prints.gdb` and `__fw0_joystick_stop2_wake_prints.gdb` for
input/wake evidence. Compare PPK2 active-event energy and settled STOP2 current;
short normal awake bursts do not establish the cost of longer awake sessions.

Local verification: **358 authoring/native tests pass** with `HOST_CC` set to
the native GCC path. The initial suite invocation omitted that environment
variable and failed STOP2 test setup; the complete rerun passed. Debug build,
target-profile and whitespace checks pass. RAM remains 552360 bytes, ROM is
876240 bytes, and SRAM4 remains 15480 bytes. Runtime stack analysis remains
2432 bytes including reserve (not a measured input-thread high-water result).
The two existing unused-function warnings are unchanged. Hardware latency,
joystick wake and power acceptance are still pending.

## Active-Poll Hardware Follow-Up

The `20260913_140936.trx` awake capture completed with no marker errors. It
contained one completed READ and no joystick WAKE or SUSPEND operations. The
input probe reported 257 polls, zero errors and owner/driver SLOW_POLL/ACTIVE
(8/2). Cycle-derived candidate packing fell from approximately 59.1 to 47.4 ms;
receipt-to-panel fell from approximately 99.4 to 91.4 ms. Both coarse kernel
probes still reported 70 ms. This is one capture comparison, not a worst-case
latency guarantee or proof of isolated CPU time saved.

The subsequent autonomous test confirmed LEFT/RIGHT/UP/DOWN wakes 1/1/1/1,
four IRQs enqueued/dequeued with zero drops, four logical activations/releases,
211 error-free awake polls and eight STOP2 entries. Wake writes/verification
passed (0xfff/0x7ff). This unit used the fixed 48/48 threshold fallback after
TRANSFORM derivation rejection; the test validates movement wake with fallback,
not calibration-derived threshold admission. These logs alone do not establish
visual continuity, long-duration false-wake rate, repeat behaviour or current.
PPK2 energy/current acceptance remains outstanding.

## Tick-Retuning Diagnostic

Source inspection found `PS_HW6_ClockPolicy_ApplyBase` calls
`PS_HW6_ClockPolicy_RetuneThreadXSysTick` even when the physical base clock already
matches. Retune unconditionally writes LOAD and clears VAL. Discarding partial
tick intervals during repeated same-frequency capability requests is the current
hypothesis for the ThreadX/cycle discrepancy; it could also extend tick-based
sleeps and timers. This is not yet a measured attribution of the entire gap.

The approved first increment only observes that existing behaviour. A bounded
stack-local snapshot records old LOAD/VAL, target LOAD, DWT cycles, ThreadX tick
and ICSR around the original writes. Three events (0x5174..0x5176) are emitted
afterward, only during the active one-press capture. No new trace buffer, polling,
clock frequency, interrupt masking, timer policy or probe layout is introduced.
Do not read CTRL as part of this diagnostic: doing so clears COUNTFLAG.

Use the existing awake HOME/AWAY, object-trace enable/prints and TraceX dump
helpers after reflashing the matching ELF. Require complete retained triples and
zero marker errors. At unchanged 24 MHz, old LOAD should be 239999; old LOAD
minus old VAL estimates the fractional tick discarded. Correlate resets with
the sampled cycles/ticks and exception state. These sequential snapshots are not
atomic; pending/active SysTick, tick crossings and preemption limit precision.
Do not sum insertion timestamps as if they were the reset instant. Preserve
proper handling of real frequency changes and STOP2 restoration when designing
the later fix. Diagnostic overhead and independent physical timing remain caveats.

Retune diagnostic verification: all **358 tests pass**, with the expanded native
trace cases also rerun directly. Tests execute the production retune function
and preserve its unchanged-reload reset, changed-reload reset and invalid-clock
behaviour, plus inactive/stale-capture suppression, snapshot fields, cycle wrap,
tick/pending changes and failed marker insertion without changing the retune
result. Debug build, target-profile, whitespace and runtime-stack checks pass.
RAM remains 552360 bytes, ROM is 876536 bytes, and SRAM4 remains 15480 bytes.
The stack-local diagnostic snapshot is 28 bytes; this is not a measured thPower
stack high-water result. Existing unused-function warnings remain unchanged.
At that checkpoint hardware retune capture was pending; the result follows.

## Same-Rate Tick Preservation

The 2026-09-13 capture `__fw0_tracex_snapshot_20260913_165641.trx`
retains matching RECEIVE/DONE markers and six complete retune triples, with
zero marker errors and unchanged 24 MHz HCLK. Each retune writes the already
correct LOAD=239999 and clears VAL. The six discarded partial intervals total
approximately 30.08 ms. Accounting for capture start/end tick phase reconciles
the reported 60 ms kernel interval with approximately 88.48 ms of cycle time.
This supports same-rate counter resets as the cause of this measurement gap;
it is not an independent physical button-to-panel measurement.

`PS_HW6_ClockPolicy_RetuneThreadXSysTick` now returns without register writes
when LOAD already matches the requested rate. This preserves the partial
countdown and pending tick, whether SysTick is enabled or temporarily disabled.
It does not read CTRL. A changed reload retains the existing LOAD/VAL update;
fractional-time preservation across actual frequency changes is not introduced.
The separate STOP2 suspend/restore functions continue to own enable/disable
and pending-state handling, without changes to RTC reconciliation or clocks.

Native regression cases execute the production retune and STOP2 suspend/restore
functions: repeated same-rate calls, pending ticks, boundary VAL values,
disabled-counter preservation, 24/48 MHz changes, invalid clocks and trace
failure isolation. The awake one-press TraceX acceptance requires that stable
24 MHz requests emit no retune
triples, and kernel/cycle durations should agree within tick quantisation and
measurement overhead. A higher printed kernel duration after this fix does not
by itself indicate slower execution. Then check HOME/AWAY timers and autonomous
wake/animation continuity separately.

Local verification: all **358 tests pass**. Debug firmware build, generated
target-profile and whitespace checks pass. Ordinary RAM remains 552360 bytes,
ROM is 876552 bytes and SRAM4 remains 15480 bytes. The runtime stack check
remains 2432 bytes including reserve against 4096 bytes; this is a static check
of covered runtime paths, not a measured thPower high-water result.

### Awake Hardware Result

Capture `__fw0_tracex_snapshot_20260913_184946.trx` confirms the same-rate
preservation behaviour for one R selection change in awake HOME. All six
clock-policy records report success at 24 MHz; no retune records occur between
the retained RECEIVE/DONE markers. The ring wrapped once during the capture,
but all 369 transaction events, including both boundary markers, remain.
Capture completion and arm/freeze statuses are successful, with zero marker
errors; candidate and panel statuses are also zero.

RECEIVE-to-DONE cycle time is 104.617 ms. Runtime stage 0 to panel completion
is 103.761 ms, and stage 0 to transaction completion is 104.580 ms. The kernel
reports 100 ms for both totals: the difference is within one 10 ms tick,
unlike the previous approximately 30 ms lost-time contribution.

Measured cycle intervals are 49.149 ms for candidate raster/packing,
16.681 ms for presentation queue/clock and 24.474 ms for panel render/transfer.
These include preemption and instrumentation, not isolated CPU work. The
approximately 105 ms transaction is not a latency optimisation claim; this
sample is longer than the prior approximately 88.5 ms sample even though its
kernel accounting is now consistent. Physical input latency before runtime
receipt is still excluded.

### Autonomous Hardware Follow-Up

The subsequent HOME/AWAY run was reported to behave as before, confirming the
observed timer actions and local animation continuity through sleep/wake after
the timebase change. Launch succeeded; two scene replacements completed with
zero failures. Admission token/completion=8/8 with lease=0 and status=0;
payload admission remains 8 chunks/4672 bytes and the schedule 4 steps/400 ms.
Six WFI returns have six measured and six reconciled sleep intervals, clock
status=0 and all wake snapshot/render/map/resume statuses=0. Display
request/completion=7/7 with result=0 and no lease fault.

The halted sample has timer due/applied=3/2, ignored/errors=0/0 and four RTC
selections. It therefore does not prove every due handler had completed at the
halt; do not label these counters a fully settled timer result. The last
projection describes fresh HOME entry (elapsed=0, digit phase=0, marker left,
fill=0), not live autonomous pixels. This is a functional regression pass
supported by the user's observation, not independent timer-accuracy or
low-current/energy measurement. No further firmware change was made here.

## Joystick Settling Outside the Bus Lease

Further analysis of `__fw0_tracex_snapshot_20260913_184946.trx` separates the
49.149 ms candidate interval from a 16.681 ms presentation queue/clock interval.
Candidate raster phases total about 26.771 ms without double-counting nested
validation: drawing 14.247 ms, copies 4.869 ms, overlap 4.721 ms, clearing
1.853 ms and outer validation 1.080 ms. The remainder includes projection,
comparison, packing, scheduling and unmarked work; it is not attributed to
isolated CPU operations by this capture.

The presentation delay has a directly observed dependency chain. The joystick
takes I2C3 at 55.762 ms and sleeps one tick while holding it. The PMIC snapshot
starts at 59.998 ms and blocks on that mutex at 60.206 ms. The display posts
its clock request at 62.717 ms. Joystick release at 66.966 ms lets the PMIC run;
its snapshot completes at 78.360 ms and the queued display clock request is
acknowledged at 78.806 ms. Actual PMIC work, not just thread scheduling, is
identified by matching owner markers and successful completion. The snapshot
wrapper is shared by periodic and interrupt paths; its marker alone does not
identify which trigger invoked it.

The approved narrow change moves the existing
`PS_DEV_TMAG3001_SAMPLE_SETTLE_TICKS` wait in
`ps_dev_tmag3001_read_raw_sample` before `ps_hw_i2c3_acquire`. No bus work
precedes this wait within the sample call. Other owners may now use I2C3 while
the input owner settles. Invalid arguments/state still return immediately;
acquisition failure now occurs after settling rather than before it. Read,
release and primary-error precedence remain unchanged. The delay, timeout,
poll eligibility, clock policy, PMIC checks and terminal STOP2 writes are not
changed. The shared raw reader is also used for wake confirmation/diagnostics.

The native regression executes the production reader and completion helpers
with a fake bus: sleep asserts no held lease, another owner can complete work
during that wait, and read asserts a held lease. It covers repeated reads,
signed XYZ/status decoding, invalid state/arguments, acquisition failure,
read failure and release failure, including preservation of the primary error.
Hardware contention reduction and wake/visual regression checks remain pending;
this is not a claim to remove all PMIC or candidate-preparation latency.

Local verification: all **359 authoring/native tests pass**, including the
three targeted joystick tests. Debug firmware build, generated target-profile,
runtime-stack and whitespace checks pass. RAM/ROM/SRAM4 remain
552360/876552/15480 bytes. Worst checked runtime stack plus reserve remains
2432/4096 bytes; no new allocation or retained payload storage is introduced.

### Bus-Lease Hardware Result

Capture `__fw0_tracex_snapshot_20260913_201020.trx` retains all 386 events
between matching RECEIVE/DONE markers, with no ring wraps or marker errors.
The L selection transaction and panel complete successfully at unchanged
24 MHz. Cycle-derived total is 101.142 ms (runtime receipt to panel 100.512 ms),
consistent with the 100 ms kernel reading. No SysTick retune records occur.

Both captured joystick reads now sleep before acquiring I2C3:
38.084 -> 47.985 ms, then release at 49.450 ms; and
88.084 -> 97.890 ms, then release at 99.344 ms. Lease intervals are about
1.465 and 1.453 ms, with successful read-completion markers. This confirms the
approved ordering change on hardware. No PMIC operation overlaps these two
settling intervals, so this run does not quantify avoided contention under the
previous overlap pattern.

Candidate preparation remains 49.327 ms. Presentation queue/clock is 0.501 ms
and panel render/transfer 25.822 ms. PMIC work occurs earlier instead: the power
owner acknowledges the initial runtime clock request at 3.783 ms, starts its
snapshot at 4.033 ms and completes it at 15.548 ms. Runtime consumes the
acknowledgement at 15.759 ms after the power owner suspends. The snapshot's
successful completion proves actual PMIC work delayed runtime progress even
though the clock acknowledgement had already been sent. This remains separate
from the fixed bus-holding sleep and is not a newly proven scheduler defect.
These two samples do not establish a worst-case latency improvement. The user
subsequently confirmed all requested autonomous checks remained normal:
joystick wake, L/R movement, animation continuity and HOME/AWAY timer behaviour.
This closes the functional regression check for moving settling outside the
bus lease. It is user-observed behaviour, not an additional instrumented trace
or a new power/current measurement. PMIC interference and candidate-preparation
cost remain separate optimisation work.

## Next: Scheduled PMIC Workloads

The next agreed step is the design in the PMIC and Power Contract's
"Planned PMIC Monitoring Schedule" section. Firmware is unchanged by this
documentation checkpoint.

The current full snapshot is due once per second, not on every owner-loop
iteration. Boot, PMIC interrupts and explicit diagnostics can also request it.
It combines battery safety, charger/VBUS status, SOC and configuration checks
into 24 single-register reads plus conditional interrupt-flag clear writes.
The 201020 trace shows real PMIC work from approximately 4.033 to 15.548 ms;
runtime consumed an earlier clock acknowledgement only at 15.759 ms. This is
evidence of synchronous owner work delaying progress, not merely a scheduled
thread or an assumed bus conflict.

Separate safety/event monitoring from slower cached telemetry and configuration
audits before tuning transfer overhead. Retain the existing active safety
cadence initially; define per-group freshness, bounded deferral, failure and
verification rules. Optional refreshes must not create periodic STOP2 wakes.
Slower periods and asleep safety deadlines still need explicit policy and
hardware evidence. UI consumers should not request fresh hardware reads merely
to redraw battery information.

The earlier proposal to consolidate adjacent registers into bounded burst reads
remains a possible implementation optimisation, not the next standalone fix.
In particular, existing full-snapshot success depends on configuration checks:
splitting the snapshot must preserve their boot/recovery admission role rather
than silently weakening validation. No GUI capability or authoring change is
required for this internal power-owner work.

## Verification

Native tests exercise actual stamp/begin/end functions and presentation/candidate
paths: explicit arming, one-shot retention, inactive no-op, token association,
clock/tick readback and continued timeout/rejection behaviour. The GDB formatter
was initially exercised against a native record, which did not expose the
bare-metal string allocation failure. A regression check now forbids inferior
string arguments in the numeric row helper.

Cache tests exercise exact-byte hits across caller addresses, corrupt bytes at
the same address, changed size/mode/scene, raster failure after a hit, borrowed
diagnostics, and timeout/late completion with caller mutation. Pixel tests
compare the fast sprite loop with the previous per-pixel mapping across opaque
and masked data, clearing/preservation, unaligned origins and display edges.
First-optimisation and API-2 substage target results are recorded above.

All **349 authoring/native tests passed** after the first optimisation. Debug firmware
build, generated target-profile check and diff whitespace check pass. ARM stack
analysis reports a worst checked path plus reserve of 2432 bytes out of 4096,
leaving 1664 bytes; this remains a static bound for the checked paths, not an
observed high-water measurement. Build totals: ordinary RAM 551656 bytes,
ROM 872240 bytes and SRAM4 15480 bytes. No additional SRAM4 payload storage or
dynamic allocation was introduced.

API-2 breakdown verification: **350 authoring/native tests pass**, including
timed/untimed byte-equivalent private payloads and verdicts, clock wraparound,
NULL-observer inactivity, partial rejection counts, capture reset, busy refusal
and late-completion retention. Debug firmware build, target-profile generation
check and diff whitespace check pass. Runtime stack analysis still reports
2432 bytes including reserve; this does not measure display-thread high water.
The breakdown build uses RAM 551744 bytes, ROM 872912 bytes, SRAM4 15480 bytes.
The first physical substage result is recorded above. Comparison against
observer-disabled operation remains outstanding.

Band-comparison build verification: **351 authoring/native tests pass**,
including the exhaustive frame-byte/production-payload comparison. Debug build,
runtime stack analysis (2432 bytes including reserve) and diff whitespace check
pass. RAM remains 551744 bytes and SRAM4 remains 15480 bytes; ROM is 872984 bytes.
Post-change target timings are recorded above; there was no overall improvement
in that sample.

Incremental-raster verification: **352 authoring/native tests pass**, including
pixel equivalence for opaque/masked sprite motion across display edges, hide/show,
overlap restoration, layer/z changes, reordering, removal, unchanged frames and
cache invalidation. Complete cached packing workspaces and verdicts match full
rasterization for the structured/dual fixtures, all tested local states and timer
visibility, including capacity rejection. Exact-byte cache tests cover warm reuse,
missing assets after a hit, changed package/mode/scene and timeout/late completion.
The private checker continues to leave live frame/DMA/catalog/runtime state intact.

Debug firmware build, target-profile check and whitespace check pass. The runtime
stack checker remains at 2432 bytes including reserve (not display-thread or
measured stack high water). Build totals are ordinary RAM 555320 bytes, ROM
874536 bytes, SRAM4 15480 bytes. API-3 target timing, visual and power results are
pending. Reflash and use the matching ELF and helpers before collecting them.
