# Scene Object Awake Development Test

Status: original awake fixture passed on HW6; GUI-authored fixture hardware test pending.
This is not normal V2 export, installation, automatic boot, or STOP2 admission.

## Delivered Path

An immutable internal-flash development egg goes through full V2 candidate
validation, catalog publication, the bounded object graph, object snapshot
projection, and the existing display-owner renderer and transfer path. Ordinary
V1 package admission still rejects V2. The installed A/B package is not changed.

The first device subset admits a V2 entry scene with continuous interaction,
local input routes, guards, variables, object actions, sparse state overrides,
looping object clips and shell exit. Timers, SFX, interaction timeout and routes
to other scenes are rejected at development activation, not silently ignored.
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

## Current GUI Fixture

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

Regenerate the checked-in fixture before building if its source changes:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py
```

The builder still accepts `--project <path.peepproj>` and `--output <path.c>`.
This is not a new Studio export capability. The reproducibility test tracks the
checked-in GUI source. The original synthetic four-corner fixture remains in
`fixture_bundle()` for native regression coverage, but is no longer the CLI default.

## Hardware Sequence

1. Flash the new Debug ELF. Let normal boot finish and open HOME or the shell
   MENU using START. Leave audio stopped and MSC inactive, then halt.
2. Source `__fw0_object_scene_awake_enable.gdb` and resume. It only queues work
   for `thRuntime`; it does not call target functions from GDB or write storage.
3. Watch the small two-frame sprite. Alternate A then B at different points in
   its cycle. The lower square must change sides without restarting the sprite.
4. Halt once and source `__fw0_object_scene_awake_prints.gdb`. Require active V2,
   successful launch/render/queue/wait, no lease fault and visible movement.
   The animation's last projected phase/residual and actual display result are
   reported separately from thread/input counters.
5. START opens the shell; B is a local state transition in this fixture. Reset
   ends the development session. Shell suspension keeps automatic STOP2 blocked.

These two new helpers require development/scene APIs `1/22` and use device-resident
results, not GDB convenience-variable history. The GUI sprite asset IDs cycle
65537..65538. State IDs 1/2 select marker X=32/120, Y=104. Automatic STOP2 entry
count must not advance while the development scene is active. This awake test is
not a power measurement; halt/resume also cannot establish real-time cadence.

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
changes to 120 and back. GUI-fixture hardware verification remains pending.

Local verification: **261 authoring/native tests pass**, target-profile generated
files are current, and the full HW6 Debug firmware links successfully. Linker
usage is RAM 438,344 bytes, ROM 852,424 bytes and SRAM4 15,480 bytes; no new SRAM4
objects are introduced. The new explicit path uses 4,180 bytes of static state
plus normal alignment. Existing scene descriptor slots are reused.

ARM Debug stack-usage reports give local frames of 32 bytes for development
activation, 16 for snapshot projection entry, 48 for the projector, 48 for the
display handoff requester, 72 for tick conversion, 24 for the service and 32 for
the display-owner model copy wrapper. These are compiler local-frame sizes, not
measured whole-call-chain high-water marks. Target stack and timing evidence
remain outstanding.

Full firmware build and native checks are necessary but do not prove panel
animation, input responsiveness or DMA timing. Record the hardware result before
claiming this slice complete. Next are V2 scene timers/effects, production
admission and STOP2/LPBAM continuity; ordinary export remains blocked until the
advertised firmware capability actually supports the package being built.
