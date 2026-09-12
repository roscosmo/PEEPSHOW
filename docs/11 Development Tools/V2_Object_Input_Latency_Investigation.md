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

## One-Press Capture

`g_ps_object_latency_probe` API 1 is independent of the existing probe layouts.
It occupies 320 bytes, has no heap or background task, and is dormant unless
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

Use bounded TraceX captures to separate execution from waits/preemption during
boot, steady playback, local state changes, scene replacement, timer expiry,
audio and installation. Correlate owner work and clocks with PPK2 captures;
record trace/debug configuration and compare an untraced, debugger-detached
power baseline. Record latency distributions, stack high-water evidence and
awake residency rather than inferring headroom from one saved stack pointer or
successful queue counters. The one-shot probe remains useful for repeatable
before/after comparisons but does not replace this broader analysis.

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
Post-optimisation target timings are pending; no numerical speedup is claimed.

All **349 authoring/native tests pass** after this increment. Debug firmware
build, generated target-profile check and diff whitespace check pass. ARM stack
analysis reports a worst checked path plus reserve of 2432 bytes out of 4096,
leaving 1664 bytes; this remains a static bound for the checked paths, not an
observed high-water measurement. Build totals: ordinary RAM 551656 bytes,
ROM 872240 bytes and SRAM4 15480 bytes. No additional SRAM4 payload storage or
dynamic allocation was introduced.
