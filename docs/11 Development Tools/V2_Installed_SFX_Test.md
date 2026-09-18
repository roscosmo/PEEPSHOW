# Installed V2 Resident SFX Test

Status: 2026-09-17, firmware admission and native tests implemented; installed
audible behavior confirmed on hardware as recorded below. That firmware milestone
kept public API 44 audio export disabled. The subsequent API 45 increment enables
the resident subset; see [[Peep_Studio_V2_Audio_Export_Handoff]]. This is an OS
bench fixture, not a Studio export workaround or shipping-profile claim.

## Scope

The resident V2 firmware profile now accepts validated audio catalogs and local
PLAY_SFX actions, including targetless timer handlers. Existing loader validation
checks the complete ADPCM bank, cue metadata and graph cue references before
admission. Total package size, including footer, remains at most 65536 bytes.
The existing target audio counts, codec, sample rate, cue volume and priority
validation apply; no new firmware tuning values or buffers are introduced.

All scenes still require exact owner raster/payload admission. Cross-scene exits
remain action-free; shell-exit actions, mixed execution models and nonresident
V2 audio remain rejected. The profile-reason enum retains its old AUDIO value
for stable numeric diagnostics; malformed catalogs are loader AUDIO failures.

The existing runtime commits SFX effects only after successful admission.
`thAudio` owns decode, mixer, SAI/DMA and amplifier control; `thPower` owns clock
and sleep decisions. No driver, clock policy, STOP2 or FileX behavior changes.
Local changes and same-package scene replacement do not stop current SFX.
Shell suspension stops and discards SFX; resume accepts new cues without replay.
Music, fades, resumable audio and cross-scene exit effects remain excluded.

## Fixture

Generator: `tools/authoring/build_installed_sfx_fixture.py`. It reuses the
structured HOME/AWAY visuals and the previously audible development tone data,
without changing the linked embedded egg or any GUI project.

```powershell
python tools/authoring/build_installed_sfx_fixture.py --egg-output firmware/peepshow_hw6_fw0/build/Debug/installed_v2_sfx.egg
```

- Egg: `firmware/peepshow_hw6_fw0/build/Debug/installed_v2_sfx.egg`.
- Size: 54696 bytes; two scenes, two sampled assets and two cues.
- SHA-256: `af1fe2d09a3b527b47a640a355be318f29a37380315c9ee5e7ad53cea5feff9c`.
- HOME=1, AWAY=2; initial scene HOME.
- Both scenes show numbered 1-2-3-4 frames at 400 ms and fixed L/R slots.
- L/R changes selection once and plays an 80 ms tone without restarting digits.
  Selecting the already-selected side has no route and plays nothing.
- HOME's bottom square appears once after two seconds, starting a six-second
  tone. Local selection changes must not truncate it.
- A enters AWAY; B returns HOME. Exits have empty action lists and play no new
  sound. A running tone must continue across the scene change.
- AWAY has no timer and no automatic return. Returning HOME is fresh, so its
  two-second timer and subsequent tone start again.

## Hardware Procedure

This increment changes firmware, unlike the previous API 44 export test.
Build/flash the ordinary `HW6 FW0: Debug with ST-LINK` launch configuration
(prelaunch task `Build HW6 FW0 (Debug)`), then resume boot. Do not use the
battery-shutdown-test configuration. Install the egg above through USB MSC and
the shell, then PLAY. Do not run an embedded-install or development-enable helper.

1. Observe the digits and HOME reveal; hear the long tone. Alternate L/R during
   playback: short tones should overlap it, without animation restart or glitches.
2. On a fresh HOME visit, wait for the long tone, then press A while it is still
   sounding. AWAY must appear and the same tone must finish naturally. AWAY
   must not trigger a new timer tone or return by itself.
3. Return HOME with B. When its long tone starts, immediately HOLD START to open
   the shell. The tone must stop early. Wait beyond its original end, then
   Resume: no sound should restart. A fresh L/R change must still play a cue.
4. Let all voices drain, release controls, and allow normal low-power animation.
   Wake normally with a selection change; hear another cue, then let it drain.
5. Deliberately reboot: HOME must load normally and its one-shot reveal/tone
   must work again. Debugger reconnection alone is not this reboot test.

Never halt during audible playback. Once quiet, halt and collect:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_installed_sfx_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

Require audible cues and positive decoded/refill work, zero underruns and package
audio faults, and drained voices/outstanding requests/clock-held values of zero.
After shell stop, blocked=1 and matching stop request/completion with zero
send/wait/owner statuses are expected; Resume clears blocked. NOT_RUN before any
request is not a completed-work failure. Counters are cumulative, and neither
dispatch nor sleep counters prove audible output or measured low current.

## Firmware Milestone Native Evidence

47 focused tests passed across installed SFX, V2 profiles/scene candidates,
installed replacement, object timers and the public export boundary. The Debug
firmware build passed: RAM 553632 bytes, ROM 884624 bytes, SRAM4 15480 bytes.
RAM and SRAM4 use are unchanged by this admission-only firmware increment.

The exact generated bytes exercise normal installed preflight, real private
raster admission and installed entry in the native harness. Tests verify:

- rejected local admission leaves the live object graph and catalog unchanged
  and produces no SFX request;
- a successful local move commits one cue with expected samples, volume and
  priority while retaining the numbered animation phase;
- a targetless timer handler reveals the square and commits the long cue
  without local state re-entry;
- successful candidate preflight does not alter the active audio catalog;
- same-package replacement retains resident cue pointers, emits no exit cue,
  and allows another local cue in the destination;
- malformed ADPCM/cue metadata, exit SFX and local shell-exit actions reject;
- an audio package at 65536 bytes admits, while one above that ceiling rejects;
- at the API 44 firmware checkpoint, the public parser/compiler still rejected
  audio. API 45 instead verifies the same bytes through the public build path.

Native stubs do not produce sound or exercise physical SAI/DMA, shell audio
quiesce or current draw. Existing timer/SFX dispatch and failure tests supplement
this fixture. The hardware observations below do not expand the public V2
audio export subset by themselves.

## Hardware Result (2026-09-17)

The user reported the expected fixture behavior and explicitly confirmed:

- the long tone continued across HOME to AWAY;
- opening the shell stopped the long tone early, and Resume did not replay it;
- fresh selection presses produced tones, confirming the requested post-sleep
  playback check.

The supplied capture shows installed source 3, execution model 2, 54696 bytes,
slot 0, generation 20, active in AWAY/state 2. Two assets/two cues contain 50924
ADPCM bytes, package-backed=0 (fully resident).

- Audio dispatch/owner counts 20/20, send/owner statuses 0; outstanding requests
  and clock-held both 0.
- Package audio blocked/fault 0/0 after Resume, stop request/completion 6/6,
  status/send/wait/owner all 0. No halted shell snapshot is claimed; early audible
  stop and silent Resume were confirmed by the operator.
- Voice active/peak/completed 0/3/3, refill/decoded/underrun 5/3840/0. This proves
  actual decoder/refill work in the retained measurement, not just dispatch.
  It is not a complete sample-count account of all twenty requests or proof of
  the six-second cue's duration. Long-tone continuity relies on observation.
- Three scene replacements, zero failures; candidate token/completion 31/31,
  lease 0, profile/schedule/display statuses 0; payload eight chunks/4672 bytes.
- Display request/completion 26/26, result/fault 0/0. Timer due/applied/error
  2/2/0; RTC timer selections 2.
- Four-step/400 ms LPBAM schedule with publication/fault 0/0. Selected backend
  2, backend status 0. WFI returns/measured/reconciled 16/17/17, clock status 0,
  automatic STOP2 entries 19. These counters are not treated as identical-scope
  per-action deltas or measured current. The current STOP2 status was NOT_RUN.

This validates installed resident SFX with the exercised animated scene changes
and shell lifetime behavior. Full five-voice concurrent-display stress, priority
preemption, current/energy measurement, and an explicit reboot confirmation for
this audio egg are not established by this capture. API 45 subsequently enables
public resident audio export; a normal Studio-generated audio artifact remains
the next hardware integration test.

## API 45 Scene-Change Latency (2026-09-17)

Studio supplied `native_v2_lobby_garden_audio.peepproj` from commit
`efd079120281c11c9c2fd764c74e15fb67cf04f1`, built by public
`project.build_package`. Artifact:
`G:/PEEPSHOW-PeepStudio/tools/peep-studio/dist/api45-v2-audio/dev.peepshow.native_v2_lobby_garden_audio.egg`,
55416 bytes, SHA-256
`47d25a9528481a9773f45cf9c4e76452f49ea3577d0a8906c3c30f76af4cb53c`.

Installed generation 22, source/model 3/2, twelve successful replacements and
display completion 114/114 were recorded. The operator reported a scene-change
delay exceeding one second. Capture sequence 1, button A, Lobby 2 to Garden 1,
reported 2460 ms runtime receipt to panel completion (2470 ms total), at sampled
24 MHz. Candidate decode/validation took 1110 ms, admission 1300 ms, event commit
2400 ms, raster/packing 160 ms, and panel rendering/transfer 50 ms. Totals overlap.
The approximately 1100 ms outside admission is consistent with the separate
destination package decode, but that call did not have an isolated timing marker.
These are elapsed kernel times including scheduling, not isolated CPU time;
physical input/debounce and pre-runtime wake are excluded. No hardware latency
pass or complete GUI audio-lifetime acceptance is claimed from this capture.

The correction reuses the already-published immutable package metadata for both
destination decoding and matching private display candidates. Only the requested
scene is decoded/checked; unchanged container/audio data and unrelated scenes
are not revalidated during the transition. Active metadata reuse is revoked on
exit/reload, and a nonmatching candidate still takes full validation. Existing
private byte leases, raster admission and atomic rejection remain intact.

52 focused tests passed, including repeated audio scene changes without an
increase in whole-package load count, private-span rebasing, changed-byte
rejection, invalid destination, timeout/late completion after source-buffer
reuse, failed publication and ordinary installed reload. Debug build passed:
RAM 553632 bytes, ROM 885384 bytes, SRAM4 15480 bytes. RAM/SRAM4 are unchanged.
Hardware timing after this correction is recorded below; rendering cost remains.

The additional awake-display regression suite needed its host harness brought
up to date with the existing battery fault-wait probe and `TX_NOT_DONE` constant;
no battery policy or firmware behavior was changed for that harness repair.

Flash the updated normal Debug firmware, keeping this installed GUI egg. In Lobby,
halt, source `__fw0_object_latency_enable.gdb`, resume and press A once. Observe
the change without halting, let the timer cue finish, then halt and source
`__fw0_object_latency_prints.gdb`. Repeat for B from quiet Garden to Lobby. Confirm
fresh entry, local animation continuity, timer behavior and existing SFX lifetime
semantics. Do not use a development scene-enable helper for this installed test.

### Hardware Timing After Metadata Reuse

The operator reported substantially quicker transitions. Captures at sampled
24 MHz show:

| Direction | Capture | Event commit | Candidate decode | Candidate raster/packing | Panel render/transfer | Receipt to panel | Transaction total |
|---|---|---|---|---|---|---|---|
| Lobby to Garden | 1 | 250 ms | 50 ms | 150 ms | 50 ms | 310 ms | 320 ms |
| Lobby to Garden | 3 | 250 ms | 50 ms | 150 ms | 60 ms | 310 ms | 320 ms |
| Garden to Lobby, after audio ended | 4 | 110 ms | 40 ms | 20 ms | 50 ms | 160 ms | 160 ms |

All three have event/status/panel result 1/0/0. Full candidate package loads
remain 5 across captures 1 through 4, despite scene metadata misses increasing
from 3 to 8. This supports the native evidence that subsequent transitions do
not reload the whole package. Garden entry improved from 2460 to 310 ms receipt
to panel completion (about 87 percent less elapsed time).

Capture 2 returned to Lobby in 60 ms, but sampled 160 MHz throughout. It is not
used as a same-clock comparison; the capture does not identify the clock claimant.
The 24 MHz captures show candidate preparation for Garden's animated cycle at
150 ms versus 20 ms for static Lobby. Garden composed five frames including wrap
(one full, four reused, 50 elements drawn); Lobby composed two (one full, one
reused, three elements drawn). The helper's four-step expected-call footer does
not apply to static Lobby.

These results establish the latency correction for the tested transitions, not
final responsiveness acceptance. Kernel resolution is 10 ms and stage elapsed
times include preemption; totals overlap. No isolated CPU, current/energy or
physical-button-to-panel measurement is claimed. Full GUI audio-lifetime and
reboot acceptance still require explicit observations beyond these timing runs.

### TraceX Border Redraw Diagnosis and Region Correction

The frozen `__fw0_tracex_snapshot_20260917_202542.trx` contains matching sequence-1
RECEIVE/DONE markers, zero ring wraps/marker errors, and 24 MHz marker/clock
samples throughout the transaction with no SysTick retune records. DWT elapsed
time is 311.35 ms RECEIVE to DONE and 308.33 ms RECEIVE to panel completion.
Candidate decode spans 49.27 ms, raster/packing 147.24 ms, and the panel
render/transfer stage 59.00 ms. These are elapsed intervals, not isolated CPU
time or physical-button latency; nested intervals must not be summed.

Within candidate raster/packing, the first full composition spans 20.19 ms.
Four reused frames spend 20.82 ms clearing and 76.10 ms drawing; five cache/output
copies total 4.52 ms. Recorded scheduling interruptions in this stage are brief;
unmarked ISR and observer overhead remain included. The retained counter reports
one full/four reused frames and fifty element draws.

The source fixture's 160x136 border contains every object within its bounding
rectangle. The previous whole-object overlap closure therefore propagates a
digit change to all ten visible objects, although the digit does not touch any
border line. The original source commit's object geometry was checked against
the current project and matches. The operator confirmed that GUI replaced the
original audio with real audio; the artifact at the original path now has a
different digest. This trace is not evidence of the original egg's exact bytes.

The correction keeps only changed old/new rectangles, removes contained or
duplicate regions, and clips ordered recomposition to each remaining region.
Sprite transparency/opaque-white writes and primitive edge pixels are preserved;
no dirty region grows merely because it intersects a border or background.
Full validation and legacy-text cold fallback remain. Only existing private
candidate-cache pixels are modified; the published frame, active DMA payloads,
clock policy and audio behavior are unchanged.

Native regression reproduces the border mechanism: one full/four reused frames
now issue eighteen rather than fifty element draw calls with identical pixels.
The eighteen include four border calls clipped entirely out of the digit region;
draw counts are not pixel counts. Further parity tests cover partially clipped
shapes, masked/opaque sprites, motion, visibility, overlap, layer/order changes,
one-pixel regions, removal and invalidation. Complete four/eight-frame payloads,
including wrap, must match the uncached compositor. Hardware timing after the
correction and operator regression confirmation are recorded below.

Verification: 30 focused native/host tests passed across display admission,
awake rendering, shape parity, trace capture and candidate queue ownership.
Debug build passed with RAM 553640 bytes (+8), ROM 886120 bytes and SRAM4 15480
bytes (unchanged). Diff whitespace checks passed. No target timing improvement
is claimed from host execution or build success.

For the comparison, keep the currently installed GUI audio egg unchanged and
flash normal Debug firmware only. In quiet Lobby, halt and source
`__fw0_object_trace_enable.gdb`, resume and press A once. Observe Garden without
halting; allow its timer cue to finish, then halt and source
`__fw0_object_trace_prints.gdb` and `__fw0_tracex_dump.gdb`. Confirm all border,
label and marker pixels, animation continuity, timer reveal and SFX behavior.
The trace freezes on transaction completion, before the later timer cue. CLEAR
and DRAW phase pairs may now repeat per region within one reused frame. Do not
enable a development scene or separately arm the latency helper for this test.

### Hardware Timing After Clipped Regions

The frozen `__fw0_tracex_snapshot_20260917_205119.trx` contains one matching
sequence-1 RECEIVE/DONE pair, zero wraps/marker errors, all runtime-stage and
clock-policy samples at 24 MHz, and no SysTick retune triples. Button A changes
Lobby 2 to Garden 1 with event/status/panel 1/0/0. One full/four reused frames
now report eighteen element draw calls, matching the native regression.

| Elapsed work | Before clipping | After clipping |
|---|---:|---:|
| Candidate decode | 49.27 ms | 49.23 ms |
| Candidate raster/packing | 147.24 ms | 61.54 ms |
| First full composition | 20.19 ms | 20.74 ms |
| Four cached-region clears | 20.82 ms | 0.43 ms |
| Four cached-region draws | 76.10 ms | 10.66 ms |
| Five raster-cache/output copies | 4.52 ms | 4.51 ms |
| Panel render/transfer stage | 59.00 ms | 59.65 ms |
| RECEIVE to panel completion | 308.33 ms | 222.84 ms |

Candidate preparation is about 58 percent shorter; receipt-to-panel elapsed time
is about 28 percent shorter. The clear/draw reduction accounts for nearly all of
the saving, while decode and panel-stage costs are essentially unchanged.
These nested intervals cannot be added together. DWT transaction completion is
approximately 226.1 ms after RECEIVE; the coarse kernel report is 220 ms.
Both measurements exclude physical input, debounce and pre-runtime wake latency,
and include observer/preemption effects. Clock samples and absence of retune
records support this conversion, not a claim of isolated CPU or energy use.

Full candidate package loads remain 5, with metadata hits/misses 1/3. Trace and
panel completion prove real preparation/presentation work, but the supplied
capture does not itself confirm intact borders/labels, marker motion, animation
continuity or audio lifetime. After reviewing this result, the operator explicitly
confirmed unchanged borders, labels, L/R movement, animation, timer and audio
behavior ("yes everything acts as before"). This closes the exercised hardware
regression check for clipped candidate composition. It does not establish
physical-button latency, current/energy savings, or final responsiveness targets.

### Prepared Runtime Scene Admission

The next optimization removes the second destination-scene decode from runtime
display admission. Runtime still decodes and initializes a fresh destination
once, then passes its validated descriptor and staged object bank to the
same-thread admission callback. Local changes pass their existing descriptor.
Admission builds the bounded, pointer-free waiting program from that bank
without initializing another unused graph.

The loader checks active-package lifetime, byte identity and scene ownership,
and rebases the sprite catalog into the existing private candidate bytes.
The display queue receives neither the prepared scene nor its object bank.
Timeouts retain the private bytes/catalog until matching completion, including
when the source scene exits. Raster and payload capacity checks remain in place.
Runtime catalog-only cache entries are distinguished from fully decoded public
preflight entries. Install/preflight still performs full validation, and a
revoked active source cannot take the prepared path even on a cache hit.

Native checks cover exactly one decode per replacement, no additional decode
for prepared local admission, private catalog spans, byte mismatch and wrong
scene rejection, full public preflight after a runtime cache hit, timeout/late
completion after source release, and revocation/reload. The existing audio,
timer, scene rollback and shell recovery tests remain applicable. Debug build
passed: RAM 553648 bytes (+8), SRAM4 15480 bytes (unchanged); no clock, power,
rendering or audio behavior change is intended. The 42 focused tests passed;
hardware timing is recorded below.

Keep the current real-audio GUI egg installed and flash normal Debug firmware
only. In quiet Lobby, arm `__fw0_object_trace_enable.gdb`, resume, wait one
second and press A once. Allow the Garden timer cue to finish before halting;
print `__fw0_object_trace_prints.gdb` and dump with `__fw0_tracex_dump.gdb`.
Compare matching RECEIVE/DONE intervals against the 20260917_205119 capture,
especially candidate decode, graph/schedule and total receipt-to-panel time.
The decode-labelled interval still includes byte identity and descriptor/profile
checks; it is not expected to become zero. Confirm unchanged selection,
animation, timer, scene-exit audio continuity and shell discard/resume behavior.

### Hardware Timing After Prepared Scene Reuse

Capture `__fw0_tracex_snapshot_20260917_212446.trx` contains matching sequence-1
RECEIVE/DONE markers, no ring wraps or marker errors, and successful event/panel
results for Lobby 2 to Garden 1. All transaction stage and clock-policy samples
remain at 24 MHz with no SysTick retune triples. Only the RECEIVE/DONE window
is converted; the preceding arm/wake history is excluded.

| Elapsed work | Before reuse | After reuse |
|---|---:|---:|
| Candidate decode/checks | 49.23 ms | 28.92 ms |
| Graph/schedule | 5.47 ms | 3.84 ms |
| Candidate raster/packing | 61.54 ms | 61.59 ms |
| Display clock release stage | 0.47 ms | 12.41 ms |
| Panel render/transfer stage | 59.65 ms | 57.10 ms |
| RECEIVE to panel completion | 222.84 ms | 210.37 ms |
| RECEIVE to DONE | 226.10 ms | 213.61 ms |

The decode/check and graph/schedule stages together decreased by approximately
21.93 ms. This run also contains an 11.73 ms PMIC snapshot, bracketed by successful
0x5173 operation-1 markers in thPower, after its clock-release acknowledgement
and before thDisplay resumes. That owner work accounts for most of the longer
clock-release interval; this is not evidence of a slow clock-tree change.
Net receipt-to-panel improvement is 12.47 ms (5.6 percent) in this pair of runs.
Do not subtract the PMIC interval and present the remainder as measured latency.

One full/four reused frames and eighteen element draws remain unchanged. Full
candidate package loads remain five. The trace proves real completed preparation
and panel work, not merely thread scheduling. Native regressions separately
establish the single destination decode and admission lifetime behavior. This
one hardware sample does not establish typical/worst-case latency, physical
button-to-panel delay, isolated CPU time or power savings. The operator confirmed
unchanged selection, animation, timer and audio behavior ("yes all as before").
This closes the exercised hardware regression check for prepared scene reuse.

### Panel Substage Capture (API 3)

The 57.10 ms panel-stage measurement is an aggregate, not the SPI wire duration.
The existing capture brackets only 0.35 ms of model validation inside it; it
cannot otherwise separate composition, framebuffer bookkeeping and transfer.
API 3 adds capture-scoped 0x5177 markers without changing drawing, transfer,
clock or sleep behavior. `thDisplay` opens this scope only for the captured
render token and closes it on return, including failed returns. An inactive,
completed or re-armed capture cannot inherit the old scope. Candidate packing
and unrelated rendering do not emit these panel records.

Records carry phase, begin=0/end=1, capture sequence and value:

| Phase | Work | Begin value | End value |
|---|---|---|---|
| 1 | Total scoped presentation | NOT_RUN | HAL status |
| 2 | Model copy | bytes | 0 |
| 3 | Clear framebuffer/bookkeeping | bytes | 0 |
| 4 | Compose scene | elements | accumulated black count |
| 5 | Copy cursor/base frame | bytes | 0 |
| 6 | Compare dirty rows | 0 | dirty rows |
| 7 | Fill statistics, including framebuffer hash | 0 | 0 |
| 8 | Present dirty rows | rows | HAL status |
| 9 | Build a Sharp wire transaction | rows | HAL status |
| 10 | Start DMA | wire bytes | HAL status |
| 11 | Wait for DMA | timeout ms | HAL status |
| 12 | Commit presented frame/bookkeeping | bytes | 0 |

TOTAL contains the other stages; TRANSFER contains each chunk's WIRE/START/WAIT;
COMPOSE contains the existing raster VALIDATE pair. Do not sum these nested
durations. Transfer is absent for a no-dirty-row presentation. Failed wire/start
work prevents later phases; a failed transfer never commits the framebuffer.
DMA launch and wait are software call boundaries, not exact electrical transfer
boundaries. Markers are outside polling loops and interrupt callbacks. The
existing bounded busy-wait remains unchanged; no CPU/energy improvement is
claimed from adding instrumentation.

Use normal Debug firmware and the same installed real-audio GUI egg. In quiet
Lobby, halt and source `__fw0_object_trace_enable.gdb` (now requires API 3).
Resume, wait one second, press A once, and allow the Garden timer sound to end
before halting. Source `__fw0_object_trace_prints.gdb` and
`__fw0_tracex_dump.gdb`. Require retained RECEIVE/DONE and TOTAL pairs, matched
per-phase records, zero marker errors, successful panel status, and unchanged
visible/audio behavior. Confirm clock records before converting cycles. The
first hardware substage result is recorded below.

Verification: 38 focused native/host tests passed across trace lifecycle and
transfer failure/chunk/commit paths, awake rendering, display admission, shape
parity, candidate queue ownership and installed SFX. Debug build passed with
RAM 553648 bytes and SRAM4 15480 bytes (both unchanged), ROM 887248 bytes.
The build retains the two existing unused-function warnings in the owner state
machines. Diff whitespace checks passed. No flash/install action was performed
by the agent.

### First Panel Substage Result

Capture `__fw0_tracex_snapshot_20260918_073627.trx` retains matching sequence-1
RECEIVE/DONE and panel TOTAL markers, every panel phase pair, zero wraps and
marker errors, successful event/panel results, and 24 MHz stage/clock samples
without retune triples. Lobby 2 changes to Garden 1. The panel-stage interval
is 57.37 ms (scoped TOTAL 57.34 ms), receipt-to-panel is 198.76 ms, and
receipt-to-DONE is 201.99 ms. The coarse tick print reports 200/210 ms.

| Non-overlapping panel work | Elapsed |
|---|---:|
| Copy model (520 bytes) | 0.14 ms |
| Clear framebuffer/bookkeeping | 1.05 ms |
| Compose 11 elements | 19.53 ms |
| Copy base frame | 0.77 ms |
| Compare/register 128 dirty rows | 8.66 ms |
| Statistics/hash | 3.45 ms |
| Prepare/launch/wait for all transfers | 21.95 ms |
| Commit frame/bookkeeping | 0.78 ms |

The transfer interval contains three chunks of 48/48/32 rows, with wire lengths
962/962/642 bytes. Within it, wire construction totals 1.09 ms, DMA launch calls
0.13 ms and DMA wait calls 20.61 ms; all statuses are HAL_OK. These nested values
must not be added again to the table. Software wait boundaries are not exact
electrical SPI duration. The remaining roughly 1 ms of scoped TOTAL is outside
these substage pairs. No marked PMIC operation interrupted this transaction;
the lower total than the previous capture is not an instrumentation speedup.

Investigation found an avoidable ordered-list search in
`DisplayRenderer_MarkPanelRowDirty`: `ComputeDirtyRowsFromCommitted` resets the
list and scans rows in ascending order, yet each newly dirty row searches the
list from its beginning before insertion. For the measured 128 dirty rows this
executes 8128 row-order comparisons, despite every row belonging at the end.
The 8.66 ms interval includes reset and memcmp as well as insertion; it is not
all proven recoverable. There are no intervening recorded events in that
interval, though unmarked ISRs and observer overhead remain possible.

Implemented after approval: fast append when the new row sorts after the
current last row, keeping duplicate/bounds/capacity checks and the existing
sorted insertion for out-of-order callers. The empty list also appends directly.
No clock, transfer speed, DMA wait or power policy changed.

The native dirty-row harness extracts the four production row functions and
checks ascending, descending and mixed insertion after every operation,
duplicates, invalid rows, full capacity, reset, sparse committed-frame changes,
identical frames and an invalid committed baseline. It checks ordered unique
one-based rows and matching marks, and verifies neither framebuffer is modified.
Existing renderer pixel-parity and panel wire/chunk/error tests also pass.
Together, 39 focused tests passed. Debug build passed: RAM 553648 bytes and
SRAM4 15480 bytes unchanged; ROM 887280 bytes, 32 bytes above instrumentation
alone. Hardware savings and unchanged behavior remain to be confirmed.

Reflash normal Debug firmware, retain the installed real-audio GUI egg, and
repeat the quiet Lobby-to-Garden API 3 trace sequence above. Compare DIRTY_ROWS
against 8.66 ms with the same 128 dirty rows and 24 MHz clock, while checking
the complete transaction and unchanged visuals/audio. Do not attribute changes
in other phases or interruptions to the append optimization.

### Dirty-Row Append Hardware Result

Capture `__fw0_tracex_snapshot_20260918_092206.trx` retains matching sequence-1
RECEIVE/DONE and all panel phase pairs, zero wraps/marker errors, and successful
event/panel results. Runtime stage and clock records remain at 24 MHz, without
retune triples or marked owner operations inside the transaction.

| Work | Before | After |
|---|---:|---:|
| Compare/register 128 dirty rows | 8.66 ms | 1.53 ms |
| Scoped panel TOTAL | 57.34 ms | 50.26 ms |
| Runtime receipt to panel completion | 198.76 ms | 191.43 ms |
| Runtime receipt to DONE | 201.99 ms | 194.73 ms |

Dirty-row work saves 7.13 ms (about 82%). Composition remains 19.53 ms for
11 elements and black count 2467; transfer remains 21.94 ms with the same
48/48/32 row chunks, 962/962/642 wire bytes and successful HAL statuses.
Candidate work remains one full/four reused frames with 18 elements drawn.
The total improvement is consistent with the targeted change, not a change
in display workload or clock rate. These are instrumented elapsed intervals,
not isolated CPU time or physical button-to-panel latency. Trace success and
matching work counts do not replace operator confirmation of intact visuals
and audio; that confirmation remains pending for this build.

### Candidate Scratch Reset Optimization

Further analysis of `__fw0_tracex_snapshot_20260918_092206.trx` measures the
candidate interval at 61.47 ms. The first full raster takes 20.74 ms; four
partial draws total 10.60 ms, overlap checks 2.52 ms, region clears 0.44 ms,
and raster output/model copies 4.51 ms. The ten marked validation calls total
3.42 ms, but five are nested within drawing and must not be added again.

The initial pair of display-thread clock reads surrounding the packing
workspace reset spans 5.18 ms. Their attribution follows the existing
PS_DisplayWork_Begin/End call order in CheckFullSceneAnimationProfiled;
these are TraceX timestamps of profiling calls, not new dedicated reset
markers or isolated CPU measurements.

After approval, the packer resets the previous/target frames, slot lengths,
bands and frames-composed counter individually, rather than clearing the
entire workspace. Wire construction initializes each transaction before use;
only populated slots below the local used_slots counter are compared, and
each such payload is copied before its slot becomes populated. Unused payload
bytes and wire scratch are intentionally unspecified. Live DMA storage, frame
baseline behavior, admission limits, clock policy and ownership are unchanged.

Native tests compare populated payload bytes, frames, metadata and results,
not irrelevant scratch tails. Added checks use two poison patterns, repeated
checks, successful full-capacity and HOLD cases, chunk/sequence rejection,
early/late callback failures, invalid arguments, and a small check after a
full one. Unused payload tails remain poisoned; live display payloads remain
unchanged. Existing exhaustive frame-byte/band equivalence, renderer, trace,
candidate ownership and installed SFX tests also pass: 40 focused tests total.
Debug build passed with RAM 553648 bytes and SRAM4 15480 bytes unchanged;
ROM is 887352 bytes. Hardware timing savings remain unmeasured.

Reflash normal Debug firmware and retain the installed real-audio GUI egg.
Repeat the same quiet Lobby-to-Garden API 3 trace, waiting for audio to finish
before halting. Compare the reset interval and complete candidate/transaction
times at 24 MHz, with unchanged frame/element counts, output and audio behavior.
Do not claim the whole 5.18 ms is removed: frame and metadata clearing remain.

### Candidate Scratch Reset Hardware Result

Capture `__fw0_tracex_snapshot_20260918_101956.trx` wrapped once, but chronological
ring decoding retains the matching sequence-1 RECEIVE/DONE, candidate boundaries
and all panel phase pairs. Event/panel results and marker errors are zero;
stage/clock records remain at 24 MHz without retune triples.

| Work | Previous capture | Smaller reset |
|---|---:|---:|
| Initial workspace reset | 5.185 ms | 1.799 ms |
| Candidate preparation | 61.470 ms | 58.150 ms |
| Runtime receipt to panel completion | 191.427 ms | 188.896 ms |
| Runtime receipt to DONE | 194.735 ms | 192.152 ms |

The reset saves 3.386 ms (about 65%); candidate preparation saves 3.319 ms.
Both captures retain 47 candidate profiling clock reads, one full/four reused
frames and 18 elements drawn. Panel composition remains about 19.52 ms, with
11 elements, black count 2467, 128 dirty rows and identical transfer chunk
sizes. All transfer statuses remain successful.

Panel transfer elapsed time rises from 21.944 to 22.642 ms; a successful marked
joystick read spans 0.441 ms during presentation. This is additional concurrent
work, not proof that all transfer-time variation comes from that read. Therefore
the end-to-end improvement is smaller than the reset saving. Timings include
observer overhead and interrupts and exclude pre-runtime input/wake latency.
Operator confirmation of unchanged visuals and audio remains pending.
