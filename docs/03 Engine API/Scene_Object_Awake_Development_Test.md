# Scene Object Awake Development Test

Status: original and GUI continuity fixtures, plus the scoped V2 timer fixture,
passed awake HW6 checks. An opt-in V2 STOP2/LPBAM development path is now
implemented; the structured fixture passed the scoped sleep/wake and rotation
correction retest recorded below. The original awake helper
still requests the separately validated awake-only mode.
The exact GUI scene-timer fixture from `90b849c` also passed awake timer reveal
and visually observed A/B animation continuity, separately from the OS variant.
This is not normal V2 export, installation, automatic boot, or STOP2 admission.
The OS SFX variant passed audible A/B and long-tone playback plus shell-stop and
silent resume. The exact GUI numbered four-frame timer fixture from `de80153`
also passed the awake hardware check. The current payload is the two-animation
extension of the structured OS variant below. Its combined-animation hardware
check passed as recorded below; GUI's source project is unchanged.

## Dual Animation Test

The single-animation structured pass is checkpointed at
`2fa549f72ca6d88259231598963824a4f658758d`. This next increment changes only the
OS diagnostic payload, helpers, tests and documentation, not the runtime or any
advertised GUI/export capability. Generate it with:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --dual
```

The B/A slots and bottom timer square retain their proven positions and actions.
A ninth object at `(72,44)` adds four outlined cells with one filled cell moving
left-to-right every **800 ms**. The top digits still advance every **400 ms**.
Both loops begin with scene entry; A/B and the timer handler must restart neither.
Every filled indicator cell spans two digit frames. From entry, the pattern is:

| Scene ms | Digit | Indicator cell (left to right) |
|---|---|---|
| 0 | 1 | 1 |
| 400 | 2 | 1 |
| 800 | 3 | 2 |
| 1200 | 4 | 2 |
| 1600 | 1 | 3 |
| 2000 | 2 | 3; bottom square appears |
| 2400 | 3 | 4 |
| 2800 | 4 | 4 |
| 3200 | 1 | 1; combined loop repeats |

The derived common interval is 400 ms, and the combined cycle is 3200 ms:
**8 steps, 16 chunks, 9,344 of 10,512 bytes**. Both A/B states and both timer
visibility states have this payload footprint in native tests. The shared
native-row span of the two clips avoids separate extra row chunks. This is a
specific layout result, not a general promise for two animations at these rates.

Native tests compare the complete replayed payload against awake-rendered frames
over three combined cycles, including rotation initially off. They compare the
compiled schedule against independent live object advancement at partial frames,
loop wrap and large elapsed values. A at 650 ms preserves both clips; B at 950 ms
leaves digit step 2 with 250 ms remaining and indicator step 1 with 650 ms remaining.
Hiding the second clip reduces the visible schedule to four steps. Changing its
duration to 700 ms would require 112 combined steps at 100 ms; schedule building
rejects it without changing live object state. This rejection is native-tested,
not a new ordinary-export build check or a new hardware fallback policy.

Use `__fw0_object_scene_lpbam_enable.gdb` from shell HOME/MENU after boot. Resume,
observe both rates during STOP2, then alternate A/B and let the unit return to
sleep. Neither animation may restart, the marker must move once, and the bottom
square must stay fixed after its reveal. Wake with A/B, halt, and use
`__fw0_object_scene_lpbam_prints.gdb`. The helper additionally prints the slower
object's last projected phase and residual; those are not live DMA state.

The existing `--structured` generator retains the passed single-animation
variant. Existing numbered GUI project files and the original routes/handlers
are unchanged. This does not decide the future authored scene-interval policy.

Local verification: **289 authoring/native tests pass**, target-profile and
whitespace checks pass, and the Debug firmware links. The generated development
egg is 2,680 bytes. RAM remains 451,600 bytes and SRAM4 15,480 bytes; ROM is
862,640 bytes. The new slower-object probe expressions resolve against that ELF.

### Dual Animation Hardware Pass (2026-09-10)

The user confirmed the indicator advanced once per two digit frames during
STOP2, neither animation restarted on A/B, and the marker made no extra movement.
Verdict: **PASS for this dual-animation development fixture**.

The retained dump reports enabled/active/development=1/1/1, no fault, five V2 WFI
returns and five sleep measurements/reconciliations. Missing tick time totals
8,507 ms (latest 1,488 ms), with clock status zero. Wake snapshot, render,
preferred mapping and timeline resume all succeeded, and the sleep barrier is
clear. Five object renders completed with zero status/fault. The timer applied
once, with zero errors and one RTC selection; the bottom square is visible.
The current selected marker is at x=120 (A).

The reported schedule is eight steps at 400 ms. The last runtime snapshot has
digit step 3 and indicator step 1, each with 171 ms remaining, consistent with
their shared next edge. It is not a simultaneous snapshot of the live DMA phase.
Current payload/commit fields were cleared during redraw, so 16 chunks/9,344
bytes remain the native-tested footprint rather than a measurement retained in
this hardware dump. The successful wake/render evidence and visual confirmation
establish more than thread scheduling or a queued request alone.

This does not establish exact 400/800 ms timing, long-duration drift, quantified
current, or arbitrary animation-layout admission. Ordinary V2 export/installation
and GUI capabilities remain unchanged; the 400/700 ms rejection remains native
coverage only.

## Structured Sleep/Wake Retest

Generate the passed single-animation OS diagnostic payload with:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --structured
```

It retains the original 400 ms digit animation, A/B routes and two-second scene
timer, but uses eight scene-owned objects with an unambiguous layout:

- Top: digits at `(72,12)`, the only continuously animated object.
- Middle: fixed 24x24 outlines at `(28,64)` and `(116,64)`, labelled B and A.
  The 16x16 marker starts at `(32,68)` inside B; A moves it to `(120,68)` inside A.
  Repeating the already-selected button has no route and must not move it.
- Bottom: fixed 24x24 outline at `(72,112)`. Its 16x16 square at `(76,116)` appears
  once after two seconds and must never move or disappear on A/B or wake/sleep.

Use the existing `__fw0_object_scene_lpbam_enable.gdb` from the shell, then
`__fw0_object_scene_lpbam_prints.gdb` after observation and wake. The awake helper
also runs this new layout, but retains its deliberate STOP2 block. Current
payload/commit diagnostics can return to NOT_RUN after a redraw; retained wake
results, observed display behavior and low-current residency are separate evidence.

### Rotation Regression and Measurement Record

The first autonomous numbered-fixture run showed visible animation during
low-current STOP2, confirmed by the user. After reconnecting the debugger,
probes showed five WFI returns, six elapsed-time measurements/reconciliations,
12,085 ms of missing tick time, successful wake snapshot/render/map/resume,
one timer dispatch/applied with zero errors, and seven successful object renders.
This is sleep/wake evidence, **not a complete rendering pass**: the user observed
the marker moving twice per press and the nominally fixed timer square shifting.

The frame-copy path called `DisplayRenderer_DrawSceneModel` without enabling
`s_rotate_ccw`, unlike awake `DisplayRenderer_PrepareUIPage`. Rectangle primitives
therefore used panel coordinates in autonomous frames while package sprites
already transformed logical coordinates independently. Frame copying now saves,
enables and restores rotation. A regression comparing complete awake-rendered
frames against copied frames failed before this fix. It now covers both initial
rotation values, full payload replay, both structured-fixture states, timer
visibility and preservation of caller framebuffer/rotation. The previous native
harness initialized rotation to one and consequently missed the defect.

After the correction and structured-variant update, all **286 authoring/native
tests pass**, including exact regeneration of the 2,216-byte embedded diagnostic
egg and continued ordinary-export rejection. The Debug build links with RAM
451,600 bytes, ROM 862,176 bytes and unchanged SRAM4 15,480 bytes. Target-profile
and whitespace checks pass. All 145 distinct probe expressions across the five
affected helpers resolve against the built ELF; this is not a live-read guarantee.
The structured hardware retest is recorded below.

### Structured Hardware Pass (2026-09-10)

The user confirmed the labelled layout was clearer and A/B each caused only one
marker movement, correcting the prior awake/autonomous coordinate jump. Combined
with the preceding explicit confirmation of animation during low-current STOP2,
this is a **PASS for the scoped development fixture**, not production admission.

The retained dump reports enabled/active/development=1/1/1, no fault, nine
successful publications and nine completed object renders, all with zero status.
The program has four steps at 400 ms; its full payload uses eight chunks and
4,672 bytes within the existing limits. Commit status is zero. There are six
V2 WFI returns and six sleep measurements/reconciliations, totaling 9,197 ms of
missing tick time; the latest is 727 ms and clock status is zero. Wake snapshot,
render, preferred-frame mapping and timeline resume all succeeded. Barrier is
clear. The scene timer was selected for RTC wake and applied once with zero
errors; the timer marker is visible and the selected marker is at x=120 (A).

The dump's current ready/prearmed/active=0/0/0 describes the awake post-wake
state, not failure of the preceding autonomous run. Object snapshot phase 0
with 38 ms remaining and retained wake phase 3 with 26 ticks remaining refer to
different instants and must not be compared as simultaneous frames.

Exact 400 ms cadence, first-partial-interval accuracy, long-residency drift,
power consumption in amperes and arbitrary multi-animation scenes remain
unmeasured by this dump. No GUI/service/export capability is enabled by this pass.

An independent debugger connection failure initially returned each preceding
memory read's value. Repeated API reads proved the one-response delay. Reattaching
without reset restored the expected API sequence 82/22/1; all earlier shifted
dumps were discarded. Field resolution against an ELF does not validate live
remote-memory replies. No firmware workaround was added for this debugger fault.

## Delivered Path

An immutable internal-flash development egg goes through full V2 candidate
validation, catalog publication, the bounded object graph, object snapshot
projection, and the existing display-owner renderer and transfer path. Ordinary
V1 package admission still rejects V2. The installed A/B package is not changed.

The first device subset admits a V2 entry scene with continuous interaction,
local input routes, guards, variables, object actions, sparse state overrides,
looping object clips, state/scene one-shot timers, timer controls, ordinary
`PLAY_SFX` and shell exit. Interaction timeout and routes to other scenes are rejected at development
activation, not silently ignored.
The complete egg must fit the existing package-resident limit (64 KiB). This
avoids borrowing runtime assets from the installed package reader. Normal target
object/clip/geometry admission limits still apply.

`thRuntime` owns the graph and animation time. Time advances from elapsed ThreadX
ticks, preserving fractional conversion and frame residuals across local state
changes. The next visible, unmasked animation deadline shortens its queue wait;
hidden or statically masked playback advances analytically without forcing
display updates. Shell suspension pauses scene time; resume does not add the
time spent in the shell. The opt-in path below additionally reconciles STOP2.

`thDisplay` owns retained composition, cached render models and transfers. Runtime
projects a complete immutable model into one leased static buffer, sends the
four-word `(OBJECT_DISPLAY_MAGIC, DISPLAY, request_sequence, scene_activation)`
envelope, and waits for a bounded acknowledgement using the existing owner ACK
timeout. `egDebug` bit 13 belongs to this development display acknowledgement;
bits 14/15 remain package validation/reader acknowledgements. No pointers or
object ownership cross the queue. Display caches the model before presentation.
A consumed request is not proof of successful drawing: the render result and
normal display completion/success must also succeed.

Queue or presentation failures stop the development session and return to shell
error handling. An acknowledgement timeout quarantines the leased buffer until
reset, preventing reuse while a delayed display command could still read it.
There are no automatic launch retries. Launch refusals report an admission mask.

The awake-only development session blocks automatic STOP2 through the existing
runtime-busy admission bit, including while suspended in the shell. A shell-exit
action or reset ends the session and releases that restriction; the current GUI
fixture uses B for a local transition, not shell exit. Do not force manual STOP2 during this
test. No clock profiles, drivers, linker regions, pool sizes or stack settings
are changed. This is not a production always-awake fallback for V2 packages.

## GUI Continuity Fixture

Source: `examples/authoring/native_v2_continuity.peepproj`, imported unchanged
from GUI commit `5c059bfce37770319cb49f09a14246202c42e039`. Only the project
directory was copied, not Studio code or shared service changes.

`tools/authoring/build_object_development.py` now defaults to that project and
generates a separate 1,344-byte development egg in
`ps_object_development_egg_autogen.c`. The normal embedded egg and installed
package remain untouched. The fixture contains:

- An 8x16 scene-owned sprite at `(80,40)`, looping two frames, 500 ms each.
- A separate 16x16 square at `(32,104)`; the second state overrides only X to 120.
- A moves from state 1 to state 2; B returns from state 2 to state 1.
- Neither state overrides the sprite. No shell-exit action is authored.

Generate the original GUI continuity fixture explicitly:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py
```

The builder still accepts `--project <path.peepproj>` and `--output <path.c>`.
This is not a new Studio export capability. The reproducibility test tracks the
current OS SFX variant described below. The original synthetic four-corner fixture remains in
`fixture_bundle()` for native regression coverage, but is no longer the CLI default.

## Previous OS Timer Fixture

The previous development C used the OS-owned `timer_fixture_bundle()`
variant. It copies GUI's project in memory without editing any of its source
files, adds a third square at `(16,16)` and compiles a 1,872-byte egg:

- A/B retain the lower square's state overrides and never restart the sprite.
- `scene_delay`: a scene-entry five-second one-shot moves the top square to
  `(140,16)` in an action-only handler, without changing state activation.
- `state_delay`: three seconds in `marker_right` transitions back to `start`.
  B exits that state earlier and cancels its pending state expiry.
- L resets the top square to X=16 and restarts the five-second scene timer.
- R cancels the scene timer. L/R explicitly re-enter the current state, so they
  also restart its state timer when used in `marker_right`.

Regenerate that earlier variant with:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --timers
```

Without `--timers`, `--sfx` or `--project`, the builder restores the original GUI continuity
fixture. The OS timer variant remains covered by native regression tests.

## Passed GUI Scene-Timer Fixture

Source: `examples/authoring/native_v2_scene_timer.peepproj`, copied unchanged from
GUI commit `90b849c89aa869ce41c33da25f304a883c9e8887`. All five source files were
verified against that commit. `5c059bf` is the older continuity fixture commit,
not the provenance of this new timer project. No GUI implementation was imported.

The preceding checked-in development payload was this project's 1,484-byte egg:

- The original sprite and A/B lower-marker behavior remain unchanged.
- A separate 16x16 square at `(76,80)` starts hidden.
- `reveal_timer` expires once after two seconds of scene time. Its independent
  `reveal_expired` handler makes that square visible without entering a state.
- A/B neither restarts the timer nor hides the square after expiry.
- There is no state timer, L/R timer control, SFX or scene exit in this fixture.

Regenerate that exact GUI payload explicitly:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --project examples/authoring/native_v2_scene_timer.peepproj
```

The real native
runtime/scheduler tests A/B at 650/950 ms, no expiry at 1,990 ms, and dispatch at
2,050 ms without changing state activation or restarting playback (phase 0,
450 ms remaining). The square stays visible through later A/B changes; after
8,000 ms total only one expiry has applied. The final framebuffer is compared
byte-for-byte against the expected sprite and both squares. The awake hardware
reveal and visual continuity pass is recorded below.
Ordinary Studio export and the service capability surface remain unchanged.

Local verification for this exact GUI integration: **269 authoring/native tests
pass**, generated target-profile checks pass, and the full HW6 Debug build links.
RAM is 438,344 bytes, ROM 852,864 bytes and SRAM4 15,480 bytes. Only the development
payload changes in firmware C; no runtime, driver, clock or service code changes.

### Recorded GUI Scene-Timer Hardware Pass (2026-09-10)

The user saw the hidden square appear and subsequently confirmed animation
continuity during A/B changes. The device reported launch status zero,
active/development=1/1, eight completed display requests, zero render/queue/wait
errors and no lease fault. Timer due/dispatch/applied were 1/1/1, ignored/error
were 0/0, and the consumed one-shot was inactive. This records real handler and
display work, not merely a scheduled thread.

The object print helper stopped on an invalid `.effective.visible` expression.
Visibility is stored in bit 0 of `.effective.flags`; the separate GDB read
`p/u (s_ps_object_snapshot.objects[2].effective.flags & 1)` returned 1. This is a
helper defect, not evidence of a runtime failure. The helper now reads that same
flags bit; no firmware rebuild or reflash is required for this correction.
The independent timer helper completed successfully.

Verdict: PASS for awake timer reveal and user-observed A/B animation continuity.
Exact timing, phase residuals and extended no-repeat behavior remain covered by
native tests; a halted snapshot does not establish those timing properties.
No STOP2, production installation or export capability is claimed.

For the next GUI-authored hardware fixture, use four clearly distinct sequential
frames rather than two. The user found a two-frame loop harder to judge for
restarts. Preserve this passed fixture unchanged as regression evidence.

## Timer Integration

V2 uses the existing fixed `thRuntime` timer slots, selection order, pause/resume
and queue-wait deadline calculation. No second scheduler, timer allocation,
hardware timer, thread or queue is introduced. Launch synchronizes timer owners
before the first display transfer. State activation, not rendering revision,
rearms state timers; scene activation controls scene timer lifetime.

The graph stages timer actions with object/variable changes and publishes them
only after commit. Start preserves an active deadline; Restart replaces it;
Cancel removes the pending expiry. Each event's commands are consumed before
selecting another due timer. False guards consume one-shot expiry without retries.
Object time advances to the current runtime tick before a timer handler runs,
then the existing display handoff projects and presents its committed result.

The scheduler refreshes the V2 dispatch tick rather than reusing an older owner
loop timestamp. This avoids rewinding the object clock after intervening input
or display work. Dispatch remains bounded by the existing event-binding budget.
V2 timer execution or presentation failure returns to package-error handling;
a V2 shell-exit effect ends the runtime owner synchronously on `thRuntime`, before
another simultaneous expiry can run. V1's completion path is unchanged.

Shell suspension pauses relative timers and object time; resume preserves their
remaining duration. An expiry already due at suspension remains due after resume.
Exit/recreation cancels old timer owners. These are native-tested semantics,
not a new hardware suspension or STOP2 claim.

## Previous OS SFX Variant

Regenerate the previous 53,612-byte development payload with:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --sfx
```

This copies the passed GUI timer bundle in memory, leaving both GUI source
projects unchanged. It substitutes a 32x32 four-corner animation at `(80,40)`,
250 ms per frame, to make continuity easier to observe. A/B retains the marker
overrides and each matched transition plays an 80 ms tone. The two-second scene
handler reveals the hidden square and plays a six-second tone. L commits two
sound actions in order (short then long); R exits to the shell. L/R are authored
for both states. Both tones use twice the original source amplitude (about +6 dB),
with cue volume and mixer settings unchanged. These are generated test tones,
not a normalization or speaker-loudness qualification.

The initial three-second tone was too close to the START hold duration to prove
early stopping. The user heard A/B tones and uninterrupted long playback, but
shell-stop/silent-resume was initially unverified. The six-second replacement leaves
an audible interval to interrupt: press L, then immediately hold START.

Committed V2 effects feed the existing bounded audio queue in action order.
Failed transactions emit no sound; queue refusal balances outstanding requests
without rolling back the already committed scene. Local state changes do not
stop active audio. No mixer, decoder, clock profile or hardware driver changes
are made by this increment.

Shell suspension, package exit/replacement and installer entry close package
audio admission, discard queued cues and stop active voices on `thAudio` before
releasing their clock and acknowledging completion. `thRuntime` waits for this
bounded FIFO barrier before changing the package lifetime. A failed or late
acknowledgement quarantines audio admission until reset; it cannot authorize
source reuse. Ordinary SFX is discarded regardless of duration. Resume allows
new sounds but does not replay old ones. Resumable music/dialogue is a separate,
deferred capability; see [[Audio_Contract]].

Native tests cover committed input/timer sound delivery, multiple ordered cues,
transaction rollback, queue refusal, animation residuals, real suspend/resume/
exit control flow, queued-cue discard, clock-grant overlap and stop/clock/timeout/
sequence failures. Audio hardware calls are stubbed in these host tests: they
prove orchestration, not audibility, DMA timing or pop-free stopping.

Local verification: **273 authoring/native tests pass** and the HW6 Debug build
links with RAM 438,392 bytes, ROM 906,048 bytes and SRAM4 15,480 bytes. The
four-frame/audio payload is generated separately from normal embedded assets.
No additional SRAM4 allocation is introduced. The subsequent six-second test
passed shell-stop and silent resume as recorded below.

### Previous SFX Hardware Sequence

1. Flash the new Debug ELF. Let normal boot finish and open HOME or the shell
   MENU using START. Leave audio stopped and MSC inactive, then halt.
2. Source `__fw0_object_scene_awake_enable.gdb` and resume. It only queues work
   for `thRuntime`; it does not call target functions from GDB or write storage.
3. Watch the four-corner animation. Alternate A then B: the lower square changes
   sides and short tones play without restarting animation. After two seconds,
   the third square appears and a six-second tone starts. A/B during that tone
   must not stop it.
4. Once audio is quiet, halt and source `__fw0_object_scene_awake_prints.gdb`. Require active V2,
   successful launch/render/queue/wait, no lease fault and visible movement.
   The animation's last projected phase/residual and actual display result are
   reported separately from thread/input counters.
5. Resume, press L, then immediately hold START to open the shell while the long tone plays.
   Sound must stop on shell entry. Once quiet, halt and print: audio fault/status
   must be zero, request must equal complete, and outstanding/clock-held must be
   zero. Resume the target and select the shell's Resume action: animation and
   timers resume, but discarded sound must not return.
6. L must start fresh tones after resuming the package. Press R during playback:
   sound stops and the package exits. Unlike temporary suspension, exit releases
   the development session's automatic STOP2 restriction.

Do not halt during playback: debugger interruption breaks audio timing and would
invalidate that observation. Report audible artifacts separately from counters.

At this SFX checkpoint, the two object helpers required development/scene/audio-package APIs `3/22/1` and used device-resident
results, not GDB convenience-variable history. The OS sprite asset IDs cycle
65537..65540. State IDs 1/2 select marker X=32/120, Y=104. Automatic STOP2 entry
count must not advance while the development scene is active. This awake test is
not a power measurement; halt/resume also cannot establish real-time cadence.
`__fw0_state_scene_timer_prints.gdb` supplies detailed timer counters and owner
activations. Timer counters are cumulative; compare changes, not an assumed
zero baseline from a previously running installed package. No RTC selection is
required for this explicitly awake-only test.

### Recorded SFX Shell Pass

Checkpoint `521f0b1cedabadf70db9b8183f78f74eff6a0a00`: the user confirmed
the six-second tone stopped on shell entry and did not replay on Resume.
The device reported audio request/complete=4/4, send/wait/owner/status=0,
fault=0, outstanding/clock-held=0 and zero underruns. Admission was reopened
(`blocked=0`) after returning to the active development scene. Voice peak=2
and 41,216 decoded samples show real audio work, not just command delivery.
Earlier user observations confirmed short A/B tones and continued long playback.

Display request/complete=47/46 with result=NOT_RUN was captured mid-render;
it does not establish a rendering failure or successful completion of request 47.
This pass does not qualify all replacement, timeout or queue-discard cases.

## Current GUI Numbered Timer Fixture

Source: `examples/authoring/native_v2_timer_four_frames.peepproj`, imported
unchanged from `de80153ca256937612248aef36f6fd3e45a9d39c`. All four files
match their commit blob hashes. No Studio implementation was imported.

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --project examples/authoring/native_v2_timer_four_frames.peepproj
```

The checked-in development egg is now 1,908 bytes. Its 24x24 sprite at `(72,40)`
displays digits 1,2,3,4, each for 400 ms. A/B moves the independent marker
between X=32/120 at Y=104. The two-second scene timer reveals the square at
`(76,80)` once, with no target state. At exact expiry digit 2 is due, rather
than a reset to digit 1. There are no SFX, L/R controls or exit actions.

The native harness checks five complete framebuffer images: all four digits
with interleaved A/B changes, then digit 2 with the revealed timer marker.
It checks residual frame times, no premature expiry, unchanged scene/state
activation during the handler, and no repeated expiry after later A/B changes.
The SFX and earlier timer variants retain their native regression tests.

Local verification: 274 authoring/native tests pass, target-profile generation
is current, and the Debug firmware builds. Linker usage is RAM 438,392 bytes,
ROM 854,344 bytes and SRAM4 15,480 bytes. Only the generated development payload
changes in firmware C; production runtime and driver code are unchanged.

Hardware sequence:
1. Flash the Debug ELF, finish boot, hold START to open the shell MENU and halt.
2. Source `__fw0_object_scene_awake_enable.gdb` and resume.
3. While digits 2, 3 or 4 are visible, alternate A/B. The lower square moves
   without the digits jumping back to 1. The timer square appears after two
   seconds and stays visible through further state changes.
4. Halt and source `__fw0_object_scene_awake_prints.gdb`. An in-flight display
   request is not a completed-render result; combine counters with observation.

The helpers now describe this exact fixture. APIs remain `3/22/1`. Hold START
for the shell; reset ends the development session. Automatic STOP2 remains
blocked, even during shell suspension. Normal V2 export remains unavailable.
Hardware verification of this exact numbered fixture passed at checkpoint
`6caa8fe4ed16ece6c9d74cee2c8750dd8277ab4e`. The user confirmed that the clearer
numbered animation does not reset when A/B changes the lower square. The timer
applied once with zero errors and its square was visible at `(76,80)`.
Display request/complete=27/26 with result=NOT_RUN and success=0 was captured
inside framebuffer hashing, before render completion; it is not evidence of a
failed render. The single cumulative STOP2 entry is not evidence of V2 sleep.

## Low-Power Preparation Increment

The pure V2 waiting-program compiler is now implemented and tested against the
live object engine and production raster functions. It preserves the current
partial frame interval, composes complete scene models including overlays,
supports exact unequal durations and combined loops, and rejects schedules
over the existing 12-step limit. Hidden or statically masked clips require only
a held frame. The original object bank is never advanced by preparation.

Verification: 279 authoring/native tests pass, the generated target profile is
current, and the full Debug build links with unchanged RAM/ROM/SRAM4 usage.
Tests include exact pixel comparisons against the live engine at partial-frame
boundaries, loop wrap and elapsed values up to UINT64_MAX. Suspended sources,
backwards time, invalid projection steps and arithmetic/capacity failures are
rejected. Failure leaves the program unavailable rather than retaining a stale
step count.

That isolated checkpoint (`4f791fb3bdaed1c6f7bf25d0b2d6cc9897edc2f8`) did not
call the compiler from the active runtime. The following increment adds an
explicit owner-routed payload preparation check; neither increment enables
autonomous V2 playback.

### Display-Owner Payload Preparation (2026-09-10)

The one-shot request builds an immutable schedule in `thRuntime` and includes
it in the existing bounded display lease. `thDisplay` presents the matching
scene and composes complete, layer-ordered scene frames into the existing
LPBAM row/payload compiler. Off-screen composition restores the committed
framebuffer and does not change dirty tracking. The owner rejects a mismatched
scene model or an already compiled/prearmed/active autonomous program before
touching shared payloads. A schedule/payload rejection does not fail the awake
scene. No guaranteed frame-dropping fallback is used for V2.

This is preparation only: no queue build/selection, readiness publication,
LPTIM configuration, DMA start or STOP2 policy change. The captured first
remaining interval is reported, not yet applied to hardware. Ordinary V2 egg
export/install/boot remain disabled, and the numbered source/payload is unchanged.

Native tests replay actual packed row transactions through three loops and
compare each resulting framebuffer against the full-scene renderer. They also
check overlap, hidden animation/hold, capacity rejection, stale model rejection,
active/prearmed/compiled guards, source/framebuffer preservation and display
lease timeout immutability. The exact numbered fixture at 650 ms uses 4 steps,
8 transactions and 4672/10512 payload bytes, with quantum/residual 400/150 ms.
All 283 authoring/native tests pass. Debug build: RAM 444088 B, ROM 856544 B,
SRAM4 unchanged at 15480/16384 B. Physical preparation evidence is pending.

Reflash, boot into the shell, then launch the unchanged fixture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_awake_enable.gdb
```

Resume until the numbered scene is visible, halt, then queue one preparation:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_lpbam_prepare_enable.gdb
```

Resume for one second, halt and print:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_lpbam_prepare_prints.gdb
```

Require request=complete>0, schedule/payload/reason=0/0/0, steps/quantum=4/400,
composed=5 (includes the wrap frame), sequence=4, chunks<=18 and used<=capacity.
The initial residual is 1..400 ms depending on when capture occurred.
Ready/prearmed/active must stay zero. Repeat after moving the lower square with
A/B; awake input and animation must remain normal. A completed queue request
alone is not evidence of packing; check payload status, frame count and budgets.
Successful packing is not proof of autonomous playback or timing. Do not force
manual STOP2. Next: timing-aware handoff and wake reconciliation before events.

## Verification and Next Work

### Opt-In Autonomous Development Test

The preparation-only hardware pass is committed at
`18217f020b25704da59aeac28a70127ff01b1d46`: 4 steps at 400 ms, 140 ms initial
residual, 5 compositions including wrap, 8 chunks and 4672/10512 payload bytes.
Ready/prearmed/active were all zero. The user also confirmed A/B did not disturb
awake cadence after packing. This is packing evidence, not autonomous evidence.

The next path uses development request **2**, leaving request **1** awake-only.
Flash the new Debug ELF, finish boot, open the shell MENU with **HOLD START**,
then halt and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_lpbam_enable.gdb
```

Resume normally. Leave the controls released and watch 1,2,3,4 at 400 ms/frame.
The third square should appear once after two seconds. After that, use A/B to
wake and change the lower square, then release everything and allow sleep again.
The digits must continue, and each packed scene must retain the updated square.
HOLD START opens the shell; suspension still blocks sleep in this development
increment and pauses scene time. No installed package, GUI source, service API,
ordinary export capability or SRAM4 allocation changes.

After observing, wake with A/B, halt and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_lpbam_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_state_scene_timer_prints.gdb
```

If STOP2 disconnects the debugger, reconnect without reset/reflash to preserve
the retained probes. Do not use forced manual STOP2 or breakpoints at entry.
Debug-in-low-power affects residency/current measurements; confirm normal-run
low-current residency separately. A WFI return count alone does not establish
sustained STOP2 or autonomous panel output.

Implementation boundaries:

- Runtime publishes a leased immutable complete-scene schedule; display copies
  it into owner-private storage. A full-scene composer has no cursor, focus or
  four-phase-per-element prerequisite. It preserves all scene layers/overlays.
- Display runs the awake schedule and compiles the existing bounded payload and
  queue path before handoff. Runtime no longer wakes for each animation frame.
  Runtime work in progress blocks sleep; `egDebug` bit 11 is the bounded wake
  recovery notification. Input/timer handling waits for display recovery.
- V2 does not use the legacy three-step truncation or held-frame fallback.
  Rejected schedules remain awake and report failure. This opt-in animated test
  requires a nonzero quantum representable in ThreadX ticks; static HOLD-only
  programs and non-tick-representable quanta are not admitted by this increment.
- At commit, LPTIM1 ARR represents the repeating interval; CCR1 sets the offset
  to the next authored edge. Writes are acknowledged/read back. An expired
  deadline or a tick crossing during programming rejects that attempt instead
  of silently starting a new full interval. Wake residual uses the compare edge,
  not just distance to ARR. No periodic CPU animation-wake mechanism is added.
- `thPower` measures calendar RTC elapsed time around the stopped ThreadX tick,
  including midnight/month/year rollover. It publishes only missing elapsed
  milliseconds; `thRuntime` applies them once before further object work.
  Existing earliest-deadline RTC selection still wakes for scene/state timers.
  RTC failure/backwards time or a single sleep exceeding `UINT32_MAX` ms faults
  this development session rather than guessing elapsed time. Shell time pauses.
- Runtime scheduling retains its 10 ms tick granularity; RTC reads and LPTIM
  synchronization introduce their own quantization. This is not a sub-millisecond
  phase accuracy claim. Long-residency RTC/LPTIM drift remains a hardware check.

Hardware basis: ST's [AN4865](https://www.st.com.cn/resource/en/application_note/an4865-lowpower-timer-lptim-applicative-use-cases-on-stm32-microcontrollers-stmicroelectronics.pdf)
describes independent ARR period and CCRx PWM duty/edge control for STM32U5's
type-3 LPTIM, and autonomous PWM in STOP. The partial-edge implementation still
requires target timing evidence; a host register model cannot provide it.

Native coverage includes complete numbered frames through the full-scene
adapter and actual payload compiler, refusal of truncated fallback, partial
first interval and repeat period, late/write-failed commit, tick wrap, calendar
rollover, exactly-once bank time reconciliation before A/B, and RTC failure.
The structured hardware pass and its measurement limits are recorded above.
Ordinary V2 export remains disabled.

Local verification for this increment: **285 authoring/native tests pass**, the
target-profile check passes, and the full HW6 Debug firmware links. All 60
distinct probe expressions in the new launch/print helpers resolve against that
ELF. RAM usage is 451,600 bytes, ROM 861,848 bytes, and SRAM4 remains 15,480 bytes.
The build retains two pre-existing unused-function warnings in owner state
machines; there are no new warnings from this increment.

## Earlier Awake Verification

Native tests exercise the real C V2 loader, activation, graph, snapshot projector
and production primitive/sprite raster functions. Seven complete framebuffer
outputs are compared byte-for-byte with the expected fixture. Tests check a
state transition at 375 ms preserves phase 1 and its 125 ms residual, independent
marker changes, hidden/static-mask clock continuity, shell exit/re-entry,
ordinary V2 admission rejection and unsupported development-feature rejection.
The extracted real RTOS handoff is tested for queue failure, render failure,
mismatched acknowledgement and timeout quarantine.
The extracted runtime launch service also checks successful UI launch publication,
continued animation scheduling, admission rejection and load/render failure cleanup.

The first hardware attempt drew one frame then showed PKG ERROR. The development
launch incorrectly sent LAUNCH_RUNTIME through the restricted UI lifecycle helper,
which rejected it and triggered package-error cleanup. Launch now publishes the
existing UI router runtime-launch request after a successful first render. The
new native launch test reproduced the failure before this correction and passes
afterward.

### Recorded Hardware Pass (2026-09-10)

The original four-corner fixture was retested after the launch fix. The user
confirmed visible animation, A moving the independent marker without interrupting
that animation, and B exiting to the shell. The dump showed launch/render/queue/
wait status zero, no lease fault, 51/51 display requests consumed successfully,
active/development=1/1 and UI/class/lifecycle=6/2/2. STOP2 entries stayed zero,
as required for this awake-only test. This pass is checkpointed in
`013f4f8ae2ae7dcf3b4d56e6288b8a4a49270ba5`.

The GUI fixture now also has native coverage through the real C runtime and
production raster functions. Seven complete framebuffer outputs are checked.
At 650 ms A preserves frame 1 with 350 ms remaining; after another 300 ms B
preserves frame 1 with 50 ms remaining. Subsequent advances cross the loop
boundary without restarting. Underlying marker X stays 32 while effective X
changes to 120 and back.

### Recorded GUI Fixture Pass (2026-09-10)

GUI source commit: `5c059bfce37770319cb49f09a14246202c42e039`; OS integration
checkpoint: `92b03564ca2a1ef4675cc0a49c20b5cb9e5be1a9`. The user reported the
two-frame sprite running, A/B moving the separate square, and no apparent
animation restart. The dump showed active/development=1/1, successful launch,
30/30 completed display requests, zero render/queue/wait errors or lease faults,
seven applied state transitions and UI/class/lifecycle=6/2/2. The marker's final
position was `(120,104)`; projected elapsed time was 11,000 ms.

Verdict: PASS for awake rendering and A/B object overrides with visually observed
continuity. Exact phase/residual preservation is covered by native tests, not
established by one halted snapshot. STOP2 entries were cumulatively four; without
an activation baseline that does not prove or disprove entry during the fixture.

### Native Timer Verification

The real compiled V2 egg, runtime graph, extracted RTOS scheduler/completion and
production raster functions run together in the host harness. Checks cover:
scene expiry during state changes, action-only state-activation preservation,
state cancellation/re-entry, Start/Restart/Cancel, false guards, ordered
simultaneous cancellation, shell-exit invalidation, rendering failure cleanup,
scene recreation, pause/resume (including an already overdue timer), object time
before dispatch and exact final framebuffer pixels. Existing V1 scheduler tests
remain passing.

### Recorded Awake Timer Pass (2026-09-10)

The user confirmed all described fixture behaviors: A/B moved the lower square
without interrupting the animation, the five-second scene expiry moved the top
square despite local state changes, the three-second state expiry returned the
lower square left, and L/R restarted/cancelled the scene timer.

The device reported successful launch, active/development=1/1, 50/50 completed
display requests, zero render/queue/wait errors or lease faults, and
UI/class/lifecycle=6/2/2. Timer due/dispatch/applied were 2/2/2, with ignored/error
0/0: real handlers ran, not merely the runtime thread. Seven matched transactions
and six state changes are consistent with one action-only scene handler. The
final top square was `(16,16)` and configured/active timers were 1/0, consistent
with the reported reset/cancel test. Scene/state activations were 2/8.

Verdict: PASS for the described awake timer fixture, including state/scene
expiry, visually observed animation continuity, Restart and Cancel. Exact
wall-clock durations and phase residuals are native-tested, not established by
this halted snapshot. Start on idle/active, suspension/resume and broader failure
cases remain native-only coverage. Pause/resume counters 1/0 do not establish a
V2 suspension/resume pass. STOP2 entries were cumulatively one, with no fixture
entry baseline; no STOP2 or power-measurement claim is made. RTC selection was
zero, as expected for the awake-only path.

At the OS timer checkpoint, local verification was **268 authoring/native tests pass**, target-profile generated
files are current, and the full HW6 Debug firmware links successfully. Linker
usage is RAM 438,344 bytes, ROM 853,248 bytes and SRAM4 15,480 bytes; no new SRAM4
objects are introduced. The new explicit path uses 4,180 bytes of static state
plus normal alignment. Existing scene descriptor slots are reused.

The earlier awake-only ARM Debug stack-usage reports gave local frames of 32 bytes for development
activation, 16 for snapshot projection entry, 48 for the projector, 48 for the
display handoff requester, 72 for tick conversion, 24 for the service and 32 for
the display-owner model copy wrapper. These are compiler local-frame sizes, not
measured whole-call-chain high-water marks. Target stack and timing evidence
remain outstanding.

Full firmware build and native checks are necessary but do not prove panel
animation, input responsiveness or DMA timing. Record the hardware result before
claiming additional behavior complete. Next are the SFX hardware check, production
admission and STOP2/LPBAM continuity; ordinary export remains blocked until the
advertised firmware capability actually supports the package being built.
