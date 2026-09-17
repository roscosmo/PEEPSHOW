# Studio V2 Scene Connections Hardware Test

Status (2026-09-17): installed navigation/rendering and static held-frame STOP2
functional PASS after the fix. The shortened quiet battery wake also passed on
this fixture. Animated HOME/AWAY hardware regression also passed on the same
firmware. Current measurement remains pending. Public multi-scene export remains
disabled pending shared tooling/capability work and remaining timer acceptance.

## Hardware Result (2026-09-17)

The user confirmed the requested visible behavior, including Lobby/Garden
round trips and reboot entry. Installed source/model/bytes=3/2/2228, slot 0,
generation 12, scene count 2. The retained capture ends in Garden (scene 1).
Replacement attempts/failures/status=7/0/0; admission token/completion/lease/
status=37/37/0/0; display request/completion/result/fault=27/27/0/0. The user's
observation plus completed work supports the navigation/rendering pass, not
merely scheduled threads. The capture is not a separate post-reset record.

Static admission succeeds with one chunk and 584 bytes, but waiting publication
status is 1 with steps/quantum=1/0 and WFI/measured/reconciled=0/0/0. No current
measurement was supplied. The timer counters are cumulative from earlier content;
this fixture contains no timers.

Code inspection identified the static idle blocker in the tested build:
`PS_HW6_DisplayOwner_PublishDevelopmentWaiting` in `ps_hw6_owner_services.c`
rejects `program->quantum_ms == 0`, and the object display dispatch stores that
HAL_ERROR in `g_ps_object_lpbam_probe.publish_status`.
`PS_HW6_RTOS_Stop2AutoDynamicBlockerMask` in `ps_hw6_rtos_probe.c` marks the
runtime busy when that publication status is nonzero. Successful raster
admission therefore does not imply this static scene can enter STOP2. Static
idle must not be marked accepted from that capture or concealed by adding a
dummy authored animation.

## Static Idle Fix And Retest

The display owner now accepts the canonical HOLD schedule (one step, zero
quantum/remaining interval and no visible animated object) only when its
projection matches the current display model. It clears the old animation
publication and deadline; it does not start DMA or manufacture a cadence.
Power selects the existing HELD_FRAME backend only for a successfully published,
current-activation program whose matching render and schedule have completed.
Existing held-frame readiness, input/owner/queue blockers, runtime wake barrier,
EXTCOMIN maintenance and battery/scene RTC deadline arbitration remain intact.
No clock settings, battery policy, probe layouts or resource limits change.

39 focused tests pass: static/hidden HOLD, animated-to-hidden-to-animated phase
continuity through RTC-reconciled sleep, stale/failed render rejection, animated
LPBAM payloads, scene replacement, timers, battery wake/shutdown/quiesce and
package workflow. Debug build passes with RAM 553632, ROM 884672 and SRAM4
15480 bytes. Runtime stack analysis remains 2528 bytes including the 512-byte
reserve, not measured high-water usage. Native tests do not prove physical
sleep or current consumption.

Retest using the NEW normal Debug firmware. Leave the already installed
2228-byte Lobby/Garden egg in place; no USB reinstall is necessary. In each
scene, release controls for several seconds, then wake with its routed button
(A in Lobby, B in Garden). Check the correct screen, shell/Resume and reboot.
Capture the updated installed print after a settled wake. Require publication
status zero, backend requested/selected/status=1/1/0, held-ready=1 and increasing
WFI/measured counts with eventual reconciliation after runtime work. Zero
quantum is correct. No measured-current claim is possible on this battery unit.

After static idle works, the existing `__fw0_battery_wake_enable.gdb` may shorten
one safety deadline to 15 seconds on the settled Lobby/Garden scene. Its older
instruction names the animated fixture; the request itself changes only the
deadline. Resume untouched for 20 seconds, then wake normally and capture
`__fw0_battery_wake_prints.gdb`: require positive expiry and successful-due-read
deltas and unchanged visuals. Do not lower battery voltage for this test.
Finally regress the installed HOME/AWAY egg's animation and timer exits on the
same firmware. That animated regression passed as recorded below.

### Static Hardware Retest Result (2026-09-17)

The user reported that debugger pause did not respond until a button wake. That
observation alone is not sleep proof; the retained capture additionally records
four automatic STOP2 entries with zero blockers/status, four WFI returns and
four measured/reconciled sleep intervals with clock status zero. Backend
requested/selected/status/held-ready=1/1/0/1 confirms HELD_FRAME, not LPBAM.
Publication status is zero with the valid static schedule steps/quantum=1/0.

The same installed 2228-byte egg remains at generation 12. Two scene replacements
completed without failure, ending in Lobby; admission token/completion=4/4,
lease/status=0/0; display request/completion=3/3, result/fault=0/0. Timer counters
are zero as expected for this fixture. This closes functional static sleep and
button-driven scene replacement; no measured current is claimed on the battery
unit. Repeated diagnostic lines were duplicate print statements only and were
removed without changing firmware or requiring another reflash.

### Quiet Battery Wake Result (2026-09-17)

On firmware checkpoint `15164d0`, the shortened deadline test recorded arms=1,
request=0, expiry delta=1 and successful-due-read delta=1. RTC expiry/due-wake
counts were 1/1; battery reading 4041 mV, valid=1, snapshot status=0 and clock
failures=0. This proves a real successful battery read following the safety
deadline, not merely a scheduled thread or selected RTC source.

STOP2/WFI/measured/reconciled counts were 9/9/9/9. The next sleep retained a
179973-tick battery deadline and recorded 6596 elapsed ticks, consistent with
returning to sleep after the successful reading refreshed the normal deadline.
HELD_FRAME requested/selected/status/ready remained 1/1/0/1, publication=0.
Five scene replacements completed without failure; display completion=6/6,
status/fault=0/0. The current automatic blocker 0x800 is INPUT_PENDING after
the user's button wake; entry status NOT_RUN is not a completed-entry failure.

Acquisition attempts/successes/failures=13/12/1 are cumulative since boot.
No before-test failure count was supplied, so that failure cannot be dated to
this test and is not erased or presented as zero failures. The explicit test
deltas prove the successful due read. No new visual observation, current
measurement, real 30-minute interval qualification or low-voltage shipment
test is claimed from these prints. No battery policy was changed.

### Animated Regression Result (2026-09-17)

The user reinstalled the 3440-byte HOME/AWAY egg without reflashing and confirmed
the requested animation continuity, scene changes and timed return behavior.
Installed source/model/bytes=3/2/3440, slot 0 generation 14. Replacement
attempts/failures/status=10/0/0 are cumulative, not ten new transitions proven by
this single capture. Admission token/completion/lease/status=22/22/0/0 and
display request/completion/result/fault=16/16/0/0 confirm completed work.

LPBAM publication/fault=0/0, schedule=4 steps at 400 ms, payload=8 chunks/4672
bytes. WFI/measured/reconciled=5/5/5 with clock status zero. Timer due/applied/
error/RTC selections=3/3/0/3. This closes the animated functional regression
after adding static held-frame support; no current measurement was supplied.

Backend requested/selected remains 2/2 (LPBAM). Its current readiness status=1
after wake/redraw is not a failed completed sleep; readiness must be prepared
again before the next animated handoff. The current blocker is INPUT_PENDING
(0x800), entry status NOT_RUN. Total automatic entry count 19 is cumulative
across package activity and is not the new package's WFI count.

## Source And Artifact

- Source: `G:/PEEPSHOW-PeepStudio/examples/authoring/native_v2_scene_connections.peepproj`.
- GUI handoff HEAD: `dee16e47c034f326b99e829efd12c9eee86ac77e`.
- Fixture commit: `0e931537446d937508dceb744536d5295430be02`.
- Fixture directory diff against that commit was empty. Unrelated GUI timer
  edits were neither read as fixture input nor modified.
- Firmware baseline: `d7b3769`, already used for the installed HOME/AWAY pass.

Generated using the existing explicit development encoder, with no source
transformation, service capability change or linked firmware fixture change:

```powershell
python tools/authoring/build_object_development.py --project G:/PEEPSHOW-PeepStudio/examples/authoring/native_v2_scene_connections.peepproj --egg-output firmware/peepshow_hw6_fw0/build/Debug/native_v2_scene_connections.egg
```

Artifact: `firmware/peepshow_hw6_fw0/build/Debug/native_v2_scene_connections.egg`,
2228 bytes. Whole-file SHA-256:
`de9e392094298c5ab85567449ce5b74960d903fff8ef4a1dcb88027e19aafecc`.

The existing native replacement harness, in installed `selection` mode, passed
normal preflight and installed entry on these exact bytes, including both
scenes' private raster/payload admission. Manifest entry is Lobby (`main`, scene
2), not the first sorted scene (`garden`, scene 1). Both use state 1 (`start`).
The harness uses host HASH/clock/queue substitutes; this is not physical flash,
display, button-route or low-current proof. GUI separately reported host-preview
round trips and distinct rendered frames.

## Hardware Steps

1. Keep the firmware that passed HOME/AWAY. No reflash is required. The updated
   installed-print script only changes fixture descriptions, not probe layout.
2. Use the normal healthy battery/USB setup. Transfer the new egg through USB
   FLASH; leave only the intended test egg eligible for package scanning. Eject
   and reclaim normally, then PACKAGE INSTALL -> VALID -> INSTALLED -> A PLAY.
   This replaces the installed HOME/AWAY package. Do not use development-enable
   or embedded-install helpers.
3. Expect a static Lobby title, button hint and border. A enters Garden; B in
   Garden returns to Lobby. Repeat several times, releasing buttons between
   presses. B in Lobby and A in Garden have no authored route.
4. Leave each scene idle, then use its routed button to wake and switch. Confirm
   the displayed title/hint and border remain correct. There are no animations
   or timers; no timed return or square reveal is expected.
5. HOLD START opens the shell; Resume should restore the scene. Deliberately
   reboot and require Lobby again. Reconnecting GDB is not a reboot.

Wake normally and halt after the screen has settled:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

Require installed source/model/bytes=3/2/2228, scene count=2, Lobby=2 or Garden=1,
state=1, successful completed admission and display, and scene replacements
without failure. Counters only establish completed work when status is also
successful; physical rendering requires the observed screens. Timer counters
can retain earlier package activity and do not prove this fixture has timers.
Static scenes do not need the HOME/AWAY four-frame/400 ms playback schedule.
Saved stack margin is not a measured worst-case high-water value.

On failure, collect the existing package workflow and persistent install print
helpers before resetting. Public export, remaining timer acceptance cases and
production capability promotion remain separate work after this fixture.
