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
Hardware latency improvement remains unmeasured. Reflash this build, repeat the
same awake HOME/AWAY one-press trace after the reveal, and verify fixed outlines,
one marker move and continuous digits before comparing CLEAR and total timings.

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
