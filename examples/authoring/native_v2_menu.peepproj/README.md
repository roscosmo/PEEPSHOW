# Native V2 Menu

Open this directory in Peep Studio to inspect native scene ownership, placement
and a vertically arranged local state graph. This is a separate authoring example,
not a replacement for the legacy multi-scene example or a hardware test fixture.

## Interaction

- Joystick down/up cycles Start Game, Settings and Credits, wrapping at both ends.
- Each state overrides only the selection outline's Y position.
- The border, divider, title and three menu labels are shared scene objects.
- The numbered counter at the top right is a scene-owned four-frame loop at
  400 ms per frame. Changing menu selection does not restart it.
- A has no transition yet. Settings and Credits are selection states, not linked
  scenes. Native scene-exit/connection commands are not currently advertised.
- No audio, redundant selection-variable guards, shell-exit routes or migration.

## Inspection

The hierarchy lists all eight objects directly beneath the scene. Expand the
selection outline to see its three state overrides. Selecting the object edits
scene defaults; selecting an override displays that complete state's placement
and targets the outline in that state. Other objects have no override children.
The inspector's Editing selector also allows choosing a state before it has an
override. In Local logic the states are stacked in menu order; select a state
there and use Load in emulator to launch it independently of placement selection.

The source validates and passes the backend's restricted V2 export readiness
checks. This is not a hardware qualification claim.

From `tools/peep-studio`, with `PEEPSHOW_PYTHON` configured:

```powershell
node tests/native-menu-example.cjs
```

This checks the saved example without modifying it. `--create` is only for initial
generation and refuses to overwrite an existing project.
