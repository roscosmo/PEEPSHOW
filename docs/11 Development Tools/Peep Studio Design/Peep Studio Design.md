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

### Emulator

- Emulator transport controls sit above the display: Reset, Play/Pause, and
  Step Forward.
- Dotted controls labelled as reserved are drawing annotations only. Peep Studio
  must not render empty or disabled placeholder controls.
- The display preview meets its display frame directly rather than floating in a
  second workspace-style container.
- Physical controls sit below the display, with the joystick on the left, Start
  between the joystick and face/shoulder controls, and L/R/A/B on the right.
- Scene and active-state identity belong to the emulator assembly.
- Placement of the complete emulator within the application shell remains open
  until the general Peep Studio layout is drawn and approved.

### Prefabs

- Menu prefab drawings show the intended future abstraction, including named
  menu items, generated destinations, selection variables, cursor ownership,
  and optional loop-around behavior.
- Prefab sketches remain direction-only until the service, compiler, preview,
  and runtime expose typed prefab slots and generated ownership metadata.

## Bring-Up Mapping

- **Implemented:** Package Entry, Scene Flow cards and references, Local Logic
  scene-boundary nodes, the shared transition-line visual language, and the
  persistent scene-rooted project hierarchy across all workspaces.
- **Current bring-up:** inspector-owned variables, ordered conditions and
  effects, including service-validated variable and retained-object changes.
- **Wait for the general layout drawing:** application-shell and emulator
  placement changes.
- **Wait for runtime contracts:** editable prefabs, hierarchical-state behavior,
  restorable navigation, and capability-gated PeepOS triggers.
