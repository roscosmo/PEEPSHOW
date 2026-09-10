# Peep Studio UX Direction

Status: `active_direction`

Peep Studio must feel like a simple visual state-machine authoring tool, not a
JSON/schema editor. The Python authoring service remains the semantic authority,
but the GUI should translate project records into user-facing concepts.

## Product Shape

- The main workspace is the scene logic graph: states and transitions should get
  the largest area by default.
- The local logic graph should feel like a node/shader editor, not a UML state
  chart. State and prefab nodes own their trigger outputs inside the node card,
  and graph lines connect from those outputs to their single destination.
- The display preview belongs at the top of the inspector during normal logic
  editing, scaled to fit without scrollbars.
- A placement mode should promote the display preview into the main workspace so
  visual elements can later be selected, moved, aligned, and ordered.
- Low-level field names such as `route`, `set_variable`, `variable_ref`, and
  `request_render` should not be the default authoring language.
- Advanced/internal labels may remain available in debug details, tooltips, or a
  later advanced view, but the primary editor should use plain language.

## Design Source And Review

The active node-design reference may be a shared Excalidraw board. When a board
is used, the designer should attach or export the `.excalidraw` source, a PNG,
or both before implementation starts. The current live reference is:

- [Peep Studio design index](<Peep Studio Design/Peep Studio Design.md>)
- canonical board:
  `docs/11 Development Tools/Peep Studio Design/Peep-Studio_Design-notes.excalidraw`

The design index is a living summary of approved visual semantics,
implementation status, and unresolved layout work. A drawing change that alters
those decisions must update the index and relevant behavior contract before the
change is implemented. The summary should not duplicate every coordinate or
exploratory mark from the board.

Peep Studio work should treat that drawing as the visual design source and these
docs as the behavior source. Before changing graph interaction patterns, the
agent should summarize the drawing into concrete implementation notes covering:

- node-card anatomy;
- connection zones and handle behavior;
- where triggers, destinations, and transition effects are visually separated;
- drag, selection, and delete behavior;
- inspector responsibilities;
- compact and desktop layout expectations.

The agent should confirm any unclear interaction in plain English rather than
guessing from the drawing.

## User-Facing Vocabulary

- State: a named screen or moment in the interaction.
- Prefab node: a reusable self-contained authoring block, visually distinct from
  a custom state node.
- Transition: what moves from one state to another.
- When: the input that triggers a transition.
- Only if: conditions that must be true before the transition can happen.
- Then: effects that happen when the transition runs.
- Screen: the 168x144 visual output for the selected state.
- Output: a routable result exposed on the right side of a node card. Each
  output connects to at most one destination.

## Local Logic Graph Target

Peep Studio should present local STATE logic as dynamic node cards:

- a new custom state node starts with no exits;
- choosing a trigger, such as Button A, adds a visible exit row to that node;
- state entry is a card-level connection affordance, not a separate editable
  entry row. The visual design may show green oval entry handles around the
  top/bottom/corner entry zones, but these handles only mean "a transition can
  arrive here";
- side edges are reserved for exit rows. The editor should choose the left or
  right side of each exit automatically to reduce line overlap, with manual
  refinement available;
- automatic transition routing prefers the shortest clear orthogonal path.
  Node intersections, overlapping lines, backtracking, excess bends, and path
  length are penalized in that order. An outside loop is only appropriate when
  a compact route is obstructed;
- manually adjusted transition rails remain authoritative when connected nodes
  move. Endpoint segments may be re-anchored or simplified when invalid, but a
  small movement elsewhere must not reset the author's route;
- State Graph transitions use the Excalidraw language of thick blue dashed
  paths with solid blue direction arrows. The green Scene Entry connection is
  distinct and remains an entry affordance rather than a triggered transition;
- each exit row summarizes the trigger and shows condition badges, such as
  "Button A - 1 condition";
- effects that happen during a transition should be shown on the transition
  line as a compact transition-effect chip/node where useful. This separates
  "what caused the move" from "what changed during the move";
- condition and effect chips may be dragged anywhere along the transition line.
  Their visual order follows guard evaluation and action execution order, so
  adjacent chips constrain one another and cannot be dragged past each other;
- outputs are grouped by trigger type where useful: buttons, timers,
  variable/condition events, and system events;
- each exit has a single destination to preserve state-machine clarity;
- selecting a state card in Local Logic changes author selection only. It does
  not pause, reset, or reposition the emulator;
- each state hierarchy row and the selected-state inspector provide an explicit
  `Load in emulator` command for choosing the state from which the preview run
  starts;
- the emulator's current state uses a dedicated runtime indicator that remains
  visually distinct from author selection and follows transitions without
  moving the author's selection;
- while the emulator runs, the State Graph viewport keeps the runtime-active
  state within a padded visible area. It pans only when that state leaves the
  area and does not change the current zoom, node positions, manual route
  layout, or author selection;
- Reset returns to the state from which the current preview run was launched.
  Selecting a scene instead launches from its declared entry state;
- advanced/internal route IDs remain available for debugging, but are not shown
  on the node face by default.
- Local Logic Scene Entry and Scene Exit use compact grey boundary cards. Scene
  Entry has a non-connectable green directional indicator and one red circular
  output into local logic. Scene Exit has one green circular local input and a
  non-connectable red directional indicator. The board currently uses diamonds
  as directional placeholders; a pointed or triangular treatment is preferred
  because these indicators are not sockets.

Semantic transitions still compile through the Python authoring service as
bounded routes, guards, actions, and target states. React must not create
graph-only behavior that the Python service cannot validate or compile. Visual
transition-effect chips are presentation of existing service-owned route
actions. Their manually adjusted path positions are typed editor-only metadata;
they never change route semantics or compiled package output.

State cards may use badges for author-facing summaries:

- **Variables**: local state variables changed or tested by the state's exits,
  such as counters, menu selection, gold, age, or flags.
- **Objects**: retained scene objects changed by the state's entry behavior or
  transition effects, such as sprites, text sprites, primitive shapes, position,
  visibility, frame choice, or animation choice.

Exact badge counts should come from normalized service data or service-provided
summary fields. React should not infer package semantics from raw JSON in ways
the Python service cannot validate.

## Hierarchical State UX Target

A composite state groups one active child region and owns handlers shared by
its descendants. In a menu, Start Game, Settings, and Credits can be child
states of Menu Selection while Menu Selection owns one R handler. The graph
must not display or persist three generated copies of that parent handler.

Event behavior is presented in author language:

- the active child gets the trigger first;
- an enabled child handler consumes it;
- otherwise the trigger is offered to its parent;
- a parent action that does not change state leaves the selected child active;
- an explicit **Block inherited trigger** handler consumes an event without an
  action when a child must suppress the parent fallback.

Composite-state presentation must follow the live Excalidraw design language.
Before implementation, the design pass must choose whether children expand
inside the parent card, open as a focused nested canvas with breadcrumbs, or
support both. Regardless of that choice:

- the scene hierarchy remains visible and identifies the active state path;
- a composite card visibly differs from a leaf without resembling a prefab;
- parent-owned triggers remain visible on the composite card;
- a focused child graph shows inherited parent handlers without duplicating
  editable rows;
- selecting a parent handler opens one inspector record and one action order;
- moving or laying out child nodes is editor-only and does not alter hierarchy;
- the emulator reports the complete active path, not only the leaf label.

History controls use plain labels rather than requiring UML terminology:

```text
On re-entry
  Start at initial substate
  Remember previous substate
  Remember complete nested state
```

The inspector may disclose shallow/deep history terminology in advanced help.
Package launch remains fresh unless a later package-resume contract explicitly
says otherwise.

Scene Flow navigation must distinguish semantic behavior:

- **Go to** replaces a scene;
- **Go to and resume** restores remembered destination state;
- **Open** retains the current scene context;
- **Return** resumes the retained context.

These labels cannot be implemented as renderer-only variants of the existing
editor Go To reference card. They require typed service-owned navigation data
and target-profile capability support.

### Hierarchical Bring-Up Plan

1. **Contract and capability slice**: agree on event bubbling, internal-handler
   behavior, history fallback, navigation modes, runtime bounds, and suspended
   timer/audio policy. Publish capability fields before enabling controls.
2. **Service model and validation**: add composite/parent records, initial-child
   rules, internal handlers, cycle checks, deterministic ordering, history
   validation, and normalized inherited-handler summaries.
3. **Compiler and preview parity**: flatten hierarchy into bounded dispatch
   tables, expose the active path in traces, and prove child-first dispatch and
   no-reentry internal actions with deterministic tests.
4. **Local Logic UI**: create/reparent states, enter and leave a composite,
   author parent handlers once, inspect inherited behavior, and preserve nested
   editor layouts. Resolve the exact canvas modality against Excalidraw first.
5. **State history**: expose initial, shallow, and deep re-entry choices only
   after service preview and compiled execution agree.
6. **Restorable scene navigation**: add Go to and resume, Open, and Return after
   bounded runtime scene contexts and suspend/resume rules are exposed.
7. **Acceptance example and hardware gate**: rebuild the menu example so R is a
   parent handler, Settings/Credits return to the previous selection, package
   launch starts at selection one, and the same trace passes service, Studio,
   package-build, and current-HW6 tests.

Do not start with prefab abstraction. A future Menu prefab may generate or own
this hierarchy only after ordinary composite states, inherited dispatch,
history, and restoration are robust and inspectable.

## Prefab And Menu Nodes

Users should not need to build common menus from empty low-level states.
Reusable menu behavior belongs in prefab-backed nodes and Authoring Kits. This
is an intended direction, not the immediate implementation target. Peep Studio
must first make ordinary STATE logic, placement, assets, scene flow, and package
export robust through the Python service before prefab-backed authoring is made
editable.

A menu prefab node should be self-contained and customizable through slots:

- menu type;
- item labels and item count;
- selected-item variable;
- selection marker visual;
- default navigation input template;
- confirm/cancel behavior;
- one routable output per menu item choice.

Navigation inside a standard menu prefab is handled by the prefab by default.
The node can show greyed-out generated internals in the inspector so authors can
understand what is happening, but those internals are not directly editable while
the node remains prefab-backed.

Prefab outputs may represent local logic or a future scene transition. Scene
transition outputs belong in the package scene-flow graph and must remain
disabled for export until PeepOS supports multi-scene dispatch.

Authors who need behavior outside the prefab slots should create a custom node
or copy/convert the prefab into custom editable STATE records.

Prefab internals must come from typed service-owned records. The renderer must
not special-case the current example project, object names, scene names, or demo
focus roles to simulate prefab behavior.

## Action Authoring Target

The scene inspector owns variable creation, integer range editing, and deletion.
Variable IDs are stable after creation; referenced variables cannot be deleted
until their conditions and effects are removed.

Raw actions should be presented as readable effects:

- `set_variable` with `add`: "Change [counter] by [+1]".
- `set_variable` with `assign`: "Set [counter] to [1]".
- `set_element_visibility`: "Show/Hide [object]".
- `set_element_position`: "Move [object] to [x], [y]".
- `set_element_frame`: "Change [object] frame to [frame]". Frame choices are
  restricted to the selected sprite asset.
- `set_element_waiting_animation`: "Change [object] animation to [animation]"
  with explicit preserve/restart timing behavior.
- `play_sfx`: "Play sound [sound name]".
- `request_render`: backend-only refresh work; it should not be shown as an
  author effect.

The transition inspector creates, deletes, and reorders conditions and
author-visible effects through typed service commands. Effect order is runtime
execution order and must match the symbols shown along the transition. System
actions such as `request_render` and `exit_to_shell` remain hidden from this
ordered author list.

## Sound Authoring Target

Sound controls belong in the Assets workspace and Local Logic inspectors, not in
Placement.

- Assets should present package-backed sampled SFX as **Sounds**.
- Import should accept supported WAV sources and create an author-facing sound
  plus its default cue through the Python service.
- The sound card should show duration, packaged size, volume, and audition.
- Audition plays the exact compiled package bytes on the host. It is useful for
  content checking, but it is not HW6 timing, power, or speaker-quality proof.
- Local Logic and Scene Flow should expose sound playback as **Play sound**.
- Music, looping, procedural audio, and arbitrary mixing should be visibly
  unavailable until the service and target profile expose them.

## Layout Target

Normal logic mode:

- left: fixed emulator/display preview followed by the persistent project scene
  hierarchy;
- center: large per-scene state graph;
- right: selected-state or selected-transition controls.

The left hierarchy is shared by Scene Flow, Local Logic, Placement, and Assets.
A scene is always the highest authoring owner. Expanding it exposes Scene Base
objects, states and their local object changes, and scene-local variables. It
must not be replaced by a separate workspace-specific Placement tree. Scene,
Base, States, each state, and other grouping rows are independently
collapsible. Disclosure arrows change only expansion state; they never change
the selected scene, state, object, workspace, or placement edit target. Clicking
the remainder of a selectable row changes selection without implicitly
expanding or collapsing it.

State-row status must not rely on similar competing outlines. Author selection
uses a strong blue left rail and blue-tinted row. Emulator activity uses a green
runtime dot. The Placement workspace's primary preview uses an amber eye marker
that is not shown in other workspaces. Selecting a state outside Placement does
not change the Placement preview target or reset the emulator.

Scene exits do not appear in the hierarchy because they are semantic graph
links rather than tangible scene children. Sprite, text, and audio source
assets remain package-owned reusable resources in Assets and do not receive a
second hierarchy branch. A placed sprite or other asset-backed scene element
appears as an ordinary object under Scene Base or the state that changes it.

Scene-flow mode:

- left-to-right storyboard graph for moving between scenes or major screens;
- scene nodes use the shared Excalidraw visual language: a centered scene name,
  dominant screen preview, attached green scene-entry capsule, aligned
  scene-exit stems and nodes, and a dotted **Add new exit** row;
- scene cards do not show an internal-state count badge. State counts become
  misleading as prefab-backed authoring replaces exposed low-level states;
- Package Entry is a standalone movable green **Start** node with exactly one
  red output. Its visual output may use any side chosen by automatic routing or
  later manual refinement. Connecting it to a scene entry changes the package
  entry scene;
- reusable **Go to <scene>** nodes are editor-only visual references to an
  existing scene. Multiple scene exits may connect to one reference so a
  backward semantic jump can still read left-to-right in the storyboard;
- deleting a Go To reference removes only the visual alias. Attached scene
  exits retain their authored destination and fall back to the real scene node;
- adding a scene-flow exit creates the matching local scene-exit in that scene's
  Local Logic graph, and creating a local scene-exit exposes the matching exit
  in Scene Flow. These are the same authored link viewed from two workspaces;
- nodes should show small screen previews, not raw scene IDs;
- selecting a scene opens a scene-specific inspector with editable display
  name, entry-state summary, scene type, declared exits, and stable ID. Package
  Entry and Go To nodes have their own contextual inspectors;
- initial connections use automatic left-to-right orthogonal routing. Manual
  Scene Flow route refinement follows after the node language is stable;
- direct STATE-to-STATE scene replacement is now exposed for the proven HW6
  target-profile subset; push/pop, return stacks, and other scene types remain
  unavailable until the service exposes them.

Placement mode:

- left: the fixed emulator/display preview and the same persistent scene
  hierarchy used by every workspace;
- center: large scaled screen preview;
- right: inspector for selected visual elements and placement controls.

The placement branches within the scene hierarchy mirror semantic ownership
rather than flattening the resolved framebuffer:

```text
Scene
  Base objects
    scene-owned objects
  States
    State A
      objects changed by State A
    State B
      objects changed by State B
```

`Base Placement` is a first-class edit scope. State rows contain only local
object variations; they do not imply separate screens. An object inherited
unchanged by at least one declared state remains visible under Base Placement
and may also expose local changes under applicable states. An object with local
placement properties in every declared state is presented only beneath those
states, even though normalized storage retains its stable scene-level element
record. Explicit state-scoped objects likewise do not appear under Base. With
hierarchical states, child state rows nest beneath their composite parent and
inherit the parent's placement changes.

Placement target selection follows conventional tree-selection behavior:

- plain click selects one state exclusively and makes it the primary preview;
- Ctrl-click toggles individual state scopes;
- Shift-click selects a contiguous range of sibling states;
- Ctrl+A selects all state siblings in the focused hierarchy level;
- the most recently clicked selected state remains the primary preview while
  edits apply to the complete selected set;
- clicking Base Placement clears state selection and previews/edits the scene
  defaults;
- Base Placement and state scopes cannot be selected together;
- an ancestor and its descendant cannot both be edit targets because the
  ancestor already applies through inheritance.

There are no state-target checkboxes and ordinary row clicks never accumulate
targets. Selecting all currently declared states remains an explicit fixed set;
it is not normalized to Base Placement and does not include states created
later.

Adding an object uses the selected scope:

- Base Placement creates a scene-wide object inherited by current and future
  states;
- one selected state creates an object visible in that state scope;
- multiple selected states create one stable object visible in exactly those
  scopes;
- selecting a composite parent makes the object visible through that parent's
  descendants, including future descendants.

When a compatible multi-frame sprite is added to Scene Base, its default
waiting animation is bound across every current state while the retained object
itself remains scene-owned. When it is added to an explicit state set, the
animation is bound only in that set. The Placement animation toggle uses the
same scope rule and reports mixed state animation without converting the object
to state ownership.

The inspector shows a read-only edit-target breadcrumb instead of an
"Applying edits to" checkbox panel. Each editable property identifies whether
its value is defined here or inherited, identifies the inherited source when
useful, and provides a reset action for removing the local override. Resetting
an override must reveal the inherited value; it must not write a copied value
back into the state.

When multiple states are selected, the hierarchy clearly distinguishes the
primary preview state from the complete edit selection. The center canvas shows
the resolved primary state while each mutation is applied atomically to the
explicit selected-state set.

## Current GUI Bring-Up Plan

This plan is ordered by dependency. Regressions that make authored content
disappear or execute incorrectly are resolved before adding new interaction
surfaces. Each stage is exercised from a newly created project rather than by
depending on IDs or structure from the checked-in example.

### 1. Stability And Authoring Trust

- Diagnose and fix the save refresh regression where Placement retains an
  object's selection box but stops drawing its content until the workspace is
  changed.
- Diagnose and fix the regression where newly placed multi-frame sprites no
  longer animate.
- Clear incompatible inspector selections when workspace or object selection
  changes. Placement must not retain unrelated sprite-frame and audio records
  as simultaneous implicit selections.
- Hiding Placement object boxes hides only guides, labels, and bounds. Objects
  retain stable canvas hit targets and remain selectable and movable.
- Restore effect creation for every currently advertised non-audio transition
  action. Unsupported actions remain absent rather than failing after input.

This stage is complete when save/reload, workspace switching, selection,
movement, animation, and transition-effect editing remain coherent in the
blank-project acceptance project.

### 2. Selection, Keyboard, And Inspector Coherence

GUI refinements (compatibility-only work against API 38):

- Implemented: the project row is the expandable hierarchy root directly below the
  emulator. Scenes are its direct children; remove the redundant Scenes
  classification row. Project selection still opens package-wide inspector
  controls. This is hierarchy presentation, not scene-object migration.
- Implemented: double-clicking a scene card in Scene Flow opens Local Logic,
  without double-click graph zoom.
- Implemented: a single click on a parent hierarchy row selects it. A double-click
  expands/collapses its child rows; this must not hide scene objects in the
  emulator or change authored visibility. Disclosure arrows remain independent
  of selection and can expand/collapse without disturbing the current target.
- These interactions are separate: scene-card double-click navigates to logic;
  hierarchy parent-row double-click toggles expansion, including scene rows.
  A hierarchy double-click must not also navigate to another workspace.
- Implemented: transition-action inspector uses labelled rows for action type,
  object, variable, frame, animation timing and SFX. The absolute action is
  Set position, with paired X-from-left and Y-from-top pixel fields. Graph
  tooltips use the same terminology. Execution order and compact graph symbols
  are unchanged. `tests/action-inspector-check.cjs` covers narrow layouts,
  edits, ordering, deletion and read-only controls. Relative Move by remains
  unavailable until the OS agent hands over a supported shared-backend
  increment; agreed author-facing axes are positive X right and positive Y up,
  with displacement applied to underlying mutable position and edge clamping.
  Do not introduce a GUI-only movement model or change the existing absolute
  coordinate convention as part of this layout work.

- `Ctrl+Z` invokes the existing project undo operation when focus is not owned
  by a native text-editing control. Redo retains the platform convention
  already shown by the toolbar.
- `Escape` clears object, state, transition, asset, and auxiliary inspector
  selection and selects the project root. The active scene context and emulator
  session remain valid; project-root selection does not mean that the project
  has no scene.
- The project-root inspector owns package name, target profile, path, build and
  export information, and other package-wide controls currently occupying the
  left panel.
- State names are edited directly in the selected state's primary name field;
  the inspector does not repeat the name in a separate rename form.
- Author-facing object and asset names use the same direct-edit pattern as the
  service gains the required typed rename commands.

### 3. Placement And Asset Creation

- Primitive palette tools use direct canvas gestures. A line is created from
  two selected endpoints; rectangle, filled rectangle, circle, and ellipse use
  a dragged bounds gesture. The command is committed when the gesture ends and
  `Escape` cancels an unfinished draw.
- Text is a Placement palette tool. Choosing it creates build-time text at the
  selected location through the text-asset service path; authors do not visit a
  separate sprite-creation workflow merely to place a label.
- Normal sprite-sheet import asks for integer columns and rows, such as `4 x 1`.
  The service verifies that the source dimensions divide evenly and derives
  frame pixel bounds and source rectangles. Raw frame width and height are not
  normal controls.
- Assets presents one card per sprite. Its ordered frames appear as a scaled
  strip in the selected asset inspector and are not duplicated as independent
  top-level asset cards.
- Placement continues to reject records outside the 168x144 panel under the
  current authoring schema. Partly off-panel sprites are nevertheless a required
  use case so objects can enter or leave the display during movement. The next
  schema/firmware alignment must define signed bounds, host/target clipping, and
  resource accounting consistently before the GUI enables that placement.

### 4. Scene Flow Tools And Routing

Bring-up status: implemented. Scene Flow now owns its New Scene and Go To
palette, measured-node automatic routing, destination fan-out, crossing
bridges, and editor-only manual orthogonal rails. Further tuning should adjust
the shared scoring weights rather than introduce per-example route behavior.

- Remove New Scene from the project hierarchy. New Scene and Go To become
  floating Scene Flow palette tools on the left edge of the graph, matching the
  Placement palette's position, sizing, tooltips, and selected-tool behavior.
- Extend automatic Scene Flow routing to penalize node intersections,
  overlapping routes, backtracking, and unnecessary bends while preserving its
  left-to-right storyboard bias.
- When several routes enter one node, fan their final arrow segments across
  distinct entry positions so every arrow remains visible and selectable.
  This applies in both Scene Flow and Local Logic, including green entry
  connections and shared Go To/scene-exit destinations. Tips meet the visible
  socket outline, not its centre. Socket shapes and state corners stay fixed;
  only the short terminal fan is derived. Arrow hit areas are separated and
  take priority over route-section dragging. Local Logic groups arrivals by
  the chosen corner and approach side; it does not repick manually pinned
  ports. Moving nodes, retargeting arrows, and recalculating the fan preserve
  saved manual rails. Green scene-entry arrows can still target another state.
  Regression coverage includes the actual graph components in
  `tools/peep-studio/tests/graph-arrows.html`, checked at desktop and compact
  sizes by `tests/graph-arrows-check.cjs` against a running renderer.
  The scene fixture deliberately omits the optional read-only scene list. Its
  default must retain a stable identity to avoid a node-update render loop.
  Scene/state arrow selection, segment dragging and retained rails after node
  movement pass at 1440px and 760px graph viewport widths.
- Draw an unambiguous bridge on one route wherever two unrelated transition
  lines must cross. Bridges are presentation only and do not alter direction,
  selection priority, or runtime order.
- Add persistent manual Scene Flow route refinement after automatic routing and
  fan-out are stable. Authors move straight orthogonal sections, while endpoint
  joins remain attached and small node moves preserve the manual middle.

### 5. Workspace Density

- Implemented: project and inspector panels use thin vertical scrollbars with
  a stable reserved gutter so scrolling does not cover controls. Scrolling is
  retained. `tests/hierarchy-check.cjs` exercises the actual app through a
  read-only example load, checks navigation/expansion and scrollbar geometry,
  and captures desktop and compact layouts.
- Implemented: an emulator collapse control switches between the complete emulator
  and a display-only form. The display size and current preview are retained;
  transport and physical controls are hidden rather than destroyed.
- Its internal layout follows the canonical board: circular transport controls
  above a flush display; joystick left, Start and scene/state identity in the
  middle, tilted L/R buttons above B/A on the right. Reserved placeholders and
  the inactive neutral joystick control are not rendered.
- Collapse never changes playback state, preview session, audio handling, or
  editor selection. Timers and state transitions continue while playing, the
  framebuffer and runtime highlights continue updating, and expanding does not
  restart playback. The display retains its 168:144 aspect ratio and dimensions.
- Continue the general shell layout only after the living Excalidraw board
  defines the remaining workspace proportions and empty-space treatment.

### 6. Merge-Gated Audio And Hardware Acceptance

- After the next firmware/audio merge, reconcile capability versions before
  changing audio UI.
- Add direct audio naming, normalized import/conversion controls, five-SFX plus
  music-track capacity reporting, exact packaged audition, and Play sound
  transition effects only from the merged service capability surface.
- Build the acceptance project, install its current `.egg`, and exercise input,
  scene transitions, retained visuals, animation, SFX, music, shell exit, and
  STOP2 behavior on current HW6 before declaring the merged slice complete.

Hierarchical state execution, restorable navigation, PeepOS-derived triggers,
and editable prefabs remain contract-gated work. They must not be simulated in
the renderer while the executable service and target profile still report them
as unavailable.

### API 39 Scene-Object GUI Bringup

Checkpoint 1 implemented the read-only display; checkpoint 2a added supported
inspector editing and checkpoint 2b adds canvas creation, movement and deletion
alongside unchanged legacy editing. The hierarchy lists scene-owned objects once under
Base objects and state override references under their states. Placement uses
the host's resolved projections; defaults are displayed directly, without
creating legacy render models or resolving ownership in TypeScript.

- Version-2 object selection exposes defaults or the selected state's entry-time
  projection, independent-axis override names, and the authored clip reference.
  This is not a live mutable-object debugger.
- The host-preview capability requires `service.hello.scene_object_authoring`
  and per-scene `scene_capabilities`. Legacy command catalogs are not reused for
  version-2 editing. Inspector commands require both hello's command list and
  the selected scene's explicit command list and host-editing capability.
  The same capability checks apply to canvas commands. Existing-object geometry
  edits and graph mutations remain disabled until their supported increments.
- Existing palette tools create sprites from the selected frame and draw shapes
  with the same two-point gesture as legacy placement. Base creation is visible
  by default; selected-state creation passes the exact `visible_in_states` list
  to `object.add`. The host owns base-hidden/state-visible semantics.
- Sprites start with the selected static frame; an existing authored clip can
  be bound in the scene-base inspector. No inferred or state-private animation
  records are generated by the new object path.
- Canvas dragging and arrow-key nudges write only changed axes through
  `object.set_defaults` or sparse `object_override.set` commands. They retain
  current whole-object canvas bounds. Off-canvas movement remains deferred.
- Delete from base removes the scene-owned object via `object.delete`; delete
  from selected states writes visibility overrides instead. Inspector labels
  distinguish those operations. Reference-safe deletion remains host-enforced;
  a rejection retains the object and displays the host error.
- The placement inspector edits X and Y independently, visibility, and a
  size-compatible default frame. State selections issue sparse override commands
  in one batch, with inherited/overridden/mixed status and per-property reset.
  Clearing X preserves Y. No override resolution is implemented in TypeScript.
- Scene-base editing can bind an existing compatible authored clip or clear it
  to Static. State scopes show the scene animation without enabling clip edits;
  their frame field edits an explicit frame override. No state-private animation
  records are created, and clip authoring/runtime controls are not added here.
- Object edits use the existing revision, undo/redo, save and preview refresh
  lifecycle. Playback pauses during an authoring edit; this is not live runtime
  position manipulation or a promise to retain playback phase across edits.
- The emulator displays scene elapsed time instead of legacy waiting steps.
  Version-2 states do not show missing legacy waiting-animation links.
- Projects containing non-exportable scene objects show a persistent host-only
  notice and disable Build/Export, including mixed-version projects.
- Tests: `sceneCapabilities.test.ts` covers capability and presentation mapping;
  `scene-objects-check.cjs` uses an explicitly migrated temporary fixture with
  a real API 39 sidecar to check mixed-version selection, projections, rendered
  pixels, per-axis and multi-state edits, visibility/frame override clearing,
  clip unbind/rebind, two-point creation, sparse-axis drag/nudges, base movement,
  exact visibility scope, deletion/reference refusal, undo/redo, save/reload,
  disabled graph controls, and
  desktop/compact screenshots. User projects and
  repository examples are not migrated by Studio or modified by the test.

Checkpoint 3 implements transition action editing for existing version-2 routes.
Add/edit/delete/reorder submit the complete ordered list through
`object_actions.set`, preserving hidden and advanced records. The GUI exposes
relative movement (+X right, +Y up), absolute positioning (Y from top), independent
axis selection, visibility, set-frame and clear-frame actions alongside existing
variable/SFX controls. Clear-frame restores the underlying clip's current phase;
it is not an animation restart. Graph icons/tooltips identify the new actions.
Scene-replacement routes still offer only SFX. Route construction and guard
editing remain separately disabled. Handler editing is not exposed in this slice.

Real-host tests verify repeated movement, sign conventions, the differing results
of reordered set/move actions, hidden-record preservation, undo/redo and save/reload.
The test fixture is temporary; Studio does not construct unsupported graphs.

### Native Creation Priority

Migration UI is deferred by user decision. Development should prioritize new
native scene-object projects and recreating the menu example through Studio.
API 40 supplies native creation and state management. API 41 adds local graphs;
scene connections remain unavailable. Firmware development decoding/execution
does not imply authoring commands or production export exist.

Requested OS-owned increments, each advertised through hello/per-scene capabilities:

1. Delivered in API 40 and connected in Studio: version-2 initial/additional scenes,
   state creation/deletion, rename, entry selection, and state/entry layout.
2. Delivered in API 41: local input/timer graph commands. Studio connects physical
   trigger creation/rebinding, route retargeting/deletion/layout, guards and variables.
   Native timer authoring is connected in the timer increment below.
3. Scene exit creation and reciprocal local-graph endpoints/connections. Package
   entry-scene selection already exists independently.

Version 2 identifies the scene-object source format, not a required change to
the project container version. OS owns payload design and shared implementation;
Studio must not silently migrate a legacy new project as a creation workaround.
Integrate each supported increment without waiting for the whole migration or
firmware/export rollout. Keep current export restrictions until explicitly handed off.

Preserve undo/redo, save/reload, legacy editing, and visible export restrictions.
Shared schemas, compiler, service, and firmware remain OS-owned. Do not add a
parallel model or expose version-2 graph construction ahead of its handoff.

### API 40 Native Creation GUI Increment

- New projects request `scene_schema_version: 2` when `service.hello.scene_creation`
  advertises native creation. Older hosts retain their version-1 creation path.
- Additional scenes explicitly use the entry scene's source format; the host does
  not inherit version 2 when the parameter is omitted. Legacy projects stay legacy.
- State creation, rename, deletion, entry selection, and layout controls require
  both advertised state-management commands and per-scene supported commands.
  State movement and entry-arrow editing are separate from graph wiring permissions.
- Project inspector exposes entry-scene selection; scene names remain editable.
- No automatic migration, fabricated render models, or state-private animation
  records. At this increment, V2 graph construction stayed disabled (local graphs
  are enabled by the API 41 increment below); export remains disabled.
- Regression coverage uses a temporary project and the real sidecar to exercise
  native creation, state lifecycle, movement, undo/redo, scene addition, and save/reload.

### API 41 Local Graph GUI Increment

- Physical input sockets can create native local transitions and rebind triggers
  using the existing Press/Hold/Release/Repeat picker. Commands require hello and
  per-scene `local_graph_commands` plus `supported_commands`; API numbers and the
  broad graph-construction flag are not sufficient.
- Variables and guard lists use their existing shared command editors. Local
  destinations, manual routing, and transition deletion are enabled independently
  of scene connections. Ordered actions continue through `object_actions.set`.
- Scene-exit sockets remain non-connectable and their destination controls remain
  disabled for V2. No scene-flow or shared backend ownership changes are included.
- Timer bindings/handlers are exposed by the native timer increment below. Scene
  binding/handler mutations are paired; state timers use state-scoped routes.
  Unsupported OS events stay disabled. Physical-trigger creation is not reused.
- Native GUI regression coverage creates a guarded input route from scratch,
  edits its actions, checks host execution, and verifies undo/redo and save/reload.
  Migration remains deferred and V2 egg export remains visibly unavailable.

### Native Timers And HW6 Fixture

- OS reports that the exact continuity fixture at
  `5c059bfce37770319cb49f09a14246202c42e039` runs on HW6: A/B overrides work without
  visibly restarting animation. This is an awake-only development-path pass,
  not measured phase continuity, STOP2 qualification or ordinary export support.
- Native V2 timer authoring uses advertised commands and the selected profile's
  available timer source metadata, including delay bounds. Scene timers are owned
  by the scene inspector; state-entry timers are available from the state trigger
  picker. Unsupported event placeholders remain disabled.
- Scene timers expose On scene entry / By action, an action-only or local-state
  expiry destination, guards and ordered actions. They survive local state changes.
  Their independent handlers are edited in the inspector, not synthesized as
  self-routes or state-owned transitions. Graph presentation for independent
  handlers is deferred until its visual design is agreed.
- State-entry timers use one binding and a state-scoped route. Leaving and
  re-entering restarts their duration. Existing route guard/action/layout editors
  remain in use; event references are displayed as platform outputs.
- Start, Restart and Cancel timer actions target scene timers only and retain
  their position in the existing ordered `object_actions.set` editor.
- Create/delete scene bindings and their one handler in a single transaction.
  Delete removes timer wait-interest references and refuses to silently strip
  external timer actions. Undo/redo and save/reload cover the complete change.
- `examples/authoring/native_v2_scene_timer.peepproj` is the source-only handoff:
  a two-second scene timer reveals one object while A/B overrides move another.
  OS subsequently reports this timer fixture passed on HW6. Its source and
  assets are frozen unchanged for the next test; no additional power-mode or
  ordinary-export qualification is inferred from that report.
- GUI checks cover paired creation/deletion, cross-state expiry, one-shot behavior,
  action-started timers, state-entry restart, reference protection, ordered effects,
  undo/redo, save/reload and inspector layout at 1280/1440 px. Shared backend and firmware
  are unchanged. Ordinary V2 export stays disabled; migration remains deferred.

- Frozen regression fixture: `examples/authoring/native_v2_timer_four_frames.peepproj`
  at commit `de80153`.
  It retains the timer and A/B override behavior but uses four numbered 24x24
  frames (1, 2, 3, 4), 400 ms each. The 1600 ms loop does not restart naturally
  at the 2000 ms timer expiry, making an unintended reset to frame 1 observable.
  Host checks verify distinct rendered frames, sequence, wrap and continuity.
  OS reports an awake-only HW6 pass: the numbered animation visibly continues
  across A/B marker changes without resetting; the scene timer applied once with
  zero errors and its square was visible. Preserve this fixture unchanged.
- OS next owns V2 STOP2/LPBAM animation and timer continuity verification. The
  awake-only result does not establish low-power continuity. No new service
  commands or capabilities accompanied this pass; Studio continues against the
  existing advertised authoring capabilities. Ordinary V2 export remains disabled
  until the production capability handoff. Migration remains deferred.

### V2 Inspector Terminology

- Native scene overviews count scene-owned objects directly, rather than legacy
  render-model elements. State summaries distinguish scene objects from objects
  overridden by that state; these are record counts, not counts of changed fields.
- V2 inspectors omit Screen layouts, Waiting animations and their legacy links.
  V1 retains those sections and controls unchanged. This is presentation-only:
  no project data, ownership rules, migration, backend or firmware changes.
- Mixed-version GUI regression checks cover the V2 counts and hidden sections,
  then switch to V1 and verify that its legacy sections remain available.
  Frozen hardware fixtures and ordinary V2 export restrictions are unchanged.

### Timer Inspector Selection

- Scene timers and unfinished timer drafts use the shared inspector selection,
  not a second selection stored privately in the timer panel. Choosing a timer
  replaces the route/state editor; choosing a route or state clears unrelated
  timer detail. Emulator-active state remains independent of editor selection.
- A state timer with one expiry route selects that route. Its existing route
  inspector owns destination, guards and actions; the timer section adds delay
  only. Shared state-timer bindings list their routes for explicit selection.
- Scene timer actions and route actions have one editor at a time. Selecting a
  timer to inspect external references never leaves another route's action controls
  active above it. No stored action order or timer lifetime semantics change.
- Leaving a timer draft for another selection or workspace discards the draft
  without creating records. Deletion and undo of creation clear invalid timer
  selection rather than leaving a missing timer selected.
- GUI regression checks cover mutually exclusive action editors, correct action
  ownership when switching targets, state/timer transitions, draft cancellation,
  existing timer lifetimes, undo/redo and save/reload. Backend, firmware, frozen
  fixtures and export restrictions are unchanged.

### Object Action Context

- Action object pickers use the same asset and shape labels as placement.
  Duplicate labels retain their object IDs for disambiguation; stored references
  and action order do not change.
- Native movement actions show underlying and effective screen coordinates from
  the host emulator snapshot, explicitly labeled with its active state. Missing,
  different-scene or outdated-revision previews do not supply live coordinates.
- Axis-specific override notes distinguish the active emulator state from the
  authored destination state. They describe only axes touched by the action;
  they do not predict action execution or resolve runtime positions in the GUI.
- Scene timer handlers and local routes share this presentation. Legacy editing,
  shared backend, frozen fixtures and ordinary V2 export restrictions are unchanged.
- GUI checks cover readable names, coordinates, independent-axis masking,
  unavailable previews and narrow inspector layouts, plus existing timer workflows.
