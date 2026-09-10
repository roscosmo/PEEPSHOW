# Four-Frame Native V2 Timer Fixture

Separate source-only follow-up to the HW6-passed `native_v2_scene_timer.peepproj`.
That passed fixture is unchanged, including its assets and documentation.

## Visible Sequence

The scene-owned sprite displays large digits **1, 2, 3, 4**, then loops.
Each frame has the same 24x24 bounds at (72,40) and lasts 400 ms. The full loop
is 1600 ms. Frames are build-time system-font assets at scale 3, not runtime text
or variable-driven frame selection. There are no external image dependencies.

At the two-second scene timer expiry, the sprite should display **2**, not **1**.
Timer expiry is deliberately not aligned with a return to the first frame.

Everything else follows the previous timer fixture:

- A: Marker Left to Marker Right. B: return to Marker Left.
- A separate square at Y=104 changes X from 32 to 120 through a state override.
- The scene timer reveals another square at (76,80) once after 2000 ms.
- Neither state overrides the numbered sprite. Expiry has no target state.

For hardware observation, press A or B while 2, 3 or 4 is visible. The sequence
must continue, not jump back to 1. The revealed timer marker must stay visible
through further state changes.

## Verification And Handoff

Host checks verify four distinct rendered framebuffers in sequential order,
loop wrap, unchanged playback snapshots across A/B, and frame 2 after timer
expiry. The original two-frame fixture still passes the read-only host checks.

From `tools/peep-studio`, with the authoring Python selected:

```powershell
node tests/native-hardware-fixture.cjs --four-frames
```

Ordinary V2 export stays disabled. No egg or firmware output is included.
OS owns development egg generation. This new four-frame fixture is host-verified
only; it does not inherit the previous fixture's hardware pass.
