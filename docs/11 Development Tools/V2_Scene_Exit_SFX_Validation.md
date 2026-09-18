# V2 Scene-Exit SFX Validation

Status: firmware implemented; input-triggered HW6 bench behaviour confirmed.
API 50/profile revision 5 now admits public SFX-only scene exits. See
[[Peep_Studio_V2_Scene_Exit_SFX_Handoff]].

The Audio Contract defines ordering and failure semantics. Runtime owns the scene
transaction; thAudio owns playback. Input routes and timer handlers share the
replacement path. Outgoing object, variable, timer and shell mutations remain
rejected. Cues become available once in authored order after successful destination
admission and commit, not after panel transfer. They may overlap: this is not a
playlist or wait-for-audio transition. Audio failure does not roll back a scene.

## Host Checks

Native installed-package tests exercise real private raster admission, two ordered
exit cues, repeated round trips, input and timer exits, destination rejection
without effects/source mutation, and package audio catalog lifetime.
Public builds now reproduce the tested fixture bytes. Existing shell-action
and malformed-audio rejection tests remain in place.

## Bench Fixture

Generate with:

```powershell
python tools/authoring/build_scene_exit_sfx_fixture.py --egg-output build/fixtures/scene_exit_sfx.egg
```

Size: 54,660 bytes. SHA-256:
`610119f8944e1965cb4b29e081085eb957469b33db6cc7c0c8fe71849cac0f5c`.
This uses the OS development encoder, not a newly advertised public capability.
Flash updated normal Debug firmware, then transfer this egg through the existing
MSC install workflow. The embedded-install helper installs the embedded artifact,
not this file; do not use it for this test.

- Boot enters HOME. Its bottom square fills after two seconds, silently.
- A in HOME enters AWAY and starts one approximately six-second cue.
- B in AWAY returns HOME and starts one short cue. An active long cue continues
  across this scene change.
- A in AWAY and B in HOME do nothing and must not play a cue.
- L/R moves the marker silently without restarting the numbered animation.
- AWAY has no automatic return. Each scene entry is fresh.
- HOLD START during the long cue stops/discards it. Resume remains silent.
- After audio drains and controls are released, normal low-power playback returns.

Observe first, without halting during audio. Then use:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_scene_exit_sfx_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

Require audible cues, unchanged visuals, successful replacement/admission, zero
faults/underruns and no outstanding audio or held audio clock after drain.
Dispatch alone is not audible playback; WFI counters alone do not measure current.
Record physical observations before widening export or asking Studio to expose
exit SFX. Timer-triggered exit SFX and multi-cue ordering have host coverage;
this fixture physically exercises one cue per input-triggered exit only.

## HW6 Result (2026-09-18)

The operator confirmed the requested bench behaviours, including audible exit
cues, continuity across replacement, silent no-op inputs, and shell stop/discard
without replay on Resume. Installed source/model were 3/2, size 54,660 bytes,
slot 0, generation 30. The snapshot does not independently verify the file hash.

- Replacement attempts/failures/status: 5/0/0.
- Committed cues/dispatches/audio-owner requests: 5/5/5; send/owner status zero.
- Voices/outstanding requests/audio clock held: 0/0/0 after drain.
- Decoded samples: 43,520; underruns and package audio faults: zero.
- Package audio stop request/complete/status: 6/6/0; admission unblocked.
- Scene admission token/complete/lease/status: 20/20/0/0.
- Display request/complete/result/lease fault: 16/16/0/0.
- Animated schedule: four steps at 400 ms, eight chunks, 4,672 bytes.
- WFI returns/measured/reconciled: 10/10/10; clock status zero.
- Timer due/applied/error: 3/3/0.

These results plus the operator's observations support the input-exit SFX bench
pass. Counts are cumulative, not isolated per-button measurements. The backend
status 1 and automatic-entry NOT_RUN snapshot are not themselves successful
sleep-entry results; retained completed WFI/reconciliation counts are separate
evidence. No new current, rail or reboot measurement was supplied. Timer-exit
SFX and multi-cue ordering remain host-tested rather than physically tested here.

The subsequent API 50 backend increment publishes the supported action subset
without changing firmware, wire format or source schema. Studio should use the
advertisement and whole-project readiness, not special-case the fixture.
