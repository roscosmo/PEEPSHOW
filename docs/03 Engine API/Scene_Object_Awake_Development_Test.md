# Scene Object Awake Development Test

Status: original and GUI continuity fixtures, plus the scoped V2 timer fixture,
passed awake HW6 checks. V2 STOP2/LPBAM remains unimplemented.
This is not normal V2 export, installation, automatic boot, or STOP2 admission.

## Delivered Path

An immutable internal-flash development egg goes through full V2 candidate
validation, catalog publication, the bounded object graph, object snapshot
projection, and the existing display-owner renderer and transfer path. Ordinary
V1 package admission still rejects V2. The installed A/B package is not changed.

The first device subset admits a V2 entry scene with continuous interaction,
local input routes, guards, variables, object actions, sparse state overrides,
looping object clips, state/scene one-shot timers, timer controls and shell exit.
SFX, interaction timeout and routes to other scenes are rejected at development
activation, not silently ignored.
The complete egg must fit the existing package-resident limit (64 KiB). This
avoids borrowing runtime assets from the installed package reader. Normal target
object/clip/geometry admission limits still apply.

`thRuntime` owns the graph and animation time. Time advances from elapsed ThreadX
ticks, preserving fractional conversion and frame residuals across local state
changes. The next visible, unmasked animation deadline shortens its queue wait;
hidden or statically masked playback advances analytically without forcing
display updates. Shell suspension pauses scene time; resume does not add the
time spent in the shell. This timing increment does not yet reconcile STOP2.

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

The explicit development session blocks automatic STOP2 through the existing
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
checked-in GUI source. The original synthetic four-corner fixture remains in
`fixture_bundle()` for native regression coverage, but is no longer the CLI default.

## Current Timer Fixture

The checked-in development C now uses the OS-owned `timer_fixture_bundle()`
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

Regenerate the current checked-in development C with:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py --timers
```

Without `--timers`, the builder restores the original GUI continuity fixture.
The generated-C reproducibility test now checks the timer variant. Ordinary
Studio export and the authoring service capability surface are unchanged.

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

## Hardware Sequence

1. Flash the new Debug ELF. Let normal boot finish and open HOME or the shell
   MENU using START. Leave audio stopped and MSC inactive, then halt.
2. Source `__fw0_object_scene_awake_enable.gdb` and resume. It only queues work
   for `thRuntime`; it does not call target functions from GDB or write storage.
3. Watch the small two-frame sprite. Alternate A then B for the first five seconds.
   The lower square changes sides without restarting the sprite. The top square
   moves right once after five seconds despite those state changes.
4. Halt once and source `__fw0_object_scene_awake_prints.gdb`. Require active V2,
   successful launch/render/queue/wait, no lease fault and visible movement.
   The animation's last projected phase/residual and actual display result are
   reported separately from thread/input counters.
5. Resume, press A and leave it in that state. After three seconds the lower
   square returns left. The sprite and top square must not reset.
6. L resets the top square and restarts its timer. R before expiry cancels it;
   the square stays left after five seconds. L again allows a new expiry.
7. START opens the shell; B is a local state transition in this fixture. Reset
   ends the development session. Shell suspension keeps automatic STOP2 blocked.

The two object helpers require development/scene APIs `2/22` and use device-resident
results, not GDB convenience-variable history. The GUI sprite asset IDs cycle
65537..65538. State IDs 1/2 select marker X=32/120, Y=104. Automatic STOP2 entry
count must not advance while the development scene is active. This awake test is
not a power measurement; halt/resume also cannot establish real-time cadence.
`__fw0_state_scene_timer_prints.gdb` supplies detailed timer counters and owner
activations. Timer counters are cumulative; compare changes, not an assumed
zero baseline from a previously running installed package. No RTC selection is
required for this explicitly awake-only test.

## Verification and Next Work

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

Local verification: **268 authoring/native tests pass**, target-profile generated
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
claiming additional behavior complete. Next are V2 effects, production
admission and STOP2/LPBAM continuity; ordinary export remains blocked until the
advertised firmware capability actually supports the package being built.
