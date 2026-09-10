# Native V2 Scene Timer Fixture

Source-only timer fixture for OS development egg generation. Ordinary V2 export
remains disabled. Open this directory as a project in Studio.

## Behavior

- One native V2 scene, two states: Marker Left and Marker Right.
- A enters Marker Right; B returns to Marker Left.
- `position_marker` is a 16x16 square at Y=104. State overrides switch X between
  the scene default 32 and an override of 120.
- `timer_marker` is a separate 16x16 square at (76,80), initially hidden.
- Scene timer `reveal_timer` starts on scene entry, expires once after 2000 ms,
  and runs `reveal_expired`. Its only action makes `timer_marker` visible.
  It has no target state, guards or self-transition.
- A/B state changes do not restart the timer or hide its resulting marker.
- The scene-owned two-frame sprite continues its 500 ms/frame loop unchanged.

## Verification

Created and saved using public API 40/41 commands. Host acceptance verifies:
local state changes before expiry, action-only expiry without state re-entry,
the independent object's visibility change, restored position overrides,
animation continuity, and no repeated expiry after another six seconds.

From `tools/peep-studio`, with the authoring Python selected:

```powershell
node tests/native-hardware-fixture.cjs --timers
```

The command loads and previews only; it does not save or export. No development
egg or firmware-generated output is included. This timer fixture has not yet
been verified on hardware. The separate continuity fixture at `5c059bf` has an
OS-reported awake-only HW6 pass; that result does not qualify this timer fixture
or V2 STOP2 behavior.
