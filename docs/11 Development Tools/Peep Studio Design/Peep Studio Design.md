# Peep Studio Design

Status: `active_visual_reference`

The canonical live drawing is embedded below. The raw `.excalidraw` file stays
authoritative so it can be edited in Obsidian or exchanged with
excalidraw.com without maintaining a second drawing copy.

![[Peep-Studio_Design-notes.excalidraw|1200]]

Viewing and editing the embedded board requires the
[Excalidraw community plugin](https://github.com/zsviczian/obsidian-excalidraw-plugin).
The vault enables `obsidian-excalidraw-plugin`, but each local Obsidian install
must install the community plugin itself.

## Living Summary

This note is the maintained bridge between the visual board and the executable
bring-up plan. It is intentionally not a transcription of every coordinate,
label, or experimental sketch.

When a drawing revision changes approved node anatomy, interaction, vocabulary,
or workspace layout, update this summary and the relevant behavior contract in
the same milestone before implementation. Moving exploratory elements without
changing an approved decision does not require documentation churn.

The drawing is the visual authority. The UX and service contracts remain the
behavioral and semantic authority. A visual experiment does not expose a new
authoring capability until those contracts and the Python service support it.

## Approved Visual Semantics

### Package Entry

- Package Entry is a large green circular node labelled **Start**.
- It owns exactly one red output.
- The output may attach to any side of the circle. Automatic routing chooses the
  initial side; later manual refinement may select another side without changing
  package-entry semantics.
- The output connects to a scene card's entry affordance and selects the package
  entry scene.

### Scene Boundary Nodes

- Local Logic presents Scene Entry and Scene Exit as compact grey name cards.
- Scene Entry uses a green directional boundary indicator and one red circular
  output into the local graph.
- Scene Exit uses one green circular input from the local graph and a red
  directional boundary indicator.
- Directional boundary indicators are non-connectable. The diamonds in the
  current board are placeholders for a clearer pointed or triangular treatment;
  they are not graph sockets or separate project records.
- The circular sockets are the only local transition connection points.
- Scene Flow and Local Logic show two views of the same service-owned scene
  entry or exit semantics; neither workspace creates a private visual-only link.
- Scene Flow uses a floating tool palette on the left edge of its canvas, in the
  same position and visual language as the Placement palette. New Scene and Go
  To are palette tools rather than permanent rows in the project hierarchy.

### Emulator

- Emulator transport controls sit above the display: Reset, Play/Pause, and
  Step Forward.
- Dotted controls labelled as reserved are drawing annotations only. Peep Studio
  must not render empty or disabled placeholder controls.
- The display preview meets its display frame directly rather than floating in a
  second workspace-style container.
- Physical controls sit below the display, with the joystick on the left, Start
  between the joystick and face/shoulder controls, and L/R/A/B on the right.
- The joystick uses four peach circular direction buttons without a neutral
  placeholder. Tilted peach L/R ovals sit above the B/A circles, in that order.
- Scene and state names sit beneath Start in the middle column. Transport uses
  circular icon controls; the collapse affordance is at the top left.
- Scene and active-state identity belong to the emulator assembly.
- The emulator may collapse to a display-only form. Collapsing hides its
  transport and physical controls without changing preview state or package
  behavior.
- Collapse is presentation-only: a running emulator continues advancing timers,
  animation, audio, and state transitions. A paused emulator remains paused.
  The same canvas stays mounted at the same size. Expanding does not reset the
  session or change editor selection; runtime graph highlighting remains live.
- Placement of the complete emulator within the application shell remains open
  until the general Peep Studio layout is drawn and approved.

### Time Nodes

- TIME NODE and TIME NODE EXAMPLE now define the approved countdown direction.
  Preserve the grey card, green corner entries, Xy/Obj indicators, name,
  COUNTDOWN label, prominent time display and connected condition rows.
- Expiry follows the rows sequentially. The first failed check takes the
  preceding step's exit; failure at the first row takes the clock default.
  Passing every row takes the last exit. Exactly one exit's ordered actions run.
- The clock-to-row and row-to-row lines express that evaluation path, not
  independent triggers. Returning to a corner restarts the full countdown.
- Editing shows configured duration; emulation shows remaining time. Other
  time modes are deferred, including the future current-time-plus-24-hours idea.
- [[Peep_Studio_Time_Node_Design_Handoff]] records the approved behavior, the
  500-step example, shared-backend gaps and implementation order. Sequential
  fallback exits are not supplied by the current single-handler timer commands.

### Prefabs

- Menu prefab drawings show the intended future abstraction, including named
  menu items, generated destinations, selection variables, cursor ownership,
  and optional loop-around behavior.
- Prefab sketches remain direction-only until the service, compiler, preview,
  and runtime expose typed prefab slots and generated ownership metadata.

## Bring-Up Mapping

- **Implemented:** Package Entry, Scene Flow cards and references, Local Logic
  scene-boundary nodes, the shared transition-line visual language, and the
  persistent scene-rooted project hierarchy across all workspaces. The emulator
  now follows the board's internal control layout and supports display-only
  collapse without changing execution.
- **Current bring-up:** stabilize save/preview/animation behavior, complete the
  blank-project authoring acceptance path, then refine Placement tools, Assets,
  and Scene Flow routing in the order recorded by the UX direction.
- **Wait for the general layout drawing:** application-shell and emulator
  placement changes.
- **Wait for runtime contracts:** editable prefabs, hierarchical-state behavior,
  restorable navigation, and capability-gated PeepOS triggers.
