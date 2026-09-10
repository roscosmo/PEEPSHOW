# Scene Object Awake Development Test

Status: implemented for explicit HW6 development launch; hardware proof pending.
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
runtime-busy admission bit, including while suspended in the shell. B exit ends
the session and releases that restriction. Do not force manual STOP2 during this
test. No clock profiles, drivers, linker regions, pool sizes or stack settings
are changed. This is not a production always-awake fallback for V2 packages.

## Default Fixture

`tools/authoring/build_object_development.py` creates a separate 1,824-byte V2
proof egg and `ps_object_development_egg_autogen.c`. It does not modify the legacy
example project or the normal embedded egg. The generated fixture contains:

- A 32x32 four-frame sprite at `(68,28)`, with a 250 ms frame duration. A small
  square moves around its four corners.
- A separate 16x16 lower marker, whose base position is `(24,100)`.
- Two states. The second state overrides only the marker's X coordinate to 128.
- A toggles states. B returns to the shell. Neither state controls the sprite.

Regenerate the default fixture before building if its generator changes:

```powershell
tools/.venv/Scripts/python.exe tools/authoring/build_object_development.py
```

The same development builder accepts `--project <path.peepproj>` for the GUI
fixture handoff. OS should review that fixture against the subset above, generate
it explicitly and rebuild. This is not a new Studio export capability. The
checked-in default-fixture reproducibility test intentionally tracks the default;
integrating a different checked-in hardware fixture also requires updating that
test's expected fixture. `--output` can instead write a separate development C
artifact without replacing the checked-in proof.

## Hardware Sequence

1. Flash the new Debug ELF. Let normal boot finish and open HOME or the shell
   MENU using START. Leave audio stopped and MSC inactive, then halt.
2. Source `__fw0_object_scene_awake_enable.gdb` and resume. It only queues work
   for `thRuntime`; it does not call target functions from GDB or write storage.
3. Watch the four-corner animation. Press A several times at different points in
   its cycle. The lower marker must change sides without restarting the sprite.
4. Halt once and source `__fw0_object_scene_awake_prints.gdb`. Require active V2,
   successful launch/render/queue/wait, no lease fault and visible movement.
   The animation's last projected phase/residual and actual display result are
   reported separately from thread/input counters.
5. Resume and press B. The shell must work again. Optionally repeat launch and
   check START/shell resume pauses the animation rather than restarting it.

These two new helpers require development/scene APIs `1/22` and use device-resident
results, not GDB convenience-variable history. The default sprite asset IDs cycle
65537..65540. State IDs 1/2 select marker X=24/128, Y=100. Automatic STOP2 entry
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
afterward. Hardware retest remains pending.

Local verification: **260 authoring/native tests pass**, target-profile generated
files are current, and the full HW6 Debug firmware links successfully. Linker
usage is RAM 438,344 bytes, ROM 852,904 bytes and SRAM4 15,480 bytes; no new SRAM4
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
