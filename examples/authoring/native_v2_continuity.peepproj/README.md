# Native V2 Animation Continuity Fixture

Source-only API 40/41 integration fixture for OS hardware bringup.
Open this directory as a project in Peep Studio. Ordinary V2 export remains
disabled; OS owns development egg generation and hardware verification.

## Contents

- One native V2 scene, `main` (Animation Continuity).
- Two states: `start` (Marker Left) and `marker_right` (Marker Right).
- Scene-owned `continuity_sprite`: 8x16 at (80, 40), always visible, bound to
  `pulse_loop`. Two frames, 500 ms each, looping. Neither state overrides it.
- Scene-owned `position_marker`: filled 16x16 square, default (32, 104).
  Marker Right overrides only X to 120. Returning restores the scene default.
- Press A in Marker Left to enter Marker Right. Press B in Marker Right to return.
  No guards, transition actions, scene exits, audio, timers, or shell-exit routes.

Coordinates above use the authored display convention: X from left, Y from top.
Only the bitmap was reused from the existing example's cursor sheet; no example
logic, migration records, or legacy render/waiting models are included.

## Verified On Host

The fixture was created and saved through public authoring service commands.
A fresh load is validated by `tools/peep-studio/tests/native-hardware-fixture.cjs`:

1. At 650 ms the sprite is on its second frame.
2. A changes state and marker X; the complete sprite playback snapshot is unchanged.
3. After another 300 ms, B returns; playback still does not reset.
4. Another 100 ms crosses the loop boundary, then 500 ms reaches the second frame.
5. The marker's underlying X remains 32 throughout; only its effective override changes.

From `tools/peep-studio`, with `PEEPSHOW_PYTHON` pointing to the authoring Python:

```powershell
node tests/native-hardware-fixture.cjs
```

Default invocation only loads and previews; it does not save or export. `--create`
is for initially generating this fixture and refuses to replace a populated project.

## HW6 Result

OS reports the exact fixture at commit `5c059bfce37770319cb49f09a14246202c42e039`
runs on HW6: A/B overrides work without visibly restarting animation. This verifies
the awake-only development path. It is not a measured phase-continuity or STOP2
qualification claim. Ordinary V2 export remains blocked.
